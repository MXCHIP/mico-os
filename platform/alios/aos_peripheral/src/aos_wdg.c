/**
 ******************************************************************************
 * @file    aos_generic_wdg.c
 * @author  William Xu
 * @version V1.0.0
 * @date    30-Apr-2018
 * @brief   This file provide watchdog driver functions.
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2018 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#ifdef ALIOS_DEV_WDG

#include "aos_wdg.h"

/******************************************************
*                    Constants
******************************************************/

/******************************************************
*                   Enumerations
******************************************************/

/******************************************************
*                 Type Definitions
******************************************************/

/******************************************************
*                    Structures
******************************************************/

/******************************************************
*               Variables Definitions
******************************************************/

wdg_dev_t aos_wdg;


/******************************************************
*               Function Declarations
******************************************************/

/******************************************************
*               Function Definitions
******************************************************/


OSStatus platform_watchdog_init( uint32_t timeout_ms )
{
    aos_wdg.port = 0;
    aos_wdg.config.timeout = timeout_ms;
    return hal_wdg_init(&aos_wdg);
}


OSStatus platform_watchdog_kick( void )
{
    hal_wdg_reload(&aos_wdg);
    return kNoErr;
}

#endif

