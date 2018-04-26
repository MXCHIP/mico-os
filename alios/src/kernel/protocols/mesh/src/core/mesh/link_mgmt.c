/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>

#include "umesh.h"
#include "core/mesh_mgmt.h"
#include "core/mesh_forwarder.h"
#include "core/link_mgmt.h"
#include "core/network_data.h"
#include "core/sid_allocator.h"
#include "core/address_mgmt.h"
#include "core/mesh_mgmt_tlvs.h"
#include "core/keys_mgr.h"
#ifdef CONFIG_AOS_MESH_LOWPOWER
#include "core/lowpower_mgmt.h"
#endif
#include "umesh_utils.h"
#include "hal/interfaces.h"

typedef struct link_mgmt_state_s {
    neighbor_updated_t neighbor_updater;
    mm_cb_t interface_callback;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    lowpower_events_handler_t lowpower_callback;
#endif
} link_mgmt_state_t;
link_mgmt_state_t g_lm_state;

static void handle_update_nbr_timer(void *args);

static ur_error_t update_link_cost(link_nbr_stats_t *stats)
{
    uint32_t new;
    uint32_t old;

    if (stats->link_request < LINK_ESTIMATE_SENT_THRESHOLD) {
        return UR_ERROR_FAIL;
    }

    if (stats->link_accept) {
        new = ((uint32_t)LINK_ESTIMATE_COEF * stats->link_request) / stats->link_accept;
    } else {
        new = LINK_COST_MAX;
    }

    if (stats->link_cost == 0xffff) {
        stats->link_cost = new;
    } else {
        old = stats->link_cost;
        stats->link_cost = ((uint32_t)((LINK_ESTIMATE_COEF - LINK_ESTIMATE_UPDATE_ALPHA)
                                       * old) +
                            ((uint32_t)(LINK_ESTIMATE_UPDATE_ALPHA * new))) / LINK_ESTIMATE_COEF;
        if ((stats->link_cost == old) && (!stats->link_accept)) {
            stats->link_cost += LINK_ESTIMATE_COEF;
        }
    }
    stats->link_request = 0;
    stats->link_accept = 0;
    return UR_ERROR_NONE;
}

ur_error_t remove_neighbor(hal_context_t *hal, neighbor_t *neighbor)
{
    network_context_t *network;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    message_t *message;
#endif

    if (neighbor == NULL) {
        return UR_ERROR_NONE;
    }

    network = get_network_context_by_meshnetid(neighbor->netid, false);
    if (network && network->router->sid_type == STRUCTURED_SID) {
        sid_node_t *sid_node = get_allocated_child(network->sid_base, neighbor);
        if (sid_node) {
            update_sid_mapping(network->sid_base, &sid_node->node_id, false);
        }
    }

    slist_del(&neighbor->next, &hal->neighbors_list);
    ur_mem_free(neighbor->one_time_key, KEY_SIZE);
#ifdef CONFIG_AOS_MESH_LOWPOWER
    while ((message = message_queue_get_head(&neighbor->buffer_queue))) {
        message_queue_dequeue(message);
        message_free(message);
    }
#endif
    ur_mem_free(neighbor, sizeof(neighbor_t));
    hal->neighbors_num--;
    return UR_ERROR_NONE;
}

static ur_error_t send_link_request(ur_addr_t *dest, uint8_t *tlvs, uint8_t tlvs_length)
{
    ur_error_t error = UR_ERROR_MEM;
    mm_tlv_request_tlv_t *request_tlvs;
    message_t *message;
    uint8_t *data;
    uint8_t *data_orig;
    uint16_t length;
    message_info_t *info;
    neighbor_t *nbr;

    nbr = get_neighbor_by_sid(dest->netid, dest->addr.short_addr, NULL);
    if (nbr == NULL) {
        return UR_ERROR_FAIL;
    }

    length = sizeof(mm_header_t);
    if (tlvs_length) {
        length += (tlvs_length + sizeof(mm_tlv_request_tlv_t));
    }

    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);

    if (tlvs_length) {
        request_tlvs = (mm_tlv_request_tlv_t *)data;
        umesh_mm_init_tlv_base((mm_tlv_t *)request_tlvs, TYPE_TLV_REQUEST, tlvs_length);
        data += sizeof(mm_tlv_request_tlv_t);
        memcpy(data, tlvs, tlvs_length);
        data += tlvs_length;
    }

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_LINK_REQUEST,
                               data_orig, length, LINK_MGMT_1);
    if (message) {
        info = message->info;
        memcpy(&info->dest, dest, sizeof(info->dest));
        error = mf_send_message(message);
    }
    ur_mem_free(data_orig, length);

    nbr->stats.link_request++;

    MESH_LOG_DEBUG("send link request, len %d", length);
    return error;
}


