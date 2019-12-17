/**
 ******************************************************************************
 * @file    platform.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provides all MICO Peripherals mapping table and platform
 *          specific functions.
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

#include "mico_platform.h"
#include "platform.h"
#include "platform_config.h"
#include "platform_peripheral.h"
#include "platform_config.h"
#include "platform_logging.h"
#include "CheckSumUtils.h"
#include "keypad/gpio_button/button.h"


#ifdef USE_MiCOKit_EXT
#include "MiCOKit_EXT/micokit_ext.h"
#endif

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
extern WEAK void PlatformEasyLinkButtonClickedCallback(void);
extern WEAK void PlatformEasyLinkButtonLongPressedCallback(void);

/******************************************************
*               Variables Definitions
******************************************************/

const platform_gpio_t platform_gpio_pins[] =
{
    [MICO_GPIO_4]  = { 4},
    [MICO_GPIO_22] = {22},
    [MICO_GPIO_23] = {23},
    [MICO_GPIO_20] = {20},
    [MICO_GPIO_21] = {21},
    [MICO_GPIO_0]  = { 0},
    [MICO_GPIO_1]  = { 1}, 
    [MICO_GPIO_15] = {15},
    [MICO_GPIO_17] = {17},
    [MICO_GPIO_16] = {16},
    [MICO_GPIO_14] = {14},
    [MICO_GPIO_30] = {30},
    [MICO_GPIO_29] = {29},
};

const platform_pwm_t platform_pwm_peripherals[] = 
{
    [MICO_PWM_1] = { .channel = 1 },
    [MICO_PWM_2] = { .channel = 2 },
    [MICO_PWM_3] = { .channel = 3 },
    [MICO_PWM_4] = { .channel = 4 },
    [MICO_PWM_5] = { .channel = 5 },
};

const platform_i2c_t platform_i2c_peripherals[] =
{
    [MICO_I2C_1] =
    {
        .pin_scl = &platform_gpio_pins[MICO_GPIO_20],
        .pin_sda = &platform_gpio_pins[MICO_GPIO_21],
    },
    [MICO_I2C_2] =
    {
        .pin_scl = &platform_gpio_pins[MICO_GPIO_0],
        .pin_sda = &platform_gpio_pins[MICO_GPIO_1],
    },
};
platform_i2c_driver_t platform_i2c_drivers[MICO_I2C_MAX];

const platform_uart_t platform_uart_peripherals[] = 
{
	[MICO_UART_1] = {MX_UART_1}, 
    [MICO_UART_2] = {MX_UART_2}, 
};

platform_uart_driver_t platform_uart_drivers[MICO_UART_MAX];

const platform_spi_t platform_spi_peripherals[] = {
    [MICO_SPI_1] = {0},
};
platform_spi_driver_t platform_spi_drivers[MICO_SPI_MAX];

/* Flash memory devices */
const platform_flash_t platform_flash_peripherals[] = {};
platform_flash_driver_t platform_flash_drivers[MICO_FLASH_MAX];

