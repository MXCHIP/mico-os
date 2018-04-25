/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "umesh.h"
#include "umesh_hal.h"
#include "umesh_utils.h"
#ifdef CONFIG_AOS_MESH_AUTH
#include "core/auth_mgmt.h"
#endif
#include "core/mesh_forwarder.h"
#include "core/mesh_mgmt.h"
#include "core/mesh_mgmt_tlvs.h"
#include "core/router_mgr.h"
#include "core/network_data.h"
#include "core/keys_mgr.h"
#include "core/address_mgmt.h"
#include "core/link_mgmt.h"
#include "core/network_mgmt.h"
#include "core/crypto.h"
#ifdef CONFIG_AOS_MESH_LOWPOWER
#include "core/lowpower_mgmt.h"
#endif
#include "hal/interfaces.h"
#include "hal/hals.h"

typedef struct mm_device_s {
    node_state_t state;
    node_mode_t mode;
    uint8_t uuid[8];
    bool reboot_flag;
    uint8_t seclevel;
    int8_t prev_channel;
} mm_device_t;

typedef struct attach_context_s {
    neighbor_t *attach_node;
    neighbor_t *attach_candidate;
    attach_state_t attach_state;
    uint16_t sid;
    uint16_t path_cost;
    uint16_t prev_netid;
    uint16_t prev_path_cost;
    uint16_t candidate_meshnetid;
    ur_timer_t attach_timer;
    uint8_t retry_times;
    uint8_t leader_times;
    uint8_t migrate_times;
    ur_timer_t migrate_reset_timer;
} attach_context_t;

typedef struct mesh_mgmt_state_s {
    mm_device_t device;
    node_mode_t leader_mode;
    attach_context_t attach_context;
    slist_t interface_callback;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    lowpower_events_handler_t lowpower_callback;
#endif
} mesh_mgmt_state_t;

static mesh_mgmt_state_t g_mm_state;

static ur_error_t attach_start(neighbor_t *nbr);
static void become_leader(void);
static void become_detached(interface_state_t reason);
static void handle_attach_timer(void *args);
static void handle_migrate_reset_timer(void *args);

static ur_error_t send_attach_request(void);
static ur_error_t send_attach_response(network_context_t *network,
                                       ur_addr_t *dest, ur_node_id_t *node_id);
static ur_error_t send_sid_request(void);
static ur_error_t send_sid_response(network_context_t *network, ur_addr_t *dest,
                                    ur_addr_t *dest2, ur_node_id_t *node_id);
static ur_error_t send_advertisement(network_context_t *network);
static void start_advertisement_timer(void *args);
static void mesh_interface_state_callback(interface_state_t state);

static void write_prev_netinfo(void);
static uint16_t compute_network_metric(uint16_t size, uint16_t path_cost);

#ifdef CONFIG_AOS_MESH_AUTH
void nbr_authed_handler(neighbor_t *nbr, bool result)
{
    MESH_LOG_INFO("authentication completed");
 
    if (result) {
        if (nbr) {
            set_auth_state(AUTH_DONE);
            attach_start(nbr);
        }
    } else {
        become_detached(INTERFACE_DOWN_AUTH_FAILURE);
    }
}
#endif

void nbr_discovered_handler(neighbor_t *nbr)
{
    if (nbr) {
#ifdef CONFIG_AOS_MESH_AUTH
        if (is_auth_enabled() && !get_auth_result()) {
            start_auth(nbr, nbr_authed_handler);
        } else
#endif
        {
            attach_start(nbr);
        }
    } else if ((umesh_get_mode() & MODE_MOBILE) == 0) {
        become_leader();
    } else {
        become_detached(INTERFACE_DOWN_DISCOVER_FAIL);
    }
}

static bool is_in_attaching(attach_state_t state)
{
    return (state == ATTACH_IDLE || state == ATTACH_DONE)? false: true;
}

static void update_channel(hal_context_t *hal, uint8_t channel)
{
    if (umesh_mm_get_channel(hal) != channel) {
        umesh_mm_set_prev_channel();
        umesh_mm_set_channel(hal, channel);
    }
}

static void neighbor_updated_handler(hal_context_t *hal, neighbor_t *nbr)
{
    if (g_mm_state.attach_context.attach_node != nbr) {
        goto exit;
    }

    if (nbr->state == STATE_INVALID || (nbr->flags & NBR_NETID_CHANGED)) {
        become_detached(INTERFACE_DOWN_PNETID_CHANGED);
        goto exit;
    } else if (nbr->flags & NBR_SID_CHANGED) {
        become_detached(INTERFACE_DOWN_PSID_CHANGED);
        attach_start(nbr);
    }
    update_channel(hal, nbr->channel);
exit:
    nbr->flags &= (~(NBR_NETID_CHANGED | NBR_SID_CHANGED | NBR_REBOOT));
}

static void set_default_network_data(void)
{
    network_data_t network_data;
    stable_network_data_t stable_network_data;

    nd_init();
    memset(&network_data, 0, sizeof(network_data));
    memset(&stable_network_data, 0, sizeof(stable_network_data));
    network_data.version = 1;
    network_data.size = 1;
    stable_network_data.minor_version = 1;
    stable_network_data.meshnetid = nd_get_stable_meshnetid();
    stable_network_data.mcast_addr[0].m8[0] = 0xff;
    stable_network_data.mcast_addr[0].m8[1] = 0x08;
    stable_network_data.mcast_addr[0].m8[6] = (uint8_t)(
                                                  stable_network_data.meshnetid >> 8);
    stable_network_data.mcast_addr[0].m8[7] = (uint8_t)
                                              stable_network_data.meshnetid;
    stable_network_data.mcast_addr[0].m8[15] = 0xfc;
    nd_set(NULL, &network_data);
    nd_stable_set(&stable_network_data);
}

static uint16_t generate_meshnetid(uint8_t sid, uint8_t index)
{
    uint16_t meshnetid = nd_get_stable_meshnetid();

    if (index == 0 && g_mm_state.attach_context.attach_node) {
        meshnetid = g_mm_state.attach_context.attach_node->netid;
    } else if (index) {
        meshnetid = nd_get_stable_meshnetid() | ((sid << 2) | index);
    }
    return meshnetid;
}

static void start_advertisement_timer(void *args)
{
    network_context_t *network = (network_context_t *)args;

    send_advertisement(network);
    if (umesh_mm_get_mode() & MODE_RX_ON) {
        ur_start_timer(&network->advertisement_timer, network->hal->advertisement_interval,
                       start_advertisement_timer, network);
    }
}

void set_mesh_short_addr(ur_addr_t *addr, uint16_t netid, uint16_t sid)
{
    addr->addr.len = SHORT_ADDR_SIZE;
    addr->addr.short_addr = sid;
    addr->netid = netid;
}

void set_mesh_ext_addr(ur_addr_t *addr, uint16_t netid, uint8_t *value)
{
    addr->addr.len = EXT_ADDR_SIZE;
    memcpy(addr->addr.addr, value, EXT_ADDR_SIZE);
    addr->netid = netid;
}

static void set_attach_context(uint16_t sid, bool init_allocator)
{
    slist_t *networks = get_network_contexts();
    network_context_t *network;
    hal_context_t *prev_hal = NULL;
    uint8_t index = 0;
    uint8_t channel;

    g_mm_state.attach_context.attach_state = ATTACH_DONE;
    ur_stop_timer(&g_mm_state.attach_context.migrate_reset_timer, NULL);
    ur_stop_timer(&g_mm_state.attach_context.attach_timer, NULL);
    if (g_mm_state.attach_context.attach_node) {
        g_mm_state.attach_context.attach_node->state = STATE_NEIGHBOR;
        g_mm_state.attach_context.attach_node = NULL;
    }
    g_mm_state.attach_context.path_cost = 0;
    if (g_mm_state.attach_context.attach_candidate) {
        g_mm_state.attach_context.attach_candidate->flags &=
            (~(NBR_SID_CHANGED | NBR_DISCOVERY_REQUEST | NBR_NETID_CHANGED));
        g_mm_state.attach_context.attach_node = g_mm_state.attach_context.attach_candidate;
        g_mm_state.attach_context.path_cost = g_mm_state.attach_context.attach_node->path_cost +
                                       g_mm_state.attach_context.attach_node->stats.link_cost;
        g_mm_state.attach_context.attach_node->state = STATE_PARENT;
    }
    g_mm_state.attach_context.attach_candidate = NULL;
    g_mm_state.attach_context.sid = sid;
    g_mm_state.attach_context.candidate_meshnetid = BCAST_NETID;

    slist_for_each_entry(networks, network, network_context_t, next) {
        network->state = INTERFACE_UP;
        network->meshnetid = generate_meshnetid(sid, index);
        if (init_allocator) {
            sid_allocator_deinit(network);
            sid_allocator_init(network);
        }
        if (prev_hal != network->hal) {
            prev_hal = network->hal;
            if (network->hal != get_default_hal_context() || sid == LEADER_SID) {
                if (index == 0 && (g_mm_state.device.mode & MODE_LEADER)) {
                    channel = hal_umesh_get_channel(prev_hal->module);
                } else {
                    channel = prev_hal->def_channel;
                }
                umesh_mm_set_channel(prev_hal, channel);
            }
        }
        start_advertisement_timer(network);
        ++index;
    }
}

