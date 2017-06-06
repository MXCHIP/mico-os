/**
 ******************************************************************************
 * @file    platform_pwm.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide PWM driver functions.
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "platform_peripheral.h"

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
OSStatus platform_pwm_power( platform_pwm_driver_t *driver,int enable );
/******************************************************
*               Function Definitions
******************************************************/

OSStatus platform_pwm_init( platform_pwm_driver_t *driver,const platform_pwm_t* pwm, uint32_t frequency, float duty_cycle )
{
  OSStatus err = kNoErr;
  int duty = 0;
  float adjusted_duty_cycle = ( ( duty_cycle > 100.0f ) ? 1.0f : duty_cycle/100 );
  require_action_quiet( pwm != NULL, exit, err = kParamErr);

  platform_mcu_powersave_disable();

  pwmout_init((pwmout_t *)&driver->pwm_obj,pwm->mbed_pwm_pin);

  /********************Set Frequency*********************/
  if(frequency/1000000){
      pwmout_period_us((pwmout_t *)&driver->pwm_obj,frequency);
  }else if(frequency/1000){
      pwmout_period_ms((pwmout_t *)&driver->pwm_obj,frequency);
  }else {
      pwmout_period((pwmout_t *)&driver->pwm_obj,frequency);
  }

  pwmout_write((pwmout_t *)&driver->pwm_obj,adjusted_duty_cycle);

  /******************Disable TIM clock********************/
  platform_pwm_power(driver,DISABLE);

exit:
  platform_mcu_powersave_enable();
  return err;
}

OSStatus platform_pwm_start( platform_pwm_driver_t* driver )
{
  OSStatus err = kNoErr;

  platform_mcu_powersave_disable();

  require_action_quiet( driver != NULL, exit, err = kParamErr);

  /*******************Enable TIM clock*********************/
  platform_pwm_power(driver,ENABLE);
  
exit:  
  platform_mcu_powersave_enable();
  return err;
}

OSStatus platform_pwm_stop( platform_pwm_driver_t *driver )
{
  OSStatus err = kNoErr;
  
  platform_mcu_powersave_disable();

  require_action_quiet( driver != NULL, exit, err = kParamErr);
  
  /******************Disable TIM clock********************/
  platform_pwm_power(driver,DISABLE);
  
exit:  
  platform_mcu_powersave_enable();
  return err;
}

