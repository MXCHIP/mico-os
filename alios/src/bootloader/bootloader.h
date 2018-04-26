/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef __BOOTLOADER_H__
#define __BOOTLOADER_H__

#ifdef __cplusplus
extern "C" {
#endif

#define STDIO_BREAK_TO_MENU 0

#define Bootloader_REVISION "v3.0"

void bootloader_start_app( uint32_t app_addr );

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif

