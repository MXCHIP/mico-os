/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "umesh.h"
#include "umesh_utils.h"
#include "core/router_mgr.h"
#include "core/link_mgmt.h"
#include "core/mesh_mgmt.h"
#include "core/vector_router.h"

enum {
    MAX_CMD_LEN = 512,
    MAX_RETRY_TIMES = 3,
    MAX_VERTEX_NUM  = 32,
};

typedef struct sync_state_s {
    uint16_t peer_sid;
    uint8_t peer_seq;
    vertex_t *cur_vertex;
    edge_t *cur_edge;
    uint16_t cmd_len;
    uint16_t cmd_dst;
    uint8_t cmd_data[MAX_CMD_LEN];
} sync_state_t;

typedef struct vector_router_state_s {
    uint8_t status;
    uint8_t heartbeat_count;
    uint8_t heartbeat_message_interval;
    uint8_t vertex_num;
    slist_t vertex_list;
    uint16_t meshnetid;
    uint8_t retry_times;
    uint8_t sync_status;
    sync_state_t *sync_state;
    ur_timer_t sync_timer;
    ur_timer_t heartbeat_timer;
    router_t router;
} vector_router_state_t;

static vector_router_state_t g_vr_state;

#define for_each_vertex(vertex) \
    slist_for_each_entry(&g_vr_state.vertex_list, vertex, vertex_t, next)
#define vertex_me slist_first_entry(&g_vr_state.vertex_list, vertex_t, next)

static void handle_topology_sync_timer(void *args);

static vertex_t *vertex_alloc()
{
    vertex_t *vertex = NULL;
    vertex = (vertex_t *)ur_mem_alloc(sizeof(vertex_t));
    if (vertex == NULL) {
        return vertex;
    }

    vertex->sid     = INVALID_SID;
    vertex->seq     = 0;
    memset(vertex->uuid, 0xff, sizeof(vertex->uuid));
    vertex->edges   = NULL;
    vertex->dist    = INFINITY_PATH_COST;
    vertex->flag.visit = UNVISITED;
    vertex->flag.timeout = 0;
    vertex->prevhop = NULL;
    vertex->nexthop = INVALID_SID;

    return vertex;
}

static void vertex_free(vertex_t *vertex)
{
    if (vertex != NULL) {
        edge_t *edge = vertex->edges;
        while (edge != NULL) {
            edge_t *tmp_edge = edge->next;
            ur_mem_free(edge, sizeof(edge_t));
            edge = tmp_edge;
        }
        ur_mem_free(vertex, sizeof(vertex_t));
    }
}

static vertex_t *get_vertex_by_sid(uint16_t sid)
{
    vertex_t *vertex;
    for_each_vertex(vertex) {
        if (vertex->sid == sid) {
            return vertex;
        }
    }
    return NULL;
}


static vertex_t *get_vertex_by_uuid(uint8_t *uuid)
{
    vertex_t *vertex;
    for_each_vertex(vertex) {
        if (memcmp(vertex->uuid, uuid, sizeof(vertex->uuid)) == 0) {
            return vertex;
        }
    }
    return NULL;
}

static ur_error_t delete_vertex_by_uuid(uint8_t *uuid)
{
    vertex_t *vertex = get_vertex_by_uuid(uuid);
    if (vertex == NULL) {
        return UR_ERROR_FAIL;
    }

    /* delete this vertex */
    slist_del(&vertex->next, &g_vr_state.vertex_list);
    g_vr_state.vertex_num--;

    /* delete all edges targeting this vertex */
    vertex_t *cur_vertex;
    for_each_vertex(cur_vertex) {
        edge_t *edge = cur_vertex->edges, *prev_edge = NULL;
        while (edge != NULL) {
            if (edge->dst == vertex) {
                if (prev_edge == NULL) {
                    cur_vertex->edges = edge->next;
                    ur_mem_free(edge, sizeof(edge_t));
                    edge = cur_vertex->edges;
                    continue;
                } else {
                    prev_edge->next = edge->next;
                    ur_mem_free(edge, sizeof(edge_t));
                    edge = prev_edge->next;
                    continue;
                }
            } else {
                prev_edge = edge;
                edge = edge->next;
            }
        }
    }
    vertex_free(vertex);
    return UR_ERROR_NONE;
}

static ur_error_t add_vertex_by_uuid(uint8_t *uuid, uint16_t sid)
{
    vertex_t *vertex;
    vertex = get_vertex_by_uuid(uuid);
    if (vertex != NULL) {
        if (vertex->sid == sid) {
            return UR_ERROR_FAIL;
        }
        vertex->sid = sid;
        return UR_ERROR_NONE;
    }

    if (g_vr_state.vertex_num >= MAX_VERTEX_NUM) {
        return UR_ERROR_FAIL;
    }

    vertex = vertex_alloc();
    if (vertex == NULL) {
        return UR_ERROR_MEM;
    }

    g_vr_state.vertex_num++;
    slist_add_tail(&vertex->next, &g_vr_state.vertex_list);

    memcpy(vertex->uuid, uuid, sizeof(vertex->uuid));
    vertex->sid = sid;
    return UR_ERROR_NONE;
}

