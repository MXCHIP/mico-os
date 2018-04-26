
#ifndef __MICODRIVERRTC_H__
#define __MICODRIVERRTC_H__

#pragma once
#include "common.h"
#include "platform.h"
#include "platform_peripheral.h"

/** @addtogroup MICO_PLATFORM
* @{
*/

/** @defgroup MICO_RTC MICO RTC Driver
* @brief  Real-time clock (RTC) Functions
* @{
*/

/******************************************************
 *                   Macros
 ******************************************************/  

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef platform_rtc_time_t           mico_rtc_time_t;

 /******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *                     Variables
 ******************************************************/

/******************************************************
                Function Declarations
 ******************************************************/



/**@brief This function will initialize the on board CPU real time clock
 *
 * @note  This function should be called by MICO system when initializing clocks, so
 *        It is not needed to be called by application
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
void MicoRtcInitialize(void);

/**@brief This function will return the value of time read from the on board CPU real time clock. Time value must be given in the format of
 * the structure mico_rtc_time_t
 *
 * @param time        : pointer to a time structure
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoRtcGetTime(mico_rtc_time_t* time);

/**@brief This function will set MCU RTC time to a new value. Time value must be given in the format of
 * the structure mico_rtc_time_t
 *
 * @param time        : pointer to a time structure
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoRtcSetTime(mico_rtc_time_t* time);

/** @} */
/** @} */

#endif


