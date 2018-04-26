/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef UR_AUTH_DOT1X_H
#define UR_AUTH_DOT1X_H

#include "core/topology.h"
#include "hal/interface_context.h"
#include "umesh_types.h"
#include "tfs.h"

enum {
    TFS_CHALLENGE_LEN = 32,
    TFS_AUTH_CODE_LEN = 100,
};

typedef struct eapol_header_s {
    uint8_t version;
    uint8_t type;
    uint16_t length;
} eapol_header_t;

#define EAPOL_VERSION 2

#define EAPOL_EAP    0
#define EAPOL_START  1
#define EAPOL_LOGOFF 2
#define EAPOL_KEY    3
#define EAPOL_ALERT  4

typedef struct auth_context_s {
    bool is_auth_enable;
    bool is_auth_busy;
    bool is_auth_success;

    uint8_t id2_challenge[TFS_CHALLENGE_LEN];
    uint8_t local_id2[TFS_ID2_LEN+1];
    uint32_t local_id2_len;
    uint8_t peer_id2[TFS_ID2_LEN+1];
    uint8_t auth_code[TFS_AUTH_CODE_LEN];
    uint32_t auth_code_len;
    ur_addr_t peer;
    bool peer_auth_result;

    neighbor_t *auth_candidate;
    auth_state_t auth_state;
    ur_timer_t auth_timer;
    uint8_t auth_retry_times;
    auth_handler_t auth_handler;
} auth_context_t;

typedef struct dot1x_context_s {
    uint8_t identifier;
    uint8_t request[256];
    uint8_t response[256];
    neighbor_t *authenticator;
    auth_context_t auth_context;
} dot1x_context_t;

dot1x_context_t *get_dot1x_context(void);
ur_error_t handle_eapol(message_t *message);
ur_error_t send_eapol_start(dot1x_context_t *context);

#endif /* UR_AUTH_DOT1X_H */
