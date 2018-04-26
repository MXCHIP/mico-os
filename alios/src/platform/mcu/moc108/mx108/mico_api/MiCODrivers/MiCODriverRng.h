
#ifndef __MICODRIVERRNG_H__
#define __MICODRIVERRNG_H__

#pragma once
#include "common.h"



/** @addtogroup MICO_PLATFORM
* @{
*/

/** @defgroup MICO_RNG MICO RNG Driver
 * @brief  Random Number Generater(RNG) Functions
 * @{
 */

/******************************************************
 *                  Macros
 ******************************************************/  

#define PlatformRandomBytes MicoRandomNumberRead   /**< For API compatible with older version */

/******************************************************
 *               Enumerations
 ******************************************************/


/******************************************************
 *                 Type Definitions
 ******************************************************/

 /******************************************************
 *                 Function Declarations
 ******************************************************/
 


/**@brief Fill in a memory buffer with random data
 *
 * @param inBuffer        : Point to a valid memory buffer, this function will fill 
                            in this memory with random numbers after executed
 * @param inByteCount     : Length of the memory buffer (bytes)
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoRandomNumberRead( void *inBuffer, int inByteCount );

/** @} */
/** @} */

#endif


