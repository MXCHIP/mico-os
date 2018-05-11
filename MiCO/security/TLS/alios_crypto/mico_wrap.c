/**
 ******************************************************************************
 * @file    mico_wrap.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-Aug-2018
 * @brief   AliOS security API wrapper functions.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2018 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico_common.h"
#include "mico_security.h"

#include "digest_algorithm.h"

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

/******************************************************
 *               Variables Definitions
 ******************************************************/


/******************************************************
 *               Function Definitions
 ******************************************************/

void InitMd5(md5_context *ctx)
{
    *ctx = digest_md5_init();
}


void Md5Update(md5_context *ctx, unsigned char *input, int ilen)
{
    digest_md5_update(*ctx,input,ilen);
}


void Md5Final(md5_context *ctx, unsigned char output[16])
{
    digest_md5_final(*ctx, output);
}



