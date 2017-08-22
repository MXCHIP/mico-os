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

#include "platform_peripheral.h"
#include "mico_board_conf.h"

/******************************************************
*               Function Declarations
******************************************************/

#ifndef MICO_DISABLE_MCU_POWERSAVE
static unsigned long  stop_mode_power_down_hook( unsigned long sleep_ms );
//static OSStatus       select_wut_prescaler_calculate_wakeup_time( unsigned long* wakeup_time, unsigned long sleep_ms, unsigned long* scale_factor );
#else
static unsigned long  idle_power_down_hook( unsigned long sleep_ms );
#endif


/******************************************************
 *               Function Definitions
 ******************************************************/

OSStatus platform_mcu_powersave_disable( void )
{
    return kUnsupportedErr;
}

OSStatus platform_mcu_powersave_enable( void )
{
    return kUnsupportedErr;
}


/******************************************************
 *               RTOS Powersave Hooks
 ******************************************************/

void platform_idle_hook( void )
{
    __asm("wfi");
}

uint32_t platform_power_down_hook( uint32_t sleep_ms )
{
#ifdef MICO_DISABLE_MCU_POWERSAVE
    /* If MCU powersave feature is disabled, enter idle mode when powerdown hook is called by the RTOS */
    return idle_power_down_hook( sleep_ms );

#else
    /* If MCU powersave feature is enabled, enter STOP mode when powerdown hook is called by the RTOS */
    return stop_mode_power_down_hook( sleep_ms );

#endif
}

#ifdef MICO_DISABLE_MCU_POWERSAVE
/* MCU Powersave is disabled */
static unsigned long idle_power_down_hook( unsigned long sleep_ms  )
{
    UNUSED_PARAMETER( sleep_ms );
    __enable_irq();
    __asm("wfi");
    return 0;
}
#else
static unsigned long stop_mode_power_down_hook( unsigned long sleep_ms )
{
    UNUSED_PARAMETER( sleep_ms );
    __enable_irq();
    __asm("wfi");
    return 0;
}
#endif


#if 0
#ifdef MICO_DISABLE_MCU_POWERSAVE
/* MCU Powersave is disabled */
static unsigned long idle_power_down_hook( unsigned long sleep_ms  )
{
    UNUSED_PARAMETER( sleep_ms );
    ENABLE_INTERRUPTS;
    __asm("wfi");
    return 0;
}
#else
static unsigned long stop_mode_power_down_hook( unsigned long sleep_ms )
{
  unsigned long retval;
  unsigned long wut_ticks_passed;
  unsigned long scale_factor = 0;
  UNUSED_PARAMETER(sleep_ms);
  UNUSED_PARAMETER(rtc_timeout_start_time);
  UNUSED_PARAMETER(scale_factor);

  if ( ( ( SCB->SCR & (unsigned long)SCB_SCR_SLEEPDEEP_Msk) != 0) && sleep_ms < 5 ){
    SCB->SCR &= (~((unsigned long)SCB_SCR_SLEEPDEEP_Msk));
    __asm("wfi");
    SCB->SCR |= SCB_SCR_SLEEPDEEP_Msk;
    /* Note: We return 0 ticks passed because system tick is still going when wfi instruction gets executed */
    ENABLE_INTERRUPTS;
    return 0;
  }

  if ( ( ( SCB->SCR & (unsigned long)SCB_SCR_SLEEPDEEP_Msk) ) != 0 )
  {
    /* pick up the appropriate prescaler for a requested delay */
    select_wut_prescaler_calculate_wakeup_time(&rtc_timeout_start_time, sleep_ms, &scale_factor );

    DISABLE_INTERRUPTS;

    SysTick->CTRL &= (~(SysTick_CTRL_TICKINT_Msk|SysTick_CTRL_ENABLE_Msk)); /* systick IRQ off */
    RTC_ITConfig(RTC_IT_WUT, ENABLE);

    EXTI_ClearITPendingBit( RTC_INTERRUPT_EXTI_LINE );
    PWR_ClearFlag(PWR_FLAG_WU);
    RTC_ClearFlag(RTC_FLAG_WUTF);

    RTC_SetWakeUpCounter( rtc_timeout_start_time );
    RTC_WakeUpCmd( ENABLE );
    platform_rtc_enter_powersave();

    DBGMCU->CR |= 0x03; /* Enable debug in stop mode */

    /* This code will be running with BASEPRI register value set to 0, the main intention behind that is that */
    /* all interrupts must be allowed to wake the CPU from the power-down mode */
    /* the PRIMASK is set to 1( see DISABLE_INTERRUPTS), thus we disable all interrupts before entering the power-down mode */
    /* This may sound contradictory, however according to the ARM CM3 documentation power-management unit */
    /* takes into account only the contents of the BASEPRI register and it is an external from the CPU core unit */
    /* PRIMASK register value doesn't affect its operation. */
    /* So, if the interrupt has been triggered just before the wfi instruction */
    /* it remains pending and wfi instruction will be treated as a nop  */
    __asm("wfi");

    /* After CPU exits powerdown mode, the processer will not execute the interrupt handler(PRIMASK is set to 1) */
    /* Disable rtc for now */
    RTC_WakeUpCmd( DISABLE );
    RTC_ITConfig(RTC_IT_WUT, DISABLE);

    /* Initialise the clocks again */
    init_clocks( );

    /* Enable CPU ticks */
    SysTick->CTRL |= (SysTick_CTRL_TICKINT_Msk|SysTick_CTRL_ENABLE_Msk);

    /* Get the time of how long the sleep lasted */
    wut_ticks_passed = rtc_timeout_start_time - RTC_GetWakeUpCounter();
    UNUSED_VARIABLE(wut_ticks_passed);
    platform_rtc_exit_powersave( sleep_ms, (uint32_t *)&retval );
    /* as soon as interrupts are enabled, we will go and execute the interrupt handler */
    /* which triggered a wake up event */
    ENABLE_INTERRUPTS;
    wake_up_interrupt_triggered = false;
    return retval;
  }
  else
  {
    UNUSED_PARAMETER(wut_ticks_passed);
    ENABLE_INTERRUPTS;
    __asm("wfi");

    /* Note: We return 0 ticks passed because system tick is still going when wfi instruction gets executed */
    return 0;
  }
}
#endif /* MICO_DISABLE_MCU_POWERSAVE */


