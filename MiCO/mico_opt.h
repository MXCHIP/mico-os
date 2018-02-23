/**
 ******************************************************************************
 * @file    mico_opt.h
 * @author  William Xu
 * @version V1.0.0
 * @date    22-July-2015
 * @brief   This file provide MiCO default configurations
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#ifndef __MICO_OPT_H
#define __MICO_OPT_H

#ifndef MICO_PREBUILT_LIBS
#include "mico_config.h"
#include "mico_board_conf.h"
#endif


#ifdef __cplusplus
extern "C" {
#endif

/******************************************************************************
 *                            MiCO OS and APP VERSION
 ******************************************************************************/

#if !defined APP_INFO
#define APP_INFO                                 "MiCO BASIC Demo"
#endif

#if !defined FIRMWARE_REVISION
#define FIRMWARE_REVISION                       "MICO_BASIC_1_0"
#endif

#if !defined MANUFACTURER
#define MANUFACTURER                            "MXCHIP Inc."
#endif

#if !defined SERIAL_NUMBER
#define SERIAL_NUMBER                           "20170101"
#endif

#if !defined PROTOCOL
#define PROTOCOL                                "com.mxchip.basic"
#endif

/**
 *  MiCO_SDK_VERSION_XXX, should be defined in MakeFile
 */
#ifndef MiCO_SDK_VERSION_MAJOR
#define MiCO_SDK_VERSION_MAJOR                  (4)
#endif

#ifndef MiCO_SDK_VERSION_MINOR
#define MiCO_SDK_VERSION_MINOR                  (0)
#endif

#ifndef MiCO_SDK_VERSION_REVISION
#define MiCO_SDK_VERSION_REVISION               (0)
#endif

/******************************************************************************
 *                             MiCO Debug Enabler
 ******************************************************************************/

#if !defined MICO_DEBUG_MIN_LEVEL
#define MICO_DEBUG_MIN_LEVEL                    MICO_DEBUG_LEVEL_ALL
#endif

#if !defined MICO_DEBUG_TYPES_ON
#define MICO_DEBUG_TYPES_ON                     MICO_DEBUG_ON
#endif

/******************************************************************************
 *                             MiCO Main Application
 ******************************************************************************/

/**
 *  MICO_DEFAULT_APPLICATION_STACK_SIZE: Application thread stack size, Default: 1500 bytes
 */
#if !defined MICO_DEFAULT_APPLICATION_STACK_SIZE
#define MICO_DEFAULT_APPLICATION_STACK_SIZE     1500
#endif

/**
 *  Do some MiCO initializing before main, like mico_board_init, stdio uart init...
 */
#if !defined MICO_APPLICATION
#define MICO_APPLICATION                        1
#endif

/**
 *  Start standard QC test function other than application
 */
#if !defined MICO_QUALITY_CONTROL_ENABLE
#define MICO_QUALITY_CONTROL_ENABLE             0
#endif

/******************************************************************************
 *                             Wlan Configuration
 ******************************************************************************/

#define CONFIG_MODE_NONE                        (1)
#define CONFIG_MODE_USER                        (2)
#define CONFIG_MODE_WAC                         (3)
#define CONFIG_MODE_EASYLINK                    (4)
#define CONFIG_MODE_EASYLINK_WITH_SOFTAP        (4)  //Legacy definition, not supported any more
#define CONFIG_MODE_SOFTAP                      (5)
#define CONFIG_MODE_MONITOR                     (6)
#define CONFIG_MODE_MONITOR_EASYLINK            (7)
#define CONFIG_MODE_WPS                         (8)
#define CONFIG_MODE_AWS                         (9)

/**
 *  MICO_WLAN_CONFIG_MODE: wlan configuration mode, Default: EasyLink
 */
#if !defined MICO_WLAN_CONFIG_MODE
#define MICO_WLAN_CONFIG_MODE                   CONFIG_MODE_AWS
#endif

#if MICO_WLAN_CONFIG_MODE == CONFIG_MODE_WAC
#define EasyLink_Needs_Reboot
#endif

#if !defined MICO_WLAN_FORCE_OTA_ENABLE
#define MICO_WLAN_FORCE_OTA_ENABLE               1
#endif

#if !defined MICO_WLAN_AUTO_CONFIG
#define MICO_WLAN_AUTO_CONFIG                    1
#endif


#if !defined MICO_WLAN_AUTO_SOFTAP_WHEN_DISCONNECTED
#define MICO_WLAN_AUTO_SOFTAP_WHEN_DISCONNECTED  0
#endif

/**
 *  EasyLink_TimeOut: Easylink configuration timeout, Default: 60 secs
 */
#if !defined EasyLink_TimeOut
#define EasyLink_TimeOut                        60000
#endif

/**
 *  EasyLink_ConnectWlan_Timeout: Connect to wlan after wlan is configured
 *  Restart wlan configuration mode after timeout. Default: 20 seconds.
 */
#if !defined EasyLink_ConnectWlan_Timeout
#define EasyLink_ConnectWlan_Timeout            20000 
#endif

