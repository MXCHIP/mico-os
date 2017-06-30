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

#ifndef __MICO_BOARD_H__
#define __MICO_BOARD_H__

#ifdef __cplusplus
extern "C"
{
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

enum
{   
    MICO_GPIO_2,
    MICO_GPIO_4,
    MICO_GPIO_5,
    MICO_GPIO_6,
    MICO_GPIO_7,
    MICO_GPIO_8,
    MICO_GPIO_9,
    MICO_GPIO_12,
    MICO_GPIO_17,
    MICO_GPIO_18,
    MICO_GPIO_19,
    MICO_GPIO_27,  
    MICO_GPIO_29,
    MICO_GPIO_30,
    RF_RESET,
    RF_DIO0,
    RF_DIO1,
    RF_DIO2,
    RF_DIO3,
    MICO_GPIO_MAX, /* Denotes the total number of GPIO port aliases. Not a valid GPIO alias */
    MICO_GPIO_NONE,
};

enum
{
  MICO_SPI_1,
  MICO_SPI_MAX, /* Denotes the total number of SPI port aliases. Not a valid SPI alias */
  MICO_SPI_NONE,
};

typedef enum
{
  MICO_QSPI_1,
  MICO_QSPI_MAX,/* Denotes the total number of QSPI port aliases. Not a valid QSPI alias */
  MICO_QSPI_NONE,
}mico_qspi_t;

enum
{
  MICO_I2C_1,
  MICO_I2C_MAX, /* Denotes the total number of I2C port aliases. Not a valid I2C alias */
  MICO_I2C_NONE,
};

enum
{
    MICO_PWM_MAX, /* Denotes the total number of PWM port aliases. Not a valid PWM alias */
    MICO_PWM_NONE,
};

enum
{
    MICO_ADC_1,
    MICO_ADC_2,
    MICO_ADC_MAX, /* Denotes the total number of ADC port aliases. Not a valid ADC alias */
    MICO_ADC_NONE,
};


enum
{
    MICO_UART_1,
    MICO_UART_2,
    MICO_UART_MAX, /* Denotes the total number of UART port aliases. Not a valid UART alias */
    MICO_UART_NONE,
};

enum
{
  MICO_FLASH_EMBEDDED,
  MICO_FLASH_QSPI,
  MICO_FLASH_MAX,
  MICO_FLASH_NONE,
};

typedef enum
{
  MICO_PARTITION_FILESYS,
  MICO_PARTITION_USER_MAX
} mico_user_partition_t;

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
} ;


#ifdef BOOTLOADER
#define MICO_STDIO_UART          (MICO_UART_1)
#define STDIO_UART_BAUDRATE (115200)
#else
#define MICO_STDIO_UART          (MICO_UART_1)
#define STDIO_UART_BAUDRATE (115200)
#endif

#define MICO_UART_FOR_APP        (MICO_UART_2)
#define MICO_MFG_TEST            (MICO_UART_2)
#define MICO_CLI_UART            (MICO_UART_1)

/* Components connected to external I/Os*/
// #define USE_QUAD_SPI_FLASH
//#define USE_QUAD_SPI_DMA

#define BOOT_SEL            (MICO_GPIO_NONE)
#define MFG_SEL             (MICO_GPIO_NONE)
#define EasyLink_BUTTON     (MICO_GPIO_NONE)
#define MICO_SYS_LED        (MICO_GPIO_NONE)
#define MICO_RF_LED         (MICO_GPIO_NONE)

/* Arduino extention connector */
#define Arduino_RXD         (MICO_GPIO_29)
#define Arduino_TXD         (MICO_GPIO_30)
#define Arduino_D2          (MICO_GPIO_NONE)
#define Arduino_D3          (MICO_GPIO_NONE)
#define Arduino_D4          (MICO_GPIO_NONE)
#define Arduino_D5          (MICO_GPIO_NONE)
#define Arduino_D6          (MICO_GPIO_NONE)
#define Arduino_D7          (MICO_GPIO_NONE)

#define Arduino_D8          (MICO_GPIO_NONE)
#define Arduino_D9          (MICO_GPIO_NONE)
#define Arduino_CS          (MICO_GPIO_5)
#define Arduino_SI          (MICO_GPIO_4)
#define Arduino_SO          (MICO_GPIO_7)
#define Arduino_SCK         (MICO_GPIO_6)
#define Arduino_SDA         (MICO_GPIO_18)
#define Arduino_SCL         (MICO_GPIO_17)

#define Arduino_A0          (MICO_GPIO_NONE)
#define Arduino_A1          (MICO_GPIO_NONE)
#define Arduino_A2          (MICO_GPIO_NONE)
#define Arduino_A3          (MICO_GPIO_NONE)
#define Arduino_A4          (MICO_ADC_NONE)
#define Arduino_A5          (MICO_ADC_NONE)

#define Arduino_I2C         (MICO_I2C_1)
#define Arduino_SPI         (MICO_SPI_1)
#define Arduino_UART        (MICO_UART_2)

// #define USE_MiCOKit_EXT

// #ifdef USE_MiCOKit_EXT
// #define MICO_I2C_CP         (Arduino_I2C)
// #include "micokit_ext_def.h"
// #else
// #define MICO_I2C_CP         (MICO_I2C_NONE)
// #endif //USE_MiCOKit_EXT

#define EML3047

#define MICO_I2C_CP         (MICO_I2C_NONE)

void mico_board_init(void);


#ifdef __cplusplus
} /*extern "C" */
#endif

#endif
