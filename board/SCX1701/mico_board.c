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
#include "platform_peripheral.h"
#include "platform_logging.h"
#include "spi_flash_platform_interface.h"
#include "wlan_platform_common.h"
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

/* This table maps STM32 pins to GPIO definitions on the schematic*/
const platform_gpio_t platform_gpio_pins[] =
{
  [MICO_SW2]                      = { PE_11 },
  [MICO_SW3]                      = { PE_10 },

  [FLASH_CS]                      = { PA_4 },   //Flash chip select
};


const platform_pwm_t *platform_pwm_peripherals = NULL;

const platform_i2c_t *platform_i2c_peripherals = NULL;

const platform_uart_t platform_uart_peripherals[] =
{
    [MICO_STDIO_UART] =
    {
        .mbed_tx_pin = STDIO_UART_TX,
        .mbed_rx_pin = STDIO_UART_RX,
        .mbed_rts_pin = NC,
        .mbed_cts_pin = NC ,
    }
};

const platform_spi_t platform_spi_peripherals[] =
{
    [MICO_SPI_1]  =
    {
        .mbed_sclk_pin    = PA_5,
        .mbed_mosi_pin    = PB_5,
        .mbed_miso_pin    = PB_4,
    }
};


/* Flash memory devices */
const platform_flash_t platform_flash_peripherals[] =
{
  [MICO_FLASH_EMBEDDED] =
  {
    .flash_type                   = FLASH_TYPE_EMBEDDED,
    .flash_start_addr             = 0x08000000,
    .flash_length                 = 0x100000,   //1MBytes
  },
  [MICO_FLASH_SPI] =
  {
    .flash_type                   = FLASH_TYPE_SPI,
    .flash_start_addr             = 0x000000,
    .flash_length                 = 0x200000,   //2M
  },
};

/* Logic partition on flash devices */
const mico_logic_partition_t mico_partitions[] =
{
  [MICO_PARTITION_BOOTLOADER] =
  {
    .partition_owner           = MICO_FLASH_EMBEDDED,
    .partition_description     = "Bootloader",
    .partition_start_addr      = 0x08000000,
    .partition_length          =     0x8000,    //32k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_APPLICATION] =
  {
    .partition_owner           = MICO_FLASH_EMBEDDED,
    .partition_description     = "Application",
    .partition_start_addr      = 0x08008000,
    .partition_length          =   0x80000, //0xF8000,   //1024k-32k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_ATE] =
  {
    .partition_owner           = MICO_FLASH_NONE,
  },
  [MICO_PARTITION_RF_FIRMWARE] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "RF Firmware",
    .partition_start_addr      = 0x2000,
    .partition_length          = 0x3E000,  //248k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
  },
  [MICO_PARTITION_OTA_TEMP] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "OTA Storage",
    .partition_start_addr      = 0x40000,
    .partition_length          = 0x80000, //512k bytes 80000
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_PARAMETER_1] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "PARAMETER1",
    .partition_start_addr      = 0x0,
    .partition_length          = 0x1000, // 4k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_PARAMETER_2] =
  {
    .partition_owner           = MICO_FLASH_SPI,
    .partition_description     = "PARAMETER2",
    .partition_start_addr      = 0x1000,
    .partition_length          = 0x1000, //4k bytes
    .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  },
  [MICO_PARTITION_FILESYS] =
  {
     .partition_owner          = MICO_FLASH_SPI,
     .partition_description    = "FILESYS",
     .partition_start_addr     = 0x100000,
     .partition_length         = 0x100000, //1M bytes
     .partition_options        = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
  }
};


#if defined ( USE_MICO_SPI_FLASH )
const mico_spi_device_t mico_spi_flash =
{
  .port        = MICO_SPI_1,
  .chip_select = FLASH_CS,
  .speed       = 40000000,
  .mode        = (SPI_CLOCK_RISING_EDGE | SPI_CLOCK_IDLE_HIGH | SPI_USE_DMA | SPI_MSB_FIRST ),
  .bits        = 8
};
#endif

//const platform_adc_t platform_adc_peripherals[] =
//{
//  //[MICO_ADC_1] = { ADC1, ADC_Channel_11, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_21] },//PC1
//  //[MICO_ADC_2] = { ADC1, ADC_Channel_13, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_19] },//PC3
//};

