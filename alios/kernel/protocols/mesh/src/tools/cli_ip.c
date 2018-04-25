/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <aos/aos.h>

#include "umesh.h"
#include "umesh_hal.h"
#include "umesh_utils.h"
#include "core/mesh_mgmt.h"
#include "core/router_mgr.h"
#include "ip/ip.h"
#include "ip/lwip_adapter.h"
#include "hal/interfaces.h"
#include "tools/cli.h"
#include "tools/diags.h"

#include "lwip/sockets.h"
#include "lwip/inet_chksum.h"
#include "lwip/inet.h"
#include "lwip/netif.h"

extern void response_append(const char *format, ...);

enum {
    DEF_ICMP6_PAYLOAD_SIZE = 8,
};

typedef struct cli_state_s {
    uint16_t icmp_seq;
    uint16_t icmp_acked;
    int icmp_socket;
} cli_state_t;
static cli_state_t g_cl_state;

typedef void (* data_handler_t)(const uint8_t *payload, uint16_t length);
typedef struct listen_sockets_s {
    int socket;
    data_handler_t handler;
} listen_sockets_t;

#ifdef WITH_LWIP
static void sock_read_cb(int fd, void *arg)
{
    listen_sockets_t *lsock = arg;
    int length = 0;
    uint8_t *buffer;

    buffer = ur_mem_alloc(UR_IP6_MTU + UR_IP6_HLEN);
    if (buffer) {
        length = lwip_recv(lsock->socket, buffer, UR_IP6_MTU + UR_IP6_HLEN, 0);
        if (length > 0) {
            lsock->handler(buffer, length);
        }
        ur_mem_free(buffer, UR_IP6_MTU + UR_IP6_HLEN);
    }
}
#endif

int echo_socket(data_handler_t handler)
{
    static listen_sockets_t g_echo;
#if LWIP_IPV6
    int domain = AF_INET6;
    int protocol = IPPROTO_ICMPV6;
#else
    int domain = AF_INET;
    int protocol = IPPROTO_ICMP;
#endif

    g_echo.socket = lwip_socket(domain, SOCK_RAW, protocol);
    g_echo.handler = handler;
#ifdef WITH_LWIP
    aos_poll_read_fd(g_echo.socket, sock_read_cb, &g_echo);
#endif
    return g_echo.socket;
}

#ifdef CONFIG_AOS_MESH_DEBUG
int autotest_udp_socket(data_handler_t handler, uint16_t port)
{
    static listen_sockets_t udp_autotest;
#if LWIP_IPV6
    struct sockaddr_in6 addr;
    int domain = AF_INET6;

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
#else
    struct sockaddr_in addr;
    int domain = AF_INET;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
#endif
    udp_autotest.socket = lwip_socket(domain, SOCK_DGRAM, IPPROTO_UDP);
    lwip_bind(udp_autotest.socket, (const struct sockaddr *)&addr, sizeof(addr));
    udp_autotest.handler = handler;
#ifdef WITH_LWIP
    aos_poll_read_fd(udp_autotest.socket, sock_read_cb, &udp_autotest);
#endif
    return udp_autotest.socket;
}
#endif

int ip_sendto(int socket, const uint8_t *payload, uint16_t length,
              void *dest, uint16_t port)
{
#if LWIP_IPV6
    struct sockaddr_in6 sock_addr;
    uint8_t index;

    sock_addr.sin6_len = sizeof(sock_addr);
    sock_addr.sin6_family = AF_INET6;
    sock_addr.sin6_port = htons(port);
    for (index = 0; index < UR_IP6_ADDR_SIZE / sizeof(uint32_t); ++index) {
        sock_addr.sin6_addr.un.u32_addr[index] = ((ur_ip6_addr_t *)dest)->m32[index];
    }
#else
    struct sockaddr_in sock_addr;

    sock_addr.sin_len = sizeof(sock_addr);
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);
    sock_addr.sin_addr.s_addr = ((ur_ip4_addr_t *)dest)->m32;
#endif

    return lwip_sendto(socket, payload, length, 0, (struct sockaddr *)&sock_addr,
                       sizeof(sock_addr));
}

