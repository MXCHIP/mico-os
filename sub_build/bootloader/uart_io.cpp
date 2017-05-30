/**
 ******************************************************************************
 * @file    menu.c
 * @author  William Xu
 * @version V2.0.0
 * @date    05-Oct-2014
 * @brief   his file provides the software which contains the main menu routine.
 *          The main menu gives the options of:
 *             - downloading a new binary file,
 *             - uploading internal flash memory,
 *             - executing the binary file already loaded
 ******************************************************************************
 *
 *  The MIT License
 *  Copyright (c) 2014 MXCHIP Inc.
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

/* Includes ------------------------------------------------------------------*/
#include "mico.h"
#include "bootloader.h"

#ifdef __MBED__
#include "mbed.h"
#endif


/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
extern const platform_uart_t            platform_uart_peripherals[];

#ifdef __MBED__
RawSerial boot_serial(platform_uart_peripherals[MICO_STDIO_UART].mbed_tx_pin,
                     platform_uart_peripherals[MICO_STDIO_UART].mbed_rx_pin,
                     STDIO_UART_BAUDRATE);
#endif

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/


void uart_putchar( int c )
{
#ifdef __MBED__
    boot_serial.putc(c);
#else
    MicoUartSend( MICO_STDIO_UART, &c, 1 );
#endif
}

int uart_getchar(char *inbuf, uint32_t timeout )
{
#ifdef __MBED__
    uint32_t start_time, cur_time;
    start_time = mico_rtos_get_time();

    while(1)
    {
        cur_time = mico_rtos_get_time( );
        if ( timeout != MICO_NEVER_TIMEOUT && (cur_time - start_time) > timeout )
            return kTimeoutErr;

        if( boot_serial.readable() ){
            *inbuf = boot_serial.getc();
            return kNoErr;
        }
        else{
            mico_rtos_delay_milliseconds(5);
        }
    }
#else
    if (MicoUartRecv( MICO_STDIO_UART, inbuf, 1, timeout )!=kNoErr)
      return -1;
    else
      return 0;
#endif
}
