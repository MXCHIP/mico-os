/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>
#include <assert.h>

#include "umesh.h"
#include "core/mesh_mgmt.h"
#include "core/address_mgmt.h"
#include "core/router_mgr.h"
#include "hal/interfaces.h"
#include "umesh_utils.h"
#ifdef CONFIG_AOS_MESH_LOWPOWER
#include "core/lowpower_mgmt.h"
#endif

#ifndef DEFAULT_ROUTER
#define DEFAULT_ROUTER SID_ROUTER
#endif

typedef struct router_mgmr_state_s {
    slist_t  router_list;
    router_t *default_router;
    mm_cb_t interface_callback;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    lowpower_events_handler_t lowpower_callback;
#endif
} router_mgmr_state_t;
static router_mgmr_state_t g_rm_state;

extern neighbor_t *get_neighbor_by_sid(uint16_t meshnetid, uint16_t sid, hal_context_t **hal);
extern void sid_router_register(void);
#ifdef CONFIG_AOS_MESH_SUPER
extern void vector_router_register(void);
#endif

uint16_t ur_router_get_next_hop(network_context_t *network, uint16_t dest_sid)
{
    uint16_t next_hop = INVALID_SID;

    if (network == NULL) {
        network = g_rm_state.default_router->network;
    }
    if (network == NULL) {
        return INVALID_SID;
    }

    if (get_neighbor_by_sid(umesh_mm_get_meshnetid(network), dest_sid, NULL) != NULL) {
        return dest_sid;
    }

    if (network->router != NULL) {
        next_hop = network->router->cb.get_next_hop_sid(dest_sid);
    } else if (g_rm_state.default_router != NULL) {
        next_hop = g_rm_state.default_router->cb.get_next_hop_sid(dest_sid);
    }

    return next_hop;
}

void ur_router_neighbor_updated(neighbor_t *neighbor)
{
    if (g_rm_state.default_router != NULL &&
        g_rm_state.default_router->cb.handle_neighbor_updated != NULL) {
        g_rm_state.default_router->cb.handle_neighbor_updated(neighbor);
    }
}

router_t *ur_get_router_by_id(uint8_t id)
{
    router_t *router;
    slist_for_each_entry(&g_rm_state.router_list, router, router_t, next) {
        if (router->id == id) {
            return router;
        }
    }
    return NULL;
}

uint8_t ur_router_get_default_router(void)
{
    if (g_rm_state.default_router != NULL) {
        return g_rm_state.default_router->id;
    }
    return 0;
}

ur_error_t ur_router_set_default_router(uint8_t id)
{
    router_t *router;

    router = ur_get_router_by_id(id);
    if (router == NULL) {
        return UR_ERROR_FAIL;
    }

    if (router == g_rm_state.default_router) {
        return UR_ERROR_NONE;
    }

    if (g_rm_state.default_router->cb.stop != NULL) {
        g_rm_state.default_router->cb.stop();
    }

    g_rm_state.default_router = router;
    if (g_rm_state.default_router->cb.start != NULL) {
        g_rm_state.default_router->cb.start();
    }

    return UR_ERROR_NONE;
}

ur_error_t ur_router_send_message(router_t *router, uint16_t dst,
                                  uint8_t *payload, uint16_t length)
{
    ur_error_t error = UR_ERROR_MEM;
    message_t *message;
    uint8_t *data;
    uint8_t *data_orig;
    message_info_t *info;
    uint16_t msg_length;

    if (router == NULL || payload == NULL) {
        return UR_ERROR_FAIL;
    }

    if (ur_get_router_by_id(router->id) == NULL) {
        return UR_ERROR_FAIL;
    }

    /* not in use, ignore */
    if (!router->network) {
        return UR_ERROR_NONE;
    }

    msg_length = sizeof(mm_header_t) + sizeof(router->id) + length;

    data = ur_mem_alloc(msg_length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;

    data += sizeof(mm_header_t);
    *data = router->id;
    data += sizeof(router->id);
    memcpy(data, payload, length);
    data += length;

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_ROUTING_INFO_UPDATE,
                               data_orig, msg_length, ROUTER_MGR_1);
    if (message) {
        info = message->info;
        info->network = router->network;
        set_mesh_short_addr(&info->dest, umesh_mm_get_meshnetid(NULL), dst);
        error = address_resolve(message);
        if (error == UR_ERROR_NONE) {
            error = mf_send_message(message);
        } else if (error == UR_ERROR_DROP) {
            message_free(message);
        }
    }
    ur_mem_free(data_orig, msg_length);

    MESH_LOG_DEBUG("router %d send routing info to %04x, len %d", router->id, dst, msg_length);
    return error;
}