#ifdef CONFIG_AOS_MESH_DEBUG
typedef struct cli_autotest_s {
    int udp_socket;
    ur_timer_t timer;
    ur_timer_t print_timer;
    uint16_t times;
    uint16_t seq;
    uint16_t acked;
    uint16_t length;
#if LWIP_IPV6
    ur_ip6_addr_t target;
#else
    ur_ip4_addr_t target;
#endif
    slist_t acked_list;
} cli_autotest_t;
cli_autotest_t g_cli_autotest;

enum {
    AUTOTEST_PRINT_WAIT_TIME = 15000,
    AUTOTEST_UDP_PORT        = 7335,
};

typedef struct autotest_acked_s {
    slist_t next;
    uint16_t subnetid;
    uint16_t sid;
    uint16_t acked;
    uint16_t seq;
} autotest_acked_t;

#define for_each_acked(acked) \
    slist_for_each_entry(&g_cli_autotest.acked_list, acked, autotest_acked_t, next)

enum {
    AUTOTEST_REQUEST       = 1,
    AUTOTEST_REPLY         = 2,
    AUTOTEST_ECHO_INTERVAL = 1000,
};

typedef struct autotest_cmd_s {
    uint16_t seq;
    uint16_t type;
    ur_ip6_addr_t addr;
}  __attribute__((packed)) autotest_cmd_t;

static void handle_autotest_print_timer(void *args)
{
    autotest_acked_t *acked;
    uint8_t num = 0;

    g_cli_autotest.print_timer = NULL;
    while (!slist_empty(&g_cli_autotest.acked_list)) {
        acked = slist_first_entry(&g_cli_autotest.acked_list, autotest_acked_t,
                                  next);
        response_append("%04x:%d\%, ",
                        ntohs(acked->sid), (acked->acked * 100) / g_cli_autotest.seq);
        slist_del(&acked->next, &g_cli_autotest.acked_list);
        ur_mem_free(acked, sizeof(autotest_acked_t));
        num++;
    }
    response_append("\r\nnum %d\r\n", num);
}

static void handle_autotest_timer(void *args)
{
#if LWIP_IPV6
    uint8_t *payload;
    const ip6_addr_t *src;
    autotest_cmd_t *cmd;

    g_cli_autotest.timer = NULL;
    payload = (uint8_t *)ur_mem_alloc(g_cli_autotest.length);
    if (payload) {
        src = ur_adapter_get_default_ipaddr();
        cmd = (autotest_cmd_t *)payload;
        cmd->type = AUTOTEST_REQUEST;
        cmd->seq = htons(g_cli_autotest.seq++);
        memcpy(&cmd->addr, src, sizeof(ur_ip6_addr_t));
        ip_sendto(g_cli_autotest.udp_socket, payload, g_cli_autotest.length,
                   &g_cli_autotest.target, AUTOTEST_UDP_PORT);
        ur_mem_free(payload, g_cli_autotest.length);
        g_cli_autotest.times--;
    }
    if (g_cli_autotest.times) {
        ur_start_timer(&g_cli_autotest.timer, AUTOTEST_ECHO_INTERVAL, handle_autotest_timer, NULL);
    } else {
        ur_start_timer(&g_cli_autotest.print_timer, AUTOTEST_PRINT_WAIT_TIME,
                       handle_autotest_print_timer, NULL);
    }
#else
    uint8_t *payload;
    const ip4_addr_t *src;
    autotest_cmd_t *cmd;

    g_cli_autotest.timer = NULL;
    payload = (uint8_t *)ur_mem_alloc(g_cli_autotest.length);
    if (payload) {
        src = ur_adapter_get_default_ipaddr();
        cmd = (autotest_cmd_t *)payload;
        cmd->type = AUTOTEST_REQUEST;
        cmd->seq = htons(g_cli_autotest.seq++);
        memcpy(&cmd->addr, src, sizeof(ur_ip4_addr_t));
        ip_sendto(g_cli_autotest.udp_socket, payload, g_cli_autotest.length,
                   &g_cli_autotest.target, AUTOTEST_UDP_PORT);
        ur_mem_free(payload, g_cli_autotest.length);
        g_cli_autotest.times--;
    }
    if (g_cli_autotest.times) {
        ur_start_timer(&g_cli_autotest.timer, AUTOTEST_ECHO_INTERVAL, handle_autotest_timer, NULL);
    } else {
        ur_start_timer(&g_cli_autotest.print_timer, AUTOTEST_PRINT_WAIT_TIME,
                       handle_autotest_print_timer, NULL);
    }
#endif
}