static void build_and_send_link_request(bool send)
{
    neighbor_t *nbr = umesh_mm_get_attach_node();
    ur_addr_t addr;

    if (nbr == NULL || (send == false && (nbr->flags & NBR_LINK_ESTIMATED))) {
        return;
    }

#ifdef CONFIG_AOS_MESH_LOWPOWER
    uint8_t tlv_types[4] = {TYPE_UCAST_CHANNEL, TYPE_TIMESTAMP, TYPE_TIME_SLOT, TYPE_BUFQUEUE_SIZE};
    uint8_t tlv_length = (umesh_get_mode() & MODE_RX_ON)? 2: sizeof(tlv_types);
#else
    uint8_t tlv_types[2] = {TYPE_UCAST_CHANNEL, TYPE_TIMESTAMP};
    uint8_t tlv_length = sizeof(tlv_types);
#endif
    set_mesh_short_addr(&addr, nbr->netid, nbr->sid);
    send_link_request(&addr, tlv_types, tlv_length);
}

static void update_neighbors_link_cost(hal_context_t *hal)
{
    ur_error_t error;
    neighbor_t *nbr;

    slist_for_each_entry(&hal->neighbors_list, nbr, neighbor_t, next) {
        error = update_link_cost(&nbr->stats);
        if (error != UR_ERROR_NONE || nbr->stats.reverse_rssi < RSSI_THRESHOLD ||
            (umesh_now_ms() - nbr->last_heard) > (hal->neighbor_alive_interval / 2)) {
            nbr->flags &= (~NBR_LINK_ESTIMATED);
        }
        if (nbr->stats.link_cost >= LINK_COST_THRESHOLD) {
            nbr->state = STATE_INVALID;
            g_lm_state.neighbor_updater(hal, nbr);
            remove_neighbor(hal, nbr);
        }
    }
}

static void handle_link_quality_timer(void *args)
{
    hal_context_t *hal = (hal_context_t *)args;

    MESH_LOG_DEBUG("handle link quality update timer");

    ur_start_timer(&hal->link_quality_update_timer, hal->link_quality_update_interval,
                   handle_link_quality_timer, hal);
    update_neighbors_link_cost(hal);
    build_and_send_link_request(false);
}

static neighbor_t *new_neighbor(hal_context_t *hal, const mac_address_t *addr,
                                uint8_t *tlvs, uint16_t length, bool is_attach)
{
    neighbor_t *nbr;

    if (hal->neighbors_num < MAX_NEIGHBORS_NUM) {
        nbr = (neighbor_t *)ur_mem_alloc(sizeof(neighbor_t));
        memset(nbr, 0, sizeof(neighbor_t));
        if (nbr == NULL) {
            return NULL;
        }

        slist_add(&nbr->next, &hal->neighbors_list);
        hal->neighbors_num++;
        goto get_nbr;
    }

    if (!is_attach) {
        return NULL;
    }

    slist_for_each_entry(&hal->neighbors_list, nbr, neighbor_t, next) {
        if (nbr->state == STATE_PARENT) {
            continue;
        }
        if (nbr->state == STATE_CHILD) {
            continue;
        }

        MESH_LOG_INFO("sid %04x mac " EXT_ADDR_FMT " is replaced",
                      nbr->sid, EXT_ADDR_DATA(nbr->mac));
        goto get_nbr;
    }
    return NULL;

get_nbr:
    memcpy(nbr->mac, addr->addr, sizeof(nbr->mac));
    nbr->netid = BCAST_NETID;
    nbr->sid = BCAST_SID;
    nbr->path_cost = INFINITY_PATH_COST;
    nbr->mode = 0;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    dlist_init(&nbr->buffer_queue);
#endif
    nbr->stats.link_cost = 256;
    nbr->stats.link_request = 0;
    nbr->stats.link_accept = 0;
    nbr->state = STATE_INVALID;
    nbr->flags = 0;
    nbr->last_heard = umesh_now_ms();
    return nbr;
}

