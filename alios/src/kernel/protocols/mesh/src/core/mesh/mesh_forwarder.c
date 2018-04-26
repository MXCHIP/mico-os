/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "umesh.h"
#include "umesh_hal.h"
#include "umesh_utils.h"
#ifdef CONFIG_AOS_MESH_AUTH
#include "core/auth_mgmt.h"
#endif
#include "core/mesh_mgmt.h"
#include "core/mesh_forwarder.h"
#include "core/router_mgr.h"
#include "core/network_data.h"
#include "core/mcast.h"
#include "core/sid_allocator.h"
#include "core/keys_mgr.h"
#include "core/link_mgmt.h"
#include "core/crypto.h"
#include "core/address_mgmt.h"
#include "core/fragments.h"
#ifdef CONFIG_AOS_MESH_LOWPOWER
#include "core/lowpower_mgmt.h"
#endif
#include "hal/interface_context.h"
#include "hal/interfaces.h"
#include "tools/diags.h"

typedef struct received_frame_s {
    hal_context_t *hal;
    message_t *message;
    frame_info_t frame_info;
} received_frame_t;

enum {
    SENDING_TIMEOUT = 5000,
};

enum {
    SENT_SUCCESS = 0,
    SENT_FAIL    = -1,
    SENT_DROP    = -2,
};

typedef struct mesh_fwd_state_s {
    mm_cb_t interface_callback;
#ifdef CONFIG_AOS_MESH_LOWPOWER
    lowpower_events_handler_t lowpower_callback;
#endif
} mesh_fwd_state_t;
static mesh_fwd_state_t g_mf_state;

static void send_datagram(void *args);
static void handle_datagram(void *message);
static neighbor_t *get_next_node(message_info_t *info);

static inline bool is_mesh_header(uint8_t control)
{
    return (control & MESH_HEADER_DISPATCH_MASK) == MESH_HEADER_DISPATCH;
}

static inline bool is_fragment_header(uint8_t control)
{
    return (control & FRAG_HEADER_DISPATCH_MASK) == FRAG_HEADER_DISPATCH;
}

static inline bool is_mcast_header(uint8_t control)
{
    return (control & MCAST_HEADER_DISPATCH_MASK) == MCAST_HEADER_DISPATCH;
}

static inline bool is_local_ucast_address(message_info_t *info)
{
    mac_address_t *addr;
    bool matched = false;
    network_context_t *network;

    addr = &info->dest.addr;
    switch (addr->len) {
        case SHORT_ADDR_SIZE:
            network = get_network_context_by_meshnetid(info->dest.netid, false);
            if (network == NULL || is_unique_netid(info->dest.netid) == false) {
                return matched;
            }
            if (addr->short_addr == umesh_mm_get_local_sid()) {
                matched = true;
            }
            break;
        case EXT_ADDR_SIZE:
            if (memcmp(addr->addr, umesh_mm_get_local_uuid(), sizeof(addr->addr)) == 0) {
                matched = true;
            }
            break;
        default:
            matched = false;
    }

    if (matched && info->dest2.addr.len != 0) {
        matched = false;
        memcpy(&info->dest, &info->dest2, sizeof(info->dest));
        info->dest2.addr.len = 0;
    }
    return matched;
}

void message_sent_task(void *args)
{
    hal_context_t *hal = (hal_context_t *)args;
    message_t *message;
    uint16_t msg_length;
    bool free_msg = false;

    if (hal->last_sent == UR_ERROR_BUFFER) {
        hal->send_message = NULL;
        hal->frag_info.offset = 0;
        return;
    }

    ur_stop_timer(&hal->sending_timer, hal);
    if (hal->send_message == NULL) {
        return;
    }
    message = hal->send_message;
    msg_length = message_get_msglen(message);

    if (hal->frag_info.offset >= msg_length &&
        hal->last_sent == UR_ERROR_NONE) {
        free_msg = true;
    } else if (hal->last_sent == UR_ERROR_FAIL &&
               message->retries >= MESSAGE_RETRIES) {
        free_msg = true;
    } else if (hal->last_sent == UR_ERROR_DROP) {
        free_msg = true;
    }

    if (free_msg == false) {
        if (hal->last_sent == UR_ERROR_NONE) {
            message->frag_offset = hal->frag_info.offset;
            message->retries = 0;
        } else {
            hal->frag_info.offset = message->frag_offset;
            message->retries++;
        }
    }

    if (free_msg) {
        message_queue_dequeue(message);
        message_free(message);
        hal->send_message = NULL;
        hal->frag_info.offset = 0;
    }
    send_datagram(hal);
}

static ur_error_t mapping_error(int error)
{
    if (error == SENT_SUCCESS) {
        return UR_ERROR_NONE;
    } else if (error == SENT_DROP) {
        return UR_ERROR_DROP;
    } else {
        return UR_ERROR_FAIL;
    }
}

static void handle_sent(void *context, frame_t *frame, int error)
{
    hal_context_t *hal = (hal_context_t *)context;
    ur_error_t umesh_error;

    hal->link_stats.out_frames++;
    umesh_error = mapping_error(error);
    if (umesh_error != UR_ERROR_NONE) {
        hal->link_stats.out_errors++;
    }
    hal->last_sent = umesh_error;
    umesh_task_schedule_call(message_sent_task, hal);
}

