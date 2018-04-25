/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <assert.h>
#include <string.h>

#include "umesh.h"
#include "umesh_hal.h"
#include "umesh_utils.h"
#include "core/mcast.h"
#ifdef CONFIG_AOS_MESH_AUTH
#include "core/auth_mgmt.h"
#endif
#include "core/link_mgmt.h"
#include "core/router_mgr.h"
#include "core/network_data.h"
#include "core/mesh_mgmt.h"
#include "core/mesh_forwarder.h"
#include "core/network_data.h"
#include "core/mesh_forwarder.h"
#include "core/crypto.h"
#include "core/address_mgmt.h"
#include "core/fragments.h"
#include "core/network_mgmt.h"
#ifdef CONFIG_NET_LWIP
#include "ip/compress6.h"
#include "ip/lwip_adapter.h"
#endif
#include "hal/interfaces.h"
#include "tools/cli.h"

typedef struct transmit_frame_s {
    struct pbuf *buf;
    ur_addr_t dest;
} transmit_frame_t;

typedef struct urmesh_state_s {
    mm_cb_t mm_cb;
    slist_t adapter_callback;
    bool initialized;
    bool started;
} urmesh_state_t;

static urmesh_state_t g_um_state = {.initialized = false , .started = false};

static ur_error_t umesh_interface_up(void)
{
    ur_adapter_callback_t *callback;

    slist_for_each_entry(&g_um_state.adapter_callback, callback,
                         ur_adapter_callback_t, next) {
        callback->interface_up();
    }

    umesh_post_event(CODE_MESH_CONNECTED, 0);
    MESH_LOG_DEBUG("mesh interface up");

    return UR_ERROR_NONE;
}

static ur_error_t umesh_interface_down(interface_state_t state)
{
    ur_adapter_callback_t *callback;

    slist_for_each_entry(&g_um_state.adapter_callback, callback,
                         ur_adapter_callback_t, next) {
        callback->interface_down();
    }

    umesh_post_event(CODE_MESH_DISCONNECTED, 0);
    MESH_LOG_DEBUG("mesh interface down");
    return UR_ERROR_NONE;
}

static void output_frame_handler(void *args)
{
    transmit_frame_t *frame = (transmit_frame_t *)args;
    struct pbuf *buf = frame->buf;
    message_t *message = NULL;
    uint8_t append_length;
    ur_error_t error = UR_ERROR_NONE;
    uint16_t ip_hdr_len = 0;
    uint16_t lowpan_hdr_len = 0;
    uint8_t *ip_payload = NULL;
    uint8_t *lowpan_payload = NULL;
    message_info_t *info;
    uint16_t frame_len;

    if (umesh_mm_get_device_state() < DEVICE_STATE_LEAF) {
        pbuf_free(buf);
        ur_mem_free(frame, sizeof(transmit_frame_t));
        return;
    }

    append_length = sizeof(mcast_header_t);
    frame_len = buf->tot_len + append_length;
#if LWIP_IPV6
    ip_payload = ur_mem_alloc((UR_IP6_HLEN + UR_UDP_HLEN) * 2);
    if (ip_payload == NULL) {
        error = UR_ERROR_MEM;
        goto out;
    }
    pbuf_copy_partial(buf, ip_payload, UR_IP6_HLEN + UR_UDP_HLEN, 0);
    lowpan_payload = ip_payload + UR_IP6_HLEN + UR_UDP_HLEN;
    error = lp_header_compress(ip_payload, lowpan_payload, &ip_hdr_len, &lowpan_hdr_len);
    if (error != UR_ERROR_NONE) {
        goto out;
    }
    frame_len = lowpan_hdr_len + append_length;
#endif

    message = message_alloc(frame_len, UMESH_1);
    if (message == NULL) {
        error = UR_ERROR_FAIL;
        goto out;
    }
    message_set_payload_offset(message, -append_length);
    if (lowpan_payload) {
        pbuf_header(buf, -ip_hdr_len);
        pbuf_chain(message->data, buf);
        message_copy_from(message, lowpan_payload, lowpan_hdr_len);
    } else {
        pbuf_copy(message->data, buf);
    }

    info = message->info;
    memcpy(&info->dest, &frame->dest, sizeof(frame->dest));
    info->type = MESH_FRAME_TYPE_DATA;
    info->flags = 0;

    error = address_resolve(message);
    if (error == UR_ERROR_NONE) {
        mf_send_message(message);
    } else if (error == UR_ERROR_DROP) {
        message_free(message);
    }

out:
    ur_mem_free(ip_payload, (UR_IP6_HLEN + UR_UDP_HLEN) * 2);
    ur_mem_free(frame, sizeof(*frame));
    pbuf_free(buf);
}

