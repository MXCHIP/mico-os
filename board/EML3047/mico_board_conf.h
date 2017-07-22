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

#ifndef __MICO_BOARD_CONF_H__
#define __MICO_BOARD_CONF_H__

#ifdef __cplusplus
extern "C"
{
#endif


/******************************************************
*                      Macros
******************************************************/

/******************************************************
*                    Constants
******************************************************/

#define HARDWARE_REVISION   "1.0"
#define DEFAULT_NAME        "EML3047"
#define MODEL               "EML3047"


/* MICO RTOS tick rate in Hz */
// #define MICO_DEFAULT_TICK_RATE_HZ                   (1000) 

/************************************************************************
 * Uncomment to disable watchdog. For debugging only */
//#define MICO_DISABLE_WATCHDOG

/************************************************************************
 * Uncomment to disable standard IO, i.e. printf(), etc. */
//#define MICO_DISABLE_STDIO

/************************************************************************
 * Uncomment to disable MCU powersave API functions */
//#define MICO_DISABLE_MCU_POWERSAVE

/************************************************************************
 * Enable press space go to boot */
#define MICO_ENABLE_STDIO_TO_BOOT

/***********************************************************************
 * Enable press '#' go to MFG */
#define MICO_ENABLE_STDIO_TO_MFG

/************************************************************************
 * Uncomment to enable MCU real time clock */
// #define MICO_ENABLE_MCU_RTC

/************************************************************************
 * Restore default and start easylink after press down EasyLink button for 3 seconds. */
// #define RestoreDefault_TimeOut                      (3000)

/************************************************************************
 * Restore default and start easylink after press down EasyLink button for 3 seconds. */
#define MCU_CLOCK_HZ            (32000000)

/************************************************************************
 * How many bits are used in NVIC priority configuration */
// #define CORTEX_NVIC_PRIO_BITS   (4)

/************************************************************************
 * Enable write protection to write-disabled embedded flash sectors */
//#define MCU_ENABLE_FLASH_PROTECT

/************************************************************************
 * Enable write protection to write-disabled embedded flash sectors */
#define PARAMETER_PARTITION_SIZE     (1024)


#ifdef __cplusplus
} /*extern "C" */
#endif

#endif