static ur_error_t sid_allocated_handler(message_info_t *info,
                                        uint8_t *tlvs, uint16_t tlvs_length)
{
    network_context_t *network;
    bool init_allocator = false;
    mm_sid_tv_t *allocated_sid = NULL;
    mm_node_type_tv_t *allocated_node_type = NULL;
    mm_netinfo_tv_t *netinfo;
    mm_mcast_addr_tv_t *mcast;
    stable_network_data_t stable_network_data;

    network = info->network;
    allocated_sid = (mm_sid_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_ALLOCATE_SID);
    allocated_node_type = (mm_node_type_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_NODE_TYPE);
    netinfo = (mm_netinfo_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_NETWORK_INFO);
    mcast = (mm_mcast_addr_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_MCAST_ADDR);
    if (allocated_sid == NULL || allocated_node_type == NULL || netinfo == NULL || mcast == NULL) {
        return UR_ERROR_FAIL;
    }

    stable_network_data.main_version =
        (netinfo->stable_version & STABLE_MAIN_VERSION_MASK) >> STABLE_MAIN_VERSION_OFFSET;
    stable_network_data.minor_version = (netinfo->version & STABLE_MINOR_VERSION_MASK);
    stable_network_data.meshnetid = get_main_netid(info->src.netid);
    memcpy(stable_network_data.mcast_addr, &mcast->mcast, sizeof(mcast->mcast));
    nd_stable_set(&stable_network_data);
    write_prev_netinfo();

    switch (allocated_node_type->type) {
        case LEAF_NODE:
            g_mm_state.device.state = DEVICE_STATE_LEAF;
            break;
        case ROUTER_NODE:
            g_mm_state.device.state = DEVICE_STATE_ROUTER;
            if (g_mm_state.device.mode & MODE_SUPER) {
                g_mm_state.device.state = DEVICE_STATE_SUPER_ROUTER;
            }
            break;
        default:
            g_mm_state.attach_context.attach_state = ATTACH_IDLE;
            return UR_ERROR_FAIL;
    }
    if (g_mm_state.attach_context.attach_node == NULL ||
        network->meshnetid != g_mm_state.attach_context.attach_candidate->netid ||
        g_mm_state.attach_context.sid != allocated_sid->sid) {
        init_allocator = true;
    }
    set_attach_context(allocated_sid->sid, init_allocator);
    g_mm_state.leader_mode = netinfo->leader_mode;
    mesh_interface_state_callback(INTERFACE_UP);
    umesh_mm_set_prev_channel();
    MESH_LOG_INFO("allocate sid 0x%04x, become %d in net %04x",
                  g_mm_state.attach_context.sid, g_mm_state.device.state, network->meshnetid);
    return UR_ERROR_NONE;
}

void become_leader(void)
{
    ur_configs_t configs;

    if (umesh_mm_get_mode() & MODE_MOBILE) {
        return;
    }

    g_mm_state.device.state = DEVICE_STATE_LEADER;
    g_mm_state.leader_mode = g_mm_state.device.mode;

    set_default_network_data();
    memset(&configs, 0, sizeof(configs));
    ur_configs_read(&configs);
    if (g_mm_state.device.reboot_flag == true) {
        g_mm_state.device.reboot_flag = false;
        configs.main_version++;
        configs.main_version %= 7;
    }
    nd_set_stable_main_version(configs.main_version);
    set_attach_context(LEADER_SID, true);
    umesh_mm_set_prev_channel();
    mesh_interface_state_callback(INTERFACE_UP);
    calculate_network_key();
    MESH_LOG_INFO("become leader");
}

void umesh_mm_init_tlv_base(mm_tlv_t *tlv, uint8_t type, uint8_t length)
{
    tlv->type = type;
    tlv->length = length;
}

void umesh_mm_init_tv_base(mm_tv_t *tv, uint8_t type)
{
    tv->type = type;
}

static uint8_t get_tv_value_length(uint8_t type)
{
    uint8_t length;

    switch (type) {
        case TYPE_VERSION:
        case TYPE_MODE:
        case TYPE_NODE_TYPE:
        case TYPE_SCAN_MASK:
        case TYPE_FORWARD_RSSI:
        case TYPE_REVERSE_RSSI:
        case TYPE_SID_TYPE:
        case TYPE_ADDR_QUERY:
        case TYPE_DEF_HAL_TYPE:
        case TYPE_STATE_FLAGS:
        case TYPE_UCAST_CHANNEL:
        case TYPE_BCAST_CHANNEL:
#ifdef CONFIG_AOS_MESH_AUTH
        case TYPE_ID2_AUTH_RESULT:
#endif
            length = 1;
            break;
        case TYPE_SRC_SID:
        case TYPE_DEST_SID:
        case TYPE_ATTACH_NODE_SID:
        case TYPE_ALLOCATE_SID:
        case TYPE_TARGET_SID:
        case TYPE_NETWORK_SIZE:
        case TYPE_PATH_COST:
        case TYPE_LINK_COST:
        case TYPE_WEIGHT:
        case TYPE_BUFQUEUE_SIZE:
            length = 2;
            break;
        case TYPE_SSID_INFO:
            length = 3;
            break;
        case TYPE_TIME_SLOT:
            length = 5;
            break;
        case TYPE_TIMESTAMP:
            length = 4;
            break;
        case TYPE_NODE_ID:
        case TYPE_ATTACH_NODE_ID:
            length = sizeof(mm_node_id_tv_t) - 1;
            break;
        case TYPE_NETWORK_INFO:
            length = sizeof(mm_netinfo_tv_t) - 1;
            break;
        case TYPE_SRC_UUID:
        case TYPE_DEST_UUID:
        case TYPE_ATTACH_NODE_UUID:
        case TYPE_SRC_MAC_ADDR:
        case TYPE_TARGET_UUID:
            length = 8;
            break;
        case TYPE_MCAST_ADDR:
        case TYPE_SYMMETRIC_KEY:
            length = 16;
            break;
#ifdef CONFIG_AOS_MESH_AUTH
        case TYPE_NODE_ID2:
            length = 24;
            break;
        case TYPE_ID2_CHALLENGE:
            length = 32;
            break;
#endif
        default:
            length = 0;
            break;
    }
    return length;
}

static uint8_t get_tv_value(network_context_t *network,
                            uint8_t *data, uint8_t type, void *context)
{
    uint8_t length = 0;

    switch (type) {
        case TYPE_VERSION:
            *data = type;
            data++;
            *data = (nd_get_stable_main_version() << STABLE_MAIN_VERSION_OFFSET) |
                    nd_get_stable_minor_version();
            length = 2;
            break;
        case TYPE_MCAST_ADDR:
            length += set_mm_mcast_tv(data);
            break;
        case TYPE_TARGET_UUID:
            length += set_mm_uuid_tv(data, type, umesh_mm_get_local_uuid());
            break;
        case TYPE_UCAST_CHANNEL:
        case TYPE_BCAST_CHANNEL:
            length += set_mm_channel_tv(network->hal, data);
            break;
#ifdef CONFIG_AOS_MESH_LOWPOWER
        case TYPE_TIME_SLOT:
        case TYPE_BUFQUEUE_SIZE:
            length += lowpower_set_info(type, data, context);
            break;
#endif
        case TYPE_TIMESTAMP:
            length += set_mm_timestamp_tv(data, umesh_get_timestamp());
            break;
        default:
            break;
    }
    return length;
}

