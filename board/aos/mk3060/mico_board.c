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
#include "mico_board.h"
#include "mico_board_conf.h"
#include "platform_logging.h"
#include "CheckSumUtils.h"

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

const platform_gpio_t platform_gpio_pins[] =
{
    [MICO_GPIO_4]  = {12},
    [MICO_GPIO_22] = {18},
    [MICO_GPIO_23] = {17},
    [MICO_GPIO_20] = {11},
    [MICO_GPIO_21] = {21},
    [MICO_GPIO_31] = { 0}, //??
    [MICO_GPIO_0]  = { 7},
    [MICO_GPIO_1]  = { 8},
    [MICO_GPIO_15] = { 6},
    [MICO_GPIO_17] = { 3},
    [MICO_GPIO_16] = { 4},
    [MICO_GPIO_14] = { 5},
    [MICO_GPIO_30] = { 1},
    [MICO_GPIO_29] = { 2},
};

#if 0
const platform_pwm_t *platform_pwm_peripherals = NULL;

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
};

const platform_adc_t platform_adc_peripherals[] = {};
#endif
/******************************************************
*               Function Definitions
******************************************************/

void mico_board_init( void )
{
  MicoGpioInitialize( (mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL );
  MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
  MicoGpioInitialize( (mico_gpio_t)MICO_RF_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
  MicoGpioOutputHigh( (mico_gpio_t)MICO_RF_LED );
  
  MicoGpioInitialize((mico_gpio_t)BOOT_SEL, INPUT_PULL_UP);
  MicoGpioInitialize((mico_gpio_t)MFG_SEL, INPUT_PULL_UP);
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

#define BOOT_MODE_REG (*(uint32_t *)0x40001C)

#define BOOT_MODE_APP   0
#define BOOT_MODE_ATE   1
#define BOOT_MODE_QC    2

bool MicoShouldEnterMFGMode(void)
{
    return BOOT_MODE_REG == BOOT_MODE_QC ? true : false;
}