static ur_error_t tx_frame(struct pbuf *buf, ur_addr_t *dest)
{
    ur_error_t error = UR_ERROR_FAIL;
    transmit_frame_t *frame;

    if (umesh_mm_get_device_state() < DEVICE_STATE_LEAF) {
        return error;
    }
    frame = (transmit_frame_t *)ur_mem_alloc(sizeof(transmit_frame_t));
    if (frame) {
        memcpy(&frame->dest, dest, sizeof(ur_addr_t));
        pbuf_ref(buf);
        frame->buf = buf;
        error = umesh_task_schedule_call(output_frame_handler, frame);
        if (error != UR_ERROR_NONE) {
            pbuf_free(buf);
            ur_mem_free(frame, sizeof(transmit_frame_t));
        }
    }
    return error;
}

ur_error_t umesh_output_sid(struct pbuf *buf, uint16_t netid, uint16_t sid)
{
    ur_addr_t dest;

    dest.addr.len = SHORT_ADDR_SIZE;
    dest.addr.short_addr = sid;
    dest.netid = netid;
    return tx_frame(buf, &dest);
}

ur_error_t umesh_output_uuid(struct pbuf *buf, uint8_t *uuid)
{
    ur_addr_t dest;

    dest.addr.len = EXT_ADDR_SIZE;
    memcpy(dest.addr.addr, uuid, sizeof(dest.addr.addr));
    return tx_frame(buf, &dest);
}

ur_error_t umesh_input(message_t *message)
{
    ur_adapter_callback_t *callback;

#if LWIP_IPV6
    ur_error_t error = UR_ERROR_NONE;
    uint8_t *header;
    uint16_t header_size;
    uint16_t lowpan_header_size;
    message_info_t *info;
    message_t *message_header;

    header = ur_mem_alloc(UR_IP6_HLEN + UR_UDP_HLEN);
    if (header == NULL) {
        error = UR_ERROR_FAIL;
        goto exit;
    }
    message_copy_to(message, 0, header, UR_IP6_HLEN + UR_UDP_HLEN);
    info = message->info;

    header_size = message_get_msglen(message);
    error = lp_header_decompress(header, &header_size, &lowpan_header_size,
                                 &info->src, &info->dest);
    if (error != UR_ERROR_NONE) {
        goto handle_non_ipv6;
    }

    message_set_payload_offset(message, -lowpan_header_size);
    message_header = message_alloc(header_size, UMESH_2);
    if (message_header == NULL) {
        error = UR_ERROR_FAIL;
        goto exit;
    }

    message_copy_from(message_header, header, header_size);
    message_concatenate(message_header, message, false);
    message = message_header;

handle_non_ipv6:
    slist_for_each_entry(&g_um_state.adapter_callback, callback,
                         ur_adapter_callback_t, next) {
        callback->input(message->data);
    }

exit:
    message_free((message_t *)message);

    ur_mem_free(header, UR_IP6_HLEN + UR_UDP_HLEN);
    return error;
#else
    slist_for_each_entry(&g_um_state.adapter_callback, callback,
                         ur_adapter_callback_t, next) {
        callback->input(message->data);
    }
    message_free((message_t *)message);
    return UR_ERROR_NONE;
#endif
}

#ifdef CONFIG_AOS_DDA
#include <stdlib.h>
int csp_get_args(const char ***pargv);
static void parse_args(void)
{
    int i, argc;
    const char **argv;
    argc = csp_get_args(&argv);
    for (i = 0; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0) {
            g_cli_silent = 1;
            continue;
        }

        if (strcmp(argv[i], "--mesh-log") == 0) {
#ifdef CONFIG_AOS_MESH_DEBUG
            ur_log_set_level(str2lvl(argv[i + 1]));
#endif

            i += 1;
            continue;
        }

        if (strcmp(argv[i], "--mesh-mode") == 0) {
            int mode = atoi(argv[i + 1]);
            umesh_mm_set_mode(mode);

            i += 1;
            continue;
        }

        if (strcmp(argv[i], "--mesh-router") == 0) {
            int id = atoi(argv[i + 1]);
            ur_router_set_default_router(id);

            i += 1;
            continue;
        }
    }
}
#endif