uint16_t tlvs_set_value(network_context_t *network, uint8_t *buf,
                        const uint8_t *tlvs, uint8_t tlvs_length, void *context)
{
    uint8_t index;
    uint8_t *base = buf;

    for (index = 0; index < tlvs_length; index++) {
        if (tlvs[index] & TYPE_LENGTH_FIXED_FLAG) {
            buf += get_tv_value(network, buf, tlvs[index], context);
        }
    }

    return (buf - base);
}

int16_t tlvs_calc_length(const uint8_t *tlvs, uint8_t tlvs_length)
{
    int16_t length = 0;
    uint8_t index;

    for (index = 0; index < tlvs_length; index++) {
        if (tlvs[index] & TYPE_LENGTH_FIXED_FLAG) {
            if (get_tv_value_length(tlvs[index]) != 0) {
                length += (get_tv_value_length(tlvs[index]) + sizeof(mm_tv_t));
            } else {
                return -1;
            }
        }
    }

    return length;
}

mm_tv_t *umesh_mm_get_tv(const uint8_t *data, const uint16_t length,
                         uint8_t type)
{
    uint16_t offset = 0;
    mm_tv_t *tv = NULL;

    while (offset < length) {
        tv = (mm_tv_t *)(data + offset);
        if (tv->type == type) {
            break;
        }

        if (tv->type & TYPE_LENGTH_FIXED_FLAG) {
            if (get_tv_value_length(tv->type) != 0) {
                offset += sizeof(mm_tv_t) + get_tv_value_length(tv->type);
            } else {
                return NULL;
            }
        } else {
            offset += sizeof(mm_tlv_t) + ((mm_tlv_t *)tv)->length;
        }
    }

    if (offset >= length) {
        tv = NULL;
    }

    return tv;
}

static void handle_attach_timer(void *args)
{
    bool detached = false;

    MESH_LOG_DEBUG("handle attach timer");
    g_mm_state.attach_context.attach_timer = NULL;
    switch (g_mm_state.attach_context.attach_state) {
        case ATTACH_REQUEST:
            if (g_mm_state.attach_context.retry_times < ATTACH_REQUEST_RETRY_TIMES) {
                ++g_mm_state.attach_context.retry_times;
                send_attach_request();
                ur_start_timer(&g_mm_state.attach_context.attach_timer, ATTACH_REQUEST_INTERVAL,
                               handle_attach_timer, NULL);
            } else {
                detached = true;
            }
            break;
        case ATTACH_SID_REQUEST:
            if (g_mm_state.attach_context.retry_times < ATTACH_REQUEST_RETRY_TIMES) {
                ++g_mm_state.attach_context.retry_times;
                send_sid_request();
                ur_start_timer(&g_mm_state.attach_context.attach_timer, ATTACH_REQUEST_INTERVAL,
                               handle_attach_timer, NULL);
            } else {
                detached = true;
            }
            break;
        default:
            break;
    }

    if (detached) {
        g_mm_state.attach_context.attach_state = ATTACH_IDLE;
        g_mm_state.attach_context.candidate_meshnetid = BCAST_NETID;
        g_mm_state.attach_context.attach_candidate = NULL;
        if (g_mm_state.device.state < DEVICE_STATE_LEAF) {
            g_mm_state.attach_context.leader_times++;
            if (g_mm_state.attach_context.leader_times >= BECOME_LEADER_TIMEOUT) {
                g_mm_state.attach_context.leader_times = 0;
                become_leader();
            } else {
                become_detached(INTERFACE_DOWN_ATTACH_FAIL);
            }
            return;
        }
        if (g_mm_state.device.state > DEVICE_STATE_ATTACHED) {
            umesh_mm_set_channel(get_default_hal_context(), g_mm_state.device.prev_channel);
            start_advertisement_timer(get_default_network_context());
        }
    }
}

static void handle_migrate_reset_timer(void *args)
{
    g_mm_state.attach_context.migrate_reset_timer = NULL;
    g_mm_state.attach_context.prev_netid = BCAST_NETID;
    g_mm_state.attach_context.candidate_meshnetid = BCAST_NETID;
}

ur_error_t send_advertisement(network_context_t *network)
{
    ur_error_t error = UR_ERROR_MEM;
    message_t *message;
    uint16_t length;
    uint8_t *data;
    uint8_t *data_orig;
    message_info_t *info;

    if (network == NULL || umesh_mm_get_local_sid() == BCAST_SID) {
        return UR_ERROR_FAIL;
    }

    length = sizeof(mm_header_t) + sizeof(mm_netinfo_tv_t) + sizeof(mm_cost_tv_t);
    if (network->hal->module->type == MEDIA_TYPE_WIFI) {
        length += sizeof(mm_channel_tv_t);
    }
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
    if (network->router->sid_type == STRUCTURED_SID) {
        data += set_mm_ssid_info_tv(network, data);
    }
    data += set_mm_path_cost_tv(data);
    data += set_mm_channel_tv(network->hal, data);

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_ADVERTISEMENT,
                               data_orig, length, MESH_MGMT_1);
    if (message) {
        info = message->info;
        info->network = network;
        set_mesh_short_addr(&info->dest, BCAST_NETID, BCAST_SID);
        error = mf_send_message(message);
    }
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send advertisement, len %d", length);
    return error;
}

static ur_error_t send_attach_request(void)
{
    ur_error_t error = UR_ERROR_NONE;
    uint16_t length;
    uint8_t *data;
    uint8_t *data_orig;
    message_t *message = NULL;
    message_info_t *info;
    const mac_address_t *mac;
    uint32_t timestamp;

    if (g_mm_state.attach_context.attach_candidate == NULL) {
        return UR_ERROR_FAIL;
    }

    length = sizeof(mm_header_t) + sizeof(mm_uuid_tv_t) + sizeof(mm_timestamp_tv_t);
    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);
    data += set_mm_uuid_tv(data, TYPE_SRC_UUID, g_mm_state.device.uuid);
    timestamp = umesh_get_timestamp();
    data += set_mm_timestamp_tv(data, timestamp);
    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_ATTACH_REQUEST,
                               data_orig, length, MESH_MGMT_2);
    if (message == NULL) {
        ur_mem_free(data_orig, length);
        return UR_ERROR_MEM;
    }

    info = message->info;
    mac = umesh_mm_get_mac_address();
    calculate_one_time_key(NULL, timestamp, mac->addr);
    set_mesh_short_addr(&info->dest, g_mm_state.attach_context.attach_candidate->netid,
                        g_mm_state.attach_context.attach_candidate->sid);
    error = mf_send_message(message);
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send attach request, len %d", length);

    return error;
}

static ur_error_t send_attach_response(network_context_t *network,
                                       ur_addr_t *dest, ur_node_id_t *node_id)
{
    ur_error_t error = UR_ERROR_MEM;
    message_t *message;
    uint8_t *data;
    uint8_t *data_orig;
    uint16_t length;
    message_info_t *info;
    mm_symmetric_key_tv_t *symmetric_key;

    length = sizeof(mm_header_t) + sizeof(mm_cost_tv_t) +
             sizeof(mm_uuid_tv_t) + sizeof(mm_timestamp_tv_t);

    if (umesh_mm_get_seclevel() > SEC_LEVEL_0) {
        length += sizeof(mm_symmetric_key_tv_t);
    }

    if (is_unique_sid(node_id->sid)) {
        length += (sizeof(mm_sid_tv_t) + sizeof(mm_node_type_tv_t) +
                   sizeof(mm_netinfo_tv_t) + sizeof(mm_mcast_addr_tv_t));
    } else if ((node_id->mode & MODE_MOBILE) == 0 && network->router->sid_type == STRUCTURED_SID) {
        return UR_ERROR_FAIL;
    }
#ifdef CONFIG_AOS_MESH_LOWPOWER
    if ((node_id->mode & MODE_RX_ON) == 0) {
        length += sizeof(mm_time_slot_tv_t);
    }
#endif

    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);
    data += set_mm_uuid_tv(data, TYPE_SRC_UUID, g_mm_state.device.uuid);
    data += set_mm_path_cost_tv(data);
    data += set_mm_timestamp_tv(data, umesh_get_timestamp());
#ifdef CONFIG_AOS_MESH_LOWPOWER
    if ((node_id->mode & MODE_RX_ON) == 0) {
        data += lowpower_set_info(TYPE_TIME_SLOT, data, NULL);
    }
