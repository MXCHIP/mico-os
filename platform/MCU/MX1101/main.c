/**
******************************************************************************
* @file    crt0_IAR.h 
* @author  William Xu
* @version V1.0.0
* @date    16-Sep-2014
* @brief   __low_level_init called by IAR before main.
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

#include "gpio.h"
#include "uart.h"
#include "irqs.h"
#include "clk.h"
#include "uart.h"
#include "gpio.h"
#include "spi_flash.h"
#include "watchdog.h"
#include "timeout.h"
#include "cache.h"
#include "delay.h"
#include "wakeup.h"
#include "rtc.h"

#define SCB_VTOR_ADDRESS         ( ( volatile unsigned long* ) 0xE000ED08 )

int user_main( void )
{
	/* Setup the interrupt vectors address */
	*SCB_VTOR_ADDRESS = 0xB000;

	init_clocks();
	init_architecture();
	init_platform();
	
	main();

	/* Should never get here, unless there is an error in vTaskStartScheduler */
	for(;;);
}