static ur_error_t update_vertex(uint8_t cmd, uint8_t *uuid, uint16_t sid)
{
    switch (cmd) {
        case COMMAND_VERTEX_UPDATE:
            return add_vertex_by_uuid(uuid, sid);
        case COMMAND_VERTEX_DELETE:
            return delete_vertex_by_uuid(uuid);
    }

    return UR_ERROR_FAIL;
}

static ur_error_t update_edge(uint16_t src_sid, uint16_t dst_sid, uint8_t cost)
{
    vertex_t *src, *dst;
    edge_t *edge, *prev_edge;

    if (src_sid == dst_sid) {
        return UR_ERROR_FAIL;
    }

    src = get_vertex_by_sid(src_sid);
    if (src == NULL) {
        return UR_ERROR_FAIL;
    }

    dst = get_vertex_by_sid(dst_sid);
    if (dst == NULL) {
        return UR_ERROR_FAIL;
    }

    edge = src->edges;
    prev_edge = NULL;
    while (edge != NULL) {
        if (edge->dst == dst) {
            break;
        }
        prev_edge = edge;
        edge = edge->next;
    }

    if (edge != NULL) {
        if (cost != (uint8_t)INFINITY_PATH_COST) {
            if (edge->cost == cost) {
                return UR_ERROR_FAIL;
            }
            edge->cost = cost;
        } else {
            if (prev_edge == NULL) {
                src->edges = edge->next;
            } else {
                prev_edge->next = edge->next;
            }
            ur_mem_free(edge, sizeof(edge_t));
        }
    } else {
        if (cost == (uint8_t)INFINITY_PATH_COST) {
            return UR_ERROR_FAIL;
        }

        edge = (edge_t *)ur_mem_alloc(sizeof(edge_t));
        if (edge == NULL) {
            return UR_ERROR_MEM;
        }

        edge->dst = dst;
        edge->cost = cost;
        edge->next = NULL;
        if (prev_edge == NULL) {
            src->edges = edge;
        } else {
            prev_edge->next = edge;
        }
    }
    return UR_ERROR_NONE;
}

static uint16_t scan_new_edges()
{
    neighbor_t *neighbor;
    uint16_t newedges;
    vertex_t *vertex;

    newedges = 0;
    for_each_vertex(vertex) {
        neighbor = get_neighbor_by_sid(umesh_get_meshnetid(), vertex->sid, NULL);
        if (neighbor != NULL) {
            uint16_t src, dst;
            uint8_t  cost;
            src = vertex_me->sid;
            dst = vertex->sid;
            if (neighbor->state == STATE_INVALID ||
                neighbor->stats.link_cost == LINK_COST_MAX) {
                cost = (uint8_t)INFINITY_PATH_COST;
            } else {
                cost = (uint8_t)((neighbor->stats.link_cost) >> 4);
            }
            if (update_edge(src, dst, cost) == UR_ERROR_NONE) {
                newedges++;
            }
        }
    }

    return newedges;
}

static void print_routing_table(void)
{
    vertex_t *vertex;
    printf("routing table:\n");
    for_each_vertex(vertex) {
        printf("%04x: ", vertex->sid);
        if (vertex->edges != NULL) {
            printf("edges");
            edge_t *edge = vertex->edges;
            while (edge != NULL) {
                printf("-(%04x,%d)", edge->dst->sid, edge->cost);
                edge = edge->next;
            }
            printf(", ");
        }
        printf("dist-%d, ", vertex->dist);
        printf("flag-%02x", *((uint8_t *)&vertex->flag));
        if (vertex->prevhop != NULL) {
            printf(", prevhop:%04x", vertex->prevhop->sid);
        }
        printf(", nexthop:%04x", vertex->nexthop);
        printf("\n");
    }
}

static vertex_t *get_next_visiting_vertex()
{
    vertex_t *vertex;
    vertex_t *min_dist_vertex = NULL;
    for_each_vertex(vertex) {
        if (vertex->flag.visit == UNVISITED && vertex->dist != INFINITY_PATH_COST) {
            if (min_dist_vertex == NULL) {
                min_dist_vertex = vertex;
            } else if (vertex->dist < min_dist_vertex->dist) {
                min_dist_vertex = vertex;
            }
        }
    }
    return min_dist_vertex;
}

static void figure_out_routing_info(void)
{
    vertex_t *vertex;
    for_each_vertex(vertex) {
        vertex_t *prevhop = vertex;
        if (prevhop->prevhop == NULL) {
            vertex->nexthop = INVALID_SID;
            continue;
        }
        while (prevhop != NULL) {
            if (prevhop->prevhop == vertex_me) {
                vertex->nexthop = prevhop->sid;
                break;
            }
            prevhop = prevhop->prevhop;
        }
    }
}

static void dijkstra(void)
{
    vertex_t *vertex;
    for_each_vertex(vertex) {
        vertex->dist = INFINITY_PATH_COST;
        vertex->flag.visit = UNVISITED;
    }
    vertex_me->dist = 0;

    while ((vertex = get_next_visiting_vertex()) != NULL) {
        vertex->flag.visit = VISITED;

        edge_t *edge = vertex->edges;
        while (edge != NULL) {
            if (edge->dst->flag.visit == UNVISITED) {
                uint16_t alt_dist;
                alt_dist = vertex->dist + edge->cost;
                if (alt_dist < edge->dst->dist) {
                    edge->dst->dist = alt_dist;
                    edge->dst->prevhop = vertex;
                }
            }
            edge = edge->next;
        }
    }

    figure_out_routing_info();
}