#endif

    if (umesh_mm_get_seclevel() > SEC_LEVEL_0) {
        symmetric_key = (mm_symmetric_key_tv_t *)data;
        umesh_mm_init_tv_base((mm_tv_t *)symmetric_key, TYPE_SYMMETRIC_KEY);
        memcpy(symmetric_key->symmetric_key, get_key(GROUP_KEY1_INDEX), KEY_SIZE);
        data += sizeof(mm_symmetric_key_tv_t);
    }
    if (is_unique_sid(node_id->sid)) {
        data += set_mm_sid_tv(data, TYPE_ALLOCATE_SID, node_id->sid);
        data += set_mm_allocated_node_type_tv(data, node_id->type);
        data += set_mm_netinfo_tv(network, data);
        data += set_mm_mcast_tv(data);
    }

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_ATTACH_RESPONSE,
                               data_orig, length, MESH_MGMT_3);
    if (message) {
        info = message->info;
        info->network = network;
        memcpy(&info->dest, dest, sizeof(info->dest));
        error = mf_send_message(message);
    }
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send attach response, len %d", length);
    return error;
}

static ur_error_t handle_attach_request(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    mm_uuid_tv_t *uuid;
    mm_timestamp_tv_t *timestamp;
    uint8_t *tlvs;
    uint16_t tlvs_length;
    neighbor_t *node;
    network_context_t *network;
    message_info_t *info;

    info = message->info;
    network = (network_context_t *)info->network;

    MESH_LOG_DEBUG("handle attach request");

    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t);
    tlvs = ur_mem_alloc(tlvs_length);
    if (tlvs == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, sizeof(mm_header_t), tlvs, tlvs_length);

    uuid = (mm_uuid_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_SRC_UUID);
    if (uuid == NULL) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    if ((node = update_neighbor(info, tlvs, tlvs_length, true)) == NULL) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    if (g_mm_state.device.state < DEVICE_STATE_LEADER ||
        g_mm_state.attach_context.attach_candidate) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    if (is_pf_mode(umesh_get_mode()) || (node && node == g_mm_state.attach_context.attach_node)) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    timestamp = (mm_timestamp_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_TIMESTAMP);
    if (timestamp) {
        if (node->one_time_key == NULL) {
            node->one_time_key = ur_mem_alloc(KEY_SIZE);
        }
        error = calculate_one_time_key(node->one_time_key,
                                       timestamp->timestamp, node->mac);
    }

    if (error == UR_ERROR_NONE) {
        ur_node_id_t node_id;
        node_id.sid = INVALID_SID;
        memcpy(node_id.uuid, uuid->uuid, sizeof(node_id.uuid));
        if (umesh_mm_get_mode() & MODE_SUPER) {
            node_id.attach_sid = SUPER_ROUTER_SID;
        } else {
            node_id.attach_sid = umesh_mm_get_local_sid();
        }
        node_id.mode = info->mode;
        if (is_bcast_sid(&info->dest) == false &&
            (info->mode & MODE_LOW_MASK) == 0 &&
            (network->router->sid_type == STRUCTURED_SID)) {
            error = sid_allocator_alloc(network, &node_id);
            if (error != UR_ERROR_NONE) {
                node_id.sid = INVALID_SID;
            }
        }
        send_attach_response(network, &info->src_mac, &node_id);
        MESH_LOG_INFO("attach response to " EXT_ADDR_FMT "",
                      EXT_ADDR_DATA(info->src_mac.addr.addr));
    }

exit:
    ur_mem_free(tlvs, tlvs_length);
    return error;
}

static ur_error_t handle_attach_response(message_t *message)
{
    ur_error_t error;
    neighbor_t *nbr;
    mm_cost_tv_t *path_cost;
    uint8_t *tlvs;
    uint16_t tlvs_length;
    message_info_t *info;
    mm_symmetric_key_tv_t *symmetric_key;

    info = message->info;
    if (is_in_attaching(g_mm_state.attach_context.attach_state) == false) {
        return UR_ERROR_NONE;
    }

    MESH_LOG_DEBUG("handle attach response");

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

    path_cost = (mm_cost_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_PATH_COST);
    if (path_cost == NULL) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    symmetric_key = (mm_symmetric_key_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_SYMMETRIC_KEY);
    if (umesh_mm_get_seclevel() > SEC_LEVEL_0) {
        if (symmetric_key == NULL) {
            error = UR_ERROR_FAIL;
            goto exit;
        }
        set_key(GROUP_KEY1_INDEX, symmetric_key->symmetric_key, KEY_SIZE);
    }

    if (info->src.netid == g_mm_state.attach_context.prev_netid &&
        (g_mm_state.attach_context.prev_path_cost < (path_cost->cost + nbr->stats.link_cost))) {
        return UR_ERROR_NONE;
    }
    update_mm_timestamp(tlvs, tlvs_length);
    nbr->attach_candidate_timeout = 0;
    if (g_mm_state.attach_context.attach_candidate == NULL) {
        g_mm_state.attach_context.attach_candidate = nbr;
        g_mm_state.attach_context.attach_candidate->flags &= (~NBR_SID_CHANGED);
    }

    g_mm_state.device.state = DEVICE_STATE_ATTACHED;
    ur_stop_timer(&g_mm_state.attach_context.attach_timer, NULL);
    error = sid_allocated_handler(info, tlvs, tlvs_length);
    if (error != UR_ERROR_NONE) {
        g_mm_state.attach_context.attach_state = ATTACH_SID_REQUEST;
        send_sid_request();
        g_mm_state.attach_context.retry_times = 1;
        ur_start_timer(&g_mm_state.attach_context.attach_timer, ATTACH_REQUEST_INTERVAL,
                       handle_attach_timer, NULL);
    }

exit:
    ur_mem_free(tlvs, tlvs_length);
    return error;
}

static ur_error_t send_sid_request(void)
{
    ur_error_t error = UR_ERROR_NONE;
    network_context_t *network = get_default_network_context();
    message_t *message;
    uint8_t *data;
    uint8_t *data_orig;
    uint16_t length;
    message_info_t *info;
    ur_node_id_t node_id;
    uint16_t netid;
    uint16_t sid;

    if (g_mm_state.attach_context.attach_candidate == NULL) {
        return UR_ERROR_FAIL;
    }

    length = sizeof(mm_header_t) + sizeof(mm_node_id_tv_t) + sizeof(mm_uuid_tv_t) +
             sizeof(mm_mode_tv_t);
    if (is_unique_sid(g_mm_state.attach_context.sid) &&
        g_mm_state.attach_context.attach_candidate->netid == network->meshnetid) {
        length += sizeof(mm_sid_tv_t);
    }

    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);
    node_id.sid = g_mm_state.attach_context.attach_candidate->sid;
    node_id.mode = g_mm_state.device.mode;
    node_id.meshnetid = g_mm_state.attach_context.attach_candidate->netid;
    data += set_mm_node_id_tv(data, TYPE_ATTACH_NODE_ID, &node_id);
    data += set_mm_uuid_tv(data, TYPE_SRC_UUID, g_mm_state.device.uuid);
    data += set_mm_mode_tv(data);

    if ((data - data_orig) < length) {
        data += set_mm_sid_tv(data, TYPE_SRC_SID, g_mm_state.attach_context.sid);
    }

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_SID_REQUEST,
                               data_orig, length, MESH_MGMT_4);
    if (message == NULL) {
        ur_mem_free(data_orig, length);
        return UR_ERROR_MEM;
    }

    info = message->info;
    info->network = network;
    // dest
    sid = g_mm_state.attach_context.attach_candidate->sid;
    netid = g_mm_state.attach_context.attach_candidate->netid;
    if ((g_mm_state.device.mode & MODE_MOBILE) || (network->router->sid_type != STRUCTURED_SID)) {
        netid = get_main_netid(netid);
        set_mesh_short_addr(&info->dest2, netid, BCAST_SID);
    }
    set_mesh_short_addr(&info->dest, netid, sid);

    error = mf_send_message(message);
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send sid request, len %d", length);
    return error;
}