static ur_error_t resolve_message_info(received_frame_t *frame,
                                       message_info_t *info, uint8_t *buf)
{
    mesh_header_control_t *control;
    uint8_t offset;
    mesh_short_addr_t *short_addr;
    mesh_ext_addr_t *ext_addr;
    mesh_subnetid_t *subnetid;
    mesh_netid_t *netid;
    uint8_t version;

    control = (mesh_header_control_t *)buf;
    version = control->control[2] & MESH_HEADER_VER_MASK;
    if (version != MESH_VERSION_1) {
        return UR_ERROR_FAIL;
    }

    info->hal_type = frame->hal->module->type;
    info->src_channel = frame->frame_info.channel;
    info->reverse_rssi = frame->frame_info.rssi;
    info->forward_rssi = 127;
    info->src_mac.netid = BCAST_NETID;
    memcpy(&info->src_mac.addr, &frame->frame_info.peer, sizeof(info->src_mac.addr));

    offset = sizeof(mesh_header_control_t);
    netid = (mesh_netid_t *)(buf + offset);
    info->src.netid = netid->netid;
    offset += sizeof(mesh_netid_t);

    info->type = (control->control[0] & MESH_FRAME_TYPE_MASK) >>
                 MESH_FRAME_TYPE_OFFSET;
    info->hops = (control->control[0] & MESH_HOPS_LEFT_MASK) >>
                 MESH_HOPS_LEFT_OFFSET;
    switch ((control->control[0] & MESH_HEADER_SRC_MASK) >>
            MESH_HEADER_SRC_OFFSET) {
        case SHORT_ADDR_MODE:
            short_addr = (mesh_short_addr_t *)(buf + offset);
            info->src.addr.len = SHORT_ADDR_SIZE;
            info->src.addr.short_addr = short_addr->addr;
            offset += SHORT_ADDR_SIZE;
            break;
        default:
            info->src.addr.len = 0;
            break;
    }

    switch ((control->control[1] & MESH_HEADER_DESTNETID_MASK) >>
            MESH_HEADER_DESTNETID_OFFSET) {
        case NO_DEST_NETID:
            info->dest.netid = netid->netid;
            break;
        case BCAST_DEST_NETID:
            info->dest.netid = BCAST_NETID;
            break;
        case SUB_DEST_NETID:
            subnetid = (mesh_subnetid_t *)(buf + offset);
            info->dest.netid = mk_sub_netid(netid->netid, subnetid->netid);
            offset += sizeof(mesh_subnetid_t);
            break;
        case DEST_NETID:
            netid = (mesh_netid_t *)(buf + offset);
            info->dest.netid = netid->netid;
            offset += sizeof(mesh_netid_t);
            break;
        default:
            break;
    }

    switch ((control->control[1] & MESH_HEADER_DEST_MASK) >>
            MESH_HEADER_DEST_OFFSET) {
        case NO_ADDR_MODE:
            info->dest.addr.len = 0;
            break;
        case SHORT_ADDR_MODE:
            short_addr = (mesh_short_addr_t *)(buf + offset);
            info->dest.addr.len = SHORT_ADDR_SIZE;
            info->dest.addr.short_addr = short_addr->addr;
            offset += sizeof(mesh_short_addr_t);
            break;
        case EXT_ADDR_MODE:
            ext_addr = (mesh_ext_addr_t *)(buf + offset);
            info->dest.addr.len = EXT_ADDR_SIZE;
            memcpy(info->dest.addr.addr, ext_addr->addr, sizeof(ext_addr->addr));
            offset += sizeof(mesh_ext_addr_t);
            break;
        case BCAST_ADDR_MODE:
            info->dest.addr.len = SHORT_ADDR_SIZE;
            info->dest.addr.short_addr = BCAST_SID;
            break;
        default:
            break;
    }

    switch ((control->control[1] & MESH_HEADER_DEST2_MASK) >>
            MESH_HEADER_DEST2_OFFSET) {
        case NO_ADDR_MODE:
            break;
        case SHORT_ADDR_MODE:
            short_addr = (mesh_short_addr_t *)(buf + offset);
            info->dest2.addr.len = SHORT_ADDR_SIZE;
            info->dest2.addr.short_addr = short_addr->addr;
            offset += sizeof(mesh_short_addr_t);
            break;
        case EXT_ADDR_MODE:
            ext_addr = (mesh_ext_addr_t *)(buf + offset);
            info->dest2.addr.len = EXT_ADDR_SIZE;
            memcpy(info->dest2.addr.addr, ext_addr->addr, sizeof(ext_addr->addr));
            offset += sizeof(mesh_ext_addr_t);
            break;
        case BCAST_ADDR_MODE:
            info->dest2.addr.len = SHORT_ADDR_SIZE;
            info->dest2.addr.short_addr = BCAST_SID;
            break;
        default:
            break;
    }

    if (control->control[1] & (ENABLE_SEC << MESH_HEADER_SEC_OFFSET)) {
        info->flags |= ENCRYPT_ENABLE_FLAG;
    }

    if (control->control[1] & (1 << MESH_HEADER_IES_OFFSET)) {
        info->flags |= HEADER_IES_FLAG;
    }

    info->header_ies_offset = offset;
    info->payload_offset = offset;
    return UR_ERROR_NONE;
}

