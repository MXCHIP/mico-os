/**
 ******************************************************************************
 * @file    platform_watchdog.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide WDG driver functions.
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico_board.h"
#include "mico_board_conf.h"
#include "platform_peripheral.h"

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
#ifndef MICO_DISABLE_WATCHDOG
//static __IO uint32_t LsiFreq = 0;
//static __IO uint32_t CaptureNumber = 0, PeriodValue = 0;
//static mico_semaphore_t  _measureLSIComplete_SEM = NULL;
//uint16_t tmpCC4[2] = {0, 0};
#endif

/******************************************************
 *               Function Declarations
 ******************************************************/
#ifndef MICO_DISABLE_WATCHDOG
//static uint32_t GetLSIFrequency(void);
#endif

/******************************************************
 *               Function Definitions
 ******************************************************/

OSStatus platform_watchdog_init( uint32_t timeout_ms )
{
// PLATFORM_TO_DO
#ifndef MICO_DISABLE_WATCHDOG
  OSStatus err = kNoErr;

  return err;
#else
  UNUSED_PARAMETER( timeout_ms );
  return kUnsupportedErr;
#endif
}

OSStatus platform_watchdog_deinit( void )
{
    // PLATFORM_TO_DO
    return kNoErr;
}

OSStatus platform_watchdog_kick( void )
{
#ifndef MICO_DISABLE_WATCHDOG
  return kNoErr;
#else
  return kUnsupportedErr;
#endif
}

bool platform_watchdog_check_last_reset( void )
{
    return false;
}