static ur_error_t send_sid_response(network_context_t *network,
                                    ur_addr_t *dest, ur_addr_t *dest2,
                                    ur_node_id_t *node_id)
{
    ur_error_t error = UR_ERROR_NONE;
    uint8_t *data;
    uint8_t *data_orig;
    message_t *message;
    uint16_t length;
    message_info_t *info;

    if (network == NULL) {
        return UR_ERROR_FAIL;
    }

    length = sizeof(mm_header_t) + sizeof(mm_sid_tv_t) + sizeof(mm_node_type_tv_t) +
             sizeof(mm_netinfo_tv_t) + sizeof(mm_mcast_addr_tv_t);

    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;

    data += sizeof(mm_header_t);
    data += set_mm_sid_tv(data, TYPE_ALLOCATE_SID, node_id->sid);
    data += set_mm_allocated_node_type_tv(data, node_id->type);
    data += set_mm_netinfo_tv(network, data);
    data += set_mm_mcast_tv(data);

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_SID_RESPONSE,
                               data_orig, length, MESH_MGMT_5);
    if (message == NULL) {
        ur_mem_free(data_orig, length);
        return UR_ERROR_MEM;
    }

    info = message->info;
    info->network = network;
    // dest
    memcpy(&info->dest, dest, sizeof(info->dest));
    memcpy(&info->dest2, dest2, sizeof(info->dest));

    error = mf_send_message(message);
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send sid response %04x:%d, len %d", node_id->sid, node_id->type, length);
    return error;
}

static ur_error_t handle_sid_request(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    mm_node_id_tv_t *attach_node_id;
    mm_sid_tv_t *src_sid;
    mm_uuid_tv_t *uuid;
    mm_mode_tv_t *mode;
    uint8_t *tlvs;
    uint16_t tlvs_length;
    network_context_t *network;
    message_info_t *info;
    ur_node_id_t node_id;

    info = message->info;
    network = info->network;
    if (g_mm_state.attach_context.attach_candidate ||
        g_mm_state.device.state < DEVICE_STATE_LEADER) {
        return UR_ERROR_FAIL;
    }

    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t);
    tlvs = ur_mem_alloc(tlvs_length);
    if (tlvs == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, sizeof(mm_header_t), tlvs, tlvs_length);
    attach_node_id = (mm_node_id_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_ATTACH_NODE_ID);
    src_sid = (mm_sid_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_SRC_SID);
    uuid = (mm_uuid_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_SRC_UUID);
    mode = (mm_mode_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_MODE);
    if (uuid == NULL || mode == NULL) {
        ur_mem_free(tlvs, tlvs_length);
        return UR_ERROR_FAIL;
    }

    MESH_LOG_DEBUG("handle sid request");

    memset(&node_id, 0, sizeof(node_id));
    node_id.sid = INVALID_SID;
    if (src_sid) {
        node_id.sid = src_sid->sid;
    }
    if (mode->mode & MODE_MOBILE) {
        network = get_sub_network_context(network->hal);
    }
    memcpy(node_id.uuid, uuid->uuid, sizeof(node_id.uuid));
    neighbor_t *node = get_neighbor_by_mac_addr(node_id.uuid, NULL);
    node_id.sid = (node? node_id.sid: INVALID_SID);
    if (attach_node_id) {
#ifdef CONFIG_AOS_MESH_SUPER
        node_id.attach_sid = (attach_node_id->mode & MODE_SUPER)? SUPER_ROUTER_SID: attach_node_id->sid;
#else
        node_id.attach_sid = attach_node_id->sid;
#endif
    }
    node_id.mode = mode->mode;
    error = sid_allocator_alloc(network, &node_id);
    if (error == UR_ERROR_NONE) {
        ur_addr_t dest;
        ur_addr_t dest2;
        set_mesh_short_addr(&dest, attach_node_id->meshnetid, attach_node_id->sid);
        set_mesh_ext_addr(&dest2, BCAST_NETID, uuid->uuid);
        network = get_network_context_by_meshnetid(dest.netid, true);
        error = send_sid_response(network, &dest, &dest2, &node_id);
    }
    ur_mem_free(tlvs, tlvs_length);
    return error;
}

static ur_error_t handle_sid_response(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    uint8_t *tlvs;
    uint16_t tlvs_length;
    message_info_t *info;

    MESH_LOG_DEBUG("handle sid response");

    info = message->info;
    if (g_mm_state.attach_context.attach_candidate == NULL) {
        return UR_ERROR_NONE;
    }
    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t);
    tlvs = ur_mem_alloc(tlvs_length);
    if (tlvs == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, sizeof(mm_header_t), tlvs, tlvs_length);
    error = sid_allocated_handler(info, tlvs, tlvs_length);
    ur_mem_free(tlvs, tlvs_length);
    return error;
}

static void mesh_interface_state_callback(interface_state_t state)
{
    mm_cb_t *callback;

    slist_for_each_entry(&g_mm_state.interface_callback, callback, mm_cb_t, next) {
        state == INTERFACE_UP? callback->interface_up(): callback->interface_down(state);
    }
}

ur_error_t send_address_error(network_context_t *network, ur_addr_t *dest)
{
    ur_error_t error = UR_ERROR_MEM;
    message_t *message;
    uint8_t *data;
    uint8_t *data_orig;
    uint16_t length;
    message_info_t *info;

    length = sizeof(mm_header_t);

    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_ADDRESS_ERROR,
                               data_orig, length, MESH_MGMT_7);
    if (message) {
        info = message->info;
        info->network = network;
        memcpy(&info->dest, dest, sizeof(info->dest));
        error = mf_send_message(message);
    }
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send address error, len %d", length);
    return error;
}

ur_error_t handle_address_error(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    message_info_t *info;

    info = message->info;
    MESH_LOG_DEBUG("handle address error");

    if (g_mm_state.attach_context.attach_node == NULL) {
        return error;
    }

    if (memcmp(info->src_mac.addr.addr, g_mm_state.attach_context.attach_node->mac,
               EXT_ADDR_SIZE) != 0) {
        return error;
    }

    attach_start(g_mm_state.attach_context.attach_node);
    return error;
}

void become_detached(interface_state_t reason)
{
    slist_t *networks;
    network_context_t *network;

    if (g_mm_state.device.state != DEVICE_STATE_DETACHED) {
        reset_network_context();
        MESH_LOG_INFO("become detached, reason %d", reason);
        g_mm_state.device.state = DEVICE_STATE_DETACHED;
        g_mm_state.attach_context.attach_state = ATTACH_IDLE;
        ur_stop_timer(&g_mm_state.attach_context.attach_timer, NULL);
        ur_stop_timer(&g_mm_state.attach_context.migrate_reset_timer, NULL);
        g_mm_state.attach_context.sid = BCAST_SID;
        g_mm_state.attach_context.path_cost = INFINITY_PATH_COST;
        g_mm_state.attach_context.prev_netid = INVALID_NETID;
        g_mm_state.attach_context.prev_path_cost = INFINITY_PATH_COST;
        if (g_mm_state.attach_context.attach_node != NULL &&
            g_mm_state.attach_context.attach_node->state == STATE_PARENT) {
            g_mm_state.attach_context.attach_node->state = STATE_NEIGHBOR;
        }
        g_mm_state.attach_context.attach_node = NULL;
        g_mm_state.attach_context.attach_candidate = NULL;
        g_mm_state.attach_context.candidate_meshnetid = BCAST_NETID;
        g_mm_state.attach_context.migrate_times = 0;
    }

    write_prev_netinfo();
    nd_init();
    mesh_interface_state_callback(reason);
    networks = get_network_contexts();
    slist_for_each_entry(networks, network, network_context_t, next) {
        sid_allocator_deinit(network);
    }
}

static neighbor_t *choose_attach_candidate(neighbor_t *nbr)
{
    slist_t *nbrs;
    int8_t cmp_mode;
    neighbor_t *attach_candidate = NULL;
    uint16_t cur_metric;
    uint16_t new_metric;
    hal_context_t *hal = get_default_hal_context();
    network_context_t *network = get_default_network_context();
    uint16_t netid = (nbr? nbr->netid: INVALID_NETID);

    if (nbr && nbr->attach_candidate_timeout == 0 &&
        (network->router->sid_type != STRUCTURED_SID || nbr->ssid_info.free_slots > 0)) {
        attach_candidate = nbr;
    }
    nbrs = umesh_get_nbrs(hal->module->type);
    slist_for_each_entry(nbrs, nbr, neighbor_t, next) {
        cmp_mode = umesh_mm_compare_mode(g_mm_state.device.mode, nbr->mode);
        if (cmp_mode < 0 || nbr->attach_candidate_timeout > 0 || (nbr->mode & MODE_MOBILE) ||
            (network->router->sid_type == STRUCTURED_SID && nbr->ssid_info.free_slots < 1) ||
            is_unique_netid(nbr->netid) == false || is_unique_sid(nbr->sid) == false ||
            (is_unique_netid(netid) && nbr->netid != netid)) {
            continue;
        }
        if (attach_candidate == NULL) {
            attach_candidate = nbr;
        } else {
            cur_metric = compute_network_metric(0, attach_candidate->path_cost);
            new_metric = compute_network_metric(0, nbr->path_cost);
            if (new_metric < cur_metric) {
                attach_candidate = nbr;
            }
        }
    }
    return attach_candidate;
}

