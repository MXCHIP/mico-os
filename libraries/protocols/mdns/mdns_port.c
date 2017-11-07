/**
 ******************************************************************************
 * @file    mdns_port.c
 * @author  William Xu
 * @version V1.0.0
 * @date    8-Aug-2017
 * @brief   This file provide the mdns port layer for mico system
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico.h"
#include "mdns_port.h"
#include "mdns.h"
#include "mdns_private.h"

/******************************************************************************
 *                                  Macros
 ******************************************************************************/

#if CONFIG_XMDNS
/* IPv4 group for multicast DNS queries: 239.255.255.251 */
#define INADDR_MULTICAST_MDNS \
    {                         \
        htonl(0xEFFFFFFBUL)   \
    }

/* IPv6 group for multicast DNS queries: FF0::FB */
#define INADDR6_MULTICAST_MDNS                                     \
    {                                                              \
        {                                                          \
            {                                                      \
                htonl(0xFF050000), htonl(0), htonl(0), htonl(0xFB) \
            }                                                      \
        }                                                          \
    }

#else
/* IPv4 group for multicast DNS queries: 224.0.0.251 */
#define INADDR_MULTICAST_MDNS \
    {                         \
        htonl(0xE00000FBUL)   \
    }

/* IPv6 group for multicast DNS queries: FF02::FB */
#define INADDR6_MULTICAST_MDNS                                     \
    {                                                              \
        {                                                          \
            {                                                      \
                htonl(0xFF020000), htonl(0), htonl(0), htonl(0xFB) \
            }                                                      \
        }                                                          \
    }
#endif

/******************************************************************************
 *                                 Constants
 ******************************************************************************/

#define MDNS_QUERIER_THREAD_STACK (2048)
#define MDNS_RESPONDER_THREAD_STACK (1500)

/******************************************************************************
 *                             Variable Definitions
 ******************************************************************************/
mdns_responder_stats mr_stats;

static mico_thread_t mdns_querier_thread, mdns_responder_thread;
static bool is_responder_started, is_querier_started;

struct sockaddr_in mdns_mquery_v4group = {sizeof(struct sockaddr_in), AF_INET, htons(5353), INADDR_MULTICAST_MDNS};
struct sockaddr_in6 mdns_mquery_v6group = {sizeof(struct sockaddr_in6), AF_INET6, htons(5353), 0, INADDR6_MULTICAST_MDNS, 0};

/******************************************************************************
 *                         Static Function Declarations
 ******************************************************************************/

static void net_status_changed_delegate(WiFiEvent event, void *arg);
static void system_will_poweroff_delegate(void *arg);

/******************************************************************************
 *                             Function Definitions
 ******************************************************************************/

static void net_status_changed_delegate(WiFiEvent event, void *arg)
{
    UNUSED_PARAMETER(arg);
    switch (event)
    {
    case NOTIFY_STATION_UP:
        mdns_iface_group_state_change(INTERFACE_STA, JOIN);
        mdns_iface_state_change(INTERFACE_STA, UP);
        break;
    case NOTIFY_AP_UP:
        mdns_iface_group_state_change(INTERFACE_UAP, JOIN);
        mdns_iface_state_change(INTERFACE_UAP, UP);
        break;
#if PLATFORM_ETH_ENABLE
    case NOTIFY_ETH_UP:
        mdns_iface_group_state_change(INTERFACE_ETH, JOIN);
        mdns_iface_state_change(INTERFACE_ETH, UP);
        break;
#endif
    default:
        break;
    }
}

static void system_will_poweroff_delegate(void *arg)
{
    UNUSED_PARAMETER(arg);
    mdns_iface_state_change(INTERFACE_STA, DOWN);
    mdns_iface_state_change(INTERFACE_UAP, DOWN);
#if PLATFORM_ETH_ENABLE
    mdns_iface_state_change(INTERFACE_ETH, DOWN);
#endif
}

/**************************Thread and System Functions**************************/

void *mdns_thread_create(mdns_thread_entry entry, int id)
{
    void *ret = NULL;
    if (id == MDNS_THREAD_RESPONDER)
    {
        if (mico_rtos_create_thread(&mdns_responder_thread, MICO_APPLICATION_PRIORITY,
                                    "mdns_resp_thread", (mico_thread_function_t)entry,
                                    MDNS_RESPONDER_THREAD_STACK, 0) == kNoErr)
        {
            ret = &mdns_responder_thread;
        }
    }
    else if (id == MDNS_THREAD_QUERIER)
    {
        if (mico_rtos_create_thread(&mdns_querier_thread, MICO_APPLICATION_PRIORITY,
                                    "mdns_querier_thread", (mico_thread_function_t)entry,
                                    MDNS_QUERIER_THREAD_STACK, 0) == kNoErr)
        {
            ret = &mdns_querier_thread;
        }
    }
    return ret;
}

void mdns_thread_delete(void *t)
{
    mico_rtos_delete_thread((mico_thread_t *)t);
}