static uint8_t insert_mesh_header(network_context_t *network,
                                  message_info_t *info)
{
    mesh_header_control_t *control;
    mesh_short_addr_t *short_addr;
    mesh_ext_addr_t *ext_addr;
    mesh_netid_t *netid;
    mesh_subnetid_t *subnetid;
    uint8_t length;
    hal_context_t *hal;
    uint8_t hops;

    hal = network->hal;
    control = (mesh_header_control_t *)hal->frame.data;
    control->control[0] = MESH_HEADER_DISPATCH;
    control->control[1] = 0;
    control->control[2] = 0;
    length = sizeof(mesh_header_control_t);
    if (info->type != MESH_FRAME_TYPE_DATA) {
        control->control[0] |= (MESH_FRAME_TYPE_CMD << MESH_FRAME_TYPE_OFFSET);
    }

    if (info->hops) {
        hops = info->hops;
    } else {
        hops = FORWARD_HOP_LIMIT;
    }
    control->control[0] = (control->control[0] & ~MESH_HOPS_LEFT_MASK) |
                          (hops << MESH_HOPS_LEFT_OFFSET);

    netid = (mesh_netid_t *)(hal->frame.data + length);
    netid->netid = umesh_mm_get_meshnetid(network);
    length += sizeof(mesh_netid_t);

    switch (info->src.addr.len) {
        case 0:
            control->control[0] |= (NO_ADDR_MODE << MESH_HEADER_SRC_OFFSET);
            break;
        case SHORT_ADDR_SIZE:
            control->control[0] |= (SHORT_ADDR_MODE << MESH_HEADER_SRC_OFFSET);
            short_addr = (mesh_short_addr_t *)(hal->frame.data + length);
            short_addr->addr = info->src.addr.short_addr;
            length += sizeof(mesh_short_addr_t);
            break;
        case EXT_ADDR_SIZE:
            control->control[0] |= (EXT_ADDR_MODE << MESH_HEADER_SRC_OFFSET);
            ext_addr = (mesh_ext_addr_t *)(hal->frame.data + length);
            memcpy(ext_addr->addr, info->src.addr.addr, sizeof(ext_addr->addr));
            length += sizeof(mesh_ext_addr_t);
            break;
        default:
            return 0;
    }

    if (info->dest.netid == BCAST_NETID) {
        control->control[1] |= (BCAST_DEST_NETID << MESH_HEADER_DESTNETID_OFFSET);
    } else if (info->dest.netid == netid->netid) {
        control->control[1] |= (NO_DEST_NETID << MESH_HEADER_DESTNETID_OFFSET);
    } else if (is_same_mainnet(info->dest.netid, netid->netid)) {
        control->control[1] |= (SUB_DEST_NETID << MESH_HEADER_DESTNETID_OFFSET);
        subnetid = (mesh_subnetid_t *)(hal->frame.data + length);
        subnetid->netid = get_sub_netid(info->dest.netid);
        length += sizeof(mesh_subnetid_t);
    } else {
        control->control[1] |= (DEST_NETID << MESH_HEADER_DESTNETID_OFFSET);
        netid = (mesh_netid_t *)(hal->frame.data + length);
        netid->netid = info->dest.netid;
        length += sizeof(mesh_netid_t);
    }

    if (info->dest.addr.len == SHORT_ADDR_SIZE) {
        if (info->dest.addr.short_addr == BCAST_SID) {
            control->control[1] |= (BCAST_ADDR_MODE << MESH_HEADER_DEST_OFFSET);
        } else {
            control->control[1] |= (SHORT_ADDR_MODE << MESH_HEADER_DEST_OFFSET);
            short_addr = (mesh_short_addr_t *)(hal->frame.data + length);
            short_addr->addr = info->dest.addr.short_addr;
            length += sizeof(mesh_short_addr_t);
        }
    } else {
        return 0;
    }

    switch (info->dest2.addr.len) {
        case 0:
            control->control[1] |= (NO_ADDR_MODE << MESH_HEADER_DEST2_OFFSET);
            break;
        case SHORT_ADDR_SIZE:
            control->control[1] |= (SHORT_ADDR_MODE << MESH_HEADER_DEST2_OFFSET);
            short_addr = (mesh_short_addr_t *)(hal->frame.data + length);
            short_addr->addr = info->dest2.addr.short_addr;
            length += sizeof(mesh_short_addr_t);
            break;
        case EXT_ADDR_SIZE:
            control->control[1] |= (EXT_ADDR_MODE << MESH_HEADER_DEST2_OFFSET);
            ext_addr = (mesh_ext_addr_t *)(hal->frame.data + length);
            memcpy(ext_addr->addr, info->dest2.addr.addr, sizeof(ext_addr->addr));
            length += sizeof(mesh_ext_addr_t);
            break;
        default:
            return 0;
    }

    if (info->flags & ENCRYPT_ENABLE_FLAG) {
        control->control[1] |= (ENABLE_SEC << MESH_HEADER_SEC_OFFSET);
    }

    control->control[2] |= MESH_VERSION_1;

    info->payload_offset = length;
    info->header_ies_offset = length;
    return length;
}

static void handle_sending_timer(void *args)
{
    hal_context_t *hal = (hal_context_t *)args;

    hal->sending_timer = NULL;
    if (hal->send_message == NULL) {
        return;
    }
    hal->last_sent = UR_ERROR_FAIL;
    message_sent_task(hal);
    hal->link_stats.sending_timeouts++;
}

