
#ifndef __MICODRIVERGPIO_H__
#define __MICODRIVERGPIO_H__

#pragma once
#include "common.h"
#include "platform.h"
#include "platform_peripheral.h"

/** @addtogroup MICO_PLATFORM
* @{
*/


/** @defgroup MICO_GPIO MICO GPIO Driver
* @brief  General Purpose Input/Output pin (GPIO) Functions
* @{
*/

/******************************************************
 *                   Macros
 ******************************************************/

#define MicoGpioInputGet mico_gpio_input_get

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/
typedef platform_pin_config_t                   mico_gpio_config_t;

typedef platform_gpio_irq_trigger_t             mico_gpio_irq_trigger_t;

typedef platform_gpio_irq_callback_t            mico_gpio_irq_handler_t;


/******************************************************
*                 Function Declarations
******************************************************/


/**@brief Initialises a GPIO pin
 *
 * @note  Prepares a GPIO pin for use.
 *
 * @param gpio          : the gpio pin which should be initialised
 * @param configuration : A structure containing the required
 *                        gpio configuration
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoGpioInitialize( mico_gpio_t gpio, mico_gpio_config_t configuration );


/**@brief DeInitialises a GPIO pin
 *
 * @note  Set a GPIO pin in default state.
 *
 * @param  gpio          : the gpio pin which should be deinitialised
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoGpioFinalize( mico_gpio_t gpio );


/**@brief Sets an output GPIO pin high
 *
 * @note  Using this function on a gpio pin which is set to input mode is undefined.
 *
 * @param gpio          : the gpio pin which should be set high
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoGpioOutputHigh( mico_gpio_t gpio );


/**@brief Sets an output GPIO pin low
 *
 * @note  Using this function on a gpio pin which is set to input mode is undefined.
 *
 * @param gpio          : the gpio pin which should be set low
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoGpioOutputLow( mico_gpio_t gpio );

/** Trigger an output GPIO pin
 *
 * Trigger an output GPIO pin's output. Using this function on a
 * gpio pin which is set to input mode is undefined.
 *
 * @param gpio          : the gpio pin which should be set low
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoGpioOutputTrigger( mico_gpio_t gpio );



/**@brief Get the state of an input GPIO pin
 *
 * @note Get the state of an input GPIO pin. Using this function on a
 * gpio pin which is set to output mode will return an undefined value.
 *
 * @param gpio          : the gpio pin which should be read
 *
 * @return    true  : if high
 * @return    fasle : if low
 */
bool MicoGpioInputGet( mico_gpio_t gpio );


/**@brief Enables an interrupt trigger for an input GPIO pin
 *
 * @note Enables an interrupt trigger for an input GPIO pin.
 * Using this function on a gpio pin which is set to
 * output mode is undefined.
 *
 * @param gpio    : the gpio pin which will provide the interrupt trigger
 * @param trigger : the type of trigger (rising/falling edge)
 * @param handler : a function pointer to the interrupt handler
 * @param arg     : an argument that will be passed to the
 *                  interrupt handler
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoGpioEnableIRQ( uint32_t gpio, mico_gpio_irq_trigger_t trigger, mico_gpio_irq_handler_t handler, void *arg );

/**@brief Disables an interrupt trigger for an input GPIO pin
 *
 * @note Disables an interrupt trigger for an input GPIO pin.
 * Using this function on a gpio pin which has not been set up
 * using @ref mico_gpio_input_irq_enable is undefined.
 *
 * @param gpio    : the gpio pin which provided the interrupt trigger
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoGpioDisableIRQ( uint32_t gpio );

/** @} */
/** @} */
OSStatus MicoGpioOp(char cmd, uint32_t id, char mode);
#endif


