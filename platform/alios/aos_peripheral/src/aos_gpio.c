/**
 ******************************************************************************
 * @file    platform_gpio.c
 * @author  William Xu
 * @version V1.0.0
 * @date    30-Apr-2018
 * @brief   This file provide GPIO driver functions.
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2018 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#ifdef ALIOS_DEV_GPIO

#include "aos_gpio.h"

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
*               Variables Definitions
******************************************************/

/******************************************************
*               Function Declarations
******************************************************/

/******************************************************
*               Function Definitions
******************************************************/

OSStatus platform_gpio_init( platform_gpio_driver_t* driver, const platform_gpio_t* gpio, platform_pin_config_t config )
{
    driver->port = gpio->port;
    driver->config = config;

    return (OSStatus) hal_gpio_init( driver );
}

OSStatus platform_gpio_deinit( platform_gpio_driver_t* driver )
{
    return (OSStatus) hal_gpio_deinit( driver );
}

OSStatus platform_gpio_output_high( platform_gpio_driver_t* driver )
{
    return (OSStatus) hal_gpio_output_high( driver );
}

OSStatus platform_gpio_output_low( platform_gpio_driver_t* driver )
{
    return (OSStatus) hal_gpio_output_low( driver );
}


OSStatus platform_gpio_output_trigger( platform_gpio_driver_t* driver )
{
    return (OSStatus) hal_gpio_output_toggle( driver );
}

bool platform_gpio_input_get( platform_gpio_driver_t* driver )
{
    uint32_t value;
    hal_gpio_input_get(driver, &value);
    return (value)? true:false;
}

OSStatus platform_gpio_irq_enable( platform_gpio_driver_t* irq_driver, const platform_gpio_t* gpio, platform_gpio_irq_trigger_t trigger, platform_gpio_irq_callback_t handler, void* arg )
{
    return hal_gpio_enable_irq(irq_driver, trigger, handler, arg);
}

OSStatus platform_gpio_irq_disable( platform_gpio_driver_t* irq_driver )
{
    return hal_gpio_clear_irq(irq_driver);
}

#endif

