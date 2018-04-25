/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef UR_AUTH_MGMT_H
#define UR_AUTH_MGMT_H

#include "core/auth_dot1x.h"
#include "core/auth_eap.h"

bool is_auth_enabled(void);
bool is_auth_busy(void);
void auth_enable(void);
void auth_disable(void);
ur_error_t auth_init(void);
neighbor_t *get_auth_candidate(void);
auth_state_t get_auth_state(void);
void set_auth_state(auth_state_t state);
bool get_auth_result(void);

ur_error_t handle_auth_start(dot1x_context_t *context, const uint8_t *buf);
ur_error_t handle_auth_relay(dot1x_context_t *context, const uint8_t *buf, uint16_t len);

ur_error_t send_auth_start(dot1x_context_t *context);
ur_error_t start_auth(neighbor_t *nbr, auth_handler_t handler);
ur_error_t stop_auth(void);

#endif  /* UR_AUTH_MGMT_H */
