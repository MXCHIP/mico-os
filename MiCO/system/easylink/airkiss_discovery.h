/**
 ******************************************************************************
 * @file    airkiss_discovery.h
 * @author  William Xu
 * @version V1.0.0
 * @date    28-10-2015
 * @brief   Head file for WECHAT device discovery protocol
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

#pragma once

#include "mico.h"

#ifdef __cplusplus
extern "C" {
#endif

void airkiss_discovery_lib_version( uint8_t *major, uint8_t *minor, uint8_t *revision );

OSStatus airkiss_discovery_start( char *appid, char *deviceid );

OSStatus airkiss_discovery_stop( void );


#ifdef __cplusplus
} /*extern "C" */
#endif


