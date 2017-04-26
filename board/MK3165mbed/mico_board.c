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

extern OSStatus host_platform_init( void );

/******************************************************
*               Variables Definitions
******************************************************/

const platform_gpio_t platform_gpio_pins[] =
{
  /* Common GPIOs for internal use */
   [FLASH_PIN_QSPI_CS ]                = { QSPI_CS },
   [FLASH_PIN_QSPI_CLK]                = { QSPI_CLK },
   [FLASH_PIN_QSPI_D0]                 = { QSPI_D0 },
   [FLASH_PIN_QSPI_D1]                 = { QSPI_D1 },
   [FLASH_PIN_QSPI_D2]                 = { QSPI_D2 },
   [FLASH_PIN_QSPI_D3]                 = { QSPI_D3 },

  /* GPIOs for external use */
  [MICO_GPIO_2]                       = { MBED_GPIO_2 },
  [MICO_GPIO_4]                       = { MBED_GPIO_4 },   // SPI_MOSI
  [MICO_GPIO_5]                       = { MBED_GPIO_5 },   // SPI_CS
  [MICO_GPIO_6]                       = { MBED_GPIO_6 },   // SPI_SCK
  [MICO_GPIO_7]                       = { MBED_GPIO_7 },   // SPI_MISO
  [MICO_GPIO_8]                       = { MBED_GPIO_8 },   // UART_TXD_DEBUG
  [MICO_GPIO_9]                       = { MBED_GPIO_9 },   // EASY LINK
  [MICO_GPIO_12]                      = { MBED_GPIO_12 },   // UART_RXD_DEBUG
  [MICO_GPIO_14]                      = { MBED_GPIO_14 },   // WL_HOST_WAKE
  [MICO_GPIO_16]                      = { MBED_GPIO_16 },
  [MICO_GPIO_17]                      = { MBED_GPIO_17 },   // I2C_SCL
  [MICO_GPIO_18]                      = { MBED_GPIO_18 },   // I2C_SDA
  [MICO_GPIO_19]                      = { MBED_GPIO_19 },
  [MICO_GPIO_27]                      = { MBED_GPIO_27 },   //
  [MICO_GPIO_29]                      = { MBED_GPIO_29 },   // UART_RXD_USER
  [MICO_GPIO_30]                      = { MBED_GPIO_30 },   // UART_TXD_USER
  [MICO_GPIO_31]                      = { MBED_GPIO_31 },   // RF LED
  [MICO_GPIO_33]                      = { MBED_GPIO_33 },   // SYS LED
  [MICO_GPIO_34]                      = { MBED_GPIO_34 },
  [MICO_GPIO_35]                      = { MBED_GPIO_35 },
  [MICO_GPIO_36]                      = { MBED_GPIO_36 },
  [MICO_GPIO_37]                      = { MBED_GPIO_37 },
  [MICO_GPIO_38]                      = { MBED_GPIO_38 },   // ADC_1
};

platform_gpio_driver_t      platform_gpio_drivers[MICO_GPIO_MAX];
platform_gpio_irq_driver_t  platform_gpio_irq_drivers[MICO_GPIO_MAX];


// const platform_pwm_t *platform_pwm_peripherals = NULL;

// const platform_adc_t platform_adc_peripherals[] =
// {
//   [MICO_ADC_1] = { ADC1, ADC_Channel_4, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_38] },
//   [MICO_ADC_2] = { ADC1, ADC_Channel_5, RCC_APB2Periph_ADC1, 1, (platform_gpio_t*)&platform_gpio_pins[MICO_GPIO_34] },
// };