static void restart_topology_sync_timer()
{
    ur_start_timer(&g_vr_state.sync_timer, TOPOLOGY_SYNC_TIMEOUT,
                   handle_topology_sync_timer, NULL);
}

static ur_error_t resend_last_command()
{
    ++g_vr_state.retry_times;
    if (g_vr_state.retry_times < MAX_RETRY_TIMES) {
        ur_router_send_message(&g_vr_state.router, g_vr_state.sync_state->cmd_dst,
                               g_vr_state.sync_state->cmd_data, g_vr_state.sync_state->cmd_len);
        ur_start_timer(&g_vr_state.sync_timer, TOPOLOGY_SYNC_TIMEOUT,
                       handle_topology_sync_timer, NULL);
        MESH_LOG_DEBUG("vector router: timeout resend to %04x, len = %d",
                       g_vr_state.sync_state->cmd_dst, g_vr_state.sync_state->cmd_len);
        return UR_ERROR_NONE;
    }
    return UR_ERROR_FAIL;
}

static void send_topology_sync_ack()
{
    router_command_t *cmd;

    cmd = (router_command_t *)g_vr_state.sync_state->cmd_data;
    cmd->cmd = COMMAND_TOPOLOGY_SYNC_ACK;
    cmd->seq = g_vr_state.sync_state->peer_seq;
    cmd->sid = vertex_me->sid;
    g_vr_state.sync_state->cmd_len = sizeof(router_command_t);
    g_vr_state.sync_state->cmd_dst = g_vr_state.sync_state->peer_sid;
    g_vr_state.retry_times = 0;
    MESH_LOG_DEBUG("vector router: send topology sync ack to %04x, len = %d",
                   g_vr_state.sync_state->peer_sid, g_vr_state.sync_state->cmd_len);
    ur_router_send_message(&g_vr_state.router, g_vr_state.sync_state->peer_sid,
                           (uint8_t *)cmd, sizeof(router_command_t));
    restart_topology_sync_timer();
}

static void send_topology_sync_select()
{
    router_command_t *cmd;

    cmd = (router_command_t *)g_vr_state.sync_state->cmd_data;
    cmd->cmd = COMMAND_TOPOLOGY_SYNC_SELECT;
    cmd->seq = ++vertex_me->seq;
    cmd->sid = vertex_me->sid;
    g_vr_state.sync_state->cmd_len = sizeof(router_command_t);
    g_vr_state.sync_state->cmd_dst = g_vr_state.sync_state->peer_sid;
    g_vr_state.retry_times = 0;
    MESH_LOG_DEBUG("vector router: send topology sync select to %04x, len = %d",
                   g_vr_state.sync_state->peer_sid, g_vr_state.sync_state->cmd_len);
    ur_router_send_message(&g_vr_state.router, g_vr_state.sync_state->peer_sid,
                           (uint8_t *)cmd, sizeof(router_command_t));
    restart_topology_sync_timer();
}

static void send_topology_sync_data()
{
    uint8_t *data;
    router_command_t *cmd;
    uint16_t len;
    vertex_t *vertex;
    edge_t *edge;

    data = g_vr_state.sync_state->cmd_data;
    cmd = (router_command_t *)data;
    cmd->cmd = COMMAND_TOPOLOGY_SYNC_DATA;
    cmd->seq = ++vertex_me->seq;
    cmd->sid = vertex_me->sid;
    len = sizeof(router_command_t);
    if (g_vr_state.sync_status == TOPOLOGY_SYNC_SERVER_SENDING_VERTEX) {
        vertex_tv_t *tv;
        vertex = g_vr_state.sync_state->cur_vertex;
        while (vertex != NULL && (len + sizeof(vertex_tv_t)) < MAX_CMD_LEN) {
            tv = (vertex_tv_t *)(data + len);
            tv->type = TYPE_VERTEX;
            memcpy(tv->uuid, vertex->uuid, sizeof(vertex->uuid));
            tv->sid = vertex->sid;

            len += sizeof(vertex_tv_t);
            vertex = slist_first_entry(&vertex->next, vertex_t, next);
        }
        if (vertex != NULL) {
            g_vr_state.sync_state->cur_vertex = vertex;
        } else {
            g_vr_state.sync_status = TOPOLOGY_SYNC_SERVER_SENDING_EDGE;
            g_vr_state.sync_state->cur_vertex = vertex_me;
            g_vr_state.sync_state->cur_edge = vertex_me->edges;
        }
    }

    if (g_vr_state.sync_status == TOPOLOGY_SYNC_SERVER_SENDING_EDGE) {
        edge_tv_t *tv;
        vertex = g_vr_state.sync_state->cur_vertex;
        edge = g_vr_state.sync_state->cur_edge;
        while (edge != NULL && (len + sizeof(edge_tv_t)) < MAX_CMD_LEN) {
            tv = (edge_tv_t *)(data + len);
            tv->type = TYPE_EDGE;
            tv->src = vertex->sid;
            tv->dst = edge->dst->sid;
            tv->cost = edge->cost;

            len += sizeof(edge_tv_t);
            edge = edge->next;
            while (edge != NULL || vertex != NULL) {
                if (edge != NULL) {
                    break;
                } else {
                    vertex = slist_first_entry(&vertex->next, vertex_t, next);
                    if (vertex != NULL) {
                        edge = vertex->edges;
                    }
                }
            }
        }

        if (edge != NULL) {
            g_vr_state.sync_state->cur_vertex = vertex;
            g_vr_state.sync_state->cur_edge = edge;
        } else {
            g_vr_state.sync_state->cur_vertex = NULL;
            g_vr_state.sync_state->cur_edge = NULL;
            g_vr_state.sync_status = TOPOLOGY_SYNC_SERVER_EXITING;
        }
    }

    if (g_vr_state.sync_status == TOPOLOGY_SYNC_SERVER_EXITING) {
        none_tv_t *tv;
        if ((len + sizeof(none_tv_t)) < MAX_CMD_LEN) {
            tv = (none_tv_t *)(data + len);
            tv->type = TYPE_NONE;
            len += sizeof(none_tv_t);
            g_vr_state.sync_status = TOPOLOGY_SYNC_SERVER_EXITING;
        }
    }

    g_vr_state.retry_times = 0;
    g_vr_state.sync_state->cmd_dst = g_vr_state.sync_state->peer_sid;
    g_vr_state.sync_state->cmd_len = len;
    MESH_LOG_DEBUG("vector router: send topology data to %04x, seq = %d, len = %d",
                   g_vr_state.sync_state->peer_sid, vertex_me->seq, len);
    ur_router_send_message(&g_vr_state.router, g_vr_state.sync_state->peer_sid,
                           data, len);
    restart_topology_sync_timer();
}