void mdns_thread_yield(void *t)
{
    mico_rtos_thread_yield();
}

uint32_t mdns_time_ms(void)
{
    mico_time_t current;
    mico_time_get_time(&current);
    return current;
}

int mdns_rand_range(int n)
{
    int r;
    srand(mico_rtos_get_time());
    r = rand();
    return r / (RAND_MAX / n + 1);
}

/****************************Mdns Main Entry Functions***************************/
int mdns_start(const char *domain, char *hostname)
{
    OSStatus err = kNoErr;

    err = mico_system_notify_register(mico_notify_WIFI_STATUS_CHANGED,
                                      (void *)net_status_changed_delegate, NULL);
    require_noerr(err, exit);
    err = mico_system_notify_register(mico_notify_SYS_WILL_POWER_OFF,
                                      (void *)system_will_poweroff_delegate, NULL);
    require_noerr(err, exit);

    if (hostname)
    {
        if (is_responder_started != true)
        {
            err = responder_launch(domain, hostname);
            require_noerr(err, exit);
            is_responder_started = true;
        }
        else
        {
            LOG("mdns responder already started");
        }
    }
    if (is_querier_started != true)
    {
        err = query_launch();
        require_noerr(err, exit);
        is_querier_started = true;
    }
    else
    {
        LOG("mdns querier already started");
    }

exit:
    return err;
}

void mdns_stop(void)
{
    LOG("Stopping mdns.\r\n");
    if (is_responder_started == true)
    {
        responder_halt();
        is_responder_started = false;
    }
    else
    {
        LOG("Can't stop mdns responder; responder not started");
    }
    if (is_querier_started == true)
    {
        query_halt();
        is_querier_started = false;
    }
    else
    {
        LOG("Can't stop mdns querier; querier not started");
    }

    mico_system_notify_register(mico_notify_WIFI_STATUS_CHANGED, (void *)net_status_changed_delegate, NULL);
    mico_system_notify_register(mico_notify_SYS_WILL_POWER_OFF, (void *)system_will_poweroff_delegate, NULL);
}

/****************************Mdns Socket Functions***************************/
int mdns_socket_mcast(void)
{
    int sock;
    int yes = 1;
    unsigned char ttl = 255;

#if MICO_CONFIG_IPV6
    struct sockaddr_in6 in_addr6;
    struct ipv6_mreq mc6;
#else
    struct sockaddr_in in_addr;
#endif

    struct ip_mreq mc;

#if MICO_CONFIG_IPV6
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
#else
    sock = socket(AF_INET, SOCK_DGRAM, 0);
#endif

    if (sock < 0)
    {
        LOG("error: could not open multicast socket\n");
        return ERR_MDNS_FSOC;
    }

    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0)
    {
        LOG("error: failed to set SO_REUSEADDR option\n");
        close(sock);
        return ERR_MDNS_FREUSE;
    }

#if MICO_CONFIG_IPV6
    mc6.ipv6mr_interface = INTERFACE_STA;
    memcpy(&mc6.ipv6mr_multiaddr, &mdns_mquery_v6group.sin6_addr, sizeof(struct in6_addr));
    setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mc6, sizeof(mc6));

    in_addr6.sin6_family = AF_INET6;
    in_addr6.sin6_port = htons(5353);
    in_addr6.sin6_addr = in6_addr_any;
    if (bind(sock, (struct sockaddr *)&in_addr6, sizeof(in_addr6)))
    {
        close(sock);
        return ERR_MDNS_FBIND;
    }
#else
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = htons(5353);
    in_addr.sin_addr = in_addr_any;
    if (bind(sock, (struct sockaddr *)&in_addr, sizeof(in_addr)))
    {
        close(sock);
        return ERR_MDNS_FBIND;
    }
#endif

    /* join multicast group */
    mc.imr_multiaddr = mdns_mquery_v4group.sin_addr;
    mc.imr_interface = in_addr_any;
    setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc, sizeof(mc));

    /* set other IP-level options */
    setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl));

    return sock;
}

#if CONFIG_DNSSD_QUERY
bool dns_socket_used;
int dns_socket_ucast(uint16_t port)
{
    int sock;
    int yes = 1;
    struct sockaddr_in in_addr;

    if (dns_socket_used)
    {
        LOG("error: unicast-socket is already used.");
        return ERR_MDNS_INUSE;
    }

    memset(&in_addr, 0, sizeof(in_addr));
    in_addr.sin_family = AF_INET;
    in_addr.sin_port = port;
    in_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        LOG("error: could not open unicast socket\n");
        return ERR_MDNS_FSOC;
    }
#ifdef SO_REUSEPORT
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&yes,
                   sizeof(yes)) < 0)
    {
        LOG("error: failed to set SO_REUSEPORT option\n");
        close(sock);
        return ERR_MDNS_FREUSE;
    }
