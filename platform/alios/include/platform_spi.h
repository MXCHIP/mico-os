/**
 ******************************************************************************
 * @file    platform_spi.h
 * @author  William Xu
 * @version V1.0.0
 * @date    01-May-2018
 * @brief   This file provide mico platform driver header file
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2018 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */
#ifndef __PLATFORM_SPI_H__
#define __PLATFORM_SPI_H__


#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/* SPI mode constants */
#define SPI_CLOCK_RISING_EDGE  ( 1 << 0 )
#define SPI_CLOCK_FALLING_EDGE ( 0 << 0 )
#define SPI_CLOCK_IDLE_HIGH    ( 1 << 1 )
#define SPI_CLOCK_IDLE_LOW     ( 0 << 1 )
#define SPI_USE_DMA            ( 1 << 2 )
#define SPI_NO_DMA             ( 0 << 2 )
#define SPI_MSB_FIRST          ( 1 << 3 )
#define SPI_LSB_FIRST          ( 0 << 3 )


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
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/**
 * SPI message segment
 */
typedef struct {
    const void* tx_buffer;
    void*       rx_buffer;
    uint32_t    length;
} platform_spi_message_segment_t;

/**
 * SPI configuration
 */
typedef struct
{
    uint32_t                speed;
    uint8_t                 mode;
    uint8_t                 bits;
    const void*             cs;
    void*                   cs_drv;
} platform_spi_config_t;

/**
 * SPI slave transfer direction
 */
typedef enum {
    SPI_SLAVE_TRANSFER_WRITE, /* SPI master writes data to the SPI slave device */
    SPI_SLAVE_TRANSFER_READ   /* SPI master reads data from the SPI slave device */
} platform_spi_slave_transfer_direction_t;

typedef enum {
    SPI_SLAVE_TRANSFER_SUCCESS,             /* SPI transfer successful */
    SPI_SLAVE_TRANSFER_INVALID_COMMAND,     /* Command is invalid */
    SPI_SLAVE_TRANSFER_ADDRESS_UNAVAILABLE, /* Address specified in the command is unavailable */
    SPI_SLAVE_TRANSFER_LENGTH_MISMATCH,     /* Length specified in the command doesn't match with the actual data length */
    SPI_SLAVE_TRANSFER_READ_NOT_ALLOWED,    /* Read operation is not allowed for the address specified */
    SPI_SLAVE_TRANSFER_WRITE_NOT_ALLOWED,   /* Write operation is not allowed for the address specified */
    SPI_SLAVE_TRANSFER_HARDWARE_ERROR,      /* Hardware error occurred during transfer */
    SPI_SLAVE_TRANSFER_STATUS_MAX = 0xff,   /* Denotes maximum value. Not a valid status */
} platform_spi_slave_transfer_status_t;

typedef struct
{
    int          port;
    int          chip_select;
    uint32_t     speed;
    uint8_t      mode;
    uint8_t      bits;
} platform_spi_device_t;


/******************************************************
 *                    Structures
 ******************************************************/


/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/




#ifdef __cplusplus
} /*"C" */
#endif

#endif





