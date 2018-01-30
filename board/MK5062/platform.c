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
//  /* Common GPIOs for internal use */
    //  /* GPIOs for external use */
    [MICO_GPIO_1]                         = {GPIOB, 27},
    [MICO_GPIO_2]                         = {GPIOB, 24},
    [MICO_GPIO_3]                         = {GPIOC, 14}, //swdio

    [MICO_GPIO_7]                         = {GPIOB, 20},
    [MICO_GPIO_8]                         = {GPIOB, 8},
    [MICO_GPIO_9]                         = {GPIOB, 7}, // debug uart tx
    [MICO_GPIO_10]                        = {GPIOB, 6}, // debug uart rx
    [MICO_GPIO_11]                        = {GPIOA, 25}, // easylink
    [MICO_GPIO_12]                        = {GPIOA, 23}, // USB DM
    [MICO_GPIO_13]                        = {GPIOA, 22}, // USB DP
    [MICO_GPIO_16]                        = {GPIOB, 26}, // BOOT
    [MICO_GPIO_17]                        = {GPIOA, 10}, // RESET
    [MICO_GPIO_20]                        = {GPIOB, 31}, // user uart rts
    [MICO_GPIO_21]                        = {GPIOC, 0},  // user uart cts
    [MICO_GPIO_22]                        = {GPIOB, 28}, // user uart tx
    [MICO_GPIO_23]                        = {GPIOB, 29}, // user uart rx
    [MICO_GPIO_26]                        = {GPIOC, 11},
    [MICO_GPIO_27]                        = {GPIOC, 12},
    [MICO_GPIO_28]                        = {GPIOC, 13}, // SWCLK
    [MICO_GPIO_30]                        = {GPIOB, 25}, // status
};

const platform_adc_t *platform_adc_peripherals = NULL;

const platform_pwm_t *platform_pwm_peripherals = NULL;

const platform_spi_t *platform_spi_peripherals = NULL;

platform_spi_driver_t *platform_spi_drivers = NULL;

const platform_spi_slave_driver_t *platform_spi_slave_drivers = NULL;

const platform_uart_t platform_uart_peripherals[] =
{
  [MICO_UART_DEBUG] =
  {
    .uart                            = FUART,
    .pin_tx                          = &platform_gpio_pins[STDIO_UART_TX],
    .pin_rx                          = &platform_gpio_pins[STDIO_UART_RX],
    .pin_cts                         = NULL,
    .pin_rts                         = NULL,
  },
  [MICO_UART_DATA] =
  {
    .uart                            = BUART,
    .pin_tx                          = &platform_gpio_pins[APP_UART_TX],
    .pin_rx                          = &platform_gpio_pins[APP_UART_RX],
    .pin_cts                         = NULL,
    .pin_rts                         = NULL,
  },
};

platform_uart_driver_t platform_uart_drivers[MICO_UART_MAX];

const platform_i2c_t *platform_i2c_peripherals = NULL;

/* Flash memory devices */
const platform_flash_t platform_flash_peripherals[] =
{
  [MICO_FLASH_SPI] =
  {
    .flash_type                   = FLASH_TYPE_SPI,
    .flash_start_addr             = 0x000000,
    .flash_length                 = 0x200000,
    .flash_protect_opt            = FLASH_HALF_PROTECT,
  }
};

platform_flash_driver_t platform_flash_drivers[MICO_FLASH_MAX];

/* Logic partition on flash devices */
const mico_logic_partition_t mico_partitions[] =
{
  [MICO_PARTITION_BOOTLOADER] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "Bootloader",
    .partition_start_addr      = 0x0,
    .partition_length          = 0xA000,    //40k bytes + 4k bytes empty space
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_APPLICATION] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "Application",
    .partition_start_addr      = 0xB000,
    .partition_length          = 0xC0000,   //768k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_ATE] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "ATE",
    .partition_start_addr      = 0xCB000,
    .partition_length          = 0x50000,  //320k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_RF_FIRMWARE] =
  {
    .partition_owner           = MICO_FLASH_NONE,
    .partition_description     = "RF Firmware",
    .partition_start_addr      = 0x0,
    .partition_length          = 0x0, 
    .partition_options         = PAR_OPT_READ_DIS | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_OTA_TEMP] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "OTAStorage",
    .partition_start_addr      = 0x11B000,
    .partition_length          = 0xC0000, //768k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_PARAMETER_1] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "PARAMETER1",
    .partition_start_addr      = 0x1DB000,
    .partition_length          = 0x1000, // 4k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_PARAMETER_2] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "PARAMETER2",
    .partition_start_addr      = 0x1DC000,
    .partition_length          = 0x1000, //4k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  }
};



/******************************************************
*           Interrupt Handler Definitions
******************************************************/

MICO_RTOS_DEFINE_ISR( FuartInterrupt )
{
  platform_uart_irq( &platform_uart_drivers[MICO_UART_DEBUG] );
}

MICO_RTOS_DEFINE_ISR( BuartInterrupt )
{
  platform_uart_irq( &platform_uart_drivers[MICO_UART_DATA] );
}

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

bool MicoShouldEnterMFGMode(void)
{
    return false;
}

bool MicoShouldEnterBootloader(void)
{
  if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==true)
    return true;
  else
    return false;
}