static bool neighbor_is_alive(hal_context_t *hal, neighbor_t *nbr)
{
    uint32_t threshold;
    int32_t time_offset = umesh_now_ms() - nbr->last_heard;
    bool ret;

#ifdef CONFIG_AOS_MESH_LOWPOWER
    if ((umesh_get_mode() & MODE_RX_ON) == 0 || (nbr->mode & MODE_RX_ON) == 0) {
        threshold = SCHEDULE_SLOT_INTERVAL * SCHEDULE_SLOTS_SIZE * LINK_ESTIMATE_SENT_THRESHOLD;
    } else
#endif
    {
        threshold = hal->neighbor_alive_interval;
    }
    ret = (time_offset < threshold)? true: false;
    return ret;
}

static void start_update_nbr_timer(hal_context_t *hal)
{
    if (hal->update_nbr_timer == NULL && (umesh_get_mode() & MODE_RX_ON)) {
        ur_start_timer(&hal->update_nbr_timer, hal->advertisement_interval,
                       handle_update_nbr_timer, hal);
    }
}

static void handle_update_nbr_timer(void *args)
{
    neighbor_t *node;
    hal_context_t *hal = (hal_context_t *)args;
    uint16_t sid = umesh_mm_get_local_sid();
    network_context_t *network = NULL;
    ur_node_id_t node_id;

    hal->update_nbr_timer = NULL;
    start_update_nbr_timer(hal);
    slist_for_each_entry(&hal->neighbors_list, node, neighbor_t, next) {
        if (node->state < STATE_NEIGHBOR) {
            continue;
        }

        if (node->attach_candidate_timeout > 0) {
            node->attach_candidate_timeout--;
        }

        if (neighbor_is_alive(hal, node)) {
            continue;
        }

        MESH_LOG_INFO("%04x neighbor " EXT_ADDR_FMT " become inactive",
                      sid, EXT_ADDR_DATA(node->mac));
        network = get_network_context_by_meshnetid(node->netid, false);
        if (network && network->router->sid_type == STRUCTURED_SID &&
            node->state == STATE_CHILD) {
            node_id.sid = node->sid;
            memcpy(node_id.uuid, node->mac, sizeof(node_id.uuid));
            update_sid_mapping(network->sid_base, &node_id, false);
        }
        node->state = STATE_INVALID;
        g_lm_state.neighbor_updater(hal, node);
        remove_neighbor(hal, node);
    }
}

static ur_error_t mesh_interface_up(void)
{
    slist_t *hals;
    hal_context_t *hal;

    MESH_LOG_DEBUG("link mgmt mesh interface up handler");
    if (umesh_get_mode() & MODE_RX_ON) {
        hals = get_hal_contexts();
        slist_for_each_entry(hals, hal, hal_context_t, next) {
            ur_start_timer(&hal->link_quality_update_timer, hal->link_quality_update_interval,
                           handle_link_quality_timer, hal);
        }
    }
    return UR_ERROR_NONE;
}

static ur_error_t mesh_interface_down(interface_state_t state)
{
    slist_t *hals;
    hal_context_t *hal;

    MESH_LOG_DEBUG("link mgmt mesh interface down handler, reason %d", state);
    hals = get_hal_contexts();
    slist_for_each_entry(hals, hal, hal_context_t, next) {
        ur_stop_timer(&hal->link_quality_update_timer, hal);
        ur_stop_timer(&hal->update_nbr_timer, hal);
    }
    return UR_ERROR_NONE;
}

#ifdef CONFIG_AOS_MESH_LOWPOWER
static void lowpower_radio_down_handler(schedule_type_t type)
{

}

