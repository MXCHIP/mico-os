
#ifndef __MICODRIVERPWM_H__
#define __MICODRIVERPWM_H__

#pragma once
#include "BkDriverPwm.h"

/** @addtogroup MICO_PLATFORM
  * @{
  */

/** @defgroup MICO_PWM MICO PWM Driver
  * @brief  Pulse-Width Modulation (PWM) Functions
  * @{
  */

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/
typedef bk_pwm_t       mico_pwm_t;

 /******************************************************
 *                 Function Declarations
 ******************************************************/



/**@brief Initialises a PWM pin
 *
 * @note  Prepares a Pulse-Width Modulation pin for use.
 * Does not start the PWM output (use @ref MicoPwmStart).
 *
 * @param pwm        : the PWM interface which should be initialised
 * @param frequency  : Output signal frequency in Hertz
 * @param duty_cycle : Duty cycle of signal as a floating-point percentage (0.0 to 100.0)
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoPwmInitialize(mico_pwm_t pwm, uint32_t frequency, float duty_cycle);


/**@brief Starts PWM output on a PWM interface
 *
 * @note  Starts Pulse-Width Modulation signal output on a PWM pin
 *
 * @param pwm        : the PWM interface which should be started
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoPwmStart(mico_pwm_t pwm);


/**@brief Stops output on a PWM pin
 *
 * @note  Stops Pulse-Width Modulation signal output on a PWM pin
 *
 * @param pwm        : the PWM interface which should be stopped
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoPwmStop(mico_pwm_t pwm);

/** @} */
/** @} */

#endif