// const platform_i2c_t platform_i2c_peripherals[] =
// {
//   [MICO_I2C_1] =
//   {
//     .port                         = I2C1,
//     .pin_scl                      = &platform_gpio_pins[MICO_GPIO_17],
//     .pin_sda                      = &platform_gpio_pins[MICO_GPIO_18],
//     .peripheral_clock_reg         = RCC_APB1Periph_I2C1,
//     .tx_dma                       = DMA1,
//     .tx_dma_peripheral_clock      = RCC_AHB1Periph_DMA1,
//     .tx_dma_stream                = DMA1_Stream1,
//     .rx_dma_stream                = DMA1_Stream0,
//     .tx_dma_stream_id             = 1,
//     .rx_dma_stream_id             = 0,
//     .tx_dma_channel               = DMA_Channel_0,
//     .rx_dma_channel               = DMA_Channel_1,
//     .gpio_af_scl                  = GPIO_AF_I2C1,
//     .gpio_af_sda                  = GPIO_AF_I2C1
//   },
// };

// platform_i2c_driver_t platform_i2c_drivers[MICO_I2C_MAX];

const platform_uart_t platform_uart_peripherals[MICO_UART_MAX];
platform_uart_driver_t platform_uart_drivers[MICO_UART_MAX];

// const platform_spi_t platform_spi_peripherals[] =
// {
//   [MICO_SPI_1]  =
//   {
//     .port                         = SPI2,
//     .gpio_af                      = GPIO_AF_SPI2,
//     .peripheral_clock_reg         = RCC_APB1Periph_SPI2,
//     .peripheral_clock_func        = RCC_APB1PeriphClockCmd,
//     .pin_mosi                     = &platform_gpio_pins[MICO_GPIO_4],
//     .pin_miso                     = &platform_gpio_pins[MICO_GPIO_7],
//     .pin_clock                    = &platform_gpio_pins[MICO_GPIO_6],
//     .tx_dma =
//     {
//       .controller                 = DMA1,
//       .stream                     = DMA1_Stream4,
//       .channel                    = DMA_Channel_0,
//       .irq_vector                 = DMA1_Stream4_IRQn,
//       .complete_flags             = DMA_HISR_TCIF4,
//       .error_flags                = ( DMA_HISR_TEIF4 | DMA_HISR_FEIF4 ),
//     },
//     .rx_dma =
//     {
//       .controller                 = DMA1,
//       .stream                     = DMA1_Stream3,
//       .channel                    = DMA_Channel_0,
//       .irq_vector                 = DMA1_Stream3_IRQn,
//       .complete_flags             = DMA_LISR_TCIF3,
//       .error_flags                = ( DMA_LISR_TEIF3 | DMA_LISR_FEIF3 | DMA_LISR_DMEIF3 ),
//     },
//   }
// };

// platform_spi_driver_t platform_spi_drivers[MICO_SPI_MAX];