static void lowpower_radio_up_handler(schedule_type_t type)
{
    slist_t *hals = get_hal_contexts();
    hal_context_t *hal;

    if (umesh_get_mode() & MODE_RX_ON) {
        return;
    }

    slist_for_each_entry(hals, hal, hal_context_t, next) {
        update_neighbors_link_cost(hal);
        handle_update_nbr_timer(hal);
    }
    if (type == LOWPOWER_PARENT_SCHEDULE) {
        build_and_send_link_request(true);
    }
}
#endif

void link_mgmt_init(void)
{
#ifdef CONFIG_AOS_MESH_LOWPOWER
    g_lm_state.lowpower_callback.radio_down = lowpower_radio_down_handler;
    g_lm_state.lowpower_callback.radio_up = lowpower_radio_up_handler;
    lowpower_register_callback(&g_lm_state.lowpower_callback);
#endif
    g_lm_state.interface_callback.interface_up = mesh_interface_up;
    g_lm_state.interface_callback.interface_down = mesh_interface_down;
    umesh_mm_register_callback(&g_lm_state.interface_callback);
}

neighbor_t *update_neighbor(const message_info_t *info,
                            uint8_t *tlvs, uint16_t length, bool is_attach)
{
    neighbor_t *nbr = NULL;
    mm_cost_tv_t *path_cost = NULL;
    mm_ssid_info_tv_t *ssid_info = NULL;
    mm_channel_tv_t *channel;
    hal_context_t *hal;
    network_context_t *network;

    MESH_LOG_DEBUG("update neighbor");

    hal = get_hal_context(info->hal_type);
    if (length == 0 || hal == NULL) {
        goto exit;
    }
    nbr = get_neighbor_by_mac_addr(info->src_mac.addr.addr, NULL);

    path_cost = (mm_cost_tv_t *)umesh_mm_get_tv(tlvs, length, TYPE_PATH_COST);
    ssid_info = (mm_ssid_info_tv_t *)umesh_mm_get_tv(tlvs, length, TYPE_SSID_INFO);
    channel = (mm_channel_tv_t *)umesh_mm_get_tv(tlvs, length, TYPE_UCAST_CHANNEL);

    // remove nbr, if mode changed
    if (nbr && nbr->mode != info->mode) {
        remove_neighbor(hal, nbr);
        nbr = NULL;
    }

    if (nbr == NULL) {
        nbr = new_neighbor(hal, &info->src_mac.addr, tlvs, length, is_attach);
    } else if (is_attach) {
        /* move attaching neighbor to the head of list */
        slist_del(&nbr->next, &hal->neighbors_list);
        slist_add_tail(&nbr->next, &hal->neighbors_list);
    }
    if (nbr == NULL) {
        return NULL;
    }

#ifdef CONFIG_AOS_MESH_LOWPOWER
    lowpower_update_info(nbr, tlvs, length);
#endif

    nbr->mode = (node_mode_t)info->mode;

    if (nbr->state < STATE_CANDIDATE) {
        nbr->state = STATE_NEIGHBOR;
        nbr->stats.link_cost = 256;
    }
    if (path_cost != NULL) {
        nbr->path_cost = path_cost->cost;
    }

    if (nbr->sid != info->src.addr.short_addr) {
        nbr->flags |= NBR_SID_CHANGED;
    }

    if (nbr->netid != info->src.netid) {
        nbr->flags |= NBR_NETID_CHANGED;
    }

    nbr->channel = channel? channel->channel: info->src_channel;

    if (ssid_info != NULL) {
        nbr->ssid_info.child_num = ssid_info->child_num;
        nbr->ssid_info.free_slots = ssid_info->free_slots;
    }

    network = info->network;
    if (network->router->sid_type == STRUCTURED_SID &&
        is_partial_function_sid(info->src.addr.short_addr) == false) {
        sid_node_t *sid_node = get_allocated_child(network->sid_base, nbr);
        if (sid_node == NULL) {
            nbr->state = STATE_NEIGHBOR;
        } else if (is_direct_child(network->sid_base, info->src.addr.short_addr)) {
            nbr->state = STATE_CHILD;
        } else {
            update_sid_mapping(network->sid_base, &sid_node->node_id, false);
            nbr->state = STATE_NEIGHBOR;
        }
    }
    start_update_nbr_timer(hal);
    nbr->sid = info->src.addr.short_addr;
    nbr->netid = info->src.netid;
    g_lm_state.neighbor_updater(hal, nbr);

exit:
    if (nbr) {
        nbr->stats.reverse_rssi = info->reverse_rssi;
        if (nbr->stats.reverse_rssi < RSSI_THRESHOLD) {
            nbr->flags &= (~NBR_LINK_ESTIMATED);
        }
        if (info->forward_rssi != 127) {
            nbr->stats.forward_rssi = info->forward_rssi;
        }
        nbr->last_heard = umesh_now_ms();
    }
    return nbr;
}