void process_autotest(int argc, char *argv[])
{
    char *end;
    autotest_acked_t *acked = NULL;

    if (argc == 0) {
        for_each_acked(acked) {
            response_append("%04x: %d, %d\r\n", ntohs(acked->sid), acked->seq,
                            acked->acked);
        }
        return;
    }

    ur_stop_timer(&g_cli_autotest.timer, NULL);
    ur_stop_timer(&g_cli_autotest.print_timer, NULL);

    while (!slist_empty(&g_cli_autotest.acked_list)) {
        acked = slist_first_entry(&g_cli_autotest.acked_list, autotest_acked_t,
                                  next);
        slist_del(&acked->next, &g_cli_autotest.acked_list);
        ur_mem_free(acked, sizeof(autotest_acked_t));
    }

    g_cli_autotest.seq = 0;
    g_cli_autotest.acked = 0;
    g_cli_autotest.times = 1;
    if (argc > 1) {
        g_cli_autotest.times = (uint16_t)strtol(argv[1], &end, 0);
        if (g_cli_autotest.times == 0) {
            g_cli_autotest.times = 1;
        }
    }

    g_cli_autotest.length = sizeof(autotest_cmd_t);
    if (argc > 2) {
        g_cli_autotest.length = (uint16_t)strtol(argv[2], &end, 0);
        if (g_cli_autotest.length < (sizeof(autotest_cmd_t))) {
            g_cli_autotest.length = sizeof(autotest_cmd_t);
        }
    }
    if (g_cli_autotest.length > UR_IP6_MTU) {
        response_append("exceed mesh IP6 MTU %d\r\n", UR_IP6_MTU);
        return;
    }
#if LWIP_IPV6
    string_to_ip6_addr(argv[0], &g_cli_autotest.target);
#else
    g_cli_autotest.target.m32 = inet_addr(argv[0]);
#endif

    handle_autotest_timer(NULL);
}

static bool update_autotest_acked_info(uint16_t subnetid, uint16_t sid,
                                       uint16_t seq)
{
    bool exist = false;
    autotest_acked_t *acked = NULL;

    for_each_acked(acked) {
        if (acked->sid == sid && acked->subnetid == subnetid) {
            exist = true;
            break;
        }
    }

    if (exist == false) {
        acked = (autotest_acked_t *)ur_mem_alloc(sizeof(autotest_acked_t));
        if (acked == NULL) {
            return true;
        }
        acked->subnetid = subnetid;
        acked->sid = sid;
        acked->acked = 0;
        acked->seq = seq;
        slist_add_tail(&acked->next, &g_cli_autotest.acked_list);
    }

    if (seq > acked->seq || acked->acked == 0) {
        acked->acked++;
        acked->seq = seq;
        return true;
    }
    return false;
}

