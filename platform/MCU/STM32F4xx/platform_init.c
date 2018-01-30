/**
 ******************************************************************************
 * @file    platform_init.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide functions called by MICO to drive stm32f2xx
 *          platform: - e.g. power save, reboot, platform initialize
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
#include "mico_board.h"
#include "mico_board_conf.h"
#include "platform_logging.h"
#include <string.h> // For memcmp
#include "crt0.h"
#include "mico_rtos.h"
#include "platform_init.h"


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
*               Function Declarations
******************************************************/

extern OSStatus host_platform_init( void );

/******************************************************
*               Variables Definitions
******************************************************/
extern platform_uart_t platform_uart_peripherals[];
extern platform_uart_driver_t platform_uart_drivers[];

/******************************************************
*               Function Definitions
******************************************************/

void platform_mcu_reset( void )
{
  NVIC_SystemReset();
}

/* STM32F2 common clock initialisation function
* This brings up enough clocks to allow the processor to run quickly while initialising memory.
* Other platform specific clock init can be done in init_platform() or init_architecture()
*/

/* init_clocks is executed before rtos is start up */
void init_clocks( void )
{
  //RCC_DeInit( ); /* if not commented then the LSE PA8 output will be disabled and never comes up again */
  
  /* Configure Clocks */
  RCC_HSEConfig( HSE_SOURCE );
  RCC_WaitForHSEStartUp( );
  
  RCC_HCLKConfig( AHB_CLOCK_DIVIDER );
  RCC_PCLK2Config( APB2_CLOCK_DIVIDER );
  RCC_PCLK1Config( APB1_CLOCK_DIVIDER );
  
  /* Enable the PLL */
  FLASH_SetLatency( INT_FLASH_WAIT_STATE );
  FLASH_PrefetchBufferCmd( ENABLE );
  
  /* Use the clock configuration utility from ST to calculate these values
  * http://www.st.com/st-web-ui/static/active/en/st_prod_software_internet/resource/technical/software/utility/stsw-stm32090.zip
  */
#ifdef STM32F412xG
  RCC_PLLConfig( PLL_SOURCE, PLL_M_CONSTANT, PLL_N_CONSTANT, PLL_P_CONSTANT, PPL_Q_CONSTANT, PPL_R_CONSTANT );
#else
  RCC_PLLConfig( PLL_SOURCE, PLL_M_CONSTANT, PLL_N_CONSTANT, PLL_P_CONSTANT, PPL_Q_CONSTANT );
#endif
  RCC_PLLCmd( ENABLE );
  
  while ( RCC_GetFlagStatus( RCC_FLAG_PLLRDY ) == RESET )
  {
  }
  RCC_SYSCLKConfig( SYSTEM_CLOCK_SOURCE );
  
  while ( RCC_GetSYSCLKSource( ) != 0x08 )
  {
  }
  
  /* Configure HCLK clock as SysTick clock source. */
  SysTick_CLKSourceConfig( SYSTICK_CLOCK_SOURCE );
#ifdef NO_MICO_RTOS  
  SysTick_Config( SystemCoreClock / 1000 );
#endif
}

/* init_memory is executed before rtos is start up */
WEAK void init_memory( void )
{
  
}

/* init_architecture is executed before rtos is start up */
void init_architecture( void )
{
  uint8_t i;
  
  RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
  
   /*STM32 wakeup by watchdog in standby mode, re-enter standby mode in this situation*/
  if ( (PWR_GetFlagStatus(PWR_FLAG_SB) != RESET) && RCC_GetFlagStatus(RCC_FLAG_IWDGRST) != RESET){
     RCC_ClearFlag();
     PWR_EnterSTANDBYMode();
   }
  PWR_ClearFlag(PWR_FLAG_SB);
  
  /* Initialise the interrupt priorities to a priority lower than 0 so that the BASEPRI register can mask them */
  for ( i = 0; i < 81; i++ )
  {
    NVIC ->IP[i] = 0xff;
  }
  
  NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );

  platform_init_peripheral_irq_priorities();

  /* Initialise GPIO IRQ manager */
  platform_gpio_irq_manager_init();

  /* Initialise nanosecond clock counter */
  platform_init_nanosecond_clock();

#ifdef BOOTLOADER
  return;
#endif
  
  /* Initialise RTC */
  platform_rtc_init( );

#ifndef MICO_DISABLE_MCU_POWERSAVE
  /* Initialise MCU powersave */
  platform_mcu_powersave_init( );
#endif /* ifndef MICO_DISABLE_MCU_POWERSAVE */

  platform_mcu_powersave_disable( );
}

OSStatus stdio_hardfault( char* data, uint32_t size )
{
#ifndef MICO_DISABLE_STDIO
  uint32_t idx;
  for(idx = 0; idx < size; idx++){
    while ( ( platform_uart_peripherals[ MICO_STDIO_UART ].port->SR & USART_SR_TXE ) == 0 );
    platform_uart_peripherals[ MICO_STDIO_UART ].port->DR = (data[idx] & (uint16_t)0x01FF);
    
  }
#endif
  return kNoErr;
}

static char global_cid[25] = { 0 };
const char *mico_generate_cid( uint8_t* length )
{
  uint32_t temp0,temp1,temp2;
  uint8_t temp[12];
  temp0=*(volatile uint32_t*)(0x1FFF7A10);
  temp1=*(volatile uint32_t*)(0x1FFF7A14);
  temp2=*(volatile uint32_t*)(0x1FFF7A18);
  
  temp[0] = (uint8_t)(temp0 & 0x000000FF);
  temp[1] = (uint8_t)((temp0 & 0x0000FF00)>>8);
  temp[2] = (uint8_t)((temp0 & 0x00FF0000)>>16);
  temp[3] = (uint8_t)((temp0 & 0xFF000000)>>24);
  temp[4] = (uint8_t)(temp1 & 0x000000FF);
  temp[5] = (uint8_t)((temp1 & 0x0000FF00)>>8);
  temp[6] = (uint8_t)((temp1 & 0x00FF0000)>>16);
  temp[7] = (uint8_t)((temp1 & 0xFF000000)>>24);
  temp[8] = (uint8_t)(temp2 & 0x000000FF);
  temp[9] = (uint8_t)((temp2 & 0x0000FF00)>>8);
  temp[10] = (uint8_t)((temp2 & 0x00FF0000)>>16);
  temp[11] = (uint8_t)((temp2 & 0xFF000000)>>24);
  sprintf(global_cid, "%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X%.2X",
          temp[0], temp[1], temp[2], temp[3], temp[4], temp[5], temp[6],
          temp[7], temp[8], temp[9], temp[10], temp[11]);
  *length = 12;
  return global_cid;
}

bool isWakeUpFlagPowerOn(void){
  return (RCC_GetFlagStatus(RCC_FLAG_SFTRST) == RESET) ? true : false;
};

