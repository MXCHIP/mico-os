/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#ifndef UR_AUTH_EAP_H
#define UR_AUTH_EAP_H

#include "core/auth_dot1x.h"

typedef struct eap_header_s {
    uint8_t code;
    uint8_t id;
    uint16_t length;
} eap_header_t;

// code
#define EAP_CODE_REQUEST  1
#define EAP_CODE_RESPONSE 2
#define EAP_CODE_SUCCESS  3
#define EAP_CODE_FAILURE  4

// type
#define EAP_TYPE_IDENTITY           1
#define EAP_TYPE_NOTIFICATION       2
#define EAP_TYPE_NAK                3
#define EAP_TYPE_MD5_CHALLENGE      4
#define EAP_TYPE_ONE_TIME_PASSWORD  5
#define EAP_TYPE_GENERIC_TOKEN_CARD 6
#define EAP_TYPE_TLS                13
#define EAP_TYPE_EXPANDED_TYPES     254
#define EAP_TYPE_EXPERIMENTAL       255

// EAP-ID2
#define EAP_ID2_START     1
#define EAP_ID2_IDENTITY  2
#define EAP_ID2_CHALLENGE 3
#define EAP_ID2_AUTH_CODE 4

typedef struct eap_id2_header_s {
    uint8_t oui[3];
    uint8_t type[4];
    uint8_t cmd;
}eap_id2_header_t;

ur_error_t send_eap_request(dot1x_context_t *context);
ur_error_t send_eap_response(dot1x_context_t *context);
ur_error_t send_eap_success(dot1x_context_t *context);
ur_error_t send_eap_failure(dot1x_context_t *context);

ur_error_t handle_eap_request(dot1x_context_t *context, message_t *message);
ur_error_t handle_eap_response(dot1x_context_t *context, message_t *message);
ur_error_t handle_eap_identity(dot1x_context_t *context, message_t *message);
ur_error_t handle_eap_success(dot1x_context_t *context, message_t *message);
ur_error_t handle_eap_failure(dot1x_context_t *context, message_t *message);

#endif
