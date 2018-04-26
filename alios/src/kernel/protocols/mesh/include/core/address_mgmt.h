/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef UR_AR_H
#define UR_AR_H

#include <stdint.h>

#include "core/sid_allocator.h"
#include "core/mesh_forwarder.h"
#include "hal/interface_context.h"

enum {
    UR_MESH_ADDRESS_CACHE_SIZE = 8,

    ADDR_CACHE_CHECK_INTERVAL = 60000,  /* 1 mins */
};

typedef enum {
    AQ_STATE_INVALID,
    AQ_STATE_CACHED,
    AQ_STATE_QUERY,
} cache_state_t;

typedef struct address_cache_s {
    uint8_t uuid[EXT_ADDR_SIZE];
    uint16_t meshnetid;
    uint16_t sid;
    uint16_t attach_sid;
    uint16_t attach_netid;
    uint8_t timeout;
    uint8_t retry_timeout;
    cache_state_t state;
} address_cache_t;

enum {
    ADDRESS_QUERY_TIMEOUT             = 3,    /* seconds */
    ADDRESS_QUERY_RETRY_TIMEOUT       = 3,    /* seconds */
    ADDRESS_QUERY_STATE_UPDATE_PERIOD = 1000, /* ms */
};

enum {
    ATTACH_QUERY = 0,
    TARGET_QUERY = 1,
};

void address_mgmt_init(void);
ur_error_t address_resolve(message_t *message);
ur_error_t handle_address_query(message_t *message);
ur_error_t handle_address_query_response(message_t *message);
ur_error_t handle_address_notification(message_t *message);
ur_error_t handle_address_unreachable(message_t *message);

ur_error_t send_address_unreachable(ur_addr_t *dest, ur_addr_t *target);

ur_error_t update_address_cache(media_type_t type, ur_node_id_t *target,
                                ur_node_id_t *attach);

#endif  /* UR_AR_H */
