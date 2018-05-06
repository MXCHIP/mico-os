/**
 ******************************************************************************
 * @file    mico_main.c
 * @author  William Xu
 * @version V1.0.0
 * @date    22-Aug-2016
 * @brief   MiCO initialize before main application
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 *
 ******************************************************************************
 */

/** @file
 *
 */

#include <string.h>
#include <stdlib.h>

#include "mico.h"
#include "mico_board_conf.h"

#include "mico_rtos_common.h"

#ifndef ALIOS_SUPPORT
#if MICO_QUALITY_CONTROL_ENABLE
#include "qc_test.h"
#endif
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

#ifndef STDIO_BUFFER_SIZE
#define STDIO_BUFFER_SIZE   64
#endif

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
 *               Static Function Declarations
 ******************************************************/

/******************************************************
 *               Variable Definitions
 ******************************************************/


#ifndef MICO_DISABLE_STDIO

#ifndef ALIOS_SUPPORT
static const mico_uart_config_t stdio_uart_config =
{
  .baud_rate    = MICO_STDIO_UART_BAUDRATE,
  .data_width   = DATA_WIDTH_8BIT,
  .parity       = NO_PARITY,
  .stop_bits    = STOP_BITS_1,
  .flow_control = FLOW_CONTROL_DISABLED,
  .flags        = 0,
};

static volatile ring_buffer_t stdio_rx_buffer;
static volatile uint8_t       stdio_rx_data[STDIO_BUFFER_SIZE];
#endif /* #ifndef ALIOS_SUPPORT */

mico_mutex_t MICO_WEAK stdio_tx_mutex = NULL;
#endif /* #ifndef MICO_DISABLE_STDIO */


extern int mico_debug_enabled;

/******************************************************
 *               Function Definitions
 ******************************************************/

/* mico_main is executed after rtos is start up and before real main*/
void mico_main( void )
{
#if MICO_APPLICATION

#ifdef DEBUG
    mico_debug_enabled = 1;
#else
    mico_debug_enabled = 0;
#endif

    /* Customized board configuration. */
    mico_board_init( );

#ifndef MICO_DISABLE_STDIO
    if( stdio_tx_mutex == NULL )
        mico_rtos_init_mutex( &stdio_tx_mutex );

#ifndef ALIOS_SUPPORT
    ring_buffer_init( (ring_buffer_t*) &stdio_rx_buffer, (uint8_t*) stdio_rx_data, STDIO_BUFFER_SIZE );
    mico_stdio_uart_init( &stdio_uart_config, (ring_buffer_t*) &stdio_rx_buffer );
#endif

#endif

    mico_rtos_init( );

#ifndef ALIOS_SUPPORT
#if MICO_QUALITY_CONTROL_ENABLE
#ifndef RTOS_mocOS
    if ( MicoShouldEnterMFGMode( ) ) {
        mico_system_qc_test( );
        mico_rtos_delete_thread(NULL);
        mico_rtos_thread_yield();
    }
#endif
#endif
#endif

#endif
}