static ur_error_t send_fragment(network_context_t *network, message_t *message)
{
    ur_error_t umesh_error = UR_ERROR_NONE;
    int error;
    uint16_t frag_length;
    uint16_t msg_length;
    uint8_t header_ies_length;
    frag_header_t frag_header;
    uint16_t mtu;
    message_info_t *info;
    uint8_t header_length = 0;
    uint16_t append_length = 0;
    hal_context_t *hal;
    neighbor_t *next_node = NULL;
    uint8_t *payload;
    const uint8_t *key;
    mac_address_t mac;
    int16_t hdr_ies_limit;

    hal = network->hal;
    info = ur_mem_alloc(sizeof(message_info_t));
    if (info == NULL) {
        return UR_ERROR_MEM;
    }
    memcpy(info, message->info, sizeof(message_info_t));

    if (info->dest.addr.short_addr == BCAST_SID) {
        mtu = hal_umesh_get_bcast_mtu(network->hal->module);
    } else {
        mtu = hal_umesh_get_ucast_mtu(network->hal->module);
    }
    bzero(network->hal->frame.data, mtu);

    if (info->dest.addr.len == EXT_ADDR_SIZE ||
        info->dest.addr.short_addr != BCAST_SID) {
        next_node = get_next_node(info);
        if (next_node == NULL) {
            send_address_unreachable(&info->src, &info->dest);
            umesh_error = UR_ERROR_DROP;
            goto exit;
        }
    }
    if (info->dest.addr.len == EXT_ADDR_SIZE) {
        set_mesh_short_addr(&info->dest, info->dest.netid, next_node->sid);
    }
#ifdef CONFIG_AOS_MESH_LOWPOWER
    if (next_node && (next_node->mode & MODE_RX_ON) == 0 && (next_node->flags & NBR_WAKEUP) == 0) {
        if (message->frag_offset) {
            umesh_error = UR_ERROR_DROP;
            goto exit;
        }
        message_queue_dequeue(message);
        message_queue_enqueue(&next_node->buffer_queue, message);
        umesh_error = UR_ERROR_BUFFER;
        goto exit;
    }
#endif

    if (info->flags & INSERT_MESH_HEADER) {
        header_length = insert_mesh_header(network, info);
        if (header_length == 0) {
            umesh_error = UR_ERROR_DROP;
            goto exit;
        }
    } else {
        header_length = info->header_ies_offset;
        message_copy_to(message, message->frag_offset, hal->frame.data, header_length);
        message_set_payload_offset(message, -info->payload_offset);
    }

    msg_length = message_get_msglen(message);
    hdr_ies_limit = mtu;
    if ((info->flags & INSERT_MESH_HEADER) == 0) {
        hdr_ies_limit = mtu - header_length - msg_length;
    }
    header_ies_length = insert_mesh_header_ies(network, info, hdr_ies_limit);
    header_length += header_ies_length;
    memset(&frag_header, 0, sizeof(frag_header));

    if (message->frag_offset == 0) {
        frag_length = msg_length;
        if (frag_length > (mtu - header_length)) {
            frag_header.dispatch = FRAG_1_DISPATCH;
            frag_header.size = msg_length;
            *((uint16_t *)&frag_header) = htons(*((uint16_t *)&frag_header));
            hal->frag_info.tag++;
            frag_header.tag = htons(hal->frag_info.tag);
            frag_length = (mtu + 1 - sizeof(frag_header_t) - header_length) & 0xff8;
            append_length = sizeof(frag_header) - 1;
        }
    } else {
        frag_header.dispatch = FRAG_N_DISPATCH;
        frag_header.size = msg_length;
        *((uint16_t *)&frag_header) = htons(*((uint16_t *)&frag_header));
        frag_header.offset = (uint8_t)(message->frag_offset >> 3);
        frag_header.tag = htons(hal->frag_info.tag);
        frag_length = (mtu - sizeof(frag_header_t) - header_length) & 0xff8;
        if (frag_length > (msg_length - message->frag_offset)) {
            frag_length = msg_length - message->frag_offset;
        }
        append_length = sizeof(frag_header);
    }

    if (append_length > 0) {
        memcpy(hal->frame.data + header_length, (uint8_t *)&frag_header, append_length);
    }
    payload = hal->frame.data + header_length + append_length;

    message_copy_to(message, message->frag_offset, payload, frag_length);

    if (info->key_index == ONE_TIME_KEY_INDEX) {
        if (next_node == NULL) {
            umesh_error = UR_ERROR_DROP;
            goto exit;
        }
        key = next_node->one_time_key;
    } else if (info->key_index != INVALID_KEY_INDEX) {
        key = get_key(GROUP_KEY1_INDEX);
    }

    if ((info->flags & ENCRYPT_ENABLE_FLAG) &&
        umesh_aes_encrypt(key, KEY_SIZE,
                          hal->frame.data + info->header_ies_offset,
                          header_ies_length + append_length + frag_length,
                          hal->frame.data + info->header_ies_offset) != UR_ERROR_NONE) {
        umesh_error = UR_ERROR_DROP;
        goto exit;
    }

    hal->frame.len = header_length + append_length + frag_length;
    hal->frame.key_index = info->key_index;

    if (info->type == MESH_FRAME_TYPE_DATA) {
        hal->link_stats.out_data++;
    } else {
        hal->link_stats.out_command++;
    }

    hal->frag_info.offset += frag_length;
    if (next_node) {
        mac.len = EXT_ADDR_SIZE;
        memcpy(mac.addr, next_node->mac, EXT_ADDR_SIZE);
        error = hal_umesh_send_ucast_request(hal->module, &hal->frame,
                                             &mac, handle_sent, hal);
    } else {
        error = hal_umesh_send_bcast_request(network->hal->module, &hal->frame,
                                             handle_sent, hal);
    }

    umesh_error = mapping_error(error);
 
exit:
    ur_mem_free(info, sizeof(message_info_t));
    return umesh_error;
}

static neighbor_t *get_next_node(message_info_t *info)
{
    uint16_t local_sid;
    uint16_t next_hop;
    neighbor_t *next = NULL;
    bool same_subnet = false;
    bool same_net = true;
    network_context_t *network = info->network;

    local_sid = umesh_mm_get_local_sid();
    if (info->dest.addr.len == EXT_ADDR_SIZE) {
        next = get_neighbor_by_mac_addr(info->dest.addr.addr, NULL);
        return next;
    }

    if (info->dest.addr.len == SHORT_ADDR_SIZE &&
        is_partial_function_sid(info->dest.addr.short_addr)) {
        next = get_neighbor_by_sid(info->dest.netid, info->dest.addr.short_addr, NULL);
        return next;
    }

#ifdef CONFIG_AOS_MESH_AUTH
    if (get_auth_state() < AUTH_DONE) {
        next = get_auth_candidate();
        return next;
    }
#endif

    if (local_sid == BCAST_SID) {
        next = umesh_mm_get_attach_candidate();
        return next;
    }

    if (is_partial_function_sid(local_sid)) {
        next = umesh_mm_get_attach_node();
        return next;
    }

    if (umesh_mm_get_meshnetid(network) == info->dest.netid) {
        same_subnet = true;
    } else if (is_same_mainnet(info->dest.netid,
                               umesh_mm_get_meshnetid(network)) == false) {
        same_net = false;
    }

    if (same_net == true) {
        if (same_subnet == true) {
            next_hop = ur_router_get_next_hop(network, info->dest.addr.short_addr);
            if (next_hop == LEADER_SID) {
                next_hop = get_leader_sid(info->dest.netid);
            }
            next = get_neighbor_by_sid(info->dest.netid, next_hop, NULL);
        } else if (is_subnet(network->meshnetid)) {
            next = umesh_mm_get_attach_node();
        } else {
            next_hop = ur_router_get_next_hop(network, get_leader_sid(info->dest.netid));
            next = get_neighbor_by_sid(get_main_netid(info->dest.netid), next_hop, NULL);
        }
    } else {
        next = umesh_mm_get_attach_candidate();
    }

    return next;
}

