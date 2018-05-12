/**
 ******************************************************************************
 * @file    aos_generic_uart.c
 * @author  William Xu
 * @version V1.0.0
 * @date    30-Apr-2018
 * @brief   This file provide GPIO driver functions.
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2018 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#ifdef ALIOS_DEV_UART

#include "aos_uart.h"

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

/******************************************************
*               Function Declarations
******************************************************/

extern uint32_t hal_uart_get_length_in_buffer( uart_dev_t *uart );

/******************************************************
*               Function Definitions
******************************************************/

OSStatus platform_uart_init( platform_uart_driver_t* driver, const platform_uart_t* peripheral,
                             const platform_uart_config_t* config, ring_buffer_t* optional_ring_buffer )
{
    driver->port = peripheral->port;
    memcpy(&driver->config, config, sizeof(platform_uart_config_t));

    return (OSStatus) hal_uart_init(driver);
}


OSStatus platform_uart_deinit( platform_uart_driver_t* driver )
{
    return (OSStatus) hal_uart_finalize(driver);
}


OSStatus platform_uart_transmit_bytes( platform_uart_driver_t* driver, const uint8_t* data_out, uint32_t size )
{
    return hal_uart_send(driver, data_out, size, 0xFFFFFFFF);
}


OSStatus platform_uart_receive_bytes( platform_uart_driver_t* driver, uint8_t* data_in, uint32_t expected_data_size,
                                      uint32_t timeout_ms )
{
    uint32_t recv_size = 0;
    return hal_uart_recv_II(driver, data_in, expected_data_size, &recv_size, timeout_ms);
}


uint32_t platform_uart_get_length_in_buffer( platform_uart_driver_t* driver )
{
    return hal_uart_get_length_in_buffer( driver );
}

#endif

