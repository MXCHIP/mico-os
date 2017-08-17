/**
 ******************************************************************************
 * @file    platform_init.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide functions called by MICO to drive stm32f2xx
 *          platform: - e.g. power save, reboot, platform initialize
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico_rtos.h"

#include "mico_board.h"
#include "mico_board_conf.h"

#include "platform_peripheral.h"
#include "platform_logging.h"


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
*               Function Declarations
******************************************************/

extern OSStatus host_platform_init( void );

/******************************************************
*               Variables Definitions
******************************************************/
extern platform_uart_t platform_uart_peripherals[];
extern platform_uart_driver_t platform_uart_drivers[];


#ifndef MICO_DISABLE_STDIO
static const platform_uart_config_t stdio_uart_config =
{
  .baud_rate    = STDIO_UART_BAUDRATE,
  .data_width   = DATA_WIDTH_8BIT,
  .parity       = NO_PARITY,
  .stop_bits    = STOP_BITS_1,
  .flow_control = FLOW_CONTROL_DISABLED,
  .flags        = 0,
};

static volatile ring_buffer_t stdio_rx_buffer;
static volatile uint8_t             stdio_rx_data[STDIO_BUFFER_SIZE];
mico_mutex_t        stdio_rx_mutex;
mico_mutex_t        stdio_tx_mutex;
#endif /* #ifndef MICO_DISABLE_STDIO */


/******************************************************
*               Function Definitions
******************************************************/

/******************************************************
*            NO-OS Functions
******************************************************/

void mico_main( void )
{
  
#if !defined MICO_DISABLE_STDIO && !defined NO_MICO_RTOS
    mico_rtos_init_mutex( &stdio_tx_mutex );
    mico_rtos_unlock_mutex( &stdio_tx_mutex );
    mico_rtos_init_mutex( &stdio_rx_mutex );
    mico_rtos_unlock_mutex( &stdio_rx_mutex );
#endif

    mico_board_init( );

#ifdef BOOTLOADER
  return;
#endif
  
  /* Initialise RTC */
  //platform_rtc_init( );

#ifndef MICO_DISABLE_MCU_POWERSAVE
  /* Initialise MCU powersave */
  //platform_mcu_powersave_init( );
#endif /* ifndef MICO_DISABLE_MCU_POWERSAVE */

  platform_mcu_powersave_disable( );
}


//OSStatus stdio_hardfault( char* data, uint32_t size )
//{
//#ifndef MICO_DISABLE_STDIO
//  uint32_t idx;
//  for(idx = 0; idx < size; idx++){
//    while ( ( platform_uart_peripherals[ MICO_STDIO_UART ].port->SR & USART_SR_TXE ) == 0 );
//    platform_uart_peripherals[ MICO_STDIO_UART ].port->DR = (data[idx] & (uint16_t)0x01FF);
//
//  }
//#endif
//  return kNoErr;
//}
//





