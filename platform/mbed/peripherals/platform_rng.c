/**
 ******************************************************************************
 * @file    platform_rng.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide RNG driver functions.
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
#include "mico_rtos.h"

#if DEVICE_TRNG
#include "trng_api.h"
#endif

/******************************************************
 *                   Macros
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
 *                     Variables
 ******************************************************/
#if DEVICE_TRNG
static trng_t trng_driver;
#endif
/******************************************************
 *               Function Declarations
 ******************************************************/

OSStatus platform_random_number_read( void *output, int length )
{
#if DEVICE_TRNG
    size_t output_length = 0;
    trng_init(&trng_driver);
    trng_get_bytes(&trng_driver, output, length, &output_length);
#else
    int idx;
    uint32_t *pWord = output;
    uint32_t tempRDM;
    uint8_t *pByte = NULL;
    int inWordCount;
    int remainByteCount;

    inWordCount = length/4;
    remainByteCount = length%4;
    pByte = (uint8_t *)pWord+inWordCount*4;

    for(idx = 0; idx<inWordCount; idx++, pWord++){
        srand(mico_rtos_get_time());
        *pWord = rand();
    }

    if(remainByteCount){
        srand(mico_rtos_get_time());
        tempRDM = rand();
        memcpy(pByte, &tempRDM, (size_t)remainByteCount);
    }
    
    return kNoErr;
#endif
}