/* Flash memory devices */
const platform_flash_t platform_flash_peripherals[] =
{
   [MICO_FLASH_EMBEDDED] =
   {
     .flash_type                   = FLASH_TYPE_EMBEDDED,
     .flash_start_addr             = 0x08000000,
     .flash_length                 = 0x100000,
   },
   [MICO_FLASH_QSPI] =
   {
     .flash_type                   = FLASH_TYPE_QSPI,
     .flash_start_addr             = 0x000000,
     .flash_length                 = 0x200000,
   },
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
        .partition_length = 0x8000,    //32k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [MICO_PARTITION_APPLICATION] =
    {
        .partition_owner = MICO_FLASH_EMBEDDED,
        .partition_description = "Application",
        .partition_start_addr = 0x08008000,
        .partition_length = 0xF8000,   //992k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [MICO_PARTITION_ATE] =
    {
        .partition_owner = MICO_FLASH_EMBEDDED,
        .partition_description = "ATEFirmware",
        .partition_start_addr = 0x080A0000,
        .partition_length = 0x60000, //384k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [MICO_PARTITION_PARAMETER_1] =
    {
        .partition_owner = MICO_FLASH_QSPI,
        .partition_description = "PARAMETER1",
        .partition_start_addr = 0x0,
        .partition_length = 0x1000, // 4k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [MICO_PARTITION_PARAMETER_2] =
    {
        .partition_owner = MICO_FLASH_QSPI,
        .partition_description = "PARAMETER2",
        .partition_start_addr = 0x1000,
        .partition_length = 0x1000, //4k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [MICO_PARTITION_RF_FIRMWARE] =
    {
        .partition_owner = MICO_FLASH_QSPI,
        .partition_description = "RF Firmware",
        .partition_start_addr = 0x2000,
        .partition_length = 0x6E000,  //440k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [MICO_PARTITION_OTA_TEMP] =
    {
        .partition_owner = MICO_FLASH_QSPI,
        .partition_description = "OTA Storage",
        .partition_start_addr = 0x70000,
        .partition_length = 0x98000, //608k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [MICO_PARTITION_FILESYS] =
    {
        .partition_owner = MICO_FLASH_QSPI,
        .partition_description = "FILESYS",
        .partition_start_addr = 0x118000,
        .partition_length = 0x0E8000, //928k bytes
        .partition_options = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    }
};

// #if defined ( USE_MICO_SPI_FLASH )
// const mico_spi_device_t mico_spi_flash =
// {
//   .port        = MICO_SPI_1,
//   .chip_select = FLASH_PIN_SPI_CS,
//   .speed       = 40000000,
//   .mode        = (SPI_CLOCK_RISING_EDGE | SPI_CLOCK_IDLE_HIGH | SPI_USE_DMA | SPI_MSB_FIRST ),
//   .bits        = 8
// };
// #endif

/** Wi-Fi control pins. Used by platform/MCU/wlan_platform_common.c
 */
const platform_gpio_t wifi_control_pins[] =
{
    [WIFI_PIN_RESET      ] = { PA_8 },
    [WIFI_PIN_32K_CLK    ] = { PA_9 },
};

platform_gpio_driver_t      wifi_control_pin_drivers[WIFI_PIN_CONTROL_MAX];

/* Wi-Fi SDIO bus pins. Used by platform/MCU/STM32F4xx/wlan_bus_driver/wlan_SDIO.c */
const platform_gpio_t wifi_sdio_pins[] =
{
  [WIFI_PIN_SDIO_OOB_IRQ] = { SDIO_OOB_IRQ },
  [WIFI_PIN_SDIO_CLK    ] = { SDIO_CLK },
  [WIFI_PIN_SDIO_CMD    ] = { SDIO_CMD },
  [WIFI_PIN_SDIO_D0     ] = { SDIO_D0 },
  [WIFI_PIN_SDIO_D1     ] = { SDIO_D1 },
  [WIFI_PIN_SDIO_D2     ] = { SDIO_D2 },
  [WIFI_PIN_SDIO_D3     ] = { SDIO_D3 },
};

platform_gpio_driver_t      wifi_sdio_pin_drivers[WIFI_PIN_SDIO_MAX];


// /* Bluetooth control pins.*/
// static const platform_gpio_t internal_bt_control_pins[] =
// {
//     /* Reset pin unavailable */
//     [MICO_BT_PIN_POWER      ] = { GPIOC,  3 },
//     [MICO_BT_PIN_HOST_WAKE  ] = { GPIOC,  2 },
//     [MICO_BT_PIN_DEVICE_WAKE] = { GPIOC,  1 }
// };

// const platform_gpio_t* mico_bt_control_pins[] =
// {
//     /* Reset pin unavailable */
//     [MICO_BT_PIN_POWER      ] = &internal_bt_control_pins[MICO_BT_PIN_POWER      ],
//     [MICO_BT_PIN_HOST_WAKE  ] = &internal_bt_control_pins[MICO_BT_PIN_HOST_WAKE  ],
//     [MICO_BT_PIN_DEVICE_WAKE] = &internal_bt_control_pins[MICO_BT_PIN_DEVICE_WAKE],
//     [MICO_BT_PIN_RESET      ] = NULL,
// };

// /* Bluetooth UART pins.*/
// static const platform_gpio_t internal_bt_uart_pins[] =
// {
//     [MICO_BT_PIN_UART_TX ] = { GPIOA,  2 },
//     [MICO_BT_PIN_UART_RX ] = { GPIOA,  3 },
//     [MICO_BT_PIN_UART_CTS] = { GPIOA,  0 },
//     [MICO_BT_PIN_UART_RTS] = { GPIOA,  1 },
// };

// const platform_gpio_t* mico_bt_uart_pins[] =
// {
//     [MICO_BT_PIN_UART_TX ] = &internal_bt_uart_pins[MICO_BT_PIN_UART_TX ],
//     [MICO_BT_PIN_UART_RX ] = &internal_bt_uart_pins[MICO_BT_PIN_UART_RX ],
//     [MICO_BT_PIN_UART_CTS] = &internal_bt_uart_pins[MICO_BT_PIN_UART_CTS],
//     [MICO_BT_PIN_UART_RTS] = &internal_bt_uart_pins[MICO_BT_PIN_UART_RTS],
// };

// static const platform_uart_t internal_bt_uart_peripheral =
// {
//   .port                         = USART2,
//   .pin_tx                       = &internal_bt_uart_pins[MICO_BT_PIN_UART_TX ],
//   .pin_rx                       = &internal_bt_uart_pins[MICO_BT_PIN_UART_RX ],
//   .pin_cts                      = &internal_bt_uart_pins[MICO_BT_PIN_UART_CTS ],
//   .pin_rts                      = &internal_bt_uart_pins[MICO_BT_PIN_UART_RTS ],
//   .tx_dma_config =
//   {
//     .controller                 = DMA1,
//     .stream                     = DMA1_Stream6,
//     .channel                    = DMA_Channel_4,
//     .irq_vector                 = DMA1_Stream6_IRQn,
//     .complete_flags             = DMA_HISR_TCIF6,
//     .error_flags                = ( DMA_HISR_TEIF6 | DMA_HISR_FEIF6 ),
//   },
//   .rx_dma_config =
//   {
//     .controller                 = DMA1,
//     .stream                     = DMA1_Stream5,
//     .channel                    = DMA_Channel_4,
//     .irq_vector                 = DMA1_Stream5_IRQn,
//     .complete_flags             = DMA_HISR_TCIF5,
//     .error_flags                = ( DMA_HISR_TEIF5 | DMA_HISR_FEIF5 | DMA_HISR_DMEIF5 ),
//   },
// };

// static platform_uart_driver_t internal_bt_uart_driver;
// const platform_uart_t*        mico_bt_uart_peripheral = &internal_bt_uart_peripheral;
// platform_uart_driver_t*       mico_bt_uart_driver     = &internal_bt_uart_driver;


// /* Bluetooth UART configuration. Used by libraries/bluetooth/internal/bus/UART/bt_bus.c */
// const platform_uart_config_t mico_bt_uart_config =
// {
//     .baud_rate    = 115200,
//     .data_width   = DATA_WIDTH_8BIT,
//     .parity       = NO_PARITY,
//     .stop_bits    = STOP_BITS_1,
//     .flow_control = FLOW_CONTROL_CTS_RTS, //FLOW_CONTROL_DISABLED,
// };

// /*BT chip specific configuration information*/
// const platform_bluetooth_config_t mico_bt_config =
// {
//     .patchram_download_mode      = PATCHRAM_DOWNLOAD_MODE_MINIDRV_CMD,
//     .patchram_download_baud_rate = 115200,
//     .featured_baud_rate          = 3000000
// };


// /******************************************************
// *               Function Definitions
// ******************************************************/
void platform_init_peripheral_irq_priorities( void )
{
    NVIC_SetPriority( RTC_WKUP_IRQn,        1 );   /* RTC Wake-up event   */
    NVIC_SetPriority( SDIO_IRQn,            2 );   /* WLAN SDIO           */
    NVIC_SetPriority( DMA2_Stream3_IRQn,    3 );   /* WLAN SDIO DMA       */
    NVIC_SetPriority( USART6_IRQn,          6 );   /* MICO_UART_1         */
    NVIC_SetPriority( DMA2_Stream6_IRQn,    7 );   /* MICO_UART_1 TX DMA  */
    NVIC_SetPriority( DMA2_Stream1_IRQn,    7 );   /* MICO_UART_1 RX DMA  */
    NVIC_SetPriority( USART2_IRQn,          6 );   /* BT UART             */
    NVIC_SetPriority( DMA1_Stream5_IRQn,    7 );   /* BT UART RX DMA      */
    NVIC_SetPriority( DMA1_Stream6_IRQn,    7 );   /* BT UART TX DMA      */
    NVIC_SetPriority( EXTI0_IRQn,           14 );  /* GPIO                */
    NVIC_SetPriority( EXTI1_IRQn,           14 );  /* GPIO                */
    NVIC_SetPriority( EXTI2_IRQn,           14 );  /* GPIO                */
    NVIC_SetPriority( EXTI3_IRQn,           14 );  /* GPIO                */
    NVIC_SetPriority( EXTI4_IRQn,           14 );  /* GPIO                */
    NVIC_SetPriority( EXTI9_5_IRQn,         14 );  /* GPIO                */
    NVIC_SetPriority( EXTI15_10_IRQn,       14 );  /* GPIO                */
}

void mico_board_init( void )
{
    //button_init_t init;

    platform_init_peripheral_irq_priorities();

    /* Ensure 802.11 device is in reset. */
    host_platform_init( );

    /* Initialise leds */
    mico_gpio_init       ( (mico_gpio_t) MICO_SYS_LED, OUTPUT_PUSH_PULL );
    mico_gpio_output_low ( (mico_gpio_t) MICO_SYS_LED );
    mico_gpio_init       ( (mico_gpio_t) MICO_RF_LED, OUTPUT_OPEN_DRAIN_NO_PULL );
    mico_gpio_output_high( (mico_gpio_t) MICO_RF_LED );

    /* Initialise selection IOs */
    mico_gpio_init( (mico_gpio_t) BOOT_SEL, INPUT_PULL_UP );
    mico_gpio_init( (mico_gpio_t) MFG_SEL, INPUT_PULL_UP );

    //  Initialise EasyLink buttons
//    init.gpio = EasyLink_BUTTON;
//    init.pressed_func = PlatformEasyLinkButtonClickedCallback;
//    init.long_pressed_func = PlatformEasyLinkButtonLongPressedCallback;
//    init.long_pressed_timeout = 5000;

    //button_init( IOBUTTON_EASYLINK, init );

#ifdef USE_MiCOKit_EXT
    dc_motor_init( );
    dc_motor_set( 0 );

    rgb_led_init();
    rgb_led_open(0, 0, 0);
#endif
}

 void MicoSysLed(bool onoff)
 {
   if (onoff) {
       mico_gpio_output_low( (mico_gpio_t)MICO_SYS_LED );
   } else {
       mico_gpio_output_high( (mico_gpio_t)MICO_SYS_LED );
   }
 }

 void MicoRfLed(bool onoff)
 {
   if (onoff) {
       mico_gpio_output_low( (mico_gpio_t)MICO_RF_LED );
   } else {
       mico_gpio_output_high( (mico_gpio_t)MICO_RF_LED );
   }
 }

// bool MicoShouldEnterMFGMode(void)
// {
//   if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==false)
//     return true;
//   else
//   return false;
// }

// bool MicoShouldEnterBootloader(void)
// {
//   if(MicoGpioInputGet((mico_gpio_t)BOOT_SEL)==false && MicoGpioInputGet((mico_gpio_t)MFG_SEL)==true)
//     return true;
//   else
//   return false;
// }


