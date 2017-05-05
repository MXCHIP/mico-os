/**
 ******************************************************************************
 * @file    platform_spi.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide SPI driver functions.
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico_debug.h"
#include "platform_peripheral.h"

#include "spi_api.h"
#include "dma_api.h"

/******************************************************
*                    Constants
******************************************************/

/******************************************************
*                   Enumerations
******************************************************/

enum {
    MBED_SPI_CLOCK_IDLE_LOW  = 0,        // the SCLK is Low when SPI is inactive
    MBED_SPI_CLOCK_IDLE_HIGH = 2         // the SCLK is High when SPI is inactive
};

enum {
    MBED_SPI_CPHA_1Edge         = 0,      // Serial Clk toggle at middle of 1st data bit and latch data at 1st Clk edge
    MBED_SPI_CPHA_2Edge         = 1        // Serial Clk toggle at start of 1st data bit and latch data at 2nd Clk edge
};

/******************************************************
*                 Type Definitions
******************************************************/

/******************************************************
*                    Structures
******************************************************/

/******************************************************
*               Static Function Declarations
******************************************************/

/******************************************************
*               Variables Definitions
******************************************************/

/******************************************************
*               Function Definitions
******************************************************/

static void irq_handler_asynch(void* context)
{
    platform_spi_driver_t* driver = (platform_spi_driver_t *)context;
    driver->event = spi_irq_handler_asynch(&driver->spi_obj);

    if ( (driver->event & (SPI_EVENT_ALL)) && driver->transfer_complete ) {
        // SPI peripheral is free (event happen), dequeue transaction
        mico_rtos_set_semaphore( &driver->transfer_complete );
    }
}

OSStatus platform_spi_init( platform_spi_driver_t* driver, const platform_spi_t* peripheral,
                            const platform_spi_config_t* config )
{
    OSStatus err = kNoErr;
    uint8_t polarity;
    uint8_t phase;

    platform_mcu_powersave_disable( );

    require_action_quiet( (driver != NULL) && (peripheral != NULL) && (config != NULL), exit, err = kParamErr );
    //require_action( !(config->mode & SPI_USE_DMA), exit, err = kUnsupportedErr);

    require_noerr( err, exit );

    driver->peripheral = (platform_spi_t*) peripheral;

    spi_init( (spi_t *)&driver->spi_obj, peripheral->mbed_mosi_pin, peripheral->mbed_miso_pin, peripheral->mbed_scl_pin, config->cs->mbed_pin );
    spi_frequency( (spi_t *)&driver->spi_obj, config->speed );

    if ( config->mode & SPI_CLOCK_IDLE_HIGH )
        polarity = MBED_SPI_CLOCK_IDLE_HIGH;
    else
        polarity = MBED_SPI_CLOCK_IDLE_LOW;

    if ( config->mode & SPI_CLOCK_RISING_EDGE )
        phase = (config->mode & SPI_CLOCK_IDLE_HIGH) ? MBED_SPI_CPHA_2Edge : MBED_SPI_CPHA_1Edge;
    else
        phase = (config->mode & SPI_CLOCK_IDLE_HIGH) ? MBED_SPI_CPHA_1Edge : MBED_SPI_CPHA_2Edge;

    spi_format( (spi_t *)&driver->spi_obj, config->bits, (polarity | phase), 0 );

    platform_gpio_init( config->cs_drv, config->cs, OUTPUT_PUSH_PULL );

#if DEVICE_SPI_ASYNCH
    platform_irq_init( &driver->irq, driver, irq_handler_asynch );

    if ( config->mode & SPI_USE_DMA ){
        err = mico_rtos_init_semaphore( &driver->transfer_complete, 0 );
    }
#endif

exit:
    platform_mcu_powersave_enable( );
    return err;
}


OSStatus platform_spi_deinit( platform_spi_driver_t* driver )
{
    OSStatus err = kNoErr;

#if DEVICE_SPI_ASYNCH
    platform_irq_deinit( &driver->irq );
    if( driver->transfer_complete ) err = mico_rtos_deinit_semaphore( &driver->transfer_complete );
#endif

    return err;
}


OSStatus platform_spi_transfer( platform_spi_driver_t* driver, const platform_spi_config_t* config,
                                const platform_spi_message_segment_t* segments,
                                uint16_t number_of_segments )
{

    OSStatus err = kNoErr;
    uint32_t count = 0;
    uint16_t i;

    platform_mcu_powersave_disable( );

    require_action_quiet( (driver != NULL) && (config != NULL) && (segments != NULL) && (number_of_segments != 0), exit,
                          err = kParamErr );

    /* Activate chip select */
    platform_gpio_output_low( config->cs_drv );

    for ( i = 0; i < number_of_segments; i++ ) {

        if( segments[ i ].length == 0) continue;
#if DEVICE_SPI_ASYNCH
        /* Check if we are using DMA/Async */
        if ( config->mode & SPI_USE_DMA )
        {
            if( segments[ i ].length != 0){

                spi_master_transfer( &driver->spi_obj,
                                     (const void *)segments[i].tx_buffer, segments[ i ].length,
                                     (void *)    segments[i].rx_buffer, segments[ i ].length, 
                                     config->bits,
                                     platform_irq_entry( &driver->irq ),
                                     SPI_EVENT_ALL,
                                     DMA_USAGE_ALWAYS);
                mico_rtos_get_semaphore( &driver->transfer_complete, MICO_WAIT_FOREVER );
                require_action( driver->event & (SPI_EVENT_ALL | SPI_EVENT_COMPLETE), cleanup_transfer, err = kInternalErr);
            }
        } 
        else
#endif
        {
            /* in interrupt-less mode */
            if ( config->bits == 8 ) {
                uint8_t* send_ptr = ( uint8_t* )segments[i].tx_buffer;
                uint8_t* recv_ptr  = ( uint8_t* )segments[i].rx_buffer;
        
                for( count = segments[i].length; count > 0; count-- )  {
                    uint16_t data = 0xFF;
          
                    if ( send_ptr != NULL ) data = *send_ptr++;
                    data = spi_master_write( (spi_t *)&driver->spi_obj, (int)data );
                    if ( recv_ptr != NULL ) *recv_ptr++ = data;
                }
            }
            else if ( config->bits == 16 ) {
                uint16_t* send_ptr = (uint16_t *) segments[i].tx_buffer;
                uint16_t* recv_ptr = (uint16_t *) segments[i].rx_buffer;
        
                /* Check that the message length is a multiple of 2 */
                require_action_quiet( ( count % 2 ) == 0, cleanup_transfer, err = kSizeErr);
        
                /* Transmit/receive data stream, 16-bit at time */
                for( count = segments[i].length; count > 0; count-=2 )  {
                    uint16_t data = 0xFFFF;
          
                    if ( send_ptr != NULL ) data = *send_ptr++;
                    data = spi_master_write( (spi_t *)&driver->spi_obj, (int)data );
                    if ( recv_ptr != NULL ) *recv_ptr++ = data;
                }
            }
        }
    }

    cleanup_transfer:
    ///* Deassert chip select */
    platform_gpio_output_high( config->cs_drv );

    exit:
    platform_mcu_powersave_enable( );
    return err;
}

