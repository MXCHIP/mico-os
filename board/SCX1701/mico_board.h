/**
 ******************************************************************************
 * @file    platform.h
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provides all MICO Peripherals defined for current platform.
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

#pragma once

#ifndef __PLATFORM_H_
#define __PLATFORM_H_

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/
  
/******************************************************
 *                   Enumerations
 ******************************************************/

enum
{
    MICO_SW2,
    MICO_SW3,

    FLASH_CS,

    MICO_GPIO_MAX, /* Denotes the total number of GPIO port aliases. Not a valid GPIO alias */
    MICO_GPIO_NONE,
};


enum
{
    MICO_SPI_1,
    MICO_SPI_MAX, /* Denotes the total number of SPI port aliases. Not a valid SPI alias */
    MICO_SPI_NONE,
};

enum
{
    MICO_I2C_MAX, /* Denotes the total number of I2C port aliases. Not a valid I2C alias */
    MICO_I2C_NONE,
};

enum
{
    MICO_IIS_MAX, /* Denotes the total number of IIS port aliases. Not a valid IIS alias */
    MICO_IIS_NONE,
};

enum
{
    MICO_PWM_MAX, /* Denotes the total number of PWM port aliases. Not a valid PWM alias */
    MICO_PWM_NONE,
};

enum
{
    MICO_ADC_1,   /* F412 only one ADC 0-15 channel  */
    MICO_ADC_2,
    MICO_ADC_MAX, /* Denotes the total number of ADC port aliases. Not a valid ADC alias */
    MICO_ADC_NONE,
};

enum
{
    MICO_UART_1,
    MICO_UART_MAX, /* Denotes the total number of UART port aliases. Not a valid UART alias */
    MICO_UART_NONE,
};

enum
{
    MICO_FLASH_EMBEDDED,
    MICO_FLASH_SPI,
    MICO_FLASH_MAX,
    MICO_FLASH_NONE,
};

enum
{
    MICO_PARTITION_FILESYS,
    MICO_PARTITION_USER_MAX
};

enum
{
    MICO_PARTITION_ERROR = -1,
    MICO_PARTITION_BOOTLOADER = MICO_PARTITION_USER_MAX,
    MICO_PARTITION_APPLICATION,
    MICO_PARTITION_ATE,
    MICO_PARTITION_OTA_TEMP,
    MICO_PARTITION_RF_FIRMWARE,
    MICO_PARTITION_PARAMETER_1,
    MICO_PARTITION_PARAMETER_2,
    MICO_PARTITION_MAX,
    MICO_PARTITION_NONE,
};

#define MICO_STDIO_UART       (MICO_UART_1)
#define MICO_UART_FOR_APP     (MICO_UART_1)
#define MICO_MFG_TEST         (MICO_UART_1)
#define MICO_CLI_UART         (MICO_UART_1)

#define MICO_STDIO_UART_BAUDRATE    (115200)


#define MFG_SEL               (MICO_GPIO_NONE)
#define BOOT_SEL              (MICO_GPIO_NONE)

#define  MICO_SYS_LED         (MICO_GPIO_NONE)
#define  MICO_RF_LED          (MICO_GPIO_NONE)

/* Components connected to external I/Os*/
#define USE_MICO_SPI_FLASH
//#define SFLASH_SUPPORT_MACRONIX_PARTS 
//#define SFLASH_SUPPORT_SST_PARTS
#define SFLASH_SUPPORT_WINBOND_PARTS

#define MICO_I2C_CP         (MICO_I2C_NONE)

#endif



