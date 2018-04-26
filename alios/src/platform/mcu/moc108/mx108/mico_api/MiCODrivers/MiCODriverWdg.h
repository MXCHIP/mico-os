
#ifndef __MICODRIVERWDG_H__
#define __MICODRIVERWDG_H__

#pragma once
#include "common.h"
#include "platform.h"

/** @addtogroup MICO_PLATFORM
* @{
*/

/** @defgroup MICO_WDG MICO WDG Driver
* @brief  Hardware watch dog Functions (WDG) Functions
* @note   These Functions are used by MICO's system monitor service, If any defined system monitor 
*          cannot be reloaded for some reason, system monitor service use this hardware watch dog 
*          to perform a system reset. So the watch dog reload time should be greater than system 
*          monitor's refresh time.
* @{
*/

/******************************************************
 *                   Macros
 ******************************************************/  

#define PlatformWDGInitialize   MicoWdgInitialize /**< For API compatiable with older version */
#define PlatformWDGReload       MicoWdgReload     /**< For API compatiable with older version */
#define PlatformWDGFinalize     MicoWdgFinalize   /**< For API compatiable with older version */

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                Type Definitions
 ******************************************************/

 /******************************************************
 *                    Structures
 ******************************************************/


/******************************************************
 *                     Variables
 ******************************************************/

/******************************************************
 *              Function Declarations
 ******************************************************/


/**
 * @biref This function will initialize the on board CPU hardware watch dog
 *
 * @param timeout        : Watchdag timeout, application should call MicoWdgReload befor timeout.
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoWdgInitialize( uint32_t timeout );

/**
 * @biref Reload watchdog counter.
 *
 * @param     none
 * @return    none
 */
void MicoWdgReload( void );

/**
 * @biref This function performs any platform-specific cleanup needed for hardware watch dog.
 *
 * @param     none
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoWdgFinalize( void );

/** @} */
/** @} */

#endif


