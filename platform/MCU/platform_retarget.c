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
#include "platform_toolchain.h"

// ****************************************************************************
// mico_main is a function that is called before main()
// mbed_sdk_init() is also a function that is called before main(), but unlike
// mico_main(), it is not meant for user code, but for the SDK itself to perform
// initializations before main() is called.

WEAK void mico_main(void);
WEAK void mico_main(void) {
}

#if defined(TOOLCHAIN_ARM)
extern "C" int $Super$$main(void);

extern "C" int $Sub$$main(void) {
    mbed_main();
    return $Super$$main();
}

extern "C" void _platform_post_stackheap_init (void) {
    mbed_sdk_init();
}

#elif defined(TOOLCHAIN_GCC)
int __real_main(void);

int __wrap_main(void) {
    mico_main();
    return __real_main();
}
#elif defined(TOOLCHAIN_IAR)
// IAR doesn't have the $Super/$Sub mechanism of armcc, nor something equivalent
// to ld's --wrap. It does have a --redirect, but that doesn't help, since redirecting
// 'main' to another symbol looses the original 'main' symbol. However, its startup
// code will call a function to setup argc and argv (__iar_argc_argv) if it is defined.
// Since mbed doesn't use argc/argv, we use this function to call our mbed_main.
void __iar_argc_argv() {
    mico_main();
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