static void send_vertex_update(uint8_t *uuid, uint16_t sid, uint16_t to)
{
    uint8_t *data, len;
    router_command_t *cmd;
    vertex_tv_t *tv;

    len = sizeof(router_command_t) + sizeof(vertex_tv_t);
    data = (uint8_t *)ur_mem_alloc(len);
    cmd  = (router_command_t *)data;
    cmd->cmd = COMMAND_VERTEX_UPDATE;
    cmd->seq = ++vertex_me->seq;
    cmd->sid = vertex_me->sid;
    tv = (vertex_tv_t *)(data + sizeof(router_command_t));
    tv->type = TYPE_VERTEX;
    memcpy(tv->uuid, uuid, sizeof(tv->uuid));
    tv->sid  = sid;

    MESH_LOG_DEBUG("vector router: send vertex update to %04x, len = %d", to, len);
    ur_router_send_message(&g_vr_state.router, to, data, len);
    ur_mem_free(data, len);
}

static void send_edge_update(uint16_t src, uint16_t dst, uint16_t cost,
                             uint16_t to)
{
    uint8_t *data, len;
    router_command_t *cmd;
    edge_tv_t *tv;

    len = sizeof(router_command_t) + sizeof(edge_tv_t);
    data = (uint8_t *)ur_mem_alloc(len);
    cmd = (router_command_t *)data;
    cmd->cmd = COMMAND_EDGE_UPDATE;
    cmd->seq = ++vertex_me->seq;
    cmd->sid = vertex_me->sid;
    tv = (edge_tv_t *)(data + sizeof(router_command_t));
    tv->type = TYPE_EDGE;
    tv->src = src;
    tv->dst = dst;
    tv->cost = cost;

    MESH_LOG_DEBUG("vector router: send edge update to %04x, len = %d", to, len);
    ur_router_send_message(&g_vr_state.router, to, data, len);
    ur_mem_free(data, len);
}

static void send_heartbeat_message()
{
    const uint8_t *data;
    uint16_t len;
    edge_t *edge;
    router_command_t *cmd;
    vertex_tv_t *vertex_tv;
    edge_tuple_t *edge_tuple;

    len = sizeof(router_command_t) + sizeof(vertex_tv_t);
    edge = vertex_me->edges;
    while (edge != NULL) {
        len += sizeof(vertex_me->sid) + sizeof(edge->cost);
        edge = edge->next;
    }

    data = (uint8_t *)ur_mem_alloc(len);
    if (data == NULL) {
        return;
    }

    len = 0;
    cmd = (router_command_t *)data;
    cmd->cmd = COMMAND_HEARTBEAT;
    cmd->seq = ++vertex_me->seq;
    cmd->sid = vertex_me->sid;
    len += sizeof(router_command_t);

    vertex_tv = (vertex_tv_t *)(data + len);
    vertex_tv->type = TYPE_VERTEX;
    memcpy(vertex_tv->uuid, vertex_me->uuid, sizeof(vertex_tv->uuid));
    vertex_tv->sid  = vertex_me->sid;
    len += sizeof(vertex_tv_t);

    edge = vertex_me->edges;
    while (edge != NULL) {
        edge_tuple = (edge_tuple_t *)(data + len);
        edge_tuple->dst = edge->dst->sid;
        edge_tuple->cost = edge->cost;
        len += sizeof(edge_tuple_t);
        edge = edge->next;
    }

    ur_router_send_message(&g_vr_state.router, BCAST_SID, (uint8_t *)data, len);
    ur_mem_free((uint8_t *)data, len);

    MESH_LOG_DEBUG("vector router: send heartbeat message, len = %d", len);
}

