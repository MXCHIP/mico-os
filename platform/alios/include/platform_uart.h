/**
 ******************************************************************************
 * @file    platform_uart.h
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



#ifndef __PLATFORM_UART_H__
#define __PLATFORM_UART_H__

#include "RingBufferUtils.h"


#ifdef __cplusplus
extern "C" {
#endif


/******************************************************
 *                 Type Definitions
 ******************************************************/

/**
 * UART data width
 */
typedef enum {
    DATA_WIDTH_5BIT,
    DATA_WIDTH_6BIT,
    DATA_WIDTH_7BIT,
    DATA_WIDTH_8BIT,
    DATA_WIDTH_9BIT
} platform_uart_data_width_t;

/**
 * UART stop bits
 */
typedef enum {
    STOP_BITS_1,
    STOP_BITS_2,
} platform_uart_stop_bits_t;

/**
 * UART flow control
 */
typedef enum {
    FLOW_CONTROL_DISABLED,
    FLOW_CONTROL_CTS,
    FLOW_CONTROL_RTS,
    FLOW_CONTROL_CTS_RTS
} platform_uart_flow_control_t;

/**
 * UART parity
 */
typedef enum {
    NO_PARITY,
    ODD_PARITY,
    EVEN_PARITY,
} platform_uart_parity_t;

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



#ifdef __cplusplus
} /*"C" */
#endif

#endif