static void get_tx_network_context(message_info_t *info)
{
#ifdef CONFIG_AOS_MESH_SUPER
    network_context_t *network = info->network;
    neighbor_t *nbr = NULL;
    hal_context_t *hal = NULL;

    if (network == NULL) {
        network = get_network_context_by_meshnetid(info->dest.netid, false);
    }
    if (network == NULL) {
        if (info->dest.addr.len == EXT_ADDR_SIZE) {
            nbr = get_neighbor_by_mac_addr(info->dest.addr.addr, &hal);
        } else if (info->dest.addr.len == SHORT_ADDR_SIZE) {
            nbr = get_neighbor_by_sid(info->dest.netid, info->dest.addr.short_addr, &hal);
        }
        if (nbr) {
            network = get_hal_default_network_context(hal);
        } else {
            network = get_default_network_context();
        }
    }
    info->network = network;
#else
    info->network = get_default_network_context();
#endif
}

static void set_src_info(message_info_t *info)
{
    get_tx_network_context(info);
    set_mesh_short_addr(&info->src, umesh_mm_get_meshnetid(info->network), umesh_mm_get_local_sid());
}

static bool get_rx_network_context(message_info_t *info, hal_context_t *hal)
{
#ifdef CONFIG_AOS_MESH_SUPER
    int8_t cmp_mode;
    bool is_diff_level = false;
    uint16_t meshnetid;

    if (is_unique_netid(info->dest.netid)) {
        info->network = get_network_context_by_meshnetid(info->dest.netid, false);
        if (info->network == NULL) {
            info->network = (is_subnet(info->dest.netid)? get_sub_network_context(hal): \
                                                          get_hal_default_network_context(hal));
        }
    } else if (is_unique_netid(info->src.netid)) {
        info->network = get_network_context_by_meshnetid(info->src.netid, false);
    }

    cmp_mode = umesh_mm_compare_mode(umesh_mm_get_mode() & (~MODE_LEADER),
                                     info->mode & (~MODE_LEADER));
    if (info->network == NULL) {
        info->network = ((cmp_mode <= 0)? get_hal_default_network_context(hal): \
                                          get_sub_network_context(hal));
    }
    meshnetid = umesh_mm_get_meshnetid(info->network);
    if (is_unique_netid(meshnetid)) {
        uint8_t local_subnet_type = is_subnet(meshnetid) ? 1 : 0;
        uint8_t subnet_type = is_subnet(info->src.netid) ? 1 : 0;
        if ((local_subnet_type ^ subnet_type) ||
            (cmp_mode == -1 && local_subnet_type == 0 && subnet_type == 0)) {
            is_diff_level = true;
        }
        if (cmp_mode < 0 && subnet_type) {
            is_diff_level = false;
        }
    } else if (cmp_mode == 1) {
        is_diff_level = true;
    }

    if (is_unique_netid(info->src.netid) && !is_unique_netid(info->dest.netid) && is_diff_level) {
        return false;
    }
#else
    info->network = get_default_network_context();
#endif
    return true;
}


static void set_dest_encrypt_flag(message_info_t *info)
{
    if (umesh_mm_get_seclevel() < SEC_LEVEL_1) {
        return;
    }

    if (info->type == MESH_FRAME_TYPE_DATA) {
        info->flags |= ENCRYPT_ENABLE_FLAG;
        info->key_index = GROUP_KEY1_INDEX;
        return;
    }

    if (info->type == MESH_FRAME_TYPE_CMD) {
        if (info->command == COMMAND_ADVERTISEMENT ||
            info->command == COMMAND_DISCOVERY_REQUEST ||
            info->command == COMMAND_DISCOVERY_RESPONSE ||
            info->command == COMMAND_ATTACH_REQUEST ||
#ifdef CONFIG_AOS_MESH_AUTH
            info->command == COMMAND_AUTH_DOT1X ||
#endif
            info->command == COMMAND_LINK_ACCEPT) {
            info->key_index = INVALID_KEY_INDEX;
            return;
        }
        info->flags |= ENCRYPT_ENABLE_FLAG;
        if (info->command == COMMAND_ATTACH_RESPONSE) {
            info->key_index = ONE_TIME_KEY_INDEX;
        } else {
            info->key_index = GROUP_KEY1_INDEX;
        }
    }
}