static ur_error_t handle_topology_sync_ack(const uint8_t *data, uint16_t length)
{
    router_command_t *ack;

    ack = (router_command_t *)data;
    MESH_LOG_DEBUG("vector router: received topology sync ack from %04x, seq = %d",
                   ack->sid, ack->seq);

    if (ack->seq != vertex_me->seq) {
        return UR_ERROR_FAIL;
    }

    switch (g_vr_state.sync_status) {
        case TOPOLOGY_SYNC_SERVER_SENDING_VERTEX:
        case TOPOLOGY_SYNC_SERVER_SENDING_EDGE:
            send_topology_sync_data();
            break;
        case TOPOLOGY_SYNC_SERVER_EXITING:
            ur_stop_timer(&g_vr_state.sync_timer, NULL);
            if (g_vr_state.sync_state != NULL) {
                ur_mem_free(g_vr_state.sync_state, sizeof(sync_state_t));
                g_vr_state.sync_state = NULL;
            }
            g_vr_state.sync_status = TOPOLOGY_SYNC_IDLE;
            break;
        default:
            break;
    }
    return UR_ERROR_NONE;
}

static ur_error_t handle_topology_sync_select(const uint8_t *data,
                                              uint16_t length)
{
    router_command_t *cmd = (router_command_t *)data;

    MESH_LOG_DEBUG("vector router: received topology sync select from %04x", cmd->sid);

    if (g_vr_state.status != STATUS_UP ||
        g_vr_state.sync_status != TOPOLOGY_SYNC_IDLE) {
        return UR_ERROR_FAIL;
    }


    g_vr_state.sync_state = (sync_state_t *)ur_mem_alloc(sizeof(sync_state_t));
    if (g_vr_state.sync_state == NULL) {
        return UR_ERROR_FAIL;
    }
    g_vr_state.sync_status = TOPOLOGY_SYNC_SERVER_SENDING_VERTEX;
    g_vr_state.sync_state->peer_sid = cmd->sid;
    g_vr_state.sync_state->cur_vertex = vertex_me;
    g_vr_state.sync_state->cur_edge = NULL;
    send_topology_sync_data();
    return UR_ERROR_NONE;
}

static ur_error_t handle_topology_sync_data(const uint8_t *data,
                                            uint16_t length)
{
    router_command_t *cmd;
    uint16_t len;

    cmd = (router_command_t *)data;
    MESH_LOG_DEBUG("vector router: received topology sync data from %04x, len = %d", cmd->sid,
                   length);

    if (g_vr_state.sync_status == TOPOLOGY_SYNC_IDLE) {
        router_command_t ack;
        ack.cmd = COMMAND_TOPOLOGY_SYNC_ACK;
        ack.seq = cmd->seq;
        ack.sid = vertex_me->sid;
        MESH_LOG_DEBUG("vector router: send topology sync ack to %04x, len = %d", cmd->sid,
                       sizeof(ack));
        ur_router_send_message(&g_vr_state.router, cmd->sid, (uint8_t *)&ack,
                               sizeof(ack));
        ur_stop_timer(&g_vr_state.sync_timer, NULL);
    }

    if (g_vr_state.status != STATUS_SYNC_TOPOLOGY ||
        g_vr_state.sync_status != TOPOLOGY_SYNC_CLIENT_RECEIVING_DATA) {
        return UR_ERROR_FAIL;
    }

    len = sizeof(router_command_t);
    while (len < length) {
        uint8_t type = *(data + len);
        vertex_tv_t *vertex_tv;
        edge_tv_t   *edge_tv;
        switch (type) {
            case TYPE_VERTEX:
                vertex_tv = (vertex_tv_t *)(data + len);
                if (memcmp(vertex_tv->uuid, vertex_me->uuid, sizeof(vertex_tv->uuid)) != 0) {
                    update_vertex(COMMAND_VERTEX_UPDATE, vertex_tv->uuid, vertex_tv->sid);
                }
                len += sizeof(vertex_tv_t);
                break;
            case TYPE_EDGE:
                edge_tv = (edge_tv_t *)(data + len);
                if (edge_tv->src != vertex_me->sid) {
                    update_edge(edge_tv->src, edge_tv->dst, edge_tv->cost);
                }
                len += sizeof(edge_tv_t);
                break;
            case TYPE_NONE:
                g_vr_state.status = STATUS_UP;
                g_vr_state.sync_status = TOPOLOGY_SYNC_IDLE;
                len += sizeof(none_tv_t);
                break;
            default:
                return UR_ERROR_FAIL;
                break;
        }
    }

    g_vr_state.sync_state->peer_seq = cmd->seq;
    send_topology_sync_ack();
    if (g_vr_state.sync_status == TOPOLOGY_SYNC_CLIENT_RECEIVING_DATA) {
        return UR_ERROR_NONE;
    } else {
        ur_stop_timer(&g_vr_state.sync_timer, NULL);
    }

    if (g_vr_state.sync_state != NULL) {
        ur_mem_free(g_vr_state.sync_state, sizeof(sync_state_t));
        g_vr_state.sync_state = NULL;
    }

    g_vr_state.sync_status = TOPOLOGY_SYNC_IDLE;
    g_vr_state.status = STATUS_UP;

    /* push self to network */
    scan_new_edges();
    send_heartbeat_message();

    return UR_ERROR_NONE;
}

