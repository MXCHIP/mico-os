/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "lwip/opt.h"
#include "lwip/netif.h"
#include "lwip/tcpip.h"

#include "umesh.h"
#include "umesh_hal.h"
#include "umesh_utils.h"
#include "ip/lwip_adapter.h"
#include "core/mesh_mgmt.h"

#ifdef CONFIG_AOS_MESH_TAPIF
static bool is_router;
int umesh_tapif_init(const char *ifname);
void umesh_tapif_deinit(void);
void umesh_tapif_send_pbuf(struct pbuf *pbuf);
#endif

#if LWIP_IPV6 == 0 && LWIP_IPV4 && LWIP_IGMP
#include "lwip/igmp.h"
static struct igmp_group g_group;
#endif

typedef struct lwip_adapter_state_s {
    struct netif adpif;
    ur_adapter_callback_t adapter_cb;
    const char interface_name[3];
} lwip_adapter_state_t;

static lwip_adapter_state_t g_la_state = {.interface_name = "ur"};

static void adapter_msg_input(void *arg)
{
    umesh_post_event(CODE_MESH_DATA_RECV, 0);
    g_la_state.adpif.input(arg, &g_la_state.adpif);
}

/* Receive IP frame from umesh and pass up to LwIP */
ur_error_t ur_adapter_input(struct pbuf *buf)
{
    ur_error_t error = UR_ERROR_NONE;

    if (g_la_state.adpif.input) {
        pbuf_ref(buf);
        error = umesh_task_schedule_call(adapter_msg_input, buf);
    }
    if (error != UR_ERROR_NONE) {
        pbuf_free(buf);
    }
    return error;
}

void ur_adapter_input_buf(void *buf, int len)
{
    struct pbuf *pbuf = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!pbuf) {
        return;
    }

    pbuf_take(pbuf, buf, len);
    ur_adapter_input(pbuf);
}

#if LWIP_IPV6
static inline bool is_sid_address(const uint8_t *addr)
{
    uint8_t index = 0;

    while (index < 5) {
        if (addr[index] != 0) {
            return false;
        }
        index++;
    }
    return true;
}
#endif

ur_error_t ur_adapter_resolve_ip(const void *ipaddr, ur_addr_t *addr)
{
#if LWIP_IPV6
    ur_ip6_addr_t *ip6_addr = (ur_ip6_addr_t *)ipaddr;

    if (is_sid_address(&ip6_addr->m8[8]) == false) {
        addr->addr.len = EXT_ADDR_SIZE;
        memcpy(addr->addr.addr, &ip6_addr->m8[8], sizeof(addr->addr.addr));
    } else {
        addr->addr.len = SHORT_ADDR_SIZE;
        addr->netid = ntohs(ip6_addr->m16[3]) | ntohs(ip6_addr->m16[6]);
        addr->addr.short_addr =  ntohs(ip6_addr->m16[7]);
    }
#else
    ip4_addr_t *ip4_addr = (ip4_addr_t *)ipaddr;
    struct netif *netif = &g_la_state.adpif;

    addr->addr.len = SHORT_ADDR_SIZE;
    addr->netid = umesh_get_meshnetid();
    if (ip4_addr_ismulticast(ip4_addr)) {
        addr->addr.short_addr = BCAST_SID;
    } else if (!ip4_addr_netcmp(ip4_addr, netif_ip4_addr(netif), netif_ip4_netmask(netif)) ||
               ip4_addr_cmp(ip4_addr, netif_ip4_gw(netif))) {
#ifdef CONFIG_AOS_MESH_TAPIF
        if (is_router) {
            MESH_LOG_DEBUG("should go to gateway\n");
            return UR_ERROR_FAIL;
        }
#endif
        addr->addr.short_addr = 0;
    } else {
        addr->addr.short_addr = ntohs(((ip4_addr->addr) >> 16)) - 2;
    }
#endif
    return UR_ERROR_NONE;
}

static err_t err_mapping(ur_error_t error) {
    switch (error) {
        case UR_ERROR_NONE:
            return ERR_OK;
        case UR_ERROR_FAIL:
            return ERR_VAL;
        default:
            return ERR_VAL;
    }
    return ERR_OK;
}

static err_t ur_adapter_ipv4_output(struct netif *netif, struct pbuf *p,
                                    const ip4_addr_t *dest)
{
    ur_error_t error;
    ur_addr_t addr;

    error = ur_adapter_resolve_ip(dest, &addr);

    if (error == UR_ERROR_NONE) {
        error = umesh_output_sid(p, addr.netid, addr.addr.short_addr);
    }
#ifdef CONFIG_AOS_MESH_TAPIF
    else if (is_router) {
        umesh_tapif_send_pbuf(p);
    }
#endif

    return err_mapping(error);
}