ur_error_t mf_send_message(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    message_info_t *info;
    slist_t *hals;
    hal_context_t *hal;
    hal_context_t *tx_hal;
    message_t *mcast_message;
    uint8_t append_length;
    network_context_t *network;

    info = message->info;
    if (is_local_ucast_address(info)) {
        info->network = get_default_network_context();
        set_src_info(info);
        error = umesh_task_schedule_call(handle_datagram, message);
        if (error != UR_ERROR_NONE) {
            message_free(message);
        }
        return error;
    }

    info->flags |= INSERT_MESH_HEADER;
    set_dest_encrypt_flag(info);

    set_src_info(info);
    network = info->network;
    tx_hal = network->hal;
    info->hal_type = tx_hal->module->type;
    if (info->type == MESH_FRAME_TYPE_DATA) {
        message_queue_enqueue(&tx_hal->send_queue[DATA_QUEUE], message);
        umesh_task_schedule_call(send_datagram, tx_hal);
        if (info->flags & INSERT_MCAST_FLAG) {
            hals = get_hal_contexts();
            slist_for_each_entry(hals, hal, hal_context_t, next) {
                if (hal == tx_hal) {
                    continue;
                }
                append_length = sizeof(mcast_header_t);
                mcast_message = message_alloc(message_get_msglen(message) + append_length,
                                              MESH_FORWARDER_1);
                if (mcast_message == NULL) {
                    break;
                }
                message_set_payload_offset(mcast_message, -append_length);
                message_copy(mcast_message, message);
                mcast_message->info->network = (void *)get_hal_default_network_context(hal);
                message_queue_enqueue(&hal->send_queue[DATA_QUEUE], mcast_message);
                umesh_task_schedule_call(send_datagram, hal);
            }
        }
    } else {
        message_queue_enqueue(&tx_hal->send_queue[CMD_QUEUE], message);
        umesh_task_schedule_call(send_datagram, tx_hal);
    }
    return error;
}

message_t *mf_build_message(uint8_t type, uint8_t cmd_type, uint8_t *data,
                            uint16_t len, uint8_t debug)
{
    message_t *message;
    message_info_t *info;
    mm_header_t *mm_header;

    message = message_alloc(len, debug);
    if (message == NULL) {
        return NULL;
    }

    info = message->info;
    if (type == MESH_FRAME_TYPE_CMD) {
        mm_header = (mm_header_t *)data;
        mm_header->command = cmd_type;
        info->type = MESH_FRAME_TYPE_CMD;
        info->command = cmd_type;
    }

    message_copy_from(message, data, len);
    return message;
}

static bool proxy_check(message_t *message)
{
    network_context_t *network;
    message_info_t *info;

    info = message->info;
    network = get_default_network_context();

    if (is_bcast_sid(&info->src) ||
        is_same_mainnet(info->src.netid, umesh_mm_get_meshnetid(network)) == false ||
        (umesh_mm_get_seclevel() > SEC_LEVEL_0 &&
         (info->flags & ENCRYPT_ENABLE_FLAG) == 0)) {
        if (info->type == MESH_FRAME_TYPE_DATA) {
            return false;
        }

        network = get_network_context_by_meshnetid(info->dest.netid, false);
        if (is_unique_netid(info->dest.netid) &&
            (network == NULL || info->dest.addr.short_addr != umesh_mm_get_local_sid())) {
            return false;
        }

        if (is_unique_netid(info->dest.netid) &&
            (info->dest2.addr.len != SHORT_ADDR_SIZE ||
             is_bcast_sid(&info->dest2)) == false) {
            return false;
        }

        if (info->dest2.addr.len != SHORT_ADDR_SIZE ||
            info->dest.addr.short_addr == LEADER_SID) {
            info->dest2.addr.len = 0;
        } else {
            info->dest2.addr.short_addr = LEADER_SID;
            info->dest2.netid = get_main_netid(umesh_mm_get_meshnetid(network));
        }
    }

    return true;
}

static void message_handler(void *args)
{
    ur_error_t error = UR_ERROR_NONE;
    uint8_t *nexth;
    message_t *message;
    message_t *assemble_message;
    network_context_t *network = NULL;
    hal_context_t *hal = NULL;
    message_info_t *info;
    bool recv = false;
    bool forward = false;

    if (umesh_mm_get_device_state() == DEVICE_STATE_DISABLED) {
        return;
    }
    hal = (hal_context_t *)args;
    message = message_queue_get_head(&hal->recv_queue);
    if (message == NULL) {
        return;
    }
    message_queue_dequeue(message);

    info = message->info;
    network = get_default_network_context();

    if (proxy_check(message) == false) {
        message_free(message);
        hal->link_stats.in_drops++;
        goto exit;
    }

    if (memcmp(&info->dest.addr, umesh_mm_get_mac_address(), sizeof(info->dest.addr)) == 0) {
        recv = true;
    } else if (info->dest.addr.len == SHORT_ADDR_SIZE) {
        if (is_unique_netid(info->dest.netid) &&
            is_same_mainnet(info->dest.netid, mm_get_main_netid(network))) {
            network = get_network_context_by_meshnetid(info->dest.netid, false);
            if (info->dest.addr.short_addr == BCAST_SID ||
                (network && info->dest.addr.short_addr == umesh_mm_get_local_sid())) {
                recv = true;
            } else {
                forward = true;
            }
        } else if (info->type == MESH_FRAME_TYPE_CMD && info->dest.netid == BCAST_NETID) {
            recv = true;
        }
    }

    if (recv != true && forward != true) {
        hal->link_stats.in_filterings++;
        message_free(message);
        goto exit;
    }

    if (info->flags & HEADER_IES_FLAG) {
        error = handle_mesh_header_ies(message);
        if (error != UR_ERROR_NONE) {
            message_free(message);
            goto exit;
        }
    }

    if (recv && info->dest2.addr.len != 0) {
        memcpy(&info->dest.addr, &info->dest2.addr, sizeof(info->dest.addr));
        info->dest2.addr.len = 0;
        message_set_payload_offset(message, -info->payload_offset);
        info->flags |= INSERT_MESH_HEADER;
        forward = true;
    }

    if (get_rx_network_context(info, hal) == false) {
        message_free(message);
        goto exit;
    }

#ifdef CONFIG_AOS_MESH_DEBUG
    if (info->type == MESH_FRAME_TYPE_CMD) {
        handle_diags_command(message, recv);
    }
#endif

    if (forward == true) {
        info->network = NULL;
        get_tx_network_context(info);
        if (info->type != MESH_FRAME_TYPE_DATA) {
            set_src_info(info);
        }
        network = info->network;
        hal = network->hal;

        if (info->type == MESH_FRAME_TYPE_DATA) {
            message_queue_enqueue(&hal->send_queue[DATA_QUEUE], message);
        } else {
            message_queue_enqueue(&hal->send_queue[CMD_QUEUE], message);
        }
        umesh_task_schedule_call(send_datagram, hal);
        goto exit;
    }

    if (info->type == MESH_FRAME_TYPE_DATA) {
        hal->link_stats.in_data++;
    } else {
        hal->link_stats.in_command++;
    }

    message_set_payload_offset(message, -info->payload_offset);
    nexth = message_get_payload(message);
    if (is_fragment_header(*nexth)) {
        error = frags_reassemble(message, &assemble_message);
        if (error == UR_ERROR_NONE && assemble_message) {
            message = assemble_message;
        } else {
            if (error != UR_ERROR_NONE) {
                message_free(message);
            }
            goto exit;
        }
    }

    error = umesh_task_schedule_call(handle_datagram, message);
    if (error != UR_ERROR_NONE) {
        message_free(message);
    }

exit:
    message = message_queue_get_head(&hal->recv_queue);
    if (message) {
        umesh_task_schedule_call(message_handler, hal);
    }
}