ur_error_t umesh_init(node_mode_t mode)
{
    if (g_um_state.initialized) {
        return UR_ERROR_NONE;
    }

    umesh_pal_init();

    umesh_mem_init();
    hal_umesh_init();
    g_um_state.mm_cb.interface_up = umesh_interface_up;
    g_um_state.mm_cb.interface_down = umesh_interface_down;
#if (defined CONFIG_NET_LWIP) || (defined CONFIG_AOS_MESH_TAPIF)
    ur_adapter_interface_init();
#endif
    ur_router_register_module();
    interface_init();

    umesh_mm_init(mode, &g_um_state.mm_cb);
    link_mgmt_init();
    frags_init();
    address_mgmt_init();
    mf_init();
    nd_init();
    mcast_init();
#ifdef CONFIG_AOS_MESH_AUTH
    auth_init();
    auth_enable();
#endif
#ifdef CONFIG_AOS_MESH_LOWPOWER
    extern void lowpower_init(void);
    lowpower_init();
#endif
    umesh_message_init();
    umesh_network_mgmt_init();
    g_um_state.initialized = true;
    mesh_cli_init();

    umesh_pal_ready();

#ifdef CONFIG_AOS_DDA
    parse_args();
#endif
    return UR_ERROR_NONE;
}

bool umesh_is_initialized(void)
{
    return g_um_state.initialized;
}

ur_error_t umesh_start(void)
{
    umesh_hal_module_t *hal = NULL;
    umesh_extnetid_t extnetid;
    int extnetid_len = 6;

    if (g_um_state.started) {
        return UR_ERROR_NONE;
    }

    g_um_state.started = true;
    umesh_pal_radio_wakeup();
    interface_start();
    umesh_mm_start();

    if (umesh_kv_get("extnetid", extnetid.netid, &extnetid_len) == 0) {
        extnetid.len = extnetid_len;
        umesh_set_extnetid(&extnetid);
    }

    hal = hal_umesh_get_default_module();
    assert(hal);
    hal_umesh_enable(hal);

    umesh_post_event(CODE_MESH_STARTED, 0);
    return UR_ERROR_NONE;
}

ur_error_t umesh_stop(void)
{
    umesh_hal_module_t *hal = NULL;

    if (g_um_state.started == false) {
        return UR_ERROR_NONE;
    }

    g_um_state.started = false;
    umesh_mm_stop();
    interface_stop();

    hal = hal_umesh_get_default_module();
    assert(hal);
    hal_umesh_disable(hal);
    return UR_ERROR_NONE;
}

/* per device APIs */
uint8_t umesh_get_device_state(void)
{
    return (uint8_t)umesh_mm_get_device_state();
}

ur_error_t umesh_register_callback(ur_adapter_callback_t *callback)
{
    slist_add(&callback->next, &g_um_state.adapter_callback);
    return UR_ERROR_NONE;
}

uint8_t umesh_get_mode(void)
{
    return (uint8_t)umesh_mm_get_mode();
}

ur_error_t umesh_set_mode(uint8_t mode)
{
    ur_error_t error = UR_ERROR_FAIL;

    if (umesh_get_device_state() != DEVICE_STATE_DISABLED) {
        return error;
    }

    error = umesh_mm_set_mode(mode);
    if (error == UR_ERROR_NONE) {
       interface_init();
    }
    return error;
}

const mac_address_t *umesh_get_mac_address(media_type_t type)
{
    return umesh_mm_get_mac_address();
}

uint16_t umesh_get_meshnetid(void)
{
    return umesh_mm_get_meshnetid(NULL);
}

uint16_t umesh_get_meshnetsize(void)
{
    return umesh_mm_get_meshnetsize();
}

uint16_t umesh_get_sid(void)
{
    return umesh_mm_get_local_sid();
}

slist_t *umesh_get_nbrs(media_type_t type)
{
    hal_context_t *hal;
    slist_t *nbrs = NULL;

    hal = get_hal_context(type);
    if (hal) {
        nbrs = &hal->neighbors_list;
    }
    return nbrs;
}

bool umesh_is_mcast_subscribed(const ur_ip6_addr_t *addr)
{
    return nd_is_subscribed_mcast(addr);
}

void umesh_get_extnetid(umesh_extnetid_t *extnetid)
{
    if (extnetid == NULL) {
        return;
    }
    umesh_mm_get_extnetid(extnetid);
}

ur_error_t umesh_set_extnetid(const umesh_extnetid_t *extnetid)
{
    if (extnetid == NULL) {
        return UR_ERROR_FAIL;
    }

    return umesh_mm_set_extnetid(extnetid);
}
