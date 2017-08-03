/**
 ******************************************************************************
 * @file    mdns_opt.h
 * @author  William Xu
 * @version V1.0.0
 * @date    3-August-2017
 * @brief   This file provide mdns default configurations
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#ifndef __MDNS_OPT_H
#define __MDNS_OPT_H

#include "mico_opt.h"

#ifdef __cplusplus
extern "C" {
#endif

#if !defined MDNS_DEBUG
#define MDNS_DEBUG                   MICO_DEBUG_OFF
#endif

#if !defined CONFIG_MDNS_MAX_SERVICE_ANNOUNCE
#define CONFIG_MDNS_MAX_SERVICE_ANNOUNCE       3
#endif

#if !defined MDNS_QUERY_API
#define MDNS_QUERY_API                         1
#endif

#if !defined CONFIG_MDNS_MAX_SERVICE_MONITORS
#define CONFIG_MDNS_MAX_SERVICE_MONITORS       1
#endif

#if !defined CONFIG_MDNS_SERVICE_CACHE_SIZE
#define CONFIG_MDNS_SERVICE_CACHE_SIZE         2
#endif



#ifdef __cplusplus
} /*extern "C" */
#endif

#endif //__MICO_OPT_H