#endif
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));

    if (bind(sock, (struct sockaddr *)&in_addr, sizeof(in_addr)))
    {
        close(sock);
        return ERR_MDNS_FBIND;
    }
    dns_socket_used = true;
    return sock;
}

int dns_socket_close(int s)
{
    dns_socket_used = false;
    return close(s);
}
#endif

int mdns_send_msg(struct mdns_message *m, int sock, unsigned short port, netif_t out_interface, in_addr_t to_addr)
{
    struct sockaddr_in to;

    int size, len;
    uint32_t ip;
    IPStatusTypedef interface_ip_status;

    if (sock == -1)
        return 0;

    /* get message length */
    size = (unsigned int)m->cur - (unsigned int)m->header;

    micoWlanGetIPStatus(&interface_ip_status, out_interface);
    ip = inet_addr(interface_ip_status.ip);

#ifdef CONFIG_IPV6
    int i, ipv6_valid;
    ipv6_addr_t ipv6_addrs[MICO_IPV6_NUM_ADDRESSES];

    for (i = 0; i < MICO_IPV6_NUM_ADDRESSES; i++)
    {
        ipv6_addrs[i].addr_state = IP6_ADDR_INVALID;
    }

    micoWlanGetIP6Status(ipv6_addrs, MICO_IPV6_NUM_ADDRESSES, out_interface);

    for (i = 0, ipv6_valid = 0; i < MICO_IPV6_NUM_ADDRESSES && IP6_ADDR_IS_VALID(ipv6_addrs[i].addr_state); i++)
    {
        ipv6_valid++;
    }
#endif

    /* If IP address is not set, then either interface is not up
     * or interface didn't got the IP from DHCP. In both case Packet
     * shouldn't be transmitted
    */
    if (!ip
#ifdef CONFIG_IPV6
        && !ipv6_valid
#endif
        )
    {
        LOG("Interface is not up\n\r");
        return kGeneralErr;
    }

    if (ip)
    {
        if (setsockopt(sock, IPPROTO_IP, IP_MULTICAST_IF, (char *)&ip, sizeof(ip)) == -1)
        {
            mdns_socket_close(&sock);
            return kGeneralErr;
        }

        if (to_addr)
        {
            to.sin_family = AF_INET;
            to.sin_port = port;
            to.sin_addr.s_addr = to_addr;
            len = sendto(sock, (char *)m->header, size, 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in));
        }
        else
        {
            len = sendto(sock, (char *)m->header, size, 0, (struct sockaddr *)&mdns_mquery_v4group, sizeof(mdns_mquery_v4group));
        }

        if (len < size)
        {
            mr_stats.tx_ipv4_err++;
            LOG("error: failed to send IPv4 message\r\n");
            return kGeneralErr;
        }
        mr_stats.total_tx++;
        DBG("IPV4 sent %u-byte message\r\n", size);
    }

#ifdef CONFIG_IPV6
    if (ipv6_valid)
    {
        /* Not support in LWIP currently
        if ( setsockopt(sock, IPPROTO_IPV6, IPV6_MULTICAST_IF,
                        (char *)&ipv6_addrs[0].address, sizeof(struct in6_addr)) == -1 ) {
            mdns_socket_close(&sock);
            return kGeneralErr;
        }
*/
        sendto(sock, (char *)m->header, size, 0, (struct sockaddr *)&mdns_mquery_v6group, sizeof(mdns_mquery_v6group));

        if (len < size)
        {
            mr_stats.tx_ipv6_err++;
            LOG("error: failed to send IPv6 message\r\n");
            return 0;
        }

        DBG("IPV6 sent %u-byte message\r\n", size);
    }
#endif
    return MICO_SUCCESS;
}

mico_queue_t mdns_ctrl_queue[2] = {NULL, NULL};
int mdns_ctrl_socks[2] = {-1, -1};

int mdns_socket_queue(uint8_t id, mico_queue_t **queue, int msg_size)
{
    int ret = 0;

    if (mdns_ctrl_socks[id] < 0 && msg_size > 0)
    {
        ret = mico_rtos_init_queue(&mdns_ctrl_queue[id], "CTRL_RESPONDER",
                                   msg_size, 8);
        if (ret != 0)
            return -1;
        mdns_ctrl_socks[id] = mico_rtos_init_event_fd(mdns_ctrl_queue[id]);
    }

    if (queue != NULL)
    {
        *queue = &mdns_ctrl_queue[id];
    }

    return mdns_ctrl_socks[id];
}

int mdns_socket_close(int *s)
{
    if (*s == mdns_ctrl_socks[MDNS_CTRL_RESPONDER])
        mico_rtos_deinit_queue(&mdns_ctrl_queue[MDNS_CTRL_RESPONDER]);

    if (*s == mdns_ctrl_socks[MDNS_CTRL_QUERIER])
        mico_rtos_deinit_queue(&mdns_ctrl_queue[MDNS_CTRL_QUERIER]);

    close(*s);
    *s = -1;
    return 0;
}
