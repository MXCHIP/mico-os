/**
 ******************************************************************************
 * @file    src.c
 * @author  William Xu
 * @version V1.0.0
 * @date    15-Feb-2017
 * @brief   This file provide a demo function, will be build into a library.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include <stdio.h>
#include "common.h"
#include "debug.h"

#ifdef DEBUG
#define lib_log(M, ...) custom_log("LIB", M, ##__VA_ARGS__)
#define lib_log_trace() custom_log_trace("LIB")
#else
#define lib_log(M, ...)
#define lib_log_trace()
#endif

void helloworld_lib_log( void )
{
    lib_log( "helloworld!" );
}