neighbor_t *get_neighbor_by_mac_addr(const uint8_t *addr, hal_context_t **hal)
{
    neighbor_t *nbr = NULL;
    slist_t *hals;
    hal_context_t *hal_context;

    hals = get_hal_contexts();
    slist_for_each_entry(hals, hal_context, hal_context_t, next) {
        slist_for_each_entry(&hal_context->neighbors_list, nbr, neighbor_t, next) {
            if (memcmp(addr, nbr->mac, sizeof(nbr->mac)) == 0 &&  nbr->state > STATE_INVALID) {
                if (hal) {
                    *hal = hal_context;
                }
                return nbr;
            }
        }
    }
    return nbr;
}

neighbor_t *get_neighbor_by_sid(uint16_t meshnetid, uint16_t sid, hal_context_t **hal)
{
    neighbor_t *nbr = NULL;
    slist_t *hals;
    hal_context_t *hal_context;

    if (meshnetid == BCAST_NETID || sid == BCAST_SID) {
        return NULL;
    }
    hals = get_hal_contexts();
    slist_for_each_entry(hals, hal_context, hal_context_t, next) {
        slist_for_each_entry(&hal_context->neighbors_list, nbr, neighbor_t, next) {
            if (nbr->sid == sid && nbr->netid == meshnetid && nbr->state > STATE_INVALID) {
                if (hal) {
                    *hal = hal_context;
                }
                return nbr;
            }
        }
    }
    return NULL;
}

static ur_error_t send_link_accept_and_request(ur_addr_t *dest, uint8_t *tlvs, uint8_t tlvs_length)
{
    ur_error_t error = UR_ERROR_MEM;
    message_t *message;
    uint8_t *data;
    uint8_t *data_orig;
    int16_t length;
    neighbor_t *node;
    message_info_t *info;
    network_context_t *network;

    node = get_neighbor_by_mac_addr(dest->addr.addr, NULL);
    if (node == NULL) {
        return UR_ERROR_FAIL;
    }
    network = get_network_context_by_meshnetid(dest->netid, true);
    length = tlvs_calc_length(tlvs, tlvs_length);
    if (length < 0) {
        return UR_ERROR_FAIL;
    }
    length += sizeof(mm_header_t);

    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);
    data += tlvs_set_value(network, data, tlvs, tlvs_length, node);

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_LINK_ACCEPT_AND_REQUEST,
                               data_orig, length, LINK_MGMT_2);
    if (message) {
        info = message->info;
        memcpy(&info->dest, dest, sizeof(info->dest));
        error = mf_send_message(message);
    }
    ur_mem_free(data_orig, length);
    node->stats.link_request++;

    MESH_LOG_DEBUG("send link accept and request, len %d", length);
    return error;
}

static ur_error_t send_link_accept(ur_addr_t *dest, uint8_t *tlvs, uint8_t tlvs_length)
{
    ur_error_t error = UR_ERROR_MEM;
    message_t *message;
    uint8_t *data;
    uint8_t *data_orig;
    int16_t length;
    neighbor_t *node;
    message_info_t *info;
    network_context_t *network;

    node = get_neighbor_by_mac_addr(dest->addr.addr, NULL);
    if (node == NULL) {
        return UR_ERROR_FAIL;
    }
    network = get_network_context_by_meshnetid(dest->netid, true);

    length = tlvs_calc_length(tlvs, tlvs_length);
    if (length < 0) {
        return UR_ERROR_FAIL;
    }
    length += sizeof(mm_header_t);

    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);
    data += tlvs_set_value(network, data, tlvs, tlvs_length, node);

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_LINK_ACCEPT,
                               data_orig, length, LINK_MGMT_3);
    if (message) {
        info = message->info;
        memcpy(&info->dest, dest, sizeof(info->dest));
        error = mf_send_message(message);
    }
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send link accept, len %d", length);
    return error;
}

