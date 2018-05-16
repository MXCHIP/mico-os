/**
 ******************************************************************************
 * @file    platform_wdg.h
 * @author  William Xu
 * @version V1.0.0
 * @date    01-May-2018
 * @brief   This file provide mico platform driver header file
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2018 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */


/******************************************************
 *                      Macros
 ******************************************************/

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
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/



#ifndef __PLATFORM_WDG_H__
#define __PLATFORM_WDG_H__


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************
 *               Function Declarations
 ******************************************************/

/**
 * Initialize the watchdog
 *
 * @return @ref OSStatus
 */
OSStatus platform_watchdog_init( uint32_t timeout_ms );

/**
 * Refresh the watchdog
 *
 * @return @ref OSStatus
 */
OSStatus platform_watchdog_kick( void );


/**
 * Check if last reset occurred due to watchdog reset
 *
 * @return @ref OSStatus
 */
bool platform_watchdog_check_last_reset( void );


#ifdef __cplusplus
} /*"C" */
#endif

#endif