static void handle_udp_autotest(const uint8_t *payload, uint16_t length)
{
#if LWIP_IPV6
    autotest_cmd_t *cmd;
    uint8_t *data;
    ur_ip6_addr_t dest;
    const ip6_addr_t *src;
    uint16_t seq;

    if (length == 0) {
        return;
    }

    cmd = (autotest_cmd_t *)payload;
    memcpy(&dest, &cmd->addr, sizeof(ur_ip6_addr_t));
    seq = ntohs(cmd->seq);
    if (cmd->type == AUTOTEST_REQUEST) {
        response_append("%d bytes autotest echo request from " IP6_ADDR_FMT
                        ", seq %d\r\n", length,
                        IP6_ADDR_DATA((&dest)), seq);
        data = (uint8_t *)ur_mem_alloc(length);
        if (data == NULL) {
            return;
        }
        memset(data, 0, length);
        cmd = (autotest_cmd_t *)data;
        cmd->type = AUTOTEST_REPLY;
        cmd->seq = htons(seq);
        src = ur_adapter_get_default_ipaddr();
        memcpy(&cmd->addr, src, sizeof(ur_ip6_addr_t));
        ip_sendto(g_cli_autotest.udp_socket, data, length, &dest,
                   AUTOTEST_UDP_PORT);
        ur_mem_free(data, length);
    } else if (cmd->type == AUTOTEST_REPLY) {
        g_cli_autotest.acked ++;
        if (update_autotest_acked_info(dest.m16[6], dest.m16[7], seq) == false) {
            return;
        }
        response_append("%d bytes autotest echo reply from " IP6_ADDR_FMT
                        ", seq %d\r\n", length,
                        IP6_ADDR_DATA((&dest)), seq);
    }
#else
    autotest_cmd_t *cmd;
    uint8_t *data;
    ur_ip4_addr_t dest;
    const ip4_addr_t *src;
    uint16_t seq;

    if (length == 0) {
        return;
    }

    cmd = (autotest_cmd_t *)payload;
    memcpy(&dest, &cmd->addr, sizeof(ur_ip4_addr_t));
    seq = ntohs(cmd->seq);
    if (cmd->type == AUTOTEST_REQUEST) {
        response_append("%d bytes autotest echo request from " IP4_ADDR_FMT
                        ", seq %d\r\n", length,
                        IP4_ADDR_DATA((&dest)), seq);
        data = (uint8_t *)ur_mem_alloc(length);
        if (data == NULL) {
            return;
        }
        memset(data, 0, length);
        cmd = (autotest_cmd_t *)data;
        cmd->type = AUTOTEST_REPLY;
        cmd->seq = htons(seq);
        src = ur_adapter_get_default_ipaddr();
        memcpy(&cmd->addr, src, sizeof(ur_ip4_addr_t));
        ip_sendto(g_cli_autotest.udp_socket, data, length, &dest,
                   AUTOTEST_UDP_PORT);
        ur_mem_free(data, length);
    } else if (cmd->type == AUTOTEST_REPLY) {
        g_cli_autotest.acked ++;
        if (update_autotest_acked_info(0, dest.m16[1], seq) == false) {
            return;
        }
        response_append("%d bytes autotest echo reply from " IP4_ADDR_FMT
                        ", seq %d\r\n", length,
                        IP4_ADDR_DATA((&dest)), seq);
    }
#endif
}

extern const char *state2str(node_state_t state);
extern const char *routerid2str(uint8_t id);
void process_testcmd(int argc, char *argv[])
{
    slist_t *hals;
    hal_context_t *hal;
    const char *cmd;
    slist_t *nbrs = NULL;

    if (argc < 1) {
        return;
    }

    cmd = argv[0];
    if (strcmp(cmd, "sid") == 0) {
        response_append("%04x", umesh_get_sid());
    } else if (strcmp(cmd, "state") == 0) {
        response_append("%s", state2str(umesh_get_device_state()));
    } else if (strcmp(cmd, "parent") == 0) {
        neighbor_t *nbr;
        hals = get_hal_contexts();
        slist_for_each_entry(hals, hal, hal_context_t, next) {
            nbrs = umesh_get_nbrs(hal->module->type);
            slist_for_each_entry(nbrs, nbr, neighbor_t, next) {
                if (nbr->state != STATE_PARENT) {
                    continue;
                }
                response_append(EXT_ADDR_FMT, EXT_ADDR_DATA(nbr->mac));
                return;
            }
        }
    } else if (strcmp(cmd, "netsize") == 0) {
        response_append("%d", umesh_mm_get_meshnetsize());
    } else if (strcmp(cmd, "netid") == 0) {
        response_append("%04x", umesh_get_meshnetid());
    } else if (strcmp(cmd, "icmp_acked") == 0) {
        response_append("%d", g_cl_state.icmp_acked);
    } else if (strcmp(cmd, "autotest_acked") == 0) {
        response_append("%d", g_cli_autotest.acked);
    } else if (strcmp(cmd, "ipaddr") == 0) {
#if LWIP_IPV6
        response_append(IP6_ADDR_FMT, \
                        IP6_ADDR_DATA(((const ur_ip6_addr_t *)ur_adapter_get_default_ipaddr())));
#else
        response_append(IP6_ADDR_FMT, \
                        IP6_ADDR_DATA(((const ur_ip4_addr_t *)ur_adapter_get_default_ipaddr())));
#endif
    } else if (strcmp(cmd, "router") == 0) {
        response_append("%s", routerid2str(ur_router_get_default_router()));
    }
}