uint8_t insert_mesh_header_ies(network_context_t *network, message_info_t *info,
                               int16_t hdr_ies_limit)
{
    hal_context_t *hal;
    mesh_header_control_t *control;
    uint8_t offset = 0;
    mm_tv_t *tv;
    mm_rssi_tv_t *rssi;
    neighbor_t *nbr;

    if (hdr_ies_limit < (sizeof(mm_mode_tv_t) + sizeof(mm_tv_t))) {
        return 0;
    }

    hal = network->hal;
    control = (mesh_header_control_t *)hal->frame.data;
    control->control[1] |= (1 << MESH_HEADER_IES_OFFSET);

    offset += set_mm_mode_tv(hal->frame.data + info->header_ies_offset);
    if (hdr_ies_limit < (offset + sizeof(mm_rssi_tv_t) + sizeof(mm_tv_t))) {
        goto exit;
    }

    nbr = get_neighbor_by_sid(info->dest.netid, info->dest.addr.short_addr, NULL);
    if (nbr) {
        rssi = (mm_rssi_tv_t *)(hal->frame.data + info->header_ies_offset + offset);
        umesh_mm_init_tv_base((mm_tv_t *)rssi, TYPE_REVERSE_RSSI);
        if ((umesh_get_mode() & MODE_RX_ON) && (nbr->flags & NBR_LINK_ESTIMATED) == 0) {
            if (nbr->stats.reverse_rssi > RSSI_THRESHOLD) {
                nbr->flags |= NBR_LINK_ESTIMATED;
            }
            rssi->rssi = 127;
            nbr->stats.link_request++;
        } else {
            rssi->rssi = nbr->stats.reverse_rssi;
        }
        offset += sizeof(mm_rssi_tv_t);
    }

exit:
    tv = (mm_tv_t *)(hal->frame.data + info->header_ies_offset + offset);
    tv->type = TYPE_HEADER_IES_TERMINATOR;
    offset += sizeof(mm_tv_t);
    info->payload_offset += offset;
    return offset;
}

ur_error_t handle_mesh_header_ies(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    message_info_t *info;
    uint8_t offset;
    uint8_t len;
    mm_tv_t tv;
    mm_rssi_tv_t rssi;
    mm_mode_tv_t mode;
    uint8_t tlv_types[1] = {TYPE_TIMESTAMP};

    info = message->info;
    offset = info->header_ies_offset;
    message_copy_to(message, offset, (uint8_t *)&tv, sizeof(tv));

    while (tv.type != TYPE_HEADER_IES_TERMINATOR) {
        switch (tv.type) {
            case TYPE_REVERSE_RSSI:
                message_copy_to(message, offset, (uint8_t *)&rssi, sizeof(rssi));
                if (rssi.rssi == 127) {
                    send_link_accept(&info->src_mac, tlv_types, sizeof(tlv_types));
                }
                info->forward_rssi = rssi.rssi;
                len = sizeof(mm_rssi_tv_t);
                break;
            case TYPE_MODE:
                message_copy_to(message, offset, (uint8_t *)&mode, sizeof(mode));
                info->mode = mode.mode;
                len = sizeof(mm_mode_tv_t);
                break;
            default:
                error = UR_ERROR_PARSE;
        }

        if (error != UR_ERROR_NONE) {
            break;
        }

        offset += len;
        message_copy_to(message, offset, (uint8_t *)&tv, sizeof(tv));
    }

    offset += sizeof(mm_tv_t);
    info->payload_offset = offset;
    return error;
}

ur_error_t handle_link_request(message_t *message)
{
    mm_tlv_request_tlv_t *tlvs_request;
    uint8_t *tlvs;
    uint16_t tlvs_length;
    message_info_t *info = message->info;
    uint8_t *data;
    uint16_t length;
    neighbor_t *nbr;

    MESH_LOG_DEBUG("handle link request");

    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t);
    data = ur_mem_alloc(tlvs_length);
    length = tlvs_length;
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    tlvs = data;
    message_copy_to(message, sizeof(mm_header_t), tlvs, tlvs_length);
    nbr = update_neighbor(info, tlvs, tlvs_length, true);
    if (nbr == NULL) {
        goto exit;
    }