static void enqueue_msg(void *arg)
{
    received_frame_t *rx_frame = (received_frame_t *)arg;
    hal_context_t *hal = rx_frame->hal;

    message_queue_enqueue(&hal->recv_queue, rx_frame->message);
    message_handler(hal);

    ur_mem_free(rx_frame, sizeof(received_frame_t));
}

static void handle_received_frame(void *context, frame_t *frame,
                                  frame_info_t *frame_info,
                                  int error)
{
    message_t *message;
    received_frame_t *rx_frame = NULL;
    hal_context_t *hal = (hal_context_t *)context;
    message_info_t info;
    ur_error_t umesh_error = UR_ERROR_NONE;
    const uint8_t *key;

    hal->link_stats.in_frames++;
    if (umesh_mm_get_device_state() == DEVICE_STATE_DISABLED) {
        hal->link_stats.in_drops++;
        return;
    }

    if (error != 0 || is_mesh_header(frame->data[0]) == false) {
        hal->link_stats.in_filterings++;
        return;
    }

    if (whitelist_is_enabled()) {
        whitelist_entry_t *entry;
        entry = whitelist_find(&frame_info->peer);
        if (entry == NULL) {
            hal->link_stats.in_filterings++;
            return;
        }
    }

    rx_frame = (received_frame_t *)ur_mem_alloc(sizeof(received_frame_t));
    if (rx_frame == NULL) {
        hal->link_stats.in_drops++;
        return;
    }

    rx_frame->hal = hal;
    memcpy(&rx_frame->frame_info, frame_info, sizeof(rx_frame->frame_info));
    bzero(&info, sizeof(info));
    umesh_error = resolve_message_info(rx_frame, &info, frame->data);
    if (umesh_error == UR_ERROR_NONE && (info.flags & ENCRYPT_ENABLE_FLAG)) {
        info.key_index =
          (umesh_mm_get_attach_state() == ATTACH_REQUEST? ONE_TIME_KEY_INDEX: GROUP_KEY1_INDEX);
        key = get_key(info.key_index);
        umesh_error = umesh_aes_decrypt(key, KEY_SIZE, frame->data + info.header_ies_offset,
                                   frame->len - info.header_ies_offset,
                                   frame->data + info.header_ies_offset);
    }

    if (umesh_error != UR_ERROR_NONE) {
        ur_mem_free(rx_frame, sizeof(received_frame_t));
        hal->link_stats.in_drops++;
        return;
    }

#ifdef CONFIG_AOS_MESH_AUTH
    // drop specific msg if auth doesn't finish
    if (info.type == MESH_FRAME_TYPE_CMD) {
        if ((umesh_mm_get_device_state() != DEVICE_STATE_LEADER) &&
            !get_auth_result() &&
            (info.command == COMMAND_ATTACH_REQUEST ||
            info.command == COMMAND_ATTACH_RESPONSE ||
            info.command == COMMAND_SID_REQUEST ||
            info.command == COMMAND_SID_RESPONSE ||
            info.command == COMMAND_ADDRESS_QUERY ||
            info.command == COMMAND_ADDRESS_QUERY_RESPONSE ||
            info.command == COMMAND_ADDRESS_NOTIFICATION ||
            info.command == COMMAND_LINK_REQUEST ||
            info.command == COMMAND_LINK_ACCEPT ||
            info.command == COMMAND_LINK_ACCEPT_AND_REQUEST ||
            info.command == COMMAND_ADDRESS_UNREACHABLE ||
            info.command == COMMAND_ADDRESS_ERROR ||
            info.command == COMMAND_ROUTING_INFO_UPDATE)) {
            MESH_LOG_INFO("drop msg %d due to incompleted auth", info.command);
            hal->link_stats.in_drops++;
            return;
        }
    }
#endif

    message = message_alloc(frame->len, MESH_FORWARDER_2);
    if (message == NULL) {
        ur_mem_free(rx_frame, sizeof(received_frame_t));
        hal->link_stats.in_drops++;
        return;
    }

    memcpy(message->info, &info, sizeof(info));
    message_copy_from(message, frame->data, frame->len);
    rx_frame->message = message;
    if (umesh_task_schedule_call(enqueue_msg, rx_frame) != UR_ERROR_NONE) {
        hal->link_stats.in_drops++;
        message_free(message);
        ur_mem_free(rx_frame, sizeof(received_frame_t));
    }
}

