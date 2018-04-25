/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef UR_AUTH_RELAY_H
#define UR_AUTH_RELAY_H

#include "hal/interface_context.h"
#include "core/topology.h"
#include "umesh_types.h"

/** sp socket payload format:
 *    8 bytes | 8 bytes | 1 byte  | N bytes
 *    src mac | dst mac | command | payload
 */

// command
enum {
    ID2_AUTH_REQUEST   = 1, // payload: id2
    ID2_AUTH_CHALLENGE = 2, // payload: challenge
    ID2_AUTH_CODE      = 3, // payload: auth code
    ID2_AUTH_RESULT    = 4, // payload: auth result
};

enum {
    ID2_AUTH_SOCK_FIXED_LEN = 17, // src + dst + cmd
};

typedef void (*udp_handler_t)(const uint8_t *payload, uint16_t length);

typedef struct socket_s {
    int socket;
    udp_handler_t handler;
} socket_t;

int socket_init(udp_handler_t handler);
int socket_sendmsg(int socket, const uint8_t *src, const uint8_t *dst,
                   int cmd, const uint8_t *payload, uint16_t length,
                   void *dest, uint16_t port);

#endif  /* UR_AUTH_RELAY_H */