#if LWIP_IPV6
static err_t ur_adapter_ipv6_output(struct netif *netif, struct pbuf *p,
                                    const ip6_addr_t *dest)
{
    ur_error_t error;
    ur_addr_t addr;

    if (ip6_addr_ismulticast(dest) && umesh_is_mcast_subscribed((ur_ip6_addr_t *)dest)) {
        error = umesh_output_sid(p, umesh_get_meshnetid(), BCAST_SID);
    } else if (ip6_addr_isuniquelocal(dest)) {
        ur_adapter_resolve_ip(dest, &addr);
        if (addr.addr.len == EXT_ADDR_SIZE) {
            error = umesh_output_uuid(p, addr.addr.addr);
        } else {
            error = umesh_output_sid(p, addr.netid, addr.addr.short_addr);
        }
    } else {
        error = UR_ERROR_FAIL;
    }

    return err_mapping(error);
}
#endif

static err_t ur_adapter_if_init(struct netif *netif)
{
    netif->name[0] = g_la_state.interface_name[0];
    netif->name[1] = g_la_state.interface_name[1];
    netif->num = g_la_state.interface_name[2] - '0';
#if LWIP_IPV6
    netif->output_ip6 = ur_adapter_ipv6_output;
#endif
    netif->output = ur_adapter_ipv4_output;
    netif->mtu = 1280;
    netif->flags = NETIF_FLAG_LINK_UP | NETIF_FLAG_UP | NETIF_FLAG_BROADCAST ;
    return ERR_OK;
}

ur_error_t ur_adapter_interface_init(void)
{
    g_la_state.adapter_cb.input = ur_adapter_input;
    g_la_state.adapter_cb.interface_up = ur_adapter_interface_up;
    g_la_state.adapter_cb.interface_down = ur_adapter_interface_down;
    g_la_state.adapter_cb.interface_update = ur_adapter_interface_update;
    umesh_register_callback(&g_la_state.adapter_cb);
    return UR_ERROR_NONE;
}

static void update_interface_ipaddr(void)
{
#if LWIP_IPV6
    ur_ip6_addr_t ip6_addr;
    ip6_addr_t addr6;
    uint8_t index;
    uint16_t main_netid = umesh_get_meshnetid() & 0xff00;

    for (index = 0; index < 3; index++) {
        memset(ip6_addr.m8, 0, sizeof(ip6_addr.m8));
        ip6_addr.m32[0] = htonl(0xfc000000);
        ip6_addr.m32[1] = htonl(main_netid);
        if (index == 0) {
            ip6_addr.m32[3] = \
                htonl((get_sub_netid(umesh_get_meshnetid()) << 16) | umesh_mm_get_local_sid());
        } else if (index == 1) {
            memcpy(&ip6_addr.m8[8], umesh_mm_get_local_uuid(), 8);
        } else {  // mcast addr
            ip6_addr.m8[0] = 0xff;
            ip6_addr.m8[1] = 0x08;
            ip6_addr.m8[6] = (uint8_t)(main_netid >> 8);
            ip6_addr.m8[7] = (uint8_t)main_netid;
        }
        netif_ip6_addr_set_state(&g_la_state.adpif, index, IP6_ADDR_INVALID);
        IP6_ADDR(&addr6, ip6_addr.m32[0], ip6_addr.m32[1],
                 ip6_addr.m32[2], ip6_addr.m32[3]);
        ip6_addr_copy(*(ip_2_ip6(&g_la_state.adpif.ip6_addr[index])), addr6);
        netif_ip6_addr_set_state(&g_la_state.adpif, index, IP6_ADDR_VALID);
    }
    g_la_state.adpif.ip6_autoconfig_enabled = 1;
#else
    uint16_t sid;
    ur_ip4_addr_t ip4_addr;
    ip4_addr_t ipaddr, netmask, gw;

    sid = umesh_get_sid() + 2;
    ip4_addr.m8[0] = 10;
    ip4_addr.m8[1] = 0;
    ip4_addr.m8[2] = sid >> 8;
    ip4_addr.m8[3] = sid & 0xff;
    IP4_ADDR(&gw, 10, 0, 0, 1);
    IP4_ADDR(&ipaddr, ip4_addr.m8[0], ip4_addr.m8[1], \
             ip4_addr.m8[2], ip4_addr.m8[3]);
    IP4_ADDR(&netmask, 255, 255, 0, 0);
    netif_set_addr(&g_la_state.adpif, &ipaddr, &netmask, &gw);

    ip4_set_default_multicast_netif(&g_la_state.adpif);
#if LWIP_IGMP
    ip4_addr.m8[0] = 224;
    ip4_addr.m8[1] = 0;
    ip4_addr.m8[2] = 0;
    ip4_addr.m8[3] = 252;
    g_la_state.adpif.flags |= NETIF_FLAG_IGMP;
    IP4_ADDR(&g_group.group_address, ip4_addr.m8[0], ip4_addr.m8[1], \
             ip4_addr.m8[2], ip4_addr.m8[3]);
    netif_set_client_data(&g_la_state.adpif, LWIP_NETIF_CLIENT_DATA_INDEX_IGMP, &g_group);
#endif
#endif
}

