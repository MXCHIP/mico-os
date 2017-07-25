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
#endif

#include "platform_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MiCO_SDK_VERSION_MAJOR
#define MiCO_SDK_VERSION_MAJOR      (3)
#endif

#ifndef MiCO_SDK_VERSION_MINOR
#define MiCO_SDK_VERSION_MINOR      (5)
#endif

#ifndef MiCO_SDK_VERSION_REVISION
#define MiCO_SDK_VERSION_REVISION   (1)
#endif

/* Build-in wlan configuration functions */
#define CONFIG_MODE_NONE                        (1)
#define CONFIG_MODE_USER                        (2)
#define CONFIG_MODE_WAC                         (3)
#define CONFIG_MODE_EASYLINK                    (4)
#define CONFIG_MODE_EASYLINK_WITH_SOFTAP        (4)  //Legacy definition, not supported any more
#define CONFIG_MODE_SOFTAP                      (5)
#define CONFIG_MODE_MONITOR                     (6)
#define CONFIG_MODE_MONITOR_EASYLINK            (7)

#if MICO_WLAN_CONFIG_MODE == CONFIG_MODE_WAC || MICO_WLAN_CONFIG_MODE == CONFIG_MODE_AWS
#define EasyLink_Needs_Reboot
#endif


#if !defined MICO_DEBUG_MIN_LEVEL
#define MICO_DEBUG_MIN_LEVEL              MICO_DEBUG_LEVEL_ALL
#endif

#if !defined MICO_DEBUG_TYPES_ON
#define MICO_DEBUG_TYPES_ON               MICO_DEBUG_ON
#endif

#if !defined MICO_MDNS_DEBUG
#define MICO_MDNS_DEBUG                   MICO_DEBUG_OFF
#endif


/* For legacy definition */
#ifndef MICO_WLAN_CONFIG_MODE
#define MICO_WLAN_CONFIG_MODE     MICO_CONFIG_MODE
#endif

/**
 * MICO_IPV6_NUM_ADDRESSES: Number of IPv6 addresses per interface.
 */
#if !defined MICO_CONFIG_IPV6
#define MICO_CONFIG_IPV6                0
#endif

/**
 * MICO_IPV6_NUM_ADDRESSES: Number of IPv6 addresses per interface.
 */
#if !defined MICO_IPV6_NUM_ADDRESSES
#define MICO_IPV6_NUM_ADDRESSES         3
#endif

#ifdef __cplusplus
} /*extern "C" */
#endif

#endif //__MICO_OPT_H
