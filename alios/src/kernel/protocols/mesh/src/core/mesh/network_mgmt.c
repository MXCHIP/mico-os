/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>

#include "umesh.h"
#include "umesh_utils.h"
#include "core/mesh_mgmt.h"
#include "core/mesh_forwarder.h"
#include "core/network_mgmt.h"
#include "core/topology.h"
#include "core/network_data.h"
#include "core/link_mgmt.h"
#include "core/keys_mgr.h"
#ifdef CONFIG_AOS_MESH_LOWPOWER
#include "core/lowpower_mgmt.h"
#endif
#include "hal/interfaces.h"

typedef struct discover_result_s {
    mac_address_t addr;
    uint16_t meshnetid;
    uint8_t channel;
    int8_t rssi;
    node_mode_t leader_mode;
    uint16_t net_size;
    uint16_t path_cost;
} discover_result_t;

typedef struct network_mgmt_state_s {
    bool started;
    bool discover_pending;

    ur_timer_t discover_start_timer;
    ur_timer_t discover_timer;
    uint8_t discover_channel_index;
    uint8_t discover_times;
    discover_result_t discover_result;

    discovered_handler_t handler;
    mm_cb_t interface_callback;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    lowpower_events_handler_t lowpower_callback;
#endif
} network_mgmt_state_t;
static network_mgmt_state_t g_nm_state;

static void start_discover_timer(void);
static void handle_discovery_timer(void *args);
static ur_error_t send_discovery_request(void);
static ur_error_t send_discovery_response(network_context_t *network, ur_addr_t *dest);

static void handle_discovery_timer(void *args)
{
    hal_context_t *hal = get_default_hal_context();
    neighbor_t *nbr;
    bool migrate = false;

    MESH_LOG_DEBUG("handle discovery timer");

    g_nm_state.discover_timer = NULL;
    if (g_nm_state.discover_channel_index >= hal->channel_list.num) {
        g_nm_state.discover_channel_index = 0;
        g_nm_state.discover_times++;
    }

    if (is_unique_netid(g_nm_state.discover_result.meshnetid)) {
        mm_netinfo_tv_t netinfo;
        nbr = get_neighbor_by_mac_addr(g_nm_state.discover_result.addr.addr, NULL);
        netinfo.leader_mode = g_nm_state.discover_result.leader_mode;
        netinfo.size = g_nm_state.discover_result.net_size;
        if (nbr && umesh_mm_migration_check(nbr, &netinfo)) {
            migrate = true;
        }
    }

    if (g_nm_state.discover_times < DISCOVERY_RETRY_TIMES && migrate == false) {
        umesh_mm_set_channel(hal, hal->channel_list.channels[g_nm_state.discover_channel_index]);
        send_discovery_request();
        ur_start_timer(&g_nm_state.discover_timer, hal->discovery_interval,
                       handle_discovery_timer, NULL);
        g_nm_state.discover_channel_index++;
        return;
    }

    g_nm_state.started = false;
    if (migrate) {
        umesh_mm_set_channel(hal, g_nm_state.discover_result.channel);
        nbr = get_neighbor_by_mac_addr(g_nm_state.discover_result.addr.addr, NULL);
        g_nm_state.handler(nbr);
    } else if (umesh_mm_get_device_state() >= DEVICE_STATE_LEAF) {
        start_discover_timer();
        umesh_mm_set_channel(hal, umesh_mm_get_prev_channel());
        start_discover_timer();
    } else {
        umesh_mm_set_channel(hal, hal->def_channel);
        g_nm_state.handler(NULL);
    }
}

static ur_error_t send_discovery_request(void)
{
    ur_error_t error = UR_ERROR_MEM;
    uint16_t length;
    mm_state_flags_tv_t *flag;
    uint8_t *data;
    uint8_t *data_orig;
    message_t *message = NULL;
    message_info_t *info;

    length = sizeof(mm_header_t) + sizeof(mm_state_flags_tv_t);
    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);

    flag = (mm_state_flags_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)flag, TYPE_STATE_FLAGS);
    flag->flags = umesh_mm_get_reboot_flag();
    data += sizeof(mm_state_flags_tv_t);

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_DISCOVERY_REQUEST,
                               data_orig, length, NETWORK_MGMT_1);
    if (message) {
        info = message->info;
        set_mesh_short_addr(&info->dest, BCAST_NETID, BCAST_SID);
        error = mf_send_message(message);
    }
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send discovery request in channel index %d, len %d",
                   g_nm_state.discover_channel_index, length);

    return error;
}

