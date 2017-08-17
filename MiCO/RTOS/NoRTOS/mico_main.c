/**
 ******************************************************************************
 * @file    rtos.c
 * @author  William Xu
 * @version V1.0.0
 * @date    25-Aug-2016
 * @brief   Definitions of the MiCO RTOS abstraction layer for the special case
 *          of having no RTOS
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 *
 ******************************************************************************
 */

#include "mico_common.h"
#include "mico_rtos.h"
#include "platform_peripheral.h"

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
 *               Function Declarations
 ******************************************************/
extern void mico_main(void);
extern int __real_main(void);

/******************************************************
 *               Variable Definitions
 ******************************************************/
static volatile uint32_t no_os_tick = 0;

/******************************************************
 *               Function Definitions
 ******************************************************/

void SysTick_Handler(void)
{
  no_os_tick ++;
  platform_watchdog_kick( );
}

mico_time_t mico_rtos_get_time(void)
{
    return no_os_tick;
}

int __wrap_main( void )
{
    mico_main();

    __real_main( );

    return 0;
}