static void send_datagram(void *args)
{
    ur_error_t error = UR_ERROR_NONE;
    hal_context_t *hal;
    message_t *message = NULL;
    uint8_t *lowpan_payload;
    message_info_t *info;
    int16_t offset = 0;

    hal = (hal_context_t *)args;
    message = hal->send_message;
    if (message && hal->frag_info.offset > message->frag_offset) {
        return;
    }
    if (message == NULL) {
        message = message_queue_get_head(&hal->send_queue[CMD_QUEUE]);
        if (message == NULL) {
            message = message_queue_get_head(&hal->send_queue[DATA_QUEUE]);
            if (message == NULL) {
                return;
            }
        }
        hal->send_message = message;
    }
    info = message->info;

    if (info->flags & INSERT_MCAST_FLAG) {
        offset = sizeof(mcast_header_t);
        message_set_payload_offset(message, offset);
        lowpan_payload = message_get_payload(message);
        insert_mcast_header(lowpan_payload);
        info->flags &= (~INSERT_MCAST_FLAG);
    }

    error = send_fragment(info->network, message);
    if (error == UR_ERROR_NONE) {
        ur_start_timer(&hal->sending_timer, SENDING_TIMEOUT, handle_sending_timer, hal);
    } else {
        hal->last_sent = error;
        umesh_task_schedule_call(message_sent_task, hal);
    }
}

extern ur_error_t umesh_input(message_t *message);
static void handle_datagram(void *args)
{
    ur_error_t error = UR_ERROR_NONE;
    int16_t offset;
    message_info_t *info;
    message_t *message;
    message_t *relay_message;
    uint8_t *nexth;
    slist_t *hals;
    hal_context_t *hal;
    uint8_t *payload;

    message = (message_t *)args;
    info = message->info;

    update_neighbor(info, NULL, 0, false);

    if (info->type == MESH_FRAME_TYPE_DATA) {
        nexth = message_get_payload(message);
        if (is_mcast_header(*nexth)) {
            payload = ur_mem_alloc(sizeof(mcast_header_t));
            if (payload == NULL) {
                message_free(message);
                return;
            }
            message_copy_to(message, 0, payload, sizeof(mcast_header_t));
            error = process_mcast_header(payload);
            ur_mem_free(payload, sizeof(mcast_header_t));
            if (error != UR_ERROR_NONE) {
                message_free(message);
                return;
            }
            if (info->flags & ENCRYPT_ENABLE_FLAG) {
                info->flags = ENCRYPT_ENABLE_FLAG;
            }
            info->flags |= INSERT_MESH_HEADER;
            hals = get_hal_contexts();
            slist_for_each_entry(hals, hal, hal_context_t, next) {
                relay_message = message_alloc(message_get_msglen(message), MESH_FORWARDER_3);
                if (relay_message != NULL) {
                    message_copy(relay_message, message);
                    relay_message->info->network = (void *)get_hal_default_network_context(hal);
                    message_queue_enqueue(&hal->send_queue[DATA_QUEUE], relay_message);
                    umesh_task_schedule_call(send_datagram, hal);
                }
            }
            offset = sizeof(mcast_header_t);
            message_set_payload_offset(message, -offset);
        }
        umesh_input(message);
    } else {
        umesh_mm_handle_frame_received(message);
        message_free(message);
    }
}

const ur_link_stats_t *mf_get_stats(hal_context_t *hal)
{
    uint8_t index;

    if (hal == NULL) {
        return NULL;
    }

    hal->link_stats.sending = hal->send_message ? true : false;
    hal->link_stats.send_queue_size = 0;
    for (index = CMD_QUEUE; index <= PENDING_QUEUE; index++) {
        hal->link_stats.send_queue_size += message_queue_get_size(&hal->send_queue[index]);
    }
    hal->link_stats.recv_queue_size = message_queue_get_size(&hal->recv_queue);
    return &hal->link_stats;
}

static void cleanup_sending_message(void)
{
    slist_t *hals;
    hal_context_t *hal;

    hals = get_hal_contexts();
    slist_for_each_entry(hals, hal, hal_context_t, next) {
        ur_stop_timer(&hal->sending_timer, hal);
        if (hal->send_message) {
            message_queue_dequeue(hal->send_message);
            message_free(hal->send_message);
            hal->send_message = NULL;
        }
        hal->frag_info.tag = 0;
        hal->frag_info.offset = 0;
    }
}

static ur_error_t mesh_interface_up(void)
{
    MESH_LOG_DEBUG("mesh forwarder mesh interface up handler");
    return UR_ERROR_NONE;
}

static ur_error_t mesh_interface_down(interface_state_t state)
{
    MESH_LOG_DEBUG("mesh forwarder mesh interface down handler, reason %d", state);
    cleanup_sending_message();
    return UR_ERROR_NONE;
}

#ifdef CONFIG_AOS_MESH_LOWPOWER
static void lowpower_radio_down_handler(schedule_type_t type)
{
    if (umesh_get_mode() & MODE_RX_ON) {
        return;
    }
    cleanup_sending_message();
}

static void lowpower_radio_up_handler(schedule_type_t type)
{
}
#endif

ur_error_t mf_init(void)
{
    slist_t *hals;
    hal_context_t *hal;

    hals = get_hal_contexts();
    slist_for_each_entry(hals, hal, hal_context_t, next) {
        hal_umesh_register_receiver(hal->module, handle_received_frame, hal);
    }

#ifdef CONFIG_AOS_MESH_LOWPOWER
    g_mf_state.lowpower_callback.radio_down = lowpower_radio_down_handler;
    g_mf_state.lowpower_callback.radio_up = lowpower_radio_up_handler;
    lowpower_register_callback(&g_mf_state.lowpower_callback);
#endif
    g_mf_state.interface_callback.interface_up = mesh_interface_up;
    g_mf_state.interface_callback.interface_down = mesh_interface_down;
    umesh_mm_register_callback(&g_mf_state.interface_callback);
    return UR_ERROR_NONE;
}