ur_error_t handle_router_message_received(message_t *message)
{
    router_t *router;
    uint8_t *payload;
    uint16_t length;
    uint8_t *data;
    uint16_t data_length;

    length = message_get_msglen(message);
    payload = ur_mem_alloc(length);
    if (payload == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, 0, payload, length);
    router = ur_get_router_by_id(payload[sizeof(mm_header_t)]);

    if (router == NULL || router->cb.handle_message_received == NULL) {
        ur_mem_free(payload, length);
        return UR_ERROR_FAIL;
    }

    data = payload;
    data_length = length;
    payload += sizeof(mm_header_t) + sizeof(router->id);
    length  -= sizeof(mm_header_t) + sizeof(router->id);
    router->cb.handle_message_received(payload, length);
    ur_mem_free(data, data_length);

    return UR_ERROR_NONE;
}

ur_error_t register_router(router_t *router)
{
    if (router == NULL || ur_get_router_by_id(router->id) != NULL) {
        return UR_ERROR_FAIL;
    }

    slist_add(&router->next, &g_rm_state.router_list);
    return UR_ERROR_NONE;
}

void sid_allocator_init(network_context_t *network)
{
    int sid_type = network->router->sid_type;
    if (sid_type == STRUCTURED_SID) {
        uint16_t sid = umesh_mm_get_local_sid();
        if (umesh_mm_get_device_state() == DEVICE_STATE_SUPER_ROUTER) {
            sid = SUPER_ROUTER_SID;
        }
        network->sid_base = allocator_init(sid, sid_type);
#ifdef CONFIG_AOS_MESH_SUPER
    } else {
        network->sid_base = rsid_allocator_init(sid_type);
#endif
    }
}

void sid_allocator_deinit(network_context_t *network)
{
    if (network->router->sid_type == STRUCTURED_SID) {
        allocator_deinit(network->sid_base);
#ifdef CONFIG_AOS_MESH_SUPER
    } else {
        rsid_allocator_deinit(network->sid_base);
#endif
    }
    network->sid_base = 0;
}

ur_error_t sid_allocator_alloc(network_context_t *network, ur_node_id_t *node)
{
    ur_error_t error;

    switch (network->router->sid_type) {
        case STRUCTURED_SID:
            error = allocate_sid(network->sid_base, node);
            break;
#ifdef CONFIG_AOS_MESH_SUPER
        case SHORT_RANDOM_SID:
        case RANDOM_SID:
            error = rsid_allocate_sid(network->sid_base, node);
            break;
#endif
        default:
            error = UR_ERROR_PARSE;
    }

    return error;
}

ur_error_t sid_allocator_free(network_context_t *network, ur_node_id_t *node)
{
    if (is_partial_function_sid(node->sid)) {
        network = get_default_network_context();
        update_sid_mapping(network->sid_base, node, false);
#ifdef CONFIG_AOS_MESH_SUPER
    } else {
        network = get_network_context_by_meshnetid(node->meshnetid, false);
        if (network == NULL) {
            return UR_ERROR_NONE;
        }

        if (network->router->sid_type == SHORT_RANDOM_SID ||
            network->router->sid_type == RANDOM_SID) {
            rsid_free_sid(network->sid_base, node);
        }
#endif
    }

    return UR_ERROR_NONE;
}