OSStatus platform_pwm_power( platform_pwm_driver_t *driver,int enable )
{
     OSStatus err = kNoErr;

     platform_mcu_powersave_disable();

     require_action_quiet( driver != NULL, exit, err = kParamErr);

     /******************Set TIM clock********************/
    #if defined(TIM1_BASE)
      if (driver->pwm_obj.pwm == PWM_1){
          if(enable==ENABLE)
              __HAL_RCC_TIM1_CLK_ENABLE();
          else
              __HAL_RCC_TIM1_CLK_DISABLE();
      }
    #endif
    #if defined(TIM2_BASE)
      if (driver->pwm_obj.pwm == PWM_2) {
          if(enable==ENABLE)
              __HAL_RCC_TIM2_CLK_ENABLE();
          else
              __HAL_RCC_TIM2_CLK_DISABLE();
      }
    #endif
    #if defined(TIM3_BASE)
      if (driver->pwm_obj.pwm == PWM_3) {
          if(enable==ENABLE)
              __HAL_RCC_TIM3_CLK_ENABLE();
          else
              __HAL_RCC_TIM3_CLK_DISABLE();
      }
    #endif
    #if defined(TIM4_BASE)
      if (driver->pwm_obj.pwm == PWM_4) {
          if(enable==ENABLE)
              __HAL_RCC_TIM4_CLK_ENABLE();
          else
              __HAL_RCC_TIM4_CLK_DISABLE();
      }
    #endif
    #if defined(TIM5_BASE)
      if (driver->pwm_obj.pwm == PWM_5) {
          if(enable==ENABLE)
              __HAL_RCC_TIM5_CLK_ENABLE();
          else
              __HAL_RCC_TIM5_CLK_DISABLE();
      }
    #endif
    #if defined(TIM8_BASE)
      if (driver->pwm_obj.pwm == PWM_8) {
          if(enable==ENABLE)
              __HAL_RCC_TIM8_CLK_ENABLE();
          else
              __HAL_RCC_TIM8_CLK_DISABLE();
      }
    #endif
    #if defined(TIM9_BASE)
      if (driver->pwm_obj.pwm == PWM_9) {
          if(enable==ENABLE)
              __HAL_RCC_TIM9_CLK_ENABLE();
          else
              __HAL_RCC_TIM9_CLK_DISABLE();
      }
    #endif
    #if defined(TIM10_BASE)
      if (driver->pwm_obj.pwm == PWM_10) {
          if(enable==ENABLE)
              __HAL_RCC_TIM10_CLK_ENABLE();
          else
              __HAL_RCC_TIM10_CLK_DISABLE();
      }
    #endif
    #if defined(TIM11_BASE)
      if (driver->pwm_obj.pwm == PWM_11) {
          if(enable==ENABLE)
              __HAL_RCC_TIM11_CLK_ENABLE();
          else
              __HAL_RCC_TIM11_CLK_DISABLE();
      }
    #endif
    #if defined(TIM12_BASE)
      if (driver->pwm_obj.pwm == PWM_12) {
          if(enable==ENABLE)
              __HAL_RCC_TIM12_CLK_ENABLE();
          else
              __HAL_RCC_TIM12_CLK_DISABLE();
      }
    #endif
    #if defined(TIM13_BASE)
      if (driver->pwm_obj.pwm == PWM_13) {
          if(enable==ENABLE)
              __HAL_RCC_TIM13_CLK_ENABLE();
          else
              __HAL_RCC_TIM13_CLK_DISABLE();
      }
    #endif
    #if defined(TIM14_BASE)
      if (driver->pwm_obj.pwm == PWM_14) {
          if(enable==ENABLE)
              __HAL_RCC_TIM14_CLK_ENABLE();
          else
              __HAL_RCC_TIM14_CLK_DISABLE();
      }
    #endif
    #if defined(TIM15_BASE)
      if (driver->pwm_obj.pwm == PWM_15) {
          if(enable==ENABLE)
              __HAL_RCC_TIM15_CLK_ENABLE();
          else
              __HAL_RCC_TIM15_CLK_DISABLE();
      }
    #endif
    #if defined(TIM16_BASE)
      if (driver->pwm_obj.pwm == PWM_16) {
          if(enable==ENABLE)
              __HAL_RCC_TIM16_CLK_ENABLE();
          else
              __HAL_RCC_TIM16_CLK_DISABLE();
      }
    #endif
    #if defined(TIM17_BASE)
      if (driver->pwm_obj.pwm == PWM_17) {
          if(enable==ENABLE)
              __HAL_RCC_TIM17_CLK_ENABLE();
          else
              __HAL_RCC_TIM17_CLK_DISABLE();
      }
    #endif
    #if defined(TIM18_BASE)
      if (driver->pwm_obj.pwm == PWM_18) {
          if(enable==ENABLE)
              __HAL_RCC_TIM18_CLK_ENABLE();
          else
              __HAL_RCC_TIM18_CLK_DISABLE();
      }
    #endif
    #if defined(TIM19_BASE)
      if (driver->pwm_obj.pwm == PWM_19) {
          if(enable==ENABLE)
              __HAL_RCC_TIM19_CLK_ENABLE();
          else
              __HAL_RCC_TIM19_CLK_DISABLE();
      }
    #endif
    #if defined(TIM20_BASE)
      if (driver->pwm_obj.pwm == PWM_20) {
          if(enable==ENABLE)
              __HAL_RCC_TIM20_CLK_ENABLE();
          else
              __HAL_RCC_TIM20_CLK_DISABLE();
      }
    #endif
    #if defined(TIM21_BASE)
      if (driver->pwm_obj.pwm == PWM_21) {
          if(enable==ENABLE)
              __HAL_RCC_TIM21_CLK_ENABLE();
          else
              __HAL_RCC_TIM21_CLK_DISABLE();
      }
    #endif
    #if defined(TIM22_BASE)
      if (driver->pwm_obj.pwm == PWM_22) {
          if(enable==ENABLE)
              __HAL_RCC_TIM22_CLK_ENABLE();
          else
              __HAL_RCC_TIM22_CLK_DISABLE();
      }
    #endif

   exit:
     platform_mcu_powersave_enable();
     return err;
}