static ur_error_t attach_start(neighbor_t *nbr)
{
    network_context_t *network = NULL;

    network = get_default_network_context();
    if (is_in_attaching(g_mm_state.attach_context.attach_state)) {
        return UR_ERROR_BUSY;
    }

    nbr = choose_attach_candidate(nbr);
    if (nbr == NULL) {
        if (g_mm_state.device.state < DEVICE_STATE_LEAF) {
            become_detached(INTERFACE_DOWN_ATTACH_FAIL);
        }
        return UR_ERROR_FAIL;
    }
    g_mm_state.attach_context.attach_candidate = nbr;
    g_mm_state.attach_context.attach_state = ATTACH_REQUEST;
    update_channel(network->hal, nbr->channel);
    g_mm_state.attach_context.candidate_meshnetid = nbr->netid;
    send_attach_request();
    ur_start_timer(&g_mm_state.attach_context.attach_timer, ATTACH_REQUEST_INTERVAL,
                   handle_attach_timer, NULL);
    g_mm_state.attach_context.retry_times = 1;
    ur_stop_timer(&network->advertisement_timer, network);
    umesh_network_stop_discover();

    MESH_LOG_INFO("%d node, attach start, from %04x:%04x to %04x:%x",
                  g_mm_state.device.state, g_mm_state.attach_context.attach_node ?
                  g_mm_state.attach_context.attach_node->sid : 0,
                  network->meshnetid, nbr->sid,
                  g_mm_state.attach_context.candidate_meshnetid);
    return UR_ERROR_NONE;
}

static uint16_t compute_network_metric(uint16_t size, uint16_t path_cost)
{
    return size / SIZE_WEIGHT + path_cost / PATH_COST_WEIGHT;
}

static void write_prev_netinfo(void)
{
    network_context_t *network;

    network = get_default_network_context();
    if (network) {
        g_mm_state.attach_context.prev_netid = network->meshnetid;
        g_mm_state.attach_context.prev_path_cost = g_mm_state.attach_context.path_cost;
    }
}

static bool update_migrate_times(neighbor_t *nbr)
{
    network_context_t *network = get_default_network_context();
    uint16_t netid;
    uint8_t timeout;
    uint32_t interval = network->hal->advertisement_interval * MIGRATE_RESET_TIMEOUT;

    netid = nbr->netid;
    if (netid == BCAST_NETID) {
        return false;
    }
    if (g_mm_state.attach_context.migrate_reset_timer == NULL) {
        g_mm_state.attach_context.migrate_times = 0;
        ur_start_timer(&g_mm_state.attach_context.migrate_reset_timer, interval,
                       handle_migrate_reset_timer, NULL);
        g_mm_state.attach_context.candidate_meshnetid = netid;
    } else if (netid == g_mm_state.attach_context.candidate_meshnetid) {
        g_mm_state.attach_context.migrate_times++;
    }
    if (g_mm_state.device.state == DEVICE_STATE_DETACHED) {
        timeout = DETACHED_MIGRATE_TIMEOUT;
    } else {
        timeout = MIGRATE_TIMEOUT;
    }
    if (g_mm_state.attach_context.migrate_times < timeout) {
        return false;
    }

    if (network->router->sid_type == STRUCTURED_SID &&
        nbr->ssid_info.free_slots < 1) {
        nbr = NULL;
    }
    if (nbr == NULL && g_mm_state.device.state > DEVICE_STATE_ATTACHED) {
        return false;
    }

    ur_stop_timer(&g_mm_state.attach_context.migrate_reset_timer, NULL);
    return true;
}

static void update_network_data(network_context_t *network, mm_netinfo_tv_t *netinfo)
{
    int8_t diff = (int8_t)(netinfo->version - nd_get_version(NULL));
    network_data_t network_data;

    if (diff > 0 && g_mm_state.device.state != DEVICE_STATE_LEADER) {
        network_data.version = netinfo->version;
        network_data.size = netinfo->size;
        nd_set(NULL, &network_data);
        network_data.size = get_subnetsize_from_netinfo(netinfo);
        nd_set(network, &network_data);
    }
}

static ur_error_t handle_advertisement(message_t *message)
{
    uint8_t *tlvs;
    uint16_t tlvs_length;
    neighbor_t *nbr;
    mm_netinfo_tv_t *netinfo;
    mm_cost_tv_t *path_cost;
    network_context_t *network;
    message_info_t *info;
    ur_addr_t dest;

    if (g_mm_state.device.state < DEVICE_STATE_DETACHED) {
        return UR_ERROR_NONE;
    }
 
    MESH_LOG_DEBUG("handle advertisement");

    info = message->info;
    network = info->network;

    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t);
    tlvs = ur_mem_alloc(tlvs_length);
    if (tlvs == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, sizeof(mm_header_t), tlvs, tlvs_length);

    netinfo = (mm_netinfo_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_NETWORK_INFO);
    path_cost = (mm_cost_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_PATH_COST);
    if (netinfo == NULL || info->src.netid == BCAST_NETID || path_cost == NULL) {
        ur_mem_free(tlvs, tlvs_length);
        return UR_ERROR_FAIL;
    }

    nbr = update_neighbor(info, tlvs, tlvs_length, false);
    if (nbr == NULL) {
        ur_mem_free(tlvs, tlvs_length);
        return UR_ERROR_NONE;
    }

    if (network->router->sid_type == STRUCTURED_SID &&
        network->meshnetid == nbr->netid &&
        is_direct_child(network->sid_base, info->src.addr.short_addr)) {
        sid_node_t *sid_node = get_allocated_child(network->sid_base, nbr);
        if (sid_node == NULL) {
            set_mesh_ext_addr(&dest, nbr->netid, nbr->mac);
            send_address_error(network, &dest);
        }
    }
    if (network->meshnetid == nbr->netid) {
        update_network_data(network, netinfo);
    }
    if (network == get_default_network_context() && umesh_mm_migration_check(nbr, netinfo)) {
        attach_start(nbr);
    }

    ur_mem_free(tlvs, tlvs_length);
    return UR_ERROR_NONE;
}

ur_error_t umesh_mm_handle_frame_received(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    mm_header_t mm_header;

    message_copy_to(message, 0, (uint8_t *)&mm_header, sizeof(mm_header_t));
    switch (mm_header.command & COMMAND_COMMAND_MASK) {
        case COMMAND_ADVERTISEMENT:
            handle_advertisement(message);
            break;
        case COMMAND_DISCOVERY_REQUEST:
            handle_discovery_request(message);
            break;
        case COMMAND_DISCOVERY_RESPONSE:
            handle_discovery_response(message);
            break;
        case COMMAND_ATTACH_REQUEST:
            error = handle_attach_request(message);
            break;
        case COMMAND_ATTACH_RESPONSE:
            error = handle_attach_response(message);
            break;
        case COMMAND_SID_REQUEST:
            error = handle_sid_request(message);
            break;
        case COMMAND_SID_RESPONSE:
            error = handle_sid_response(message);
            break;
        case COMMAND_ADDRESS_QUERY:
            error = handle_address_query(message);
            break;
        case COMMAND_ADDRESS_QUERY_RESPONSE:
            error = handle_address_query_response(message);
            break;
        case COMMAND_ADDRESS_NOTIFICATION:
            error = handle_address_notification(message);
            break;
        case COMMAND_LINK_REQUEST:
            error = handle_link_request(message);
            break;
        case COMMAND_LINK_ACCEPT:
            error = handle_link_accept(message);
            break;
        case COMMAND_LINK_ACCEPT_AND_REQUEST:
            error = handle_link_accept_and_request(message);
            break;
        case COMMAND_ADDRESS_UNREACHABLE:
            error = handle_address_unreachable(message);
            break;
        case COMMAND_ADDRESS_ERROR:
            error = handle_address_error(message);
            break;
        case COMMAND_ROUTING_INFO_UPDATE:
            error = handle_router_message_received(message);
            break;
#ifdef CONFIG_AOS_MESH_AUTH
        case COMMAND_AUTH_DOT1X:
            error = handle_eapol(message);
            break;
#endif
        default:
            break;
    }
    MESH_LOG_DEBUG("cmd %d error %d",
                   mm_header.command & COMMAND_COMMAND_MASK, error);
    return error;
}

