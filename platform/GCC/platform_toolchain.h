/**
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 *
 */

#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/******************************************************
 *                      Macros
 ******************************************************/

#ifndef WEAK
#ifndef __MINGW32__
#define WEAK             __attribute__((weak))
#else
/* MinGW doesn't support weak */
#define WEAK
#endif
#endif

#ifndef USED
#define USED             __attribute__((used))
#endif

#ifndef MAY_BE_UNUSED
#define MAY_BE_UNUSED    __attribute__((unused))
#endif

#ifndef NORETURN
#define NORETURN         __attribute__((noreturn))
#endif

//#ifndef ALIGNED
//#define ALIGNED(size)    __attribute__((aligned(size)))
//#endif

#ifndef SECTION
#define SECTION(name)    __attribute__((section(name)))
#endif

#ifndef NEVER_INLINE
#define NEVER_INLINE     __attribute__((noinline))
#endif

#ifndef ALWAYS_INLINE
#define ALWAYS_INLINE    __attribute__((always_inline))
#endif

#ifndef MICO_PACKED
#define MICO_PACKED(struct) struct __attribute__((packed))
#endif

#ifndef MICO_ALIGN
#define MICO_ALIGN(size) __attribute__((aligned(size)))
#endif

#ifndef MICO_UNUSED
#define MICO_UNUSED      __attribute__((__unused__))
#endif

#ifndef MICO_WEAK
#define MICO_WEAK        __attribute__((weak))
#endif

#ifndef MICO_FORCEINLINE
#define MICO_FORCEINLINE static inline __attribute__((always_inline))
#endif

#ifndef MICO_NORETURN
#define MICO_NORETURN   __attribute__((noreturn))
#endif

#ifndef MICO_UNREACHABLE
#define MICO_UNREACHABLE __builtin_unreachable()
#endif

#ifndef MICO_CALLER_ADDR
#define MICO_CALLER_ADDR() __builtin_extract_return_addr(__builtin_return_address(0))
#endif

#ifndef MICO_DEPRECATED
#define MICO_DEPRECATED(M) __attribute__((deprecated(M)))
#endif

#define MICO_DEPRECATED_SINCE(D, M) MICO_DEPRECATED(M " [since " D "]")

#ifndef MICO_SECTION
#define MICO_SECTION(name) __attribute__ ((section (name)))
#endif


// Backwards compatibility
#ifndef WEAK
#define WEAK MICO_WEAK
#endif

#ifndef PACKED
#define PACKED MICO_PACKED()
#endif

#ifndef EXTERN
#define EXTERN extern
#endif


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
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

void *memrchr( const void *s, int c, size_t n );


/* Windows doesn't come with support for strlcpy */
#ifdef WIN32
size_t strlcpy (char *dest, const char *src, size_t size);
#endif /* WIN32 */

#ifdef __cplusplus
} /* extern "C" */
#endif