/******************************************************************************
 *                             TCPIP Stack Options
 ******************************************************************************/

/**
 *  MICO_CONFIG_IP_VER_PREF: On dual stack configuration how long wait for preferred stack
 *  4 or 6
 */

#if !defined MICO_CONFIG_IP_VER_PREF
#define MICO_CONFIG_IP_VER_PREF                4
#endif

/**
 *  MICO_CONFIG_IPV6: Enable IPv4 and IPv6 dual stack apis, Default: disabled
 */
#if !defined MICO_CONFIG_IPV6
#define MICO_CONFIG_IPV6                        0
#endif

/**
 *  MICO_IPV6_NUM_ADDRESSES: Number of IPv6 addresses per interface. Default: 3
 */
#if !defined MICO_IPV6_NUM_ADDRESSES
#define MICO_IPV6_NUM_ADDRESSES                 3
#endif

/******************************************************************************
 *        MiCO System Functions, established by mico_system_init()
 ******************************************************************************/

 /**
 *  MICO_WLAN_CONNECTION_ENABLE: Start wlan connection when MiCO system starts, 
 *  Default: Enable
 */
#if !defined MICO_WLAN_CONNECTION_ENABLE
#define MICO_WLAN_CONNECTION_ENABLE             1
#endif

/**
 *  MICO_CONFIG_EASYLINK_BTN_ENABLE: Enable EasyLink Button,
 *  - Press to start easylink
 *  - Long pressed  @ref MICO_CONFIG_EASYLINK_BTN_LONG_PRESS_TIMEOUT milliseconds
 *    to clear all settings
 *  Default: Enable
 */
#if !defined MICO_CONFIG_EASYLINK_BTN_ENABLE
#define MICO_CONFIG_EASYLINK_BTN_ENABLE         1
#endif

#if !defined MICO_CONFIG_EASYLINK_BTN_IDLE_STATE
#define MICO_CONFIG_EASYLINK_BTN_IDLE_STATE     1
#endif


#if !defined MICO_CONFIG_EASYLINK_BTN_LONG_PRESS_TIMEOUT
#define MICO_CONFIG_EASYLINK_BTN_LONG_PRESS_TIMEOUT         5000
#endif

/**
 * Command line interface
 */
#if !defined MICO_CLI_ENABLE
#define MICO_CLI_ENABLE                         1
#endif

/**
 * Start a system monitor daemon, application can register some monitor
 * points, If one of these points is not executed in a predefined period,
 * a watchdog reset will occur.
 */
#if !defined MICO_SYSTEM_MONITOR_ENABLE
#define MICO_SYSTEM_MONITOR_ENABLE              1
#endif

/**
 * Add service _easylink._tcp._local. for discovery
 */
#if !defined MICO_SYSTEM_DISCOVERY_ENABLE
#define MICO_SYSTEM_DISCOVERY_ENABLE              1
#endif

/**
 * _easylink._tcp._local. service port
 */
#if !defined MICO_SYSTEM_DISCOVERY_PORT
#define MICO_SYSTEM_DISCOVERY_PORT               8000
#endif

/**
  * MiCO TCP server used for configuration and ota.
  */
#if !defined MICO_CONFIG_SERVER_ENABLE
#define MICO_CONFIG_SERVER_ENABLE                0
#endif

#if !defined MICO_CONFIG_SERVER_PORT
#define MICO_CONFIG_SERVER_PORT                 8000
#endif

#if !defined MICO_CONFIG_SERVER_REPORT_SYSTEM_DATA
#define MICO_CONFIG_SERVER_REPORT_SYSTEM_DATA   MICO_CONFIG_SERVER_ENABLE
#endif

/******************************************************************************
 *                            Debug and Log
 ******************************************************************************/

#if !defined CONFIG_APP_DEBUG
#define CONFIG_APP_DEBUG                       MICO_DEBUG_ON
#endif

#if !defined CONFIG_SYSTEM_DEBUG
#define CONFIG_SYSTEM_DEBUG                    MICO_DEBUG_ON
#endif

#if !defined CONFIG_MDNS_DEBUG
#define CONFIG_MDNS_DEBUG                      MICO_DEBUG_OFF
#endif

#if !defined CONFIG_LWS_DEBUG
#define CONFIG_LWS_DEBUG                       MICO_DEBUG_OFF
#endif

#if !defined CONFIG_ETH_DEBUG
#define CONFIG_ETH_DEBUG                       MICO_DEBUG_OFF
#endif

#if !defined CONFIG_FORCTOTA_DEBUG
#define CONFIG_FORCTOTA_DEBUG                  MICO_DEBUG_OFF
#endif



/******************************************************************************
 *                            Platform
 ******************************************************************************/

#if !defined PLATFORM_ETH_ENABLE
#define PLATFORM_ETH_ENABLE                                0
#endif

#if !defined PLATFORM_CONFIG_EASYLINK_SOFTAP_COEXISTENCE
#define PLATFORM_CONFIG_EASYLINK_SOFTAP_COEXISTENCE        0
#endif

#ifdef __cplusplus
} /*extern "C" */
#endif

#endif //__MICO_OPT_H
