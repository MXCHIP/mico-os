/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>

#include "aos/aos.h"
#include "core/auth_mgmt.h"
#include "core/auth_relay.h"
#include "core/mesh_mgmt.h"
#include "hal/interfaces.h"
#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "ip/ip.h"
#include "umesh_utils.h"
#include "umesh_config.h"

socket_t g_sock;

#ifdef WITH_LWIP
static void socket_read_cb(int fd, void *arg)
{
    socket_t *sock = arg;
    uint8_t *buffer;
    uint16_t length = 0;

    buffer = ur_mem_alloc(UR_IP6_MTU + UR_IP6_HLEN);
    if (buffer) {
        length = lwip_recv(sock->socket, buffer, UR_IP6_MTU + UR_IP6_HLEN, 0);
        if (length > 0) {
            sock->handler(buffer, length);
        }

        ur_mem_free(buffer, UR_IP6_MTU + UR_IP6_HLEN);
    }
}
#endif

int socket_sendmsg(int socket, const uint8_t *src, const uint8_t *dst,
                   int cmd, const uint8_t *payload, uint16_t length,
                   void *dest, uint16_t port)
{
    int ret;
    uint8_t *buffer;
    uint8_t *cur;
    uint16_t len = length + ID2_AUTH_SOCK_FIXED_LEN;
#if LWIP_IPV6
    ur_ip6_addr_t target;
#else
    ur_ip4_addr_t target;
#endif

    buffer = ur_mem_alloc(len);
    if (buffer == NULL) {
        return UR_ERROR_MEM;
    }
    cur = buffer;

#if LWIP_IPV6
    string_to_ip6_addr(dest, &target);
#else
    target.m32 = inet_addr(dest);
#endif

#if LWIP_IPV6
    struct sockaddr_in6 sock_addr;
    uint8_t index;

    sock_addr.sin6_len = sizeof(sock_addr);
    sock_addr.sin6_family = AF_INET6;
    sock_addr.sin6_port = htons(port);
    for (index = 0; index < UR_IP6_ADDR_SIZE / sizeof(uint32_t); ++index) {
        sock_addr.sin6_addr.un.u32_addr[index] = target.m32[index];
    }
#else
    struct sockaddr_in sock_addr;

    sock_addr.sin_len = sizeof(sock_addr);
    sock_addr.sin_family = AF_INET;
    sock_addr.sin_port = htons(port);
    sock_addr.sin_addr.s_addr = target.m32;
#endif

    // src
    memcpy(cur, src, EXT_ADDR_SIZE);
    cur += EXT_ADDR_SIZE;

    // dst
    memcpy(cur, dst, EXT_ADDR_SIZE);
    cur += EXT_ADDR_SIZE;

    // cmd
    *cur++ = cmd;

    // payload
    memcpy(cur, payload, length);

    ret = lwip_sendto(socket, buffer, len, 0, (struct sockaddr *)&sock_addr,
                      sizeof(sock_addr));
    ur_mem_free(buffer, len);
    return ret;
    // FIXME: need to consider the situation without ip
}

int socket_init(udp_handler_t handler)
{
#if LWIP_IPV6
    struct sockaddr_in6 addr;
    int domain = AF_INET6;

    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(ID2_LISTEN_PORT);
#else
    struct sockaddr_in addr;
    int domain = AF_INET;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(ID2_LISTEN_PORT);
#endif

    g_sock.socket = lwip_socket(domain, SOCK_DGRAM, IPPROTO_UDP);
    lwip_bind(g_sock.socket, (const struct sockaddr *)&addr, sizeof(addr));
    g_sock.handler = handler;
#ifdef WITH_LWIP
    aos_poll_read_fd(g_sock.socket, socket_read_cb, &g_sock);
#endif

    return g_sock.socket;
}
