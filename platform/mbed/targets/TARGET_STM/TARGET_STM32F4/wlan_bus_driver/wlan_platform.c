/**
 ******************************************************************************
 * @file    wlan_platform.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide functions called by MICO to wlan RF module
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico_board_conf.h"
#include "platform_peripheral.h"
#include "platform_logging.h"
#include "wlan_platform_common.h"

#if defined ( MICO_USE_WIFI_32K_CLOCK_MCO ) && defined ( MICO_USE_WIFI_32K_PIN )
#include "device.h"
#include "pinmap.h"
#include "PeripheralPins.h"
#include "PeripheralPins_Extra.h"
#endif

/* Used to give a 32k clock to EMW1062 wifi rf module */
OSStatus host_platform_init_wlan_powersave_clock( void )
{
#if defined ( MICO_USE_WIFI_32K_CLOCK_MCO ) && defined ( MICO_USE_WIFI_32K_PIN )
    pinmap_pinout( wifi_control_pins[ WIFI_PIN_32K_CLK ].mbed_pin, PinMap_MCO );
    /* enable LSE output on MCO1 */
    HAL_RCC_MCOConfig(RCC_MCO1, RCC_MCO1SOURCE_LSE, RCC_MCODIV_1);
    return kNoErr;
#elif defined ( MICO_USE_WIFI_32K_PIN )
    return host_platform_deinit_wlan_powersave_clock( );
#else
    return kNoErr;
#endif
}