static ur_error_t handle_vertex_update(const uint8_t *data, uint16_t length)
{
    ur_error_t error;
    router_command_t *cmd;
    vertex_tv_t *tv;
    vertex_t *vertex;
    int8_t diff;

    cmd = (router_command_t *)data;
    tv  = (vertex_tv_t *)(data + sizeof(router_command_t));
    vertex = get_vertex_by_uuid(tv->uuid);
    if (vertex == NULL) {
        return UR_ERROR_FAIL;
    }

    diff = cmd->seq - vertex->seq;
    if (diff <= 0) {
        return UR_ERROR_FAIL;
    }
    vertex->seq = cmd->seq;

    MESH_LOG_DEBUG("vector router: received vertex update from %04x", cmd->sid);

    error = update_vertex(cmd->cmd, tv->uuid, tv->sid);

    return error;
}

static ur_error_t handle_edge_update(const uint8_t *data, uint16_t length)
{
    ur_error_t error;
    edge_tv_t *tv;

    tv  = (edge_tv_t *)(data + sizeof(router_command_t));
    MESH_LOG_DEBUG("vector router: received edge update from %04x: (%04x, %04x, %d)",
                   ((router_command_t *)data)->sid, tv->src, tv->dst, tv->cost);

    error = update_edge(tv->src, tv->dst, tv->cost);

    return error;
}

static ur_error_t handle_heartbeat_message(const uint8_t *data, uint16_t length,
                                           uint8_t *newinfo)
{
    uint16_t len = 0;
    int8_t diff;
    router_command_t *cmd;
    vertex_t *vertex;
    vertex_tv_t *vertex_tv;
    edge_t *edge, *prev_edge;
    edge_tuple_t *edge_tuple;

    *newinfo = 0;
    if (g_vr_state.status != STATUS_UP) {
        return UR_ERROR_FAIL;
    }

    cmd = (router_command_t *)data;
    if (cmd->sid == vertex_me->sid) {
        return UR_ERROR_FAIL;
    }

    vertex_tv = (vertex_tv_t *)(data + sizeof(router_command_t));
    vertex = get_vertex_by_uuid(vertex_tv->uuid);
    if (vertex == NULL) {
        update_vertex(COMMAND_VERTEX_UPDATE, vertex_tv->uuid, vertex_tv->sid);
        vertex = get_vertex_by_uuid(vertex_tv->uuid);
        if (vertex == NULL) {
            return UR_ERROR_FAIL;
        }
        g_vr_state.heartbeat_message_interval = 1; /* new node */
        scan_new_edges();
        vertex->seq = cmd->seq - 1;
        (*newinfo)++;
    }

    diff = cmd->seq - vertex->seq;

    if (diff <= 0) {
        return UR_ERROR_FAIL;
    }

    MESH_LOG_DEBUG("vector router: received heartbeat message from %04x, len = %d", cmd->sid,
                   length);


    vertex->seq = cmd->seq;
    vertex->flag.timeout = 0;
    if (vertex_tv->sid != vertex->sid) {
        vertex->sid = vertex_tv->sid;
        (*newinfo)++;
    }

    /* delete edges that no longer exist */
    prev_edge = NULL;
    edge = vertex->edges;
    while (edge != NULL) {
        len = sizeof(router_command_t) + sizeof(vertex_tv_t);
        while (len < length) {
            edge_tuple = (edge_tuple_t *)(data + len);
            if (edge->dst->sid == edge_tuple->dst) {
                break;
            }
            len += sizeof(edge_tuple_t);
        }

        if (len >= length) {
            if (prev_edge == NULL) {
                vertex->edges = edge->next;
                ur_mem_free(edge, sizeof(edge_t));
                edge = vertex->edges;
            } else {
                prev_edge->next = edge->next;
                ur_mem_free(edge, sizeof(edge_t));
                edge = prev_edge->next;
            }
            (*newinfo)++;
            continue;
        }
        prev_edge = edge;
        edge = edge->next;
    }

    /* update edges */
    len = sizeof(router_command_t) + sizeof(vertex_tv_t);
    while (len < length) {
        edge_tuple = (edge_tuple_t *)(data + len);
        if (update_edge(vertex_tv->sid, edge_tuple->dst,
                        edge_tuple->cost) == UR_ERROR_NONE) {
            (*newinfo)++;
        }
        len += sizeof(edge_tuple_t);
    }

    return UR_ERROR_NONE;
}