static ur_error_t send_discovery_response(network_context_t *network, ur_addr_t *dest)
{
    ur_error_t error = UR_ERROR_MEM;
    message_t *message;
    uint8_t *data;
    uint16_t length;
    message_info_t *info;
    uint8_t *data_orig;

    if (is_pf_mode(umesh_get_mode())) {
        return UR_ERROR_FAIL;
    }

    length = sizeof(mm_header_t) + sizeof(mm_netinfo_tv_t) + sizeof(mm_channel_tv_t) +
             sizeof(mm_cost_tv_t);
    if (network->router->sid_type == STRUCTURED_SID) {
        length += sizeof(mm_ssid_info_tv_t);
    }
    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);
    data += set_mm_netinfo_tv(network, data);
    data += set_mm_channel_tv(network->hal, data);
    if (network->router->sid_type == STRUCTURED_SID) {
        data += set_mm_ssid_info_tv(network, data);
    }
    data += set_mm_path_cost_tv(data);
    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_DISCOVERY_RESPONSE,
                               data_orig, length, NETWORK_MGMT_2);
    if (message) {
        info = message->info;
        info->network = network;
        memcpy(&info->dest, dest, sizeof(info->dest));
        error = mf_send_message(message);
    }
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send discovery response, len %d", length);
    return error;
}

ur_error_t handle_discovery_request(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    mm_state_flags_tv_t *flag;
    uint8_t *tlvs;
    uint16_t tlvs_length;
    neighbor_t *nbr;
    network_context_t *network;
    message_info_t *info;

    MESH_LOG_DEBUG("handle discovery request");

    info = message->info;
    network = info->network;
    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t);
    tlvs = ur_mem_alloc(tlvs_length);
    if (tlvs == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, sizeof(mm_header_t), tlvs, tlvs_length);

    if ((nbr = update_neighbor(info, tlvs, tlvs_length, true)) == NULL) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    if (umesh_mm_get_device_state() < DEVICE_STATE_LEADER) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    flag = (mm_state_flags_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_STATE_FLAGS);
    if (flag && flag->flags) {
        nbr->flags |= NBR_REBOOT;
    } else {
        nbr->flags |= NBR_DISCOVERY_REQUEST;
    }
    nbr->flags |= NBR_WAKEUP;

    send_discovery_response(network, &info->src_mac);

exit:
    ur_mem_free(tlvs, tlvs_length);
    return error;
}

ur_error_t handle_discovery_response(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    uint8_t *tlvs;
    uint16_t tlvs_length;
    neighbor_t *nbr;
    discover_result_t *res;
    network_context_t *network;
    message_info_t *info;
    mm_netinfo_tv_t *netinfo;
    mm_channel_tv_t *channel;
    int8_t cmp;

    info = message->info;
    network = info->network;
    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t);
    tlvs = ur_mem_alloc(tlvs_length);
    if (tlvs == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, sizeof(mm_header_t), tlvs, tlvs_length);

    nbr = update_neighbor(info, tlvs, tlvs_length, true);
    if (nbr == NULL) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    if (umesh_mm_get_device_state() != DEVICE_STATE_DETACHED && g_nm_state.started == false) {
        goto exit;
    }
    nbr->flags &= (~NBR_DISCOVERY_REQUEST);
    netinfo = (mm_netinfo_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_NETWORK_INFO);
    if (netinfo == NULL) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    MESH_LOG_DEBUG("handle discovery response from %x", info->src.netid);

    channel = (mm_channel_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_UCAST_CHANNEL);
    if (channel) {
        info->src_channel = channel->channel;
    }

    if (is_unique_netid(info->src.netid) == false ||
        is_same_mainnet(network->meshnetid, info->src.netid) ||
        (network->router->sid_type == STRUCTURED_SID && nbr->ssid_info.free_slots < 1)) {
        goto exit;
    }

    res = &g_nm_state.discover_result;
    if (is_unique_netid(res->meshnetid) == false) {
        cmp = umesh_mm_compare_mode(umesh_mm_get_leader_mode(), netinfo->leader_mode);
    } else {
        cmp = umesh_mm_compare_mode(res->leader_mode, netinfo->leader_mode);
    }
    if ((is_same_mainnet(res->meshnetid, info->src.netid) == false && cmp < 0) ||
        (cmp == 0 && (is_unique_netid(res->meshnetid) == false ||
         get_main_netid(res->meshnetid) < get_main_netid(info->src.netid))) ||
        (res->meshnetid == info->src.netid && res->path_cost > nbr->path_cost)) {
        memcpy(&res->addr, &info->src_mac.addr, sizeof(res->addr));
        res->channel = info->src_channel;
        res->meshnetid = info->src.netid;
        res->leader_mode = netinfo->leader_mode;
        res->net_size = netinfo->size;
        res->path_cost = nbr->path_cost;
    }