void platform_mcu_enter_standby(uint32_t secondsToWakeup)
{
  platform_rtc_time_t time;
  uint32_t currentSecond;
  RTC_AlarmTypeDef  RTC_AlarmStructure;

#if defined(STM32F410xx) || defined(STM32F412xG) ||defined(STM32F446xx)
  PWR_WakeUpPinCmd(PWR_WakeUp_Pin,ENABLE);
#endif

#if defined(STM32F40_41xxx) || defined(STM32F427_437xx) || defined(STM32F429_439xx) || defined(STM32F401xx) || defined(STM32F411xE)
  PWR_WakeUpPinCmd(ENABLE);
#endif

  if(secondsToWakeup == MICO_WAIT_FOREVER)
    PWR_EnterSTANDBYMode();

  platform_log("Wake up in %ld seconds", secondsToWakeup);

  platform_rtc_get_time(&time);
  currentSecond = time.hr*3600 + time.min*60 + time.sec;
  currentSecond += secondsToWakeup;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_H12     = RTC_HourFormat_24;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_Hours   = currentSecond/3600%24;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_Minutes = currentSecond/60%60;
  RTC_AlarmStructure.RTC_AlarmTime.RTC_Seconds = currentSecond%60;
  RTC_AlarmStructure.RTC_AlarmDateWeekDay = 0x31;
  RTC_AlarmStructure.RTC_AlarmDateWeekDaySel = RTC_AlarmDateWeekDaySel_Date;
  RTC_AlarmStructure.RTC_AlarmMask = RTC_AlarmMask_DateWeekDay ;

  RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
  /* Disable the Alarm A */
  RTC_ITConfig(RTC_IT_ALRA, DISABLE);

  /* Clear RTC Alarm Flag */
  RTC_ClearFlag(RTC_FLAG_ALRAF);

  RTC_SetAlarm(RTC_Format_BIN, RTC_Alarm_A, &RTC_AlarmStructure);

  /* Enable RTC Alarm A Interrupt: this Interrupt will wake-up the system from
     STANDBY mode (RTC Alarm IT not enabled in NVIC) */
  RTC_ITConfig(RTC_IT_ALRA, ENABLE);

  /* Enable the Alarm A */
  RTC_AlarmCmd(RTC_Alarm_A, ENABLE);

  PWR_EnterSTANDBYMode();
}


/******************************************************
 *         IRQ Handlers Definition & Mapping
 ******************************************************/

#ifndef MICO_DISABLE_MCU_POWERSAVE
MICO_RTOS_DEFINE_ISR( RTC_WKUP_irq )
{
    EXTI_ClearITPendingBit( RTC_INTERRUPT_EXTI_LINE );
}
#endif

#endif
