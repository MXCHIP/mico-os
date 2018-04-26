/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef UR_BLE_HAL_H
#define UR_BLE_HAL_H

enum {
    BLE_DISCOVERY_TIMEOUT           = 500,    /* ms */
    BLE_AUTH_REQUEST_TIMEOUT        = 10000,  /* ms */
    BLE_AUTH_RELAY_TIMEOUT          = 10000,  /* ms */
    BLE_AUTH_RESPONSE_TIMEOUT       = 10000,  /* ms */
    BLE_LINK_QUALITY_MOBILE_TIMEOUT = 3000,   /* ms */
    BLE_LINK_QUALITY_TIMEOUT        = 30000,  /* ms */
    BLE_ADVERTISEMENT_TIMEOUT       = 20000,  /* ms, 20 seconds */
    BLE_NEIGHBOR_ALIVE_TIMEOUT      = 120000, /* ms, 2  mins */
    BLE_NOTIFICATION_TIMEOUT        = 300000, /* ms, 5 mins */
    BLE_ADDR_CACHE_ALIVE_TIMEOUT    = 10,
};

#endif  /* UR_BLE_HAL_H */