exit:
    ur_mem_free(tlvs, tlvs_length);
    return error;
}

void umesh_network_stop_discover(void)
{
    ur_stop_timer(&g_nm_state.discover_start_timer, NULL);
    ur_stop_timer(&g_nm_state.discover_timer, NULL);
    memset(&g_nm_state.discover_result, 0xff, sizeof(g_nm_state.discover_result));
}

static void start_discover(void)
{
    network_context_t *network;
    hal_context_t *hal;

    if (umesh_get_mode() & MODE_LEADER) {
        return;
    }

#ifdef CONFIG_AOS_MESH_LOWPOWER
    if ((umesh_get_mode() & MODE_RX_ON) == 0 && lowpower_is_radio_up() == false) {
        g_nm_state.discover_pending = true;
        return;
    }
#endif
    if (g_nm_state.started == false) {
        umesh_mm_set_prev_channel();
    }
    g_nm_state.started = true;
    network = get_default_network_context();
    hal = network->hal;
    g_nm_state.discover_channel_index = 0;
    g_nm_state.discover_times = 0;
    ur_start_timer(&g_nm_state.discover_timer, hal->discovery_interval,
                   handle_discovery_timer, NULL);
    memset(&g_nm_state.discover_result, 0xff, sizeof(g_nm_state.discover_result));
}

void umesh_network_mgmt_register_callback(discovered_handler_t handler)
{
    g_nm_state.handler = handler;
}

static void handle_start_discover_timer(void *args)
{
    start_discover();
}

static void start_discover_timer(void)
{
    if ((umesh_get_mode() & MODE_LEADER) == 0) {
        ur_start_timer(&g_nm_state.discover_start_timer, ACTIVE_DISCOVER_INTERVAL,
                       handle_start_discover_timer, NULL);
    }
}

static ur_error_t mesh_interface_up(void)
{
    MESH_LOG_DEBUG("network mgmt mesh interface up handler");

    ur_stop_timer(&g_nm_state.discover_start_timer, NULL);
    ur_stop_timer(&g_nm_state.discover_timer, NULL);
    if (umesh_get_device_state() == DEVICE_STATE_LEADER) {
        start_discover_timer();
    }
    return UR_ERROR_NONE;
}

static ur_error_t mesh_interface_down(interface_state_t state)
{
    MESH_LOG_DEBUG("network mgmt mesh interface down handler, reason %d", state);

    ur_stop_timer(&g_nm_state.discover_start_timer, NULL);
    if (state == INTERFACE_DOWN_PNETID_CHANGED || state == INTERFACE_DOWN_MESH_START) {
        start_discover();
    } else if (state == INTERFACE_DOWN_ATTACH_FAIL || state == INTERFACE_DOWN_DISCOVER_FAIL) {
        start_discover_timer();
    } else if (state == INTERFACE_DOWN_MESH_STOP) {
        ur_stop_timer(&g_nm_state.discover_timer, NULL);
    }
    return UR_ERROR_NONE;
}

#ifdef CONFIG_AOS_MESH_LOWPOWER
static void lowpower_radio_down_handler(schedule_type_t type)
{
    if (umesh_get_mode() & MODE_RX_ON) {
        return;
    }
    ur_stop_timer(&g_nm_state.discover_timer, NULL);
    memset(&g_nm_state.discover_result, 0xff, sizeof(g_nm_state.discover_result));
}

static void lowpower_radio_up_handler(schedule_type_t type)
{
    if (umesh_get_mode() & MODE_RX_ON) {
        return;
    }

    if (type == LOWPOWER_ATTACHING_SCHEDULE ||
        (type == LOWPOWER_ATTACHED_SCHEDULE && g_nm_state.discover_pending == true)) {
        g_nm_state.discover_pending = false;
        start_discover();
    }
}
#endif

void umesh_network_mgmt_init(void)
{
    g_nm_state.started = false;
    g_nm_state.discover_pending = false;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    g_nm_state.lowpower_callback.radio_down = lowpower_radio_down_handler;
    g_nm_state.lowpower_callback.radio_up = lowpower_radio_up_handler;
    lowpower_register_callback(&g_nm_state.lowpower_callback);
#endif
    g_nm_state.interface_callback.interface_up = mesh_interface_up;
    g_nm_state.interface_callback.interface_down = mesh_interface_down;
    umesh_mm_register_callback(&g_nm_state.interface_callback);
}