/* Wi-Fi control pins. Used by platform/MCU/wlan_platform_common.c
* SDIO: EMW1062_PIN_BOOTSTRAP[1:0] = b'00
* gSPI: EMW1062_PIN_BOOTSTRAP[1:0] = b'01
*/
const platform_gpio_t wifi_control_pins[] =
{
  [WIFI_PIN_RESET]        = { PD_15 },
  [WIFI_PIN_32K_CLK]      = { PC_6 },
  [WIFI_PIN_POWER]        = { PE_2 },
};


/* Wi-Fi SDIO bus pins. Used by platform/MCU/STM32F2xx/EMW1062_driver/wlan_SDIO.c */
const platform_gpio_t wifi_sdio_pins[] =
{
  [WIFI_PIN_SDIO_OOB_IRQ] = { SDIO_OOB_IRQ },
  [WIFI_PIN_SDIO_CLK    ] = { SDIO_CLK     },
  [WIFI_PIN_SDIO_CMD    ] = { SDIO_CMD     },
  [WIFI_PIN_SDIO_D0     ] = { SDIO_D0      },
  [WIFI_PIN_SDIO_D1     ] = { SDIO_D1      },
  [WIFI_PIN_SDIO_D2     ] = { SDIO_D2      },
  [WIFI_PIN_SDIO_D3     ] = { SDIO_D3      },
};


/******************************************************
*           Interrupt Handler Definitions
******************************************************/
#if 0
#if defined (MICO_WIFI_SHARE_SPI_BUS) || !defined (MICO_NO_WIFI)
MICO_RTOS_DEFINE_ISR( DMA2_Stream2_IRQHandler )
{
  platform_wifi_spi_rx_dma_irq( );
}
#endif

MICO_RTOS_DEFINE_ISR( USART6_IRQHandler )
{
  platform_uart_irq( &platform_uart_drivers[Arduino_UART] );
}

MICO_RTOS_DEFINE_ISR( USART3_IRQHandler )
{
  platform_uart_irq( &platform_uart_drivers[MICO_STDIO_UART] );
}

MICO_RTOS_DEFINE_ISR( DMA1_Stream4_IRQHandler )
{
  platform_uart_tx_dma_irq( &platform_uart_drivers[MICO_UART_1] );
}

MICO_RTOS_DEFINE_ISR( DMA2_Stream7_IRQHandler )
{
  platform_uart_tx_dma_irq( &platform_uart_drivers[MICO_UART_2] );
}

MICO_RTOS_DEFINE_ISR( DMA1_Stream1_IRQHandler )
{

  platform_uart_rx_dma_irq( &platform_uart_drivers[MICO_UART_1] );
}

MICO_RTOS_DEFINE_ISR( DMA2_Stream1_IRQHandler )
{
  platform_uart_rx_dma_irq( &platform_uart_drivers[MICO_UART_2] );
}
#endif

/******************************************************
*               Function Definitions
******************************************************/

void platform_init_peripheral_irq_priorities( void )
{
  /* Interrupt priority setup. Called by MiCO/platform/MCU/STM32F2xx/platform_init.c */
  NVIC_SetPriority( RTC_WKUP_IRQn    ,  1 ); /* RTC Wake-up event   */
  NVIC_SetPriority( SDIO_IRQn        ,  2 ); /* WLAN SDIO           */
  NVIC_SetPriority( DMA2_Stream2_IRQn,  3 ); /* WLAN SPI DMA        */
  NVIC_SetPriority( USART3_IRQn      ,  6 ); /* MICO_UART_1         */
  NVIC_SetPriority( USART6_IRQn      ,  6 ); /* MICO_UART_2         */
  NVIC_SetPriority( DMA1_Stream4_IRQn,  7 ); /* MICO_UART_1 TX DMA  */
  NVIC_SetPriority( DMA1_Stream1_IRQn,  7 ); /* MICO_UART_1 RX DMA  */
  NVIC_SetPriority( DMA2_Stream7_IRQn,  7 ); /* MICO_UART_2 TX DMA  */
  NVIC_SetPriority( DMA2_Stream1_IRQn,  7 ); /* MICO_UART_2 RX DMA  */
  NVIC_SetPriority( EXTI0_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI1_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI2_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI3_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI4_IRQn       , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI9_5_IRQn     , 14 ); /* GPIO                */
  NVIC_SetPriority( EXTI15_10_IRQn   , 14 ); /* GPIO                */
}

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
    MicoGpioOutputHigh( (mico_gpio_t)MICO_SYS_LED );
  } else {
    MicoGpioOutputLow( (mico_gpio_t)MICO_SYS_LED );
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
    return false;
}

