
#ifndef __MICODRIVERMFIAUTH_H__
#define __MICODRIVERMFIAUTH_H__

#pragma once
#include "common.h"
#include "platform.h"

/** @addtogroup MICO_PLATFORM
* @{
*/


/** @defgroup MICO_MFIAUTH MICO MFiAuth
  * @brief  Provide APIs for Apple Authentication Coprocessor operations
  * @{
  */

/******************************************************
 *                   Macros
 ******************************************************/  

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/



/******************************************************
 *                 Function Declarations
 ******************************************************/

/** @brief     PlatformMFiAuthInitialize
 *
 *  @abstract Performs any platform-specific initialization needed. 
 *            Example: Bring up I2C interface for communication with
 *            the Apple Authentication Coprocessor.
 *
 *  @param   i2c            :  mico_i2c_t context    
 *
 *  @return    kNoErr        : on success.
 *  @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoMFiAuthInitialize( mico_i2c_t i2c );


/** @brief    PlatformMFiAuthFinalize
 *
 *  @abstract Performs any platform-specific cleanup needed. 
 *            Example: Bringing down the I2C interface for communication with
 *            the Apple Authentication Coprocessor.
 *
 *  @param    i2c            :  mico_i2c_t context    
 *
 *  @return   none
 */
void MicoMFiAuthFinalize( void );



/** @brief    PlatformMFiAuthCreateSignature
 *
 *  @abstract Create an RSA signature from the specified SHA-1 digest 
 *            using the Apple Authentication Coprocessor.
 *
 *  @param    inDigestPtr     :    Pointer to 20-byte SHA-1 digest.
 *  @param    inDigestLen     :    Number of bytes in the digest. Must be 20.
 *  @param    outSignaturePtr :    Receives malloc()'d ptr to RSA signature. Caller must free() on success.
 *  @param    outSignatureLen :    Receives number of bytes in RSA signature.  
 *
 *  @return    kNoErr         :    on success.
 *  @return    kGeneralErr    :    if an error occurred with any step
 */
OSStatus MicoMFiAuthCreateSignature( const  void      *inDigestPtr,
                                            size_t     inDigestLen,
                                            uint8_t  **outSignaturePtr,
                                            size_t    *outSignatureLen );



/** @brief    PlatformMFiAuthCopyCertificate
 *
 *  @abstract Copy the certificate from the Apple Authentication Coprocessor.
 *
 *  @param    outCertificatePtr:   Receives malloc()'d ptr to a DER-encoded PKCS#7 message containing the certificate.
                                    Caller must free() on success.
 *  @param    outCertificateLen:   Number of bytes in the DER-encoded certificate. 
 *
 *  @return    kNoErr         :    on success.
 *  @return    kGeneralErr    :    if an error occurred with any step
 */
OSStatus MicoMFiAuthCopyCertificate( uint8_t **outCertificatePtr, size_t *outCertificateLen );

/** @} */
/** @} */
#endif // __MICODRIVERMFIAUTH_H__


