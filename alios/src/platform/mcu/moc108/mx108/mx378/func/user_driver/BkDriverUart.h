/**
 ******************************************************************************
 * @file    BkDriverUart.h
 * @brief   This file provides all the headers of UART operation functions.
 ******************************************************************************
 *
 *  The MIT License
 *  Copyright (c) 2017 BEKEN Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 */


#ifndef __BEKENDRIVERUART_H__
#define __BEKENDRIVERUART_H__

#pragma once
#include "include.h"

/** @addtogroup BK_PLATFORM
* @{
*/

/** @defgroup BK_UART MICO UART Driver
* @brief  Universal Asynchronous Receiver Transmitter (UART) Functions
* @{
*/
	
/******************************************************
 *					 Enumerations
 ******************************************************/
typedef enum
{
	BK_UART_1,
	BK_UART_MAX, /* Denotes the total number of UART port aliases. Not a valid UART alias */
	BK_UART_NONE,
} bk_uart_t;

/******************************************************
 *					  Structures
 ******************************************************/
/******************************************************
 *                 Type Definitions
 ******************************************************/
/**
 * UART data width
 */
typedef enum
{
    DATA_WIDTH_5BIT,
    DATA_WIDTH_6BIT,
    DATA_WIDTH_7BIT,
    DATA_WIDTH_8BIT,
    DATA_WIDTH_9BIT
} platform_uart_data_width_t;

/**
 * UART stop bits
 */
typedef enum
{
    STOP_BITS_1,
    STOP_BITS_2,
} platform_uart_stop_bits_t;

/**
 * UART flow control
 */
typedef enum
{
    FLOW_CONTROL_DISABLED,
    FLOW_CONTROL_CTS,
    FLOW_CONTROL_RTS,
    FLOW_CONTROL_CTS_RTS
} platform_uart_flow_control_t;

/**
 * UART parity
 */
typedef enum
{
    NO_PARITY,
    ODD_PARITY,
    EVEN_PARITY,
} platform_uart_parity_t;

typedef struct
{
    uint32_t					  baud_rate;
    platform_uart_data_width_t    data_width;
    platform_uart_parity_t 	      parity;
    platform_uart_stop_bits_t	  stop_bits;
    platform_uart_flow_control_t  flow_control;
    uint8_t					      flags;	 /**< if set, UART can wake up MCU from stop mode, reference: @ref UART_WAKEUP_DISABLE and @ref UART_WAKEUP_ENABLE*/
} platform_uart_config_t;

typedef struct
{
    uint32_t  size;
    uint32_t  head;
    uint32_t  tail;
    uint8_t  *buffer;
} ring_buffer_t;

typedef platform_uart_config_t          bk_uart_config_t;

/******************************************************
 *                 Function Declarations
 ******************************************************/
/**@brief Initialises a UART interface
 *
 * @note Prepares an UART hardware interface for communications
 *
 * @param  uart     : the interface which should be initialised
 * @param  config   : UART configuration structure
 * @param  optional_rx_buffer : Pointer to an optional RX ring buffer
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus bk_uart_initialize( bk_uart_t uart, const bk_uart_config_t *config, ring_buffer_t *optional_rx_buffer );


/**@brief Initialises a STDIO UART interface, internal use only
 *
 * @note Prepares an UART hardware interface for stdio communications
 *
 * @param  config   : UART configuration structure
 * @param  optional_rx_buffer : Pointer to an optional RX ring buffer
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
#ifdef STDIO_UART
OSStatus bk_stdio_uart_initialize( const bk_uart_config_t *config, ring_buffer_t *optional_rx_buffer );
#endif // STDIO_UART

/**@brief Deinitialises a UART interface
 *
 * @param  uart : the interface which should be deinitialised
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus bk_uart_finalize( bk_uart_t uart );


/**@brief Transmit data on a UART interface
 *
 * @param  uart     : the UART interface
 * @param  data     : pointer to the start of data
 * @param  size     : number of bytes to transmit
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus bk_uart_send( bk_uart_t uart, const void *data, uint32_t size );


/**@brief Receive data on a UART interface
 *
 * @param  uart     : the UART interface
 * @param  data     : pointer to the buffer which will store incoming data
 * @param  size     : number of bytes to receive
 * @param  timeout  : timeout in milisecond
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus bk_uart_recv( bk_uart_t uart, void *data, uint32_t size, uint32_t timeout );
OSStatus bk_uart_recv_prefetch( bk_uart_t uart, void *data, uint32_t size, uint32_t timeout );

/**@brief Read the length of the data that is already recived by uart driver and stored in buffer
 *
 * @param uart     : the UART interface
 *
 * @return    Data length
 */
uint32_t bk_uart_get_length_in_buffer( bk_uart_t uart );

/** @} */
/** @} */

#endif