void process_traceroute(int argc, char *argv[])
{
#if LWIP_IPV6
    ur_ip6_addr_t dest;
    ur_addr_t dest_addr;
    network_context_t *network;

    if (argc == 0) {
        return;
    }

    string_to_ip6_addr(argv[0], &dest);
    if (ur_adapter_resolve_ip(&dest, &dest_addr) != UR_ERROR_NONE) {
        return;
    }

    network = get_network_context_by_meshnetid(dest_addr.netid, true);
    send_trace_route_request(network, &dest_addr);
#endif
}
#endif

static void show_ipaddr(void)
{
    struct netif *netif = ur_adapter_get_netif();
#if LWIP_IPV6
    const ip6_addr_t *mcast_addr;
    uint8_t index;

    for (index = 0; index < 2; index++) {
        if (ip6_addr_isvalid(netif_ip6_addr_state(netif, index))) {
            response_append("\t" IP6_ADDR_FMT "\r\n",
                            IP6_ADDR_DATA(((ur_ip6_addr_t *)netif_ip_addr6(netif, index))));
        }
    }
    mcast_addr = ur_adapter_get_mcast_ipaddr();
    response_append("\t" IP6_ADDR_FMT "\r\n",
                    IP6_ADDR_DATA(((ur_ip6_addr_t *)mcast_addr)));
#else
    const ip4_addr_t *mcast_addr;

    response_append("\t" IP4_ADDR_FMT "\r\n",
                    IP4_ADDR_DATA(((const ur_ip4_addr_t *)netif_ip4_addr(netif))));

    mcast_addr = ur_adapter_get_mcast_ipaddr();
    if (mcast_addr) {
        response_append("\t" IP4_ADDR_FMT "\r\n", IP4_ADDR_DATA(((ur_ip4_addr_t *)mcast_addr)));
    }
#endif
}

void process_ipaddr(int argc, char *argv[])
{
    show_ipaddr();
    response_append("done\r\n");
}

void process_ping(int argc, char *argv[])
{
    ur_icmp6_header_t *header;
#if LWIP_IPV6
    ur_ip6_addr_t target;
#else
    ur_ip4_addr_t target;
#endif
    uint8_t *payload;
    uint16_t length;
    char *end;

    if (argc == 0) {
        return;
    }

    length = DEF_ICMP6_PAYLOAD_SIZE + sizeof(ur_icmp6_header_t);
#if LWIP_IPV6
    string_to_ip6_addr(argv[0], &target);
#else
    target.m32 = inet_addr(argv[0]);
#endif

    if (argc > 1) {
        length = (uint16_t)strtol(argv[1], &end, 0);
        length += sizeof(ur_icmp6_header_t);
    }

    if (length > UR_IP6_MTU) {
        response_append("exceed mesh IP MTU %d\r\n", UR_IP6_MTU);
        return;
    }

    payload = (uint8_t *)ur_mem_alloc(length);
    if (payload == NULL) {
        return;
    }
    memset(payload, 0, length);

    header = (ur_icmp6_header_t *)(payload);
    header->type = UR_ICMP6_TYPE_EREQ;
    header->code = 7;
    header->data = g_cl_state.icmp_seq;
    ++g_cl_state.icmp_seq;
    header->chksum = inet_chksum(payload, length);

    ip_sendto(g_cl_state.icmp_socket, payload, length, &target, 0);
    ur_mem_free(payload, length);
}