/* Logic partition on flash devices */
const mico_logic_partition_t mico_partitions[] = 
{
	[MICO_PARTITION_BOOTLOADER] =
	{
	    .partition_owner            = MICO_FLASH_EMBEDDED,
	    .partition_description      = "Bootloader",
	    .partition_start_addr       = 0x0,
	    .partition_length           = 0x10000,    //64k bytes
	    .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
	},
	[MICO_PARTITION_PARAMETER_1] =
    {
        .partition_owner            = MICO_FLASH_EMBEDDED,
        .partition_description      = "PARAMETER1",
        .partition_start_addr       = 0x10000,
        .partition_length           = 0x1000, // 4k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [MICO_PARTITION_PARAMETER_2] =
    {
        .partition_owner            = MICO_FLASH_EMBEDDED,
        .partition_description      = "PARAMETER2",
        .partition_start_addr       = 0x11000,
        .partition_length           = 0x1000, //4k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
	[MICO_PARTITION_APPLICATION] =
	{
	    .partition_owner            = MICO_FLASH_EMBEDDED,
	    .partition_description      = "Application",
	    .partition_start_addr       = 0x13000,
	    .partition_length           = 0xED000, //948k bytes
	    .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
	},
    [MICO_PARTITION_OTA_TEMP] =
    {
        .partition_owner           = MICO_FLASH_EMBEDDED,
        .partition_description     = "OTA Storage",
        .partition_start_addr      = 0x100000,
        .partition_length          = 0xA5E66, //664k bytes
        .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [MICO_PARTITION_PARAMETER_3] =
    {
        .partition_owner            = MICO_FLASH_EMBEDDED,
        .partition_description      = "PARAMETER3",
        .partition_start_addr       = 0x12000,
        .partition_length           = 0x1000, //4k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [MICO_PARTITION_PARAMETER_4] =
    {
        .partition_owner            = MICO_FLASH_EMBEDDED,
        .partition_description      = "PARAMETER4",
        .partition_start_addr       = 0xD000,
        .partition_length           = 0x1000, //4k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [MICO_PARTITION_SYSTEM_DATA] = 
    {
        .partition_owner            = MICO_FLASH_EMBEDDED,
        .partition_description      = "System data",
        .partition_start_addr       = 0xE000,
        .partition_length           = 0x2000,    //8k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [MICO_PARTITION_KVRO] = 
    {
        .partition_owner            = MICO_FLASH_EMBEDDED,
        .partition_description      = "KVRO",
        .partition_start_addr       = 0x1A6000,
        .partition_length           = 0x4000,    //16k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },  
    [MICO_PARTITION_KV] = 
    {
        .partition_owner            = MICO_FLASH_EMBEDDED,
        .partition_description      = "KV",
        .partition_start_addr       = 0x1A6000 + 0x4000,
        .partition_length           = 0x4000,    //16k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },   
};

const platform_adc_t platform_adc_peripherals[] = {};

/******************************************************
*               Function Definitions
******************************************************/

void init_platform( void )
{
  button_init_t init;
	  
  MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL );
  MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
  MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
  MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
  
  MicoGpioInitialize((mico_gpio_t)BOOT_SEL, INPUT_PULL_UP);
  MicoGpioInitialize((mico_gpio_t)MFG_SEL, INPUT_PULL_UP);

  init.gpio = EasyLink_BUTTON;
  init.pressed_func = PlatformEasyLinkButtonClickedCallback;
  init.long_pressed_func = PlatformEasyLinkButtonLongPressedCallback;
  init.long_pressed_timeout = RestoreDefault_TimeOut;

  button_init( IOBUTTON_EASYLINK, init );
}

void init_platform_bootloader( void )
{
  MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL );
  MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
  MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
  MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
  
  MicoGpioInitialize((mico_gpio_t)BOOT_SEL, INPUT_PULL_UP);
  MicoGpioInitialize((mico_gpio_t)MFG_SEL, INPUT_PULL_UP);

  return;
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

#ifdef USE_MiCOKit_EXT
// add test mode for MiCOKit-EXT board,check Arduino_D5 pin when system startup
bool MicoExtShouldEnterTestMode(void)
{
  if( MicoGpioInputGet((mico_gpio_t)Arduino_D5)==false ){
    return true;
  }
  else{
    return false;
  }
}
#endif

#define BOOT_MODE_REG (*(uint32_t *)0x40001C)

#define BOOT_MODE_APP   0
#define BOOT_MODE_ATE   1
#define BOOT_MODE_QC    2

bool MicoShouldEnterMFGMode(void)
{
    return BOOT_MODE_REG == BOOT_MODE_QC ? true : false;
}

bool MicoShouldEnterBootloader(void)
{
    return true;
  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==true)
    return true;
  else
    return false;
}