#ifdef CONFIG_AOS_MESH_LOWPOWER
static void lowpower_radio_down_handler(schedule_type_t type)
{
}

static void lowpower_radio_up_handler(schedule_type_t type)
{
}
#endif

ur_error_t umesh_mm_init(node_mode_t mode, mm_cb_t *mm_cb)
{
    ur_error_t error = UR_ERROR_NONE;
    ur_configs_t configs;
    hal_context_t *hal = get_default_hal_context();

    assert(mm_cb);

    // init device
    g_mm_state.device.state = DEVICE_STATE_DISABLED;
    // uuid is default mac address
    memcpy(g_mm_state.device.uuid, hal_umesh_get_mac_address(NULL),
           sizeof(g_mm_state.device.uuid));
    g_mm_state.device.seclevel = SEC_LEVEL_1;
    g_mm_state.device.prev_channel = hal->def_channel;
    register_neighbor_updater(neighbor_updated_handler);
    memset(&configs, 0, sizeof(configs));
    ur_configs_read(&configs);
    nd_set_stable_main_version(configs.main_version);
    umesh_mm_register_callback(mm_cb);
    umesh_network_mgmt_register_callback(nbr_discovered_handler);

    g_mm_state.device.mode = mode;
    if (get_hal_contexts_num() > 1) {
        g_mm_state.device.mode |= MODE_SUPER;
    }

#ifdef CONFIG_AOS_MESH_LOWPOWER
    g_mm_state.lowpower_callback.radio_down = lowpower_radio_down_handler;
    g_mm_state.lowpower_callback.radio_up = lowpower_radio_up_handler;
    lowpower_register_callback(&g_mm_state.lowpower_callback);
#endif
    return error;
}

ur_error_t umesh_mm_start(void)
{
    ur_error_t error = UR_ERROR_NONE;

    MESH_LOG_INFO("mesh started");

    g_mm_state.device.reboot_flag = true;

    if (g_mm_state.device.mode & MODE_LEADER) {
        become_leader();
    } else {
        become_detached(INTERFACE_DOWN_MESH_START);
    }
    return error;
}

ur_error_t umesh_mm_stop(void)
{
    become_detached(INTERFACE_DOWN_MESH_STOP);
    g_mm_state.device.state = DEVICE_STATE_DISABLED;
#ifdef CONFIG_AOS_MESH_AUTH
    stop_auth();
#endif
    return UR_ERROR_NONE;
}

uint16_t umesh_mm_get_meshnetid(network_context_t *network)
{
    network = (network? network: get_default_network_context());
    if (network == NULL) {
        return BCAST_NETID;
    } else if (network != get_default_network_context() ||
               is_in_attaching(g_mm_state.attach_context.attach_state) == false) {
        return network->meshnetid;
    } else {
        return g_mm_state.attach_context.attach_candidate->netid;
    }
}

uint16_t umesh_mm_get_meshnetsize(void)
{
    return nd_get_meshnetsize(NULL);
}

uint16_t umesh_mm_get_local_sid(void)
{
    if (is_in_attaching(g_mm_state.attach_context.attach_state) == false) {
        return g_mm_state.attach_context.sid;
    } else {
        return BCAST_SID;
    }
}

uint8_t *umesh_mm_get_local_uuid(void)
{
    return g_mm_state.device.uuid;
}

ur_error_t umesh_mm_set_mode(node_mode_t mode)
{
    uint8_t num;
    hal_context_t *wifi_hal;

    num = get_hal_contexts_num();
    wifi_hal = get_hal_context(MEDIA_TYPE_WIFI);
    if (mode & MODE_SUPER) {
        if ((wifi_hal == NULL && num <= 1) ||
            (mode & MODE_MOBILE)) {
            return UR_ERROR_FAIL;
        }
    } else if (num > 1) {
        return UR_ERROR_FAIL;
    }
    g_mm_state.device.mode = mode;
    return UR_ERROR_NONE;
}

node_mode_t umesh_mm_get_mode(void)
{
    return g_mm_state.device.mode;
}

int8_t umesh_mm_compare_mode(node_mode_t local, node_mode_t other)
{
    local &= (~MODE_RX_ON);
    other &= (~MODE_RX_ON);
    if ((local & MODE_HI_MASK) == (other & MODE_HI_MASK)) {
        if ((local & MODE_LOW_MASK) == (other & MODE_LOW_MASK) ||
            ((local & MODE_LOW_MASK) != 0 && (other & MODE_LOW_MASK) != 0)) {
            return 0;
        } else if ((local & MODE_LOW_MASK) == 0) {
            return 2;
        } else {
            return -2;
        }
    }

    if ((local & MODE_HI_MASK) > (other & MODE_HI_MASK)) {
        return 1;
    }

    return -1;
}

ur_error_t umesh_mm_set_seclevel(int8_t level)
{
    if (level >= SEC_LEVEL_0 && level <= SEC_LEVEL_1) {
        g_mm_state.device.seclevel = level;
        return UR_ERROR_NONE;
    }

    return UR_ERROR_FAIL;
}

int8_t umesh_mm_get_seclevel(void)
{
    return g_mm_state.device.seclevel;
}

void umesh_mm_get_extnetid(umesh_extnetid_t *extnetid)
{
    network_context_t *network;

    network = get_default_network_context();
    if (network == NULL) {
        return;
    }

    hal_umesh_get_extnetid(network->hal->module, extnetid);
}

ur_error_t umesh_mm_set_extnetid(const umesh_extnetid_t *extnetid)
{
    slist_t *networks;
    network_context_t *network;

    networks = get_network_contexts();
    slist_for_each_entry(networks, network, network_context_t, next) {
        hal_umesh_set_extnetid(network->hal->module, extnetid);
    }

    return UR_ERROR_NONE;
}

const mac_address_t *umesh_mm_get_mac_address(void)
{
    hal_context_t *hal;

    hal = get_default_hal_context();
    if (hal) {
        return &hal->mac_addr;
    }
    return NULL;
}

uint16_t umesh_mm_get_channel(hal_context_t *hal)
{
    return hal->channel;
}

void umesh_mm_set_channel(hal_context_t *hal, uint16_t channel)
{
    if (g_mm_state.device.mode & MODE_LEADER) {
        channel = hal_umesh_get_channel(hal->module);
    }
    if (hal_umesh_set_channel(hal->module, channel) == 0) {
        hal->channel = channel;
    }
}

node_state_t umesh_mm_get_device_state(void)
{
    return g_mm_state.device.state;
}

attach_state_t umesh_mm_get_attach_state(void)
{
    return g_mm_state.attach_context.attach_state;
}

neighbor_t *umesh_mm_get_attach_node(void)
{
    return g_mm_state.attach_context.attach_node;
}

neighbor_t *umesh_mm_get_attach_candidate(void)
{
    return g_mm_state.attach_context.attach_candidate;
}

uint8_t umesh_mm_get_leader_mode(void)
{
    return g_mm_state.leader_mode;
}