#ifdef CONFIG_AOS_MESH_LOWPOWER
    nbr->flags |= NBR_WAKEUP;
#endif

    tlvs_request = (mm_tlv_request_tlv_t *)umesh_mm_get_tv(tlvs, tlvs_length,
                                                           TYPE_TLV_REQUEST);
    if (tlvs_request) {
        tlvs = (uint8_t *)tlvs_request + sizeof(mm_tlv_t);
        tlvs_length = tlvs_request->base.length;
    } else {
        tlvs = NULL;
        tlvs_length = 0;
    }
    send_link_accept_and_request(&info->src_mac, tlvs, tlvs_length);

exit:
    ur_mem_free(data, length);
    return UR_ERROR_NONE;
}

ur_error_t handle_link_accept_and_request(message_t *message)
{
    uint8_t *tlvs;
    uint16_t tlvs_length;
    mm_tlv_request_tlv_t *tlvs_request;
    mm_channel_tv_t *channel;
    neighbor_t *node;
    message_info_t *info;
    network_context_t *network;
    uint8_t local_channel;
    uint8_t *data;
    uint16_t length;

    MESH_LOG_DEBUG("handle link accept and resquest");

    info = message->info;
    network = info->network;
    node = get_neighbor_by_mac_addr(info->src_mac.addr.addr, NULL);
    if (node == NULL) {
        return UR_ERROR_NONE;
    }

    node->stats.link_accept++;

    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t);
    data = ur_mem_alloc(tlvs_length);
    length = tlvs_length;
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    tlvs = data;
    message_copy_to(message, sizeof(mm_header_t), tlvs, tlvs_length);
    channel = (mm_channel_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_UCAST_CHANNEL);
    if (channel) {
        local_channel = umesh_mm_get_channel(network->hal);
        if (local_channel != channel->channel) {
            umesh_mm_set_channel(network->hal, channel->channel);
        }
    }
    update_mm_timestamp(tlvs, tlvs_length);
    tlvs_request = (mm_tlv_request_tlv_t *)umesh_mm_get_tv(tlvs, tlvs_length,
                                                           TYPE_TLV_REQUEST);
    if (tlvs_request) {
        tlvs = (uint8_t *)tlvs_request + sizeof(mm_tlv_t);
        tlvs_length = tlvs_request->base.length;
    } else {
        tlvs = NULL;
        tlvs_length = 0;
    }
    send_link_accept(&info->src_mac, tlvs, tlvs_length);
    ur_mem_free(data, length);
    return UR_ERROR_NONE;
}

ur_error_t handle_link_accept(message_t *message)
{
    uint8_t *tlvs;
    uint16_t tlvs_length;
    neighbor_t *node;
    message_info_t *info;

    MESH_LOG_DEBUG("handle link accept");
    info = message->info;
    node = get_neighbor_by_mac_addr(info->src_mac.addr.addr, NULL);
    if (node == NULL) {
        return UR_ERROR_NONE;
    }

    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t);
    if (tlvs_length) {
        tlvs = ur_mem_alloc(tlvs_length);
        if (tlvs == NULL) {
            return UR_ERROR_MEM;
        }
        message_copy_to(message, sizeof(mm_header_t), tlvs, tlvs_length);
        update_mm_timestamp(tlvs, tlvs_length);
        ur_mem_free(tlvs, tlvs_length);
    }
#ifdef CONFIG_AOS_MESH_LOWPOWER
    node->flags |= NBR_WAKEUP;
    while ((message = message_queue_get_head(&node->buffer_queue))) {
        message_queue_dequeue(message);
        mf_send_message(message);
    }
#endif

    node->stats.link_accept++;
    return UR_ERROR_NONE;
}

ur_error_t register_neighbor_updater(neighbor_updated_t updater)
{
    g_lm_state.neighbor_updater = updater;
    return UR_ERROR_NONE;
}
