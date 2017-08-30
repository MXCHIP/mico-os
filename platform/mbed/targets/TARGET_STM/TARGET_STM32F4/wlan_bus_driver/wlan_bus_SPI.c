/**
 ******************************************************************************
 * @file    wlan_bus_spi.h
 * @author  William Xu
 * @version V1.0.0
 * @date    16-Sep-2014
 * @brief   This file provides api bus communication functions with Wi-Fi RF chip.
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */


#include "mico.h"
#include "wlan_platform_common.h"

/******************************************************
 *             Constants
 ******************************************************/

#define DMA_TIMEOUT_LOOPS      (10000000)

/**
 * Transfer direction for the mico platform bus interface
 */
typedef enum
{
    /* If updating this enum, the bus_direction_mapping variable will also need to be updated */
    BUS_READ,
    BUS_WRITE
} bus_transfer_direction_t;

enum {
    MBED_SPI_CLOCK_IDLE_LOW  = 0,        // the SCLK is Low when SPI is inactive
    MBED_SPI_CLOCK_IDLE_HIGH = 2         // the SCLK is High when SPI is inactive
};

enum {
    MBED_SPI_CPHA_1Edge         = 0,      // Serial Clk toggle at middle of 1st data bit and latch data at 1st Clk edge
    MBED_SPI_CPHA_2Edge         = 1        // Serial Clk toggle at start of 1st data bit and latch data at 2nd Clk edge
};

/******************************************************
 *             Structures
 ******************************************************/

/******************************************************
 *             Variables
 ******************************************************/

static platform_spi_driver_t  wlan_spi_driver;
static platform_gpio_irq_driver_t wlan_irq_driver;
static platform_gpio_driver_t wifi_spi_pin_drivers[WIFI_PIN_SPI_MAX];

static mico_mutex_t Spi_mutex = NULL;
static bool spi_initialized = false;

static mico_semaphore_t spi_transfer_finished_semaphore = NULL;

extern const mico_spi_device_t wifi_spi_device;

/******************************************************
 *             Function declarations
 ******************************************************/
extern void wlan_notify_irq( void );

/******************************************************
 *             Function definitions
 ******************************************************/

/******************************************************************************************
 *             Wlan spi driver interface, called by wlan driver
 ******************************************************************************************/

static void spi_irq_handler( void* arg )
{
    UNUSED_PARAMETER(arg);
#ifndef MICO_DISABLE_MCU_POWERSAVE
    //platform_mcu_powersave_exit_notify( );
#endif /* ifndef MICO_DISABLE_MCU_POWERSAVE */
    wlan_notify_irq( );
}

/* This function is called by wlan driver */
OSStatus host_platform_bus_init( void )
{
    /* Setup the interrupt input for WLAN_IRQ */
    platform_gpio_init( &wifi_spi_pin_drivers[WIFI_PIN_SPI_IRQ], &wifi_spi_pins[WIFI_PIN_SPI_IRQ], INPUT_HIGH_IMPEDANCE );
    platform_gpio_irq_enable( &wlan_irq_driver, &wifi_spi_pins[WIFI_PIN_SPI_IRQ], IRQ_TRIGGER_RISING_EDGE, spi_irq_handler, 0 );

    return MicoSpiInitialize( &wifi_spi_device );
}

/* This function is called by wlan driver */
OSStatus host_platform_bus_deinit( void )
{
    return MicoSpiFinalize( &wifi_spi_device );
}

OSStatus host_platform_spi_transfer( bus_transfer_direction_t dir, uint8_t* buffer, uint16_t buffer_length )
{
    platform_spi_message_segment_t segment = { buffer, NULL, buffer_length };

    if( dir == BUS_READ)
        segment.rx_buffer = buffer;

    return MicoSpiTransfer( &wifi_spi_device, &segment, 1 );
}




