/**
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 *
 */
/*
 * @file
 * Interface functions for Newlib libC implementation
 */
#include "mico_opt.h"
#include "platform_toolchain.h"

// ****************************************************************************
// mico_main is a function that is called before main()

#ifndef ALIOS_NATIVE_APP

WEAK void mico_main(void);
WEAK void mico_main(void) {
}

int application_start(int argc, char *argv[])
{
    mico_main();
    return __real_main();
}
#endif

struct _reent;

WEAK void __rtos_malloc_lock( struct _reent *_r ) {}
WEAK void __rtos_malloc_unlock( struct _reent *_r ) {}
WEAK void __rtos_env_lock( struct _reent *_r ) {}
WEAK void __rtos_env_unlock( struct _reent *_r ) {}

void __malloc_lock( struct _reent *_r )
{
    __rtos_malloc_lock(_r);
}

void __malloc_unlock( struct _reent *_r )
{
    __rtos_malloc_unlock(_r);
}

void __env_lock( struct _reent *_r )
{
    __rtos_env_lock(_r);
}

void __env_unlock( struct _reent *_r )
{
    __rtos_env_unlock(_r);
}