static uint16_t calc_ssid_child_num(network_context_t *network)
{
    uint16_t num = 1;
    neighbor_t *nbr;
    neighbor_t *next_nbr;
    bool dup = false;
    slist_t *nbrs;

    nbrs = &network->hal->neighbors_list;
    slist_for_each_entry(nbrs, nbr, neighbor_t, next) {
        if (nbr->state != STATE_CHILD || network->meshnetid != nbr->netid) {
            continue;
        }
        dup = false;
        slist_for_each_entry(nbrs, next_nbr, neighbor_t, next) {
            if (nbr == next_nbr) {
                continue;
            }
            if (next_nbr->netid == umesh_mm_get_meshnetid(network) &&
                is_unique_sid(nbr->sid) && nbr->sid == next_nbr->sid) {
                dup = true;
            }
        }
        if (dup == false) {
            num += nbr->ssid_info.child_num;
        }
    }

    if (umesh_mm_get_device_state() == DEVICE_STATE_LEADER) {
        num += get_allocated_pf_number(network->sid_base);
    }
    return num;
}

uint16_t sid_allocator_get_num(network_context_t *network)
{
    uint16_t num = 0;

#ifdef CONFIG_AOS_MESH_SUPER
    if (network->router->sid_type == SHORT_RANDOM_SID ||
        network->router->sid_type == RANDOM_SID) {
        num = rsid_get_allocated_number(network->sid_base) + 1;
    }
#endif
    if (network->router->sid_type == STRUCTURED_SID) {
        num = calc_ssid_child_num(network);
    }

    return num;
}

static ur_error_t mesh_interface_up(void)
{
    slist_t *networks;
    network_context_t *network;
    network_context_t *default_network;
    router_t *router;
    netids_t netids;

    MESH_LOG_DEBUG("router mgr mesh interface up handler");
    networks = get_network_contexts();
    default_network = get_default_network_context();
    slist_for_each_entry(networks, network, network_context_t, next) {
        router = network->router? network->router: g_rm_state.default_router;
        router->cb.start();
        if (router->cb.handle_subscribe_event) {
            netids.meshnetid = network->meshnetid;
            netids.sid = (network == default_network)? umesh_get_sid(): LEADER_SID;
            router->cb.handle_subscribe_event(EVENT_SID_UPDATED, (uint8_t *)&netids,
                                              sizeof(netids_t));
        }
    }
    return UR_ERROR_NONE;
}

static ur_error_t mesh_interface_down(interface_state_t state)
{
    router_t *router;

    MESH_LOG_DEBUG("router mgr mesh interface down handler, reason %d", state);
    slist_for_each_entry(&g_rm_state.router_list, router, router_t, next) {
        if (router->cb.stop) {
            router->cb.stop();
        }
    }
    return UR_ERROR_NONE;
}

#ifdef CONFIG_AOS_MESH_LOWPOWER
static void lowpower_radio_down_handler(schedule_type_t type)
{
}

static void lowpower_radio_up_handler(schedule_type_t type)
{
}
#endif

void ur_router_register_module(void)
{
    slist_init(&g_rm_state.router_list);
    sid_router_register();
#ifdef CONFIG_AOS_MESH_SUPER
    vector_router_register();
#endif

    g_rm_state.default_router = slist_first_entry(&g_rm_state.router_list, router_t, next);
    assert(g_rm_state.default_router);

#ifdef CONFIG_AOS_MESH_LOWPOWER
    g_rm_state.lowpower_callback.radio_down = lowpower_radio_down_handler;
    g_rm_state.lowpower_callback.radio_up = lowpower_radio_up_handler;
    lowpower_register_callback(&g_rm_state.lowpower_callback);
#endif
    g_rm_state.interface_callback.interface_up = mesh_interface_up;
    g_rm_state.interface_callback.interface_down = mesh_interface_down;
    umesh_mm_register_callback(&g_rm_state.interface_callback);
}
