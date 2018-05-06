/**
 ******************************************************************************
 * @file    aos_uart.h
 * @author  William Xu
 * @version V1.0.0
 * @date    01-May-2018
 * @brief   This file provide platform wrapper for AliOS UART HAL
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



#ifndef __AOS_UART_H__
#define __AOS_UART_H__

#include <hal/soc/uart.h>
#include "RingBufferUtils.h"

#ifdef __cplusplus
extern "C" {
#endif


/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef struct{
    uint32_t place_holder;
} platform_uart_t;

typedef struct {
    uint32_t place_holder;
} platform_uart_driver_t;


/**
 * UART data width
 */
typedef hal_uart_data_width_t platform_uart_data_width_t;

/**
 * UART stop bits
 */
typedef hal_uart_stop_bits_t platform_uart_stop_bits_t;

/**
 * UART flow control
 */
typedef hal_uart_flow_control_t platform_uart_flow_control_t;

/**
 * UART parity
 */
typedef hal_uart_parity_t platform_uart_parity_t;


/**
 * UART configuration
 */
typedef struct
{
    uint32_t                        baud_rate;
    platform_uart_data_width_t      data_width;
    platform_uart_parity_t          parity;
    platform_uart_stop_bits_t       stop_bits;
    platform_uart_flow_control_t    flow_control;
    uint8_t                         flags;          /**< if set, UART can wake up MCU from stop mode, reference: @ref UART_WAKEUP_DISABLE and @ref UART_WAKEUP_ENABLE*/
} platform_uart_config_t;


/******************************************************
 *                    Structures
 ******************************************************/


/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

/**
 * Initialise the specified UART port
 *
 * @return @ref OSStatus
 */
OSStatus platform_uart_init( platform_uart_driver_t* driver, const platform_uart_t* peripheral,
                             const platform_uart_config_t* config, ring_buffer_t* optional_ring_buffer );

/**
 * Deinitialise the specified UART port
 *
 * @return @ref OSStatus
 */
OSStatus platform_uart_deinit( platform_uart_driver_t* driver );

/**
 * Transmit data over the specified UART port
 *
 * @return @ref OSStatus
 */
OSStatus platform_uart_transmit_bytes( platform_uart_driver_t* driver, const uint8_t* data_out, uint32_t size );

/**
 * Receive data over the specified UART port
 *
 * @return @ref OSStatus
 */
OSStatus platform_uart_receive_bytes( platform_uart_driver_t* driver, uint8_t* data_in, uint32_t expected_data_size,
                                      uint32_t timeout_ms );

/**
 * Get the received data length in ring buffer over the specified UART port
 *
 * @return
 */
uint32_t platform_uart_get_length_in_buffer( platform_uart_driver_t* driver );

#ifdef __cplusplus
} /*"C" */
#endif

#endif





