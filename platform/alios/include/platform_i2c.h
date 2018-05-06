/**
 ******************************************************************************
 * @file    platform_i2c.h
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
#ifndef __PLATFORM_I2C_H__
#define __PLATFORM_I2C_H__

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
 * I2C address width
 */
typedef enum {
    I2C_ADDRESS_WIDTH_7BIT,
    I2C_ADDRESS_WIDTH_10BIT,
    I2C_ADDRESS_WIDTH_16BIT,
} platform_i2c_bus_address_width_t;

/**
 * I2C speed mode
 */
typedef enum {
    I2C_LOW_SPEED_MODE,         /* 10Khz devices */
    I2C_STANDARD_SPEED_MODE,    /* 100Khz devices */
    I2C_HIGH_SPEED_MODE         /* 400Khz devices */
} platform_i2c_speed_mode_t;


/**
 * I2C configuration
 */
typedef struct
{
    uint16_t                            address;       /* the address of the device on the i2c bus */
    platform_i2c_bus_address_width_t    address_width;
    uint8_t                             flags;
    platform_i2c_speed_mode_t           speed_mode;    /* speed mode the device operates in */
} platform_i2c_config_t;

/**
 * I2C message
 */
typedef struct
{
    const void*  tx_buffer;
    void*        rx_buffer;
    uint16_t     tx_length;
    uint16_t     rx_length;
    uint16_t     retries;    /* Number of times to retry the message */
    bool         combined;   /**< If set, this message is used for both tx and rx. */
    //uint8_t      flags;     /* MESSAGE_DISABLE_DMA : if set, this flag disables use of DMA for the message */
} platform_i2c_message_t;


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





