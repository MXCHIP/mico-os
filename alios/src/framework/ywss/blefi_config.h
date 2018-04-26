/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef _blefi_config_h_
#define _blefi_config_h_

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

#include <stdint.h>
#include <stdbool.h>

enum alink_config_t {
    ALINK_CONFIG_NONE = 0x00,      // invalide configure
    ALINK_CONFIG_SSID_CMD = 0x01,  // configure SSID of AP
    ALINK_CONFIG_PASSWD_CMD,       // configure PASSWD of AP
    ALINK_CONFIG_BSSID_CMD,        // configure BSSID of AP
    ALINK_CONFIG_CMD_CNT,
};

typedef struct ali_dev_config {
    uint8_t ctyp;
    uint8_t clen;
    uint8_t cval[0];
} ali_dev_config_t;

typedef void (* config_handler_t)(void *cmd);

typedef struct ali_dev_config_ctx {
    bool init;
    enum alink_config_t ctyp[ALINK_CONFIG_CMD_CNT];
    config_handler_t handler[ALINK_CONFIG_CMD_CNT];
} ali_dev_config_ctx_t;

void ali_blefi_config_init();
void ali_blefi_config_deinit();
void ali_blefi_config_handler(void *buf, int length);

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif

#endif