static void handle_topology_sync_timer(void *args)
{
    g_vr_state.sync_timer = NULL;
    switch (g_vr_state.sync_status) {
        case TOPOLOGY_SYNC_CLIENT_RECEIVING_DATA:
        case TOPOLOGY_SYNC_SERVER_SENDING_VERTEX:
        case TOPOLOGY_SYNC_SERVER_SENDING_EDGE:
        case TOPOLOGY_SYNC_SERVER_EXITING:
            if (resend_last_command() == UR_ERROR_NONE) {
                break;
            }
        default:
            break;
    }
}

static void vertex_timeout_check()
{
    vertex_t *vertex;
    uint8_t newinfo = 0;
    for_each_vertex(vertex) {
        if (vertex == vertex_me) {
            continue;
        }

        vertex->flag.timeout++;
    }

    while (1) {
        for_each_vertex(vertex) {
            if (vertex->flag.timeout >= VERTEX_ALIVE_TIMEOUT / HEARTBEAT_TIMEOUT) {
                update_vertex(COMMAND_VERTEX_DELETE, vertex->uuid, vertex->sid);
                newinfo++;
                goto next_loop;
            }
        }

        break;
next_loop:
        (void)vertex;
    }

    if (newinfo > 0) {
        dijkstra();
    }
}

static void handle_heartbeat_timer(void *args)
{
    ur_start_timer(&g_vr_state.heartbeat_timer, HEARTBEAT_TIMEOUT, handle_heartbeat_timer, NULL);

    if (g_vr_state.status == STATUS_UP) {
        g_vr_state.heartbeat_count ++;
        if (scan_new_edges() > 0) {
            g_vr_state.heartbeat_message_interval = 1;
        }

        vertex_timeout_check();

        if (g_vr_state.heartbeat_count >= g_vr_state.heartbeat_message_interval) {
            send_heartbeat_message();
            g_vr_state.heartbeat_count = 0;
            g_vr_state.heartbeat_message_interval <<= 1;
            if (g_vr_state.heartbeat_message_interval > MAX_HEARTBEAT_MESSAGE_INTERVAL /
                HEARTBEAT_TIMEOUT) {
                g_vr_state.heartbeat_message_interval = MAX_HEARTBEAT_MESSAGE_INTERVAL /
                                                        HEARTBEAT_TIMEOUT;
            }
        }
    } else if (g_vr_state.status == STATUS_SYNC_TOPOLOGY) {
        if (g_vr_state.sync_status == TOPOLOGY_SYNC_IDLE) {
            neighbor_t *node = umesh_mm_get_attach_node();
            if (node == NULL) {
                return;
            }

            g_vr_state.sync_state = (sync_state_t *)ur_mem_alloc(sizeof(sync_state_t));
            if (g_vr_state.sync_state == NULL) {
                return;
            }

            g_vr_state.sync_status = TOPOLOGY_SYNC_CLIENT_RECEIVING_DATA;
            g_vr_state.sync_state->peer_sid = node->sid;
            send_topology_sync_select();
        }
    }
}

uint16_t vector_router_get_next_hop_shortid(uint16_t dest)
{
    vertex_t *vertex;

    vertex = get_vertex_by_sid(dest);

    if (vertex == NULL) {
        return INVALID_SID;
    }

    return vertex->nexthop;
}

ur_error_t vector_router_init(void)
{
    if (vertex_me != NULL) {
        return UR_ERROR_NONE;
    }

    slist_init(&g_vr_state.vertex_list);
    vertex_t *vertex = vertex_alloc();
    if (vertex == NULL) {
        return UR_ERROR_FAIL;
    }
    vertex->sid = 1; /* for test purpose */
    g_vr_state.vertex_num = 1;
    g_vr_state.meshnetid = INVALID_NETID;
    g_vr_state.status = STATUS_DOWN;
    slist_add(&vertex->next, &g_vr_state.vertex_list);

    return UR_ERROR_NONE;
}

ur_error_t vector_router_deinit(void)
{
    vertex_t *vertex;
    g_vr_state.status = STATUS_DOWN;
    g_vr_state.sync_status = TOPOLOGY_SYNC_IDLE;

    while (!slist_empty(&g_vr_state.vertex_list)) {
        vertex = slist_first_entry(&g_vr_state.vertex_list, vertex_t, next);
        slist_del(&vertex->next, &g_vr_state.vertex_list);
        vertex_free(vertex);
    }
    g_vr_state.vertex_num = 0;

    if (g_vr_state.sync_state != NULL) {
        ur_mem_free(g_vr_state.sync_state, sizeof(sync_state_t));
        g_vr_state.sync_state = NULL;
    }

    ur_stop_timer(&g_vr_state.sync_timer, NULL);
    ur_stop_timer(&g_vr_state.heartbeat_timer, NULL);
    return UR_ERROR_NONE;
}

ur_error_t vector_router_neighbor_updated(neighbor_t *neighbor)
{
    uint16_t src, dst;
    uint8_t cost;

    if (get_vertex_by_uuid(neighbor->mac) == NULL) {
        return UR_ERROR_FAIL;
    }

    src = neighbor->sid;
    dst = vertex_me->sid;
    if (neighbor->stats.link_cost != LINK_COST_MAX) {
        cost = (uint8_t)(neighbor->stats.link_cost >> 4);
    } else {
        cost = (uint8_t)INFINITY_PATH_COST;
    }

    if (update_edge(src, dst, cost) == UR_ERROR_FAIL) {
        return UR_ERROR_FAIL;
    }

    if (g_vr_state.status == STATUS_UP) {
        send_edge_update(src, dst, cost, BCAST_SID);
    }

    dijkstra();
    return UR_ERROR_NONE;
}

