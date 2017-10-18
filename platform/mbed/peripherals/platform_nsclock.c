/**
 ******************************************************************************
 * @file    platform_nsclock.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-Aug-2016
 * @brief   This file provide nanoseconds delay functions
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

/** @file
 *
 */

#include "us_ticker_api.h"

/******************************************************
 *                      Macros
 ******************************************************/


/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *               Variables Definitions
 ******************************************************/

/******************************************************
 *               Function Definitions
 ******************************************************/

uint64_t platform_get_nanosecond_clock_value( void )
{
    return 1000 * us_ticker_read();
}

void platform_deinit_nanosecond_clock( void )
{

}

void platform_reset_nanosecond_clock( void )
{
    us_ticker_init();
}

void platform_init_nanosecond_clock( void )
{
    us_ticker_init();
}

void platform_nanosecond_delay( uint64_t delayns )
{
    uint64_t current_delayns = 0, start = 0;

    start = platform_get_nanosecond_clock_value( );

    do
    {
        current_delayns = platform_get_nanosecond_clock_value( ) - start;
    }
    while ( current_delayns < delayns );
}

