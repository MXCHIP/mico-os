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

#include "mico.h"
#include "mico_board.h"
#include "platform_peripheral.h"
#include "wlan_platform_common.h"

#include "mico_platform.h"

#include "button.h"

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
extern WEAK void bootloader_start(void);

/******************************************************
 *               Variables Definitions
 ******************************************************/

const platform_gpio_t platform_gpio_pins[] =
{
        /* Common GPIOs for internal use */

        /* GPIOs for external use */
          [MICO_GPIO_2]                       = { PB_7      },   //SPI1_CS2
          [MICO_GPIO_4]                       = { SPI_MOSI  },   // SPI_MOSI
          [MICO_GPIO_5]                       = { PA_4      },   // SPI_CS SPI1_CS1
          [MICO_GPIO_6]                       = { SPI_SCK   },   // SPI_SCK
          [MICO_GPIO_7]                       = { SPI_MISO },   // SPI_MISO
          [MICO_GPIO_8]                       = { SERIAL_TX },   // UART_TXD_DEBUG
          [MICO_GPIO_9]                       = { PA_7      },  
          [MICO_GPIO_12]                      = { SERIAL_RX },   // UART_RXD_DEBUG
          [MICO_GPIO_17]                      = { I2C_SCL   },   // I2C_SCL
          [MICO_GPIO_18]                      = { I2C_SDA   },   // I2C_SDA
          [MICO_GPIO_19]                      = { PA_6      },   //USER_UART_CTS/LPUART1_CTS
          [MICO_GPIO_27]                      = { PB_1      },   //USER_UART_RTS/LPUART1_RTS_DE
          [MICO_GPIO_29]                      = { USBRX     },   // UART_RXD_USER/LPUART1_TX
          [MICO_GPIO_30]                      = { USBTX     },   // UART_TXD_USER/LPUART1_RX  
          [  RF_RESET ]                       = { PB_0      },     
          [  RF_DIO0  ]                       = { PB_6      },
          [  RF_DIO1  ]                       = { PA_12     },
          [  RF_DIO2  ]                       = { PA_11     },
          [  RF_DIO3  ]                       = { PA_8      },
};

platform_gpio_driver_t platform_gpio_drivers[MICO_GPIO_MAX];
platform_gpio_irq_driver_t platform_gpio_irq_drivers[MICO_GPIO_MAX];

// const platform_pwm_t *platform_pwm_peripherals = NULL;

// const platform_adc_t platform_adc_peripherals[] =
// {
//   [MICO_ADC_1] = { ADC1, ADC_Channel_4, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_38] },
//   [MICO_ADC_2] = { ADC1, ADC_Channel_5, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_34] },
// };

const platform_i2c_t platform_i2c_peripherals[] = 
{
        [MICO_I2C_1] = 
        {
            .mbed_scl_pin = I2C_SCL,
            .mbed_sda_pin = I2C_SDA,
        }
};

platform_i2c_driver_t platform_i2c_drivers[MICO_I2C_MAX];

const platform_uart_t platform_uart_peripherals[] = 
{
        [MICO_UART_1] =
        {
            .mbed_tx_pin = STDIO_UART_TX,
            .mbed_rx_pin = STDIO_UART_RX,
        },
        [MICO_UART_2] =
        {
            .mbed_tx_pin  = MICO_GPIO_30,
            .mbed_rx_pin  = MICO_GPIO_29,
            .mbed_rts_pin = MICO_GPIO_27,
            .mbed_cts_pin = MICO_GPIO_19,
        }
};
platform_uart_driver_t platform_uart_drivers[MICO_UART_MAX];

const platform_spi_t platform_spi_peripherals[] =
{
        [MICO_SPI_1]  =
        {
            .mbed_scl_pin  = SPI_SCK,
            .mbed_mosi_pin = SPI_MOSI,
            .mbed_miso_pin = SPI_MISO,
        }
};

platform_spi_driver_t platform_spi_drivers[MICO_SPI_MAX];

/* Flash memory devices */
const platform_flash_t platform_flash_peripherals[] =
{
        [MICO_FLASH_EMBEDDED] =
        {
            .flash_type = FLASH_TYPE_EMBEDDED,
            .flash_start_addr = 0x08000000,
            .flash_length = 0x20000, //128k
        },
        //    [MICO_FLASH_QSPI] =
        //    {
        //      .flash_type                   = FLASH_TYPE_QSPI,
        //      .flash_start_addr             = 0x000000,
        //      .flash_length                 = 0x200000,
        //    },
};

platform_flash_driver_t platform_flash_drivers[MICO_FLASH_MAX];

