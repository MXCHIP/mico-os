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
#include "include.h"
#include "rtos_pub.h"
#include "BkDriverUart.h"
#include "drv_model_pub.h"
#include "uart_pub.h"
#include "ll.h"

typedef struct
{
    UINT8 user_buffer_flag;
    ring_buffer_t *rx_buffer;
} UART_UI_ST;

static UART_UI_ST uart_st = {.user_buffer_flag = 0, .rx_buffer = NULL};

OSStatus bk_uart_initialize( bk_uart_t uart, const bk_uart_config_t *config, ring_buffer_t *optional_rx_buffer )
{
    UINT32 ret;
    UINT32 status;
    DD_HANDLE uart_hdl;

    uart_hdl = ddev_open(UART_DEV_NAME, &status, 0);
    ASSERT(DRV_FAILURE != uart_hdl);
    if(optional_rx_buffer == NULL)
    {
        uart_st.rx_buffer = NULL;
        uart_st.user_buffer_flag = 0;
    }
    else
    {
        uart_st.rx_buffer = optional_rx_buffer;
        uart_st.user_buffer_flag = 1;
    }
    ret = ddev_control(uart_hdl, CMD_UART_INIT, config);
}

#ifdef STDIO_UART
OSStatus bk_stdio_uart_initialize( const bk_uart_config_t *config, ring_buffer_t *optional_rx_buffer )
{
    return kNoErr;
}
#endif // STDIO_UART

OSStatus bk_uart_finalize( bk_uart_t uart )
{
    return kNoErr;
}

OSStatus bk_uart_send( bk_uart_t uart, const void *data, uint32_t size )
{
    bk_send_string(data);

    return kNoErr;
}

OSStatus bk_uart_recv( bk_uart_t uart, void *data, uint32_t size, uint32_t timeout )
{
    UINT32 ret;
    UINT32 status;
    DD_HANDLE uart_hdl;

    uart_hdl = ddev_open(UART_DEV_NAME, &status, 0);
    ASSERT(DRV_FAILURE != uart_hdl);
    ret = ddev_control(uart_hdl, CMD_RX_COUNT, 0);
    if(ret < size)
    {
        return kGeneralErr;
    }

    ret = ddev_read(uart_hdl, data, size, 0);
    ASSERT(size == ret);

    return kNoErr;
}

OSStatus bk_uart_recv_prefetch( bk_uart_t uart, void *data, uint32_t size, uint32_t timeout )
{
    UINT32 ret;
    UINT32 status;
    DD_HANDLE uart_hdl;
    UART_PEEK_RX_T peek;

    uart_hdl = ddev_open(UART_DEV_NAME, &status, 0);
    ASSERT(DRV_FAILURE != uart_hdl);

    peek.sig = URX_PEEK_SIG;
    peek.ptr = data;
    peek.len = size;
    ret = ddev_control(uart_hdl, CMD_RX_PEEK, &peek);
    ASSERT(size == ret);    

    return kNoErr;
}

uint32_t bk_uart_get_length_in_buffer( bk_uart_t uart )
{
    uint32_t length;
    UINT32 status;
    DD_HANDLE uart_hdl;

    uart_hdl = ddev_open(UART_DEV_NAME, &status, 0);
    ASSERT(DRV_FAILURE != uart_hdl);
    length = (uint32_t)ddev_control(uart_hdl, CMD_RX_COUNT, 0);

    return length;
}

// eof