void cli_handle_echo_response(const uint8_t *payload, uint16_t length)
{
    ur_icmp6_header_t *icmp6_header;
#if LWIP_IPV6
    ur_ip6_header_t *ip6_header;

    if (length) {
        ip6_header = (ur_ip6_header_t *)payload;
        icmp6_header = (ur_icmp6_header_t *)(payload + UR_IP6_HLEN);
        if (icmp6_header->type == UR_ICMP6_TYPE_EREP) {
            g_cl_state.icmp_acked ++;
            response_append("%d bytes from " IP6_ADDR_FMT " icmp_seq %d icmp_acked %d\r\n",
                            length - UR_IP6_HLEN - sizeof(ur_icmp6_header_t),
                            IP6_ADDR_DATA((&ip6_header->src)),
                            icmp6_header->data, g_cl_state.icmp_acked);
        }
    }
#else
    ur_ip4_header_t *ip4_header;

    if (length) {
        ip4_header = (ur_ip4_header_t *)payload;
        icmp6_header = (ur_icmp6_header_t *)(payload + MESH_IP4_HLEN);
        if (icmp6_header->type == UR_ICMP6_TYPE_EREP) {
            g_cl_state.icmp_acked ++;
            response_append("%d bytes from " IP4_ADDR_FMT " icmp_seq %d icmp_acked %d\r\n",
                            length - MESH_IP4_HLEN - sizeof(ur_icmp6_header_t),
                            IP4_ADDR_DATA((&ip4_header->src)),
                            icmp6_header->data, g_cl_state.icmp_acked);
        }
    }
#endif
}

#ifndef WITH_LWIP
struct cb_arg {
    data_handler_t handler;
    uint8_t *buffer;
    int length;
};

static void do_read_cb(void *priv)
{
    struct cb_arg *arg = priv;
    arg->handler(arg->buffer, arg->length);

    ur_mem_free(arg->buffer, UR_IP6_MTU + UR_IP6_HLEN);
    ur_mem_free(arg, sizeof(*arg));
}

static void ur_read_sock(int fd, data_handler_t handler)
{
    int length = 0;
    uint8_t *buffer;

    buffer = ur_mem_alloc(UR_IP6_MTU + UR_IP6_HLEN);
    if (buffer == NULL) {
        return;
    }

    length = lwip_recv(fd, buffer, UR_IP6_MTU + UR_IP6_HLEN, 0);
    if (length > 0) {
        struct cb_arg *arg = ur_mem_alloc(sizeof(*arg));
        arg->handler = handler;
        arg->buffer = buffer;
        arg->length = length;
        aos_schedule_call(do_read_cb, arg);
        return;
    }

    ur_mem_free(buffer, UR_IP6_MTU + UR_IP6_HLEN);
}

static void mesh_worker(void *arg)
{
#ifdef CONFIG_AOS_MESH_DEBUG
    int maxfd = g_cli_autotest.udp_socket;
#else
    int maxfd = -1;
#endif

    if (g_cl_state.icmp_socket > maxfd) {
        maxfd = g_cl_state.icmp_socket;
    }

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_cl_state.icmp_socket, &rfds);
#ifdef CONFIG_AOS_MESH_DEBUG
        FD_SET(g_cli_autotest.udp_socket, &rfds);
#endif

        lwip_select(maxfd + 1, &rfds, NULL, NULL, NULL);

        if (FD_ISSET(g_cl_state.icmp_socket, &rfds)) {
            ur_read_sock(g_cl_state.icmp_socket, cli_handle_echo_response);
        }
#ifdef CONFIG_AOS_MESH_DEBUG
        if (FD_ISSET(g_cli_autotest.udp_socket, &rfds)) {
            ur_read_sock(g_cli_autotest.udp_socket, handle_udp_autotest);
        }
#endif
    }
}
#endif

void mesh_cli_ip_init(void)
{
    g_cl_state.icmp_socket = echo_socket(&cli_handle_echo_response);
#ifdef CONFIG_AOS_MESH_DEBUG
    slist_init(&g_cli_autotest.acked_list);
    g_cli_autotest.udp_socket = autotest_udp_socket(&handle_udp_autotest,
                                                    AUTOTEST_UDP_PORT);
#endif

#ifndef WITH_LWIP
    aos_task_new("meshworker", mesh_worker, NULL, 8192);
#endif
}
