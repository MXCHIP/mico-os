/* MiCO Team
 * Copyright (c) 2017 MXCHIP Information Tech. Co.,Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __PLATFORM_PERIPHERAL_H__
#define __PLATFORM_PERIPHERAL_H__

#ifdef __cplusplus
extern "C" {
#endif


#ifdef ALIOS_DEV_GPIO
#include "aos_gpio.h"
#else
#include "platform_gpio.h"
#endif

#ifdef ALIOS_DEV_UART
#include "aos_uart.h"
#else
#include "platform_uart.h"
#endif

#ifdef ALIOS_DEV_I2C
#include "aos_i2c.h"
#else
#include "platform_i2c.h"
#endif

#ifdef ALIOS_DEV_SPI
#include "aos_spi.h"
#else
#include "platform_spi.h"
#endif

#ifdef ALIOS_DEV_WDG
#include "aos_wdg.h"
#else
#include "platform_wdg.h"
#endif

#include "platform_flash.h"
#include "platform_iis.h"


#define MICO_PERIPHERAL_UNSUPPORTED ( 0xFFFFFFFF )

/* SPI mode constants */
#define SPI_CLOCK_RISING_EDGE  ( 1 << 0 )
#define SPI_CLOCK_FALLING_EDGE ( 0 << 0 )
#define SPI_CLOCK_IDLE_HIGH    ( 1 << 1 )
#define SPI_CLOCK_IDLE_LOW     ( 0 << 1 )
#define SPI_USE_DMA            ( 1 << 2 )
#define SPI_NO_DMA             ( 0 << 2 )
#define SPI_MSB_FIRST          ( 1 << 3 )
#define SPI_LSB_FIRST          ( 0 << 3 )

/* I2C flags constants */
#define I2C_DEVICE_DMA_MASK_POSN ( 0 )
#define I2C_DEVICE_NO_DMA        ( 0 << I2C_DEVICE_DMA_MASK_POSN )
#define I2C_DEVICE_USE_DMA       ( 1 << I2C_DEVICE_DMA_MASK_POSN )

#define USE_RTC_BKP 0x00BB32F2 // Use RTC BKP to initilize system time.


#define UART_WAKEUP_MASK_POSN   0
#define UART_WAKEUP_DISABLE    (0 << UART_WAKEUP_MASK_POSN) /**< UART can not wakeup MCU from stop mode */
#define UART_WAKEUP_ENABLE     (1 << UART_WAKEUP_MASK_POSN) /**< UART can wake up MCU from stop mode */

#endif


#ifdef __cplusplus
} /*"C" */
#endif