ur_error_t ur_adapter_interface_up(void)
{
    const mac_address_t *mac_addr;
    struct netif *interface;

    interface = netif_find(g_la_state.interface_name);

    if (interface == NULL) {
        mac_addr = umesh_get_mac_address(MEDIA_TYPE_DFL);
        g_la_state.adpif.hwaddr_len = mac_addr->len;
        memcpy(g_la_state.adpif.hwaddr, mac_addr->addr, 6);

        netif_add(&g_la_state.adpif, NULL, NULL, NULL, NULL,
                  ur_adapter_if_init, tcpip_input);
        interface = &g_la_state.adpif;
    }

    assert(interface == &g_la_state.adpif);

    if (netif_is_up(interface) == 0) {
        netif_set_up(&g_la_state.adpif);
    }
    update_interface_ipaddr();

#ifdef CONFIG_AOS_MESH_TAPIF
    if (umesh_mm_get_device_state() == DEVICE_STATE_LEADER) {
        is_router = true;
        umesh_tapif_init("tun0");
    } else {
        is_router = false;
        umesh_tapif_deinit();
    }

    netif_set_default(&g_la_state.adpif);
#else
    /*
     * if we are LEADER MODE, means WiFi is connected
     * then default if should be WiFi IF
     */
    if (!(umesh_get_mode() & MODE_LEADER)) {
        netif_set_default(&g_la_state.adpif);
    }
#endif

    return UR_ERROR_NONE;
}

ur_error_t ur_adapter_interface_down(void)
{
    if (netif_is_up(&g_la_state.adpif)) {
        netif_set_down(&g_la_state.adpif);
    }

    return UR_ERROR_NONE;
}

ur_error_t ur_adapter_interface_update(void)
{
    update_interface_ipaddr();
    return UR_ERROR_NONE;
}

struct netif *ur_adapter_get_netif(void)
{
    return &g_la_state.adpif;
}

const void *ur_adapter_get_default_ipaddr(void)
{
    struct netif *netif = &g_la_state.adpif;
#if LWIP_IPV6
    uint8_t index;

    for (index = 0; index < LWIP_IPV6_NUM_ADDRESSES; index++) {
        if (ip6_addr_isvalid(netif_ip6_addr_state(netif, index))) {
            return netif_ip_addr6(netif, index);
        }
    }
    return NULL;
#else
    return netif_ip4_addr(netif);
#endif
}

const void *ur_adapter_get_mcast_ipaddr(void)
{
    struct netif *netif = &g_la_state.adpif;
#if LWIP_IPV6
    static ip6_addr_t mcast;
    uint8_t index = 2;
    const ip6_addr_t *mcast_addr = netif_ip6_addr(netif, index);

    if (ip6_addr_isvalid(netif_ip6_addr_state(netif, index))) {
        IP6_ADDR(&mcast, mcast_addr->addr[0], mcast_addr->addr[1],
                 0, htonl(0xfc));
        return &mcast;
    }
#else
    struct igmp_group *group;
    if (netif->flags & NETIF_FLAG_IGMP) {
        group = netif_get_client_data(netif, LWIP_NETIF_CLIENT_DATA_INDEX_IGMP);
        return &group->group_address;
    }
#endif
    return NULL;
}

#if LWIP_IPV6
struct netif *ur_adapter_ip6_route(const ip6_addr_t *src,
                                   const ip6_addr_t *dest)
{
    return &g_la_state.adpif;
}

bool ur_adapter_is_mcast_subscribed(const ip6_addr_t *addr)
{
    return umesh_is_mcast_subscribed((const ur_ip6_addr_t *)addr);
}

struct netif *lwip_hook_ip6_route(const ip6_addr_t *src, const ip6_addr_t *dest)
{
    return ur_adapter_ip6_route(src, dest);
}

bool lwip_hook_mesh_is_mcast_subscribed(const ip6_addr_t *dest)
{
    return ur_adapter_is_mcast_subscribed(dest);
}
#endif
