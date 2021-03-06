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


/** @file
 *  This file provide wlan IO pin define
 */

#ifndef __WLAN_PLATFORM_COMMON_H__
#define __WLAN_PLATFORM_COMMON_H__

#include "platform_peripheral.h"
#include "mico_board_conf.h"

#ifdef __cplusplus
extern "C" {
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

/**
 * WLAN control pins
 */
typedef enum
{
    WIFI_PIN_POWER,
    WIFI_PIN_RESET,
    WIFI_PIN_32K_CLK,
    WIFI_PIN_BOOTSTRAP_0,
    WIFI_PIN_BOOTSTRAP_1,
    WIFI_PIN_CONTROL_MAX,
} wifi_control_pin_t;

/**
 * WLAN SDIO pins
 */

typedef enum
{
#ifdef SDIO_1_BIT
    WIFI_PIN_SDIO_IRQ,
#else
    WIFI_PIN_SDIO_OOB_IRQ,
#endif
    WIFI_PIN_SDIO_CLK,
    WIFI_PIN_SDIO_CMD,
    WIFI_PIN_SDIO_D0,
#ifndef SDIO_1_BIT
    WIFI_PIN_SDIO_D1,
    WIFI_PIN_SDIO_D2,
    WIFI_PIN_SDIO_D3,
#endif
    WIFI_PIN_SDIO_MAX,
} wifi_sdio_pin_t;

/**
 * WLAN SPI pins
 */
typedef enum
{
    WIFI_PIN_SPI_IRQ,


    /* mbed use spi driver to initialize spi pins
    WIFI_PIN_SPI_CS,
    WIFI_PIN_SPI_CLK,
    WIFI_PIN_SPI_MOSI,
    WIFI_PIN_SPI_MISO,
    */

    WIFI_PIN_SPI_MAX,
} emw1062_spi_pin_t;

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *                 Global Variables
 ******************************************************/

/* Externed from <MiCO-SDK>/platforms/<Platform>/platform.c */
extern const platform_gpio_t        wifi_control_pins[];
extern const platform_gpio_t        wifi_sdio_pins   [];
extern const platform_gpio_t        wifi_spi_pins    [];
extern const platform_spi_t         wifi_spi;



/******************************************************
 *               Function Declarations
 ******************************************************/

extern void platform_mcu_powersave_exit_notify( void );
extern OSStatus host_platform_deinit_wlan_powersave_clock( void );

extern void set_wifi_chip_id(int type);

#ifdef __cplusplus
} /*extern "C" */
#endif

#endif

