/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef UR_MCAST_H
#define UR_MCAST_H

#include "hal/interface_context.h"

typedef struct mcast_header_s {
    uint8_t dispatch;
    uint8_t control;
    uint8_t subnetid;
    uint16_t sid;
    uint8_t sequence;
} __attribute__((packed)) mcast_header_t;

void mcast_init(void);
ur_error_t insert_mcast_header(uint8_t *message);
ur_error_t process_mcast_header(uint8_t *message);

#endif  /* UR_MCAST_H */