bool umesh_mm_migration_check(neighbor_t *nbr, mm_netinfo_tv_t *netinfo)
{
    network_context_t *network = get_default_network_context();
    int8_t cmp_mode = 0;
    bool from_same_net = false;
    bool from_same_core = false;
    neighbor_t *attach_node;
    bool leader_reboot = false;
    int8_t diff;
    uint16_t new_metric;
    uint16_t cur_metric;
    uint8_t main_version;
    uint16_t net_size = 0;

    // not try to migrate to pf node, and mode leader would not migrate
    if ((nbr->mode & MODE_MOBILE) || (g_mm_state.device.mode & MODE_LEADER)) {
        return false;
    }

    cmp_mode = umesh_mm_compare_mode(g_mm_state.leader_mode, netinfo->leader_mode);
    if (cmp_mode < 0) {
        become_detached(INTERFACE_DOWN_LEADER_MODE);
    }
    // detached node try to migrate
    if (g_mm_state.device.state == DEVICE_STATE_DETACHED) {
        return update_migrate_times(nbr);
    }

    // leader not try to migrate to the same net
    if (network->meshnetid == nbr->netid) {
        from_same_net = true;
    } else if (is_same_mainnet(network->meshnetid, nbr->netid)) {
        from_same_core = true;
    }
    attach_node = g_mm_state.attach_context.attach_node;
    if (from_same_net && attach_node == NULL) {
        return false;
    }

    if (from_same_net) {
        main_version = (netinfo->stable_version & STABLE_MAIN_VERSION_MASK) >>
                       STABLE_MAIN_VERSION_OFFSET;
        diff = (main_version + 8 - nd_get_stable_main_version()) % 8;
        if (diff > 0 && diff < 4 &&
            g_mm_state.device.state > DEVICE_STATE_ATTACHED &&
            g_mm_state.device.state != DEVICE_STATE_LEADER) {
            nd_set_stable_main_version(main_version);
            leader_reboot = true;
        }
        if ((nbr == attach_node) &&
            (attach_node->flags & NBR_REBOOT) &&
            ((attach_node->flags & NBR_SID_CHANGED) == 0)) {
            leader_reboot = true;
            attach_node->flags &= (~NBR_REBOOT);
        }
        if (leader_reboot) {
            nbr = g_mm_state.attach_context.attach_node;
            become_detached(INTERFACE_DOWN_LEADER_REBOOT);
            attach_start(nbr);
        }
        if (attach_node == nbr) {
            g_mm_state.attach_context.path_cost = nbr->path_cost + nbr->stats.link_cost;
        }

        if (g_mm_state.attach_context.path_cost <= (nbr->path_cost + PATH_COST_SWITCH_HYST) ||
            attach_node == nbr) {
            return false;
        }
    } else {
        if (!is_unique_netid(nbr->netid) ||
            ((nbr->netid == g_mm_state.attach_context.prev_netid) &&
             (g_mm_state.attach_context.prev_path_cost < nbr->path_cost))) {
            return false;
        }
        if (from_same_core) {
            net_size = (netinfo->subnet_size_1 << 8) | netinfo->subnet_size_2;
            new_metric = compute_network_metric(net_size, 0);
            cur_metric = compute_network_metric(nd_get_meshnetsize(network), 0);
            if (cur_metric < (new_metric + 5)) {
                return false;
            }
        } else {
            net_size = netinfo->size;
            new_metric = compute_network_metric(net_size, 0);
            cur_metric = compute_network_metric(nd_get_meshnetsize(NULL), 0);
            if ((is_subnet(network->meshnetid) && is_subnet(nbr->netid)) ||
                (is_subnet(network->meshnetid) == 0 && is_subnet(nbr->netid) == 0)) {
                if ((new_metric < cur_metric) ||
                    (new_metric == cur_metric && nbr->netid <= network->meshnetid)) {
                    return false;
                }
            }
        }
    }

    return update_migrate_times(nbr);
}

uint8_t umesh_mm_get_prev_channel(void)
{
    return g_mm_state.device.prev_channel;
}

void umesh_mm_set_prev_channel(void)
{
    hal_context_t *hal = get_default_hal_context();

    g_mm_state.device.prev_channel = hal->channel;
}

uint8_t umesh_mm_get_reboot_flag(void)
{
    return g_mm_state.device.reboot_flag;
}

void umesh_mm_register_callback(mm_cb_t *callback)
{
    slist_add(&callback->next, &g_mm_state.interface_callback);
}

uint8_t set_mm_netinfo_tv(network_context_t *network, uint8_t *data)
{
    mm_netinfo_tv_t *netinfo;

    netinfo = (mm_netinfo_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)netinfo, TYPE_NETWORK_INFO);
    netinfo->stable_version = (nd_get_stable_main_version() <<
                               STABLE_MAIN_VERSION_OFFSET) |
                              nd_get_stable_minor_version();
    netinfo->version = nd_get_version(NULL);
    netinfo->size = nd_get_meshnetsize(NULL);
    set_subnetsize_to_netinfo(netinfo, nd_get_meshnetsize(network));
    netinfo->leader_mode = umesh_mm_get_leader_mode();

    return sizeof(mm_netinfo_tv_t);
}

uint8_t set_mm_channel_tv(hal_context_t *hal, uint8_t *data)
{
    mm_channel_tv_t *channel;

    if (hal->module->type != MEDIA_TYPE_WIFI) {
        return 0;
    }

    channel = (mm_channel_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)channel, TYPE_UCAST_CHANNEL);
    channel->channel = umesh_mm_get_channel(hal);

    return sizeof(mm_channel_tv_t);
}

uint8_t set_mm_uuid_tv(uint8_t *data, uint8_t type, uint8_t *uuid)
{
    mm_uuid_tv_t *target_uuid;

    target_uuid = (mm_uuid_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)target_uuid, type);
    memcpy(target_uuid->uuid, uuid, sizeof(target_uuid->uuid));
    return sizeof(mm_uuid_tv_t);
}

uint8_t set_mm_path_cost_tv(uint8_t *data)
{
    mm_cost_tv_t *path_cost;

    path_cost = (mm_cost_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)path_cost, TYPE_PATH_COST);
    path_cost->cost = g_mm_state.attach_context.path_cost;
    if (g_mm_state.device.state == DEVICE_STATE_LEAF ||
        is_in_attaching(g_mm_state.attach_context.attach_state)) {
        path_cost->cost = INFINITY_PATH_COST;
    }
    return sizeof(mm_cost_tv_t);
}

uint8_t set_mm_node_id_tv(uint8_t *data, uint8_t type, ur_node_id_t *node)
{
    mm_node_id_tv_t *node_id;

    node_id = (mm_node_id_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)node_id, type);
    node_id->sid = node->sid;
    node_id->mode = node->mode;
    node_id->meshnetid = node->meshnetid;
    return sizeof(mm_node_id_tv_t);
}

uint8_t set_mm_sid_tv(uint8_t *data, uint8_t type, uint16_t sid)
{
    mm_sid_tv_t *id;

    id = (mm_sid_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)id, type);
    id->sid = sid;
    return sizeof(mm_sid_tv_t);
}

uint8_t set_mm_mode_tv(uint8_t *data)
{
    mm_mode_tv_t *mode;

    mode = (mm_mode_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)mode, TYPE_MODE);
    mode->mode = (uint8_t)g_mm_state.device.mode;
    return sizeof(mm_mode_tv_t);
}

uint8_t set_mm_allocated_node_type_tv(uint8_t *data, uint8_t type)
{
    mm_node_type_tv_t *allocated_node_type;

    allocated_node_type = (mm_node_type_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)allocated_node_type, TYPE_NODE_TYPE);
    allocated_node_type->type = type;
    return sizeof(mm_node_type_tv_t);
}

uint8_t set_mm_mcast_tv(uint8_t *data)
{
    mm_mcast_addr_tv_t *mcast;

    mcast = (mm_mcast_addr_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)mcast, TYPE_MCAST_ADDR);
    memcpy(mcast->mcast.m8, nd_get_subscribed_mcast(), 16);
    return sizeof(mm_mcast_addr_tv_t);
}

uint8_t set_mm_timestamp_tv(uint8_t *data, uint32_t time)
{
    mm_timestamp_tv_t *timestamp;

    timestamp = (mm_timestamp_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)timestamp, TYPE_TIMESTAMP);
    timestamp->timestamp = time;
    data += sizeof(mm_timestamp_tv_t);
    return sizeof(mm_timestamp_tv_t);
}

uint8_t set_mm_ssid_info_tv(network_context_t *network, uint8_t *data)
{
    mm_ssid_info_tv_t *ssid_info;
    uint16_t subnet_size = sid_allocator_get_num(network);

    if (g_mm_state.device.state == DEVICE_STATE_LEADER ||
        g_mm_state.device.state == DEVICE_STATE_SUPER_ROUTER) {
        nd_set_meshnetsize(network, subnet_size);
    }

    ssid_info = (mm_ssid_info_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)ssid_info, TYPE_SSID_INFO);
    ssid_info->child_num = subnet_size;
    ssid_info->free_slots = get_free_number(network->sid_base);
    return sizeof(mm_ssid_info_tv_t);
}

void update_mm_timestamp(uint8_t *tlvs, uint16_t tlvs_length)
{
    mm_timestamp_tv_t *timestamp;

    timestamp = (mm_timestamp_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length, TYPE_TIMESTAMP);
    if (timestamp) {
        umesh_set_timestamp(timestamp->timestamp);
    }
}
