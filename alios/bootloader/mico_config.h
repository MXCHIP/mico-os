/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#pragma once

#define APP_INFO   "Bootloader"

#define FIRMWARE_REVISION   "MICO_BASIC_1_0"
#define MANUFACTURER        "MXCHIP Inc."
#define SERIAL_NUMBER       "20140606"
#define PROTOCOL            "com.mxchip.basic"

#define CONFIG_MODE_EASYLINK                    (1)
#define CONFIG_MODE_SOFT_AP                     (2)
#define CONFIG_MODE_EASYLINK_WITH_SOFTAP        (3)
#define CONFIG_MODE_WAC                         (4)

/************************************************************************
 * Application thread stack size */
#define MICO_DEFAULT_APPLICATION_STACK_SIZE         (1500)

/************************************************************************
 * Enable wlan connection, start easylink configuration if no wlan settings are existed */
//#define MICO_WLAN_CONNECTION_ENABLE

#define MICO_WLAN_CONFIG_MODE CONFIG_MODE_EASYLINK_WITH_SOFTAP

#define EasyLink_TimeOut                60000 /**< EasyLink timeout 60 seconds. */

#define EasyLink_ConnectWlan_Timeout    20000 /**< Connect to wlan after configured by easylink.
                                                   Restart easylink after timeout: 20 seconds. */

/************************************************************************
 * Device enter MFG mode if MICO settings are erased. */
//#define MFG_MODE_AUTO

/************************************************************************
 * Command line interface */
#define MICO_CLI_ENABLE

/************************************************************************
 * Start a system monitor daemon, application can register some monitor
 * points, If one of these points is not executed in a predefined period,
 * a watchdog reset will occur. */
#define MICO_SYSTEM_MONITOR_ENABLE

/************************************************************************
 * MiCO TCP server used for configuration and ota. */
//#define MICO_CONFIG_SERVER_ENABLE
#define MICO_CONFIG_SERVER_PORT    8000