ur_error_t vector_router_message_received(const uint8_t *data, uint16_t length)
{
    uint8_t newinfo = 0;
    ur_error_t error;
    uint8_t cmd = data[0];

    switch (cmd) {
        case COMMAND_VERTEX_UPDATE:
        case COMMAND_VERTEX_DELETE:
            error = handle_vertex_update(data, length);
            if (error != UR_ERROR_FAIL) {
                ur_router_send_message(&g_vr_state.router, BCAST_SID, (uint8_t *)data, length);
                newinfo++;
            }
            break;
        case COMMAND_EDGE_UPDATE:
            error = handle_edge_update(data, length);
            if (error != UR_ERROR_FAIL) {
                ur_router_send_message(&g_vr_state.router, BCAST_SID, (uint8_t *)data, length);
                newinfo++;
            }
            break;
        case COMMAND_TOPOLOGY_SYNC_SELECT:
            handle_topology_sync_select(data, length);
            break;
        case COMMAND_TOPOLOGY_SYNC_DATA:
            handle_topology_sync_data(data, length);
            break;
        case COMMAND_TOPOLOGY_SYNC_ACK:
            handle_topology_sync_ack(data, length);
            break;
        case COMMAND_HEARTBEAT:
            error = handle_heartbeat_message(data, length, &newinfo);
            if (error != UR_ERROR_FAIL) {
                ur_router_send_message(&g_vr_state.router, BCAST_SID, (uint8_t *)data, length);
            }
            break;
        default:
            return UR_ERROR_FAIL;
            break;
    }

    if (g_vr_state.status == STATUS_UP && newinfo > 0) {
        dijkstra();
    }

    return UR_ERROR_NONE;
}

ur_error_t vector_router_event_triggered(uint8_t event, uint8_t *data,
                                         uint8_t len)
{
    if (event == EVENT_SID_UPDATED && data != NULL && len == sizeof(netids_t)) {
        netids_t *netids = (netids_t *)data;

        if (is_unique_netid(netids->meshnetid) == false || is_unique_sid(netids->sid) == false) {
            return UR_ERROR_NONE;
        }

        if (g_vr_state.meshnetid == netids->meshnetid) {
            if (vertex_me->sid == netids->sid) {
                return UR_ERROR_NONE;
            }
            vertex_me->sid = netids->sid;
            send_vertex_update(vertex_me->uuid, vertex_me->sid, BCAST_SID);
        } else {
            if (is_unique_netid(g_vr_state.meshnetid)) {
                vector_router_deinit();
                vector_router_init();
            }

            g_vr_state.meshnetid = netids->meshnetid;
            vertex_me->sid = netids->sid;
            memcpy(vertex_me->uuid, umesh_mm_get_local_uuid(), sizeof(vertex_me->uuid));
            if (netids->sid == LEADER_SID) {
                g_vr_state.status = STATUS_UP;
            } else {
                g_vr_state.status = STATUS_SYNC_TOPOLOGY;
            }
            g_vr_state.sync_status = TOPOLOGY_SYNC_IDLE;
            ur_start_timer(&g_vr_state.heartbeat_timer, STARTUP_TIMEOUT,
                           handle_heartbeat_timer, NULL);
        }
    }
    return UR_ERROR_NONE;
}

void vector_router_register(void)
{
    g_vr_state.router.id = VECTOR_ROUTER;
    g_vr_state.router.sid_type = SHORT_RANDOM_SID;
    g_vr_state.router.cb.start = vector_router_init;
    g_vr_state.router.cb.stop = vector_router_deinit;
    g_vr_state.router.cb.handle_neighbor_updated = vector_router_neighbor_updated;
    g_vr_state.router.cb.handle_message_received = vector_router_message_received;
    g_vr_state.router.cb.handle_subscribe_event = vector_router_event_triggered;
    g_vr_state.router.cb.get_next_hop_sid = vector_router_get_next_hop_shortid;
    g_vr_state.router.events.num = 1;
    g_vr_state.router.events.events[0] = EVENT_SID_UPDATED;

    register_router(&g_vr_state.router);
}

/* the following are test related functions */
ur_error_t vector_router_add_vertex(uint16_t sid)
{
    vertex_t *vertex;
    for_each_vertex(vertex) {
        if (vertex->sid == sid) {
            return UR_ERROR_FAIL;
        }
    }
    vertex = vertex_alloc();
    if (vertex == NULL) {
        return UR_ERROR_MEM;
    }
    g_vr_state.vertex_num++;
    vertex->sid = sid;
    slist_add_tail(&vertex->next, &g_vr_state.vertex_list);
    return UR_ERROR_NONE;
}

ur_error_t vector_router_update_edge(uint16_t src, uint16_t dst, uint8_t cost)
{
    return update_edge(src, dst, cost);
}

void vector_router_calculate_routing_info()
{
    dijkstra();
}

void vector_router_print_routing_table()
{
    print_routing_table();
}

