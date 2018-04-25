/**
******************************************************************************
* @file    platform.c
* @author  William Xu
* @version V1.0.0
* @date    05-Oct-2016
* @brief   This file provides all MICO Peripherals mapping table and platform
*          specific functions.
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2016 MXCHIP Inc.
*
*  Permission is hereby gra nted, free of charge, to any person obtaining a copy
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

#include "mico_common.h"
#include "mico_platform.h"

#include "mico_board.h"
#include "button.h"
#include "moc_api.h"

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

/******************************************************
*               Variables Definitions
******************************************************/
const mico_gpio_init_t gpio_init[] =
{
  {MICO_GPIO_12, INPUT_PULL_UP, 0},
  {MICO_GPIO_13, INPUT_PULL_UP, 0},
  {MICO_GPIO_14, INPUT_PULL_UP, 0},
  {MICO_GPIO_NC, 0, 0}
};

const mico_pwm_pinmap_t pwm_pinmap[] = 
{
  [MICO_PWM_1] = {.pin = MICO_GPIO_1, },
  [MICO_PWM_2] = {.pin = MICO_GPIO_2, },
  [MICO_PWM_3] = {.pin = MICO_GPIO_12,},
  [MICO_PWM_4] = {.pin = MICO_GPIO_13,},
  [MICO_PWM_5] = {.pin = MICO_GPIO_14,},
  [MICO_PWM_6] = {.pin = MICO_GPIO_7, },
};

const mico_spi_pinmap_t spi_pinmap[] =
{ 
  [MICO_SPI_1]  =
  {
    .mosi = MICO_GPIO_9,
    .miso = MICO_GPIO_7,
    .sclk = MICO_GPIO_10,
    .ssel = MICO_GPIO_8,
  },  
};

const mico_uart_pinmap_t uart_pinmap[] =
{
  [MICO_UART_1] =
  {
    .tx   = MICO_GPIO_9,
    .rx   = MICO_GPIO_10,
    .rts  = MICO_GPIO_7,
    .cts  = MICO_GPIO_8, 
  },
  [MICO_UART_2] =
  {
    .tx   = MICO_GPIO_21,
    .rx   = MICO_GPIO_22,
    .rts  = MICO_GPIO_NONE,
    .cts  = MICO_GPIO_NONE, 
  },
};

const mico_i2c_pinmap_t i2c_pinmap[] =
{
  [MICO_I2C_1] =
  {
    .sda = MICO_GPIO_8,
    .scl = MICO_GPIO_7,
  },
  [MICO_I2C_2] =
  {
    .sda = MICO_GPIO_9,
    .scl = MICO_GPIO_10,
  },  
};

const platform_peripherals_pinmap_t peripherals_pinmap = 
{
  .pwm_pinmap   = pwm_pinmap,
  .spi_pinmap   = spi_pinmap,
  .uart_pinmap  = uart_pinmap,
  .i2c_pinmap   = i2c_pinmap, 
};
/******************************************************
*               Function Definitions
******************************************************/
void mico_board_init( void )
{
#if defined (MOC) && (MOC == 1)
    extern int get_last_reset_reason(void);
    
    if ( get_last_reset_reason() & LAST_RST_CAUSE_WDT )
    {
       char *msg = malloc(1024);
       platform_log( "WARNING: Watchdog reset occured previously." );
       platform_log("msg return %p", msg);
       if (msg != NULL) {
            
            if (hardfault_get(msg, 1024) > 0) {
                platform_log("%s", msg);
            } else {
                platform_log("get 0");
            }
            free(msg);
       }
    }

    MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL );
    MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );

    MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_PUSH_PULL );
    MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
#endif
}

void MicoSysLed(bool onoff)
{
  if (onoff) {
    MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
  } else {
    MicoGpioOutputHigh( (mico_gpio_t)MICO_SYS_LED );
  }
}

void MicoRfLed(bool onoff)
{
  if (onoff) {
    MicoGpioOutputLow( (mico_gpio_t)MICO_RF_LED );
  } else {
    MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
  }
}