/* Logic partition on flash devices */
const platform_logic_partition_t mico_partitions[] =
{
    [MICO_PARTITION_BOOTLOADER] =
    {
        .partition_owner = MICO_FLASH_EMBEDDED,
        .partition_description = "Bootloader",
        .partition_start_addr = 0x08000000,
        .partition_length = 0x8000,    //32k bytesC
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [MICO_PARTITION_APPLICATION] =
    {
        .partition_owner = MICO_FLASH_EMBEDDED,
        .partition_description = "Application",
        .partition_start_addr = 0x08008000,
        .partition_length = 0x10000,   //64k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [MICO_PARTITION_ATE] =
    {
        .partition_owner = MICO_FLASH_NONE,
        .partition_description = "ATEFirmware",
        .partition_start_addr = 0x080A0000,
        .partition_length = 0x60000, //384k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [MICO_PARTITION_PARAMETER_1] =
    {
        .partition_owner = MICO_FLASH_NONE,
        .partition_description = "PARAMETER1",
        .partition_start_addr = 0x0,
        .partition_length = 0x1000, // 4k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [MICO_PARTITION_PARAMETER_2] =
    {
        .partition_owner = MICO_FLASH_NONE,
        .partition_description = "PARAMETER2",
        .partition_start_addr = 0x1000,
        .partition_length = 0x1000, //4k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [MICO_PARTITION_RF_FIRMWARE] =
    {
        .partition_owner = MICO_FLASH_NONE,
        .partition_description = "RF Firmware",
        .partition_start_addr = 0x2000,
        .partition_length = 0x6E000,  //440k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [MICO_PARTITION_OTA_TEMP] =
    {
        .partition_owner = MICO_FLASH_NONE,
        .partition_description = "OTA Storage",
        .partition_start_addr = 0x70000,
        .partition_length = 0x98000, //608k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    }
};

// /******************************************************
// *               Function Definitions
// ******************************************************/
void platform_init_peripheral_irq_priorities(void)
{
    // NVIC_SetPriority( RTC_WKUP_IRQn,        1 );   /* RTC Wake-up event   */
    // NVIC_SetPriority( SDIO_IRQn,            2 );   /* WLAN SDIO           */
    // NVIC_SetPriority( DMA2_Stream3_IRQn,    3 );   /* WLAN SDIO DMA       */
    NVIC_SetPriority(USART4_5_IRQn, 6);              /* MICO_UART_1         */
    NVIC_SetPriority(DMA1_Channel4_5_6_7_IRQn, 7); /* MICO_UART_1 TX DMA&RX DMA  */
    // NVIC_SetPriority( DMA2_Stream1_IRQn,    7 );   /* MICO_UART_1 RX DMA  */
    // NVIC_SetPriority( USART2_IRQn,          6 );   /* BT UART             */
    // NVIC_SetPriority( DMA1_Stream5_IRQn,    7 );   /* BT UART RX DMA      */
    // NVIC_SetPriority( DMA1_Stream6_IRQn,    7 );   /* BT UART TX DMA      */
    NVIC_SetPriority(EXTI0_1_IRQn, 14);  /* GPIO                */
    NVIC_SetPriority(EXTI2_3_IRQn, 14);  /* GPIO                */
    NVIC_SetPriority(EXTI4_15_IRQn, 14); /* GPIO                */
    // NVIC_SetPriority( EXTI3_IRQn,           14 );  /* GPIO                */
    // NVIC_SetPriority( EXTI4_IRQn,           14 );  /* GPIO                */
    // NVIC_SetPriority( EXTI9_5_IRQn,         14 );  /* GPIO                */
    // NVIC_SetPriority( EXTI15_10_IRQn,       14 );  /* GPIO                */
}

void mico_board_init(void)
{
    button_init_t init;

    platform_init_peripheral_irq_priorities();

    // mico_gpio_init((mico_gpio_t)MICO_SYS_LED, OUTPUT_PUSH_PULL);
    // mico_gpio_output_high((mico_gpio_t)MICO_SYS_LED);
}

void MicoSysLed(bool onoff)
{
    if (onoff)
    {
        mico_gpio_output_low(MICO_SYS_LED);
    }
    else
    {
        mico_gpio_output_high(MICO_SYS_LED);
    }
}

void MicoRfLed(bool onoff)
{
    if (onoff)
    {
        mico_gpio_output_low(MICO_RF_LED);
    }
    else
    {
        mico_gpio_output_high(MICO_RF_LED);
    }
}

bool MicoShouldEnterMFGMode(void)
{
    if (mico_gpio_input_get(BOOT_SEL) == false && mico_gpio_input_get(MFG_SEL) == false)
        return true;
    else
        return false;
}

bool MicoShouldEnterBootloader(void)
{
    if (mico_gpio_input_get(BOOT_SEL) == false && mico_gpio_input_get(MFG_SEL) == true)
        return true;
    else
        return false;
}
