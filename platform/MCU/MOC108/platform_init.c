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
#include "mico_board_conf.h"
#include "platform_logging.h"
#include <string.h> // For memcmp
#include "crt0.h"
#include "mico_rtos.h"
#include "platform_init.h"
#include "portmacro.h"



/******************************************************
*                      Macros
******************************************************/

/******************************************************
*                    Constants
******************************************************/

#ifndef STDIO_BUFFER_SIZE
#define STDIO_BUFFER_SIZE   1024
#endif

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
extern void system_reload(void);
/******************************************************
*               Variables Definitions
******************************************************/

/******************************************************
*               Function Definitions
******************************************************/


void __jump_to( uint32_t addr )
{
 // addr |= 0x00000001;  /* Last bit of jump address indicates whether destination is Thumb or ARM code */
  __asm volatile ("BX %0" : : "r" (addr) );
}


void startApplication( uint32_t app_addr )
{
    intc_deinit();
    DISABLE_INTERRUPTS();
    __jump_to( app_addr );
}

void platform_mcu_reset( void )
{
    bk_wdg_initialize(1);
}

/* STM32F2 common clock initialisation function
* This brings up enough clocks to allow the processor to run quickly while initialising memory.
* Other platform specific clock init can be done in init_platform() or init_architecture()
*/
void init_clocks( void )
{
#ifdef NO_MICO_RTOS
    fclk_init();
#endif
}

WEAK void init_memory( void )
{
  
}

void init_architecture( void )
{
}

extern void entry_main(void);

void software_init_hook(void)
{
    entry_main();
    software_init_hook_rtos();
}

OSStatus stdio_hardfault( char* data, uint32_t size )
{
  return kNoErr;
}

static char global_cid[25] = { 0 };
const char *mico_generate_cid( uint8_t* length )
{
  return global_cid;
}

bool isWakeUpFlagPowerOn(void){
  return false;
};

/******************************************************
*            NO-OS Functions
******************************************************/

#ifdef NO_MICO_RTOS

uint32_t mico_get_time_no_os(void)
{
    return fclk_get_tick();
}
#endif


