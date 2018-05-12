/**
 ******************************************************************************
 * @file    aos_generic_gpio.h
 * @author  William Xu
 * @version V1.0.0
 * @date    01-May-2018
 * @brief   This file provide platform wrapper for AliOS GPIO HAL
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2018 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */


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



#ifndef __AOS_GPIO_H__
#define __AOS_GPIO_H__

#include "platform_logging.h"
#include <hal/soc/gpio.h>

#ifdef __cplusplus
extern "C" {
#endif


/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef struct {
    uint8_t       port;    /* gpio port */
} platform_gpio_t;

typedef gpio_dev_t platform_gpio_driver_t;

/**
 * Pin configuration
 */
typedef gpio_config_t platform_pin_config_t;

/**
 * GPIO interrupt trigger
 */
typedef gpio_irq_trigger_t platform_gpio_irq_trigger_t;

/**
 * GPIO interrupt callback handler
 */

typedef void (*platform_gpio_irq_callback_t)( void* arg );


/******************************************************
 *                    Structures
 ******************************************************/


/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

/**
 * Initialise the specified GPIO pin
 *
 * @param[in] gpio   : gpio pin
 * @param[in] config : pin configuration
 *
 * @return @ref OSStatus
 */
OSStatus platform_gpio_init( platform_gpio_driver_t* driver, const platform_gpio_t* gpio, platform_pin_config_t config );


/**
 * Deinitialise the specified GPIO pin
 *
 * @param[in] gpio : gpio pin
 *
 * @return @ref OSStatus
 */
OSStatus platform_gpio_deinit( platform_gpio_driver_t* driver );


/**
 * Toggle the specified GPIO pin output high
 *
 * @param[in] gpio : gpio pin
 *
 * @return @ref OSStatus
 */
OSStatus platform_gpio_output_high( platform_gpio_driver_t* driver );


/**
 * Toggle the specified GPIO pin output low
 *
 * @param[in] gpio : gpio pin
 *
 * @return @ref OSStatus
 */
OSStatus platform_gpio_output_low( platform_gpio_driver_t* driver );


/**
 * Toggle the specified GPIO pin
 *
 * @param[in] gpio : gpio pin
 *
 * @return @ref OSStatus
 */
OSStatus platform_gpio_output_trigger( platform_gpio_driver_t* driver );


/**
 * Retrieve logic level of the GPIO input pin specified
 *
 * @param[in] gpio : gpio pin
 *
 * @return @ref OSStatus
 */
bool platform_gpio_input_get( platform_gpio_driver_t* driver );


/**
 * Enable interrupt on the GPIO input pin specified
 *
 * @param[in] gpio    : gpio pin
 * @param[in] trigger : interrupt trigger type
 * @param[in] handler : callback function that will be called when an interrupt occurs
 * @param[in] arg     : argument that will be passed into the callback function
 *
 * @return @ref OSStatus
 */
OSStatus platform_gpio_irq_enable( platform_gpio_driver_t* irq_driver, const platform_gpio_t* gpio, platform_gpio_irq_trigger_t trigger, platform_gpio_irq_callback_t handler, void* arg );


/**
 * Disable interrupt on the GPIO input pin specified
 *
 * @param[in] gpio : gpio pin
 *
 * @return @ref OSStatus
 */
OSStatus platform_gpio_irq_disable( platform_gpio_driver_t* irq_driver );

#ifdef __cplusplus
} /*"C" */
#endif

#endif





