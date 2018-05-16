/**
 ******************************************************************************
 * @file    mico_rtos.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-Aug-2018
 * @brief   This file provide the MiCO RTOS abstract layer functions.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico_common.h"
#include "mico_debug.h"
#include "mico_rtos.h"
#include "mico_rtos_internal.h"
#include "mico_rtos_common.h"
#include <aos/aos.h>
#include <k_api.h>


/******************************************************
 *                      Macros
 ******************************************************/

#define mico_rtos_enter_criticalR()     \
    do {                                \
        CPSR_ALLOC();                   \
        RHINO_CPU_INTRPT_DISABLE();     \
    } while (0)


#define mico_rtos_exit_critical()       \
    do {                                \
        RHINO_CPU_INTRPT_ENABLE();      \
    } while (0)

/******************************************************
 *                    Constants
 ******************************************************/

#ifndef TIMER_THREAD_STACK_SIZE
#define TIMER_THREAD_STACK_SIZE      1024 + 4*1024
#endif
#define TIMER_QUEUE_LENGTH  5

/*
 * Macros used by vListTask to indicate which state a task is in.
 */
#define tskBLOCKED_CHAR     ( 'B' )
#define tskREADY_CHAR       ( 'R' )
#define tskDELETED_CHAR     ( 'D' )
#define tskSUSPENDED_CHAR   ( 'S' )

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

typedef struct
{
    event_handler_t function;
    void*           arg;
} timer_queue_message_t;

typedef struct
{
    event_handler_t function;
    void*           arg;
} mico_event_message_t;


/******************************************************
 *               Function Declarations
 ******************************************************/

extern void __libc_fini_array(void);
extern void __libc_init_array (void);
extern int main(int argc, char **argv);

extern void mico_rtos_stack_overflow(char *taskname);
extern void mico_board_init( void );
extern void mico_system_qc_test( void );
extern bool MicoShouldEnterMFGMode(void);

extern void mico_main(void);
extern void pre_main (void);
extern int __real_main(void);

#ifdef __GNUC__
void __malloc_lock(struct _reent *ptr);
void __malloc_unlock(struct _reent *ptr);
#endif /* ifdef __GNUC__ */

/******************************************************
 *               Variables Definitions
 ******************************************************/

static mico_time_t mico_time_offset = 0;

extern const uint32_t mico_tick_rate_hz;
extern uint32_t       app_stack_size;
extern const int CFG_PRIO_BITS;

uint32_t  max_syscall_int_prio;
uint32_t  ms_to_tick_ratio = 1;


/******************************************************
 *               Function Definitions
 ******************************************************/

OSStatus mico_time_get_time(mico_time_t* time_ptr)
{
    *time_ptr = (mico_time_t) krhino_ticks_to_ms(krhino_sys_tick_get()) + mico_time_offset;
    return kNoErr;
}

OSStatus mico_time_set_time(mico_time_t* time_ptr)
{
    mico_time_offset = *time_ptr - (mico_time_t)krhino_ticks_to_ms(krhino_sys_tick_get());
    return kNoErr;
}

void mico_rtos_thread_msleep(uint32_t mseconds)
{
    mico_rtos_delay_milliseconds(mseconds);
}


OSStatus mico_rtos_init_event_flags( mico_event_flags_t* event_flags )
{
    UNUSED_PARAMETER( event_flags );
    check_string( 0!=0, "Unsupported\r\n" );
    return kUnsupportedErr;
}

OSStatus mico_rtos_wait_for_event_flags( mico_event_flags_t* event_flags, uint32_t flags_to_wait_for, uint32_t* flags_set, mico_bool_t clear_set_flags, mico_event_flags_wait_option_t wait_option, uint32_t timeout_ms )
{
    UNUSED_PARAMETER( event_flags );
    UNUSED_PARAMETER( flags_to_wait_for );
    UNUSED_PARAMETER( flags_set );
    UNUSED_PARAMETER( clear_set_flags );
    UNUSED_PARAMETER( wait_option );
    UNUSED_PARAMETER( timeout_ms );
    check_string( 0!=0, "Unsupported\r\n" );
    return kUnsupportedErr;
}

OSStatus mico_rtos_set_event_flags( mico_event_flags_t* event_flags, uint32_t flags_to_set )
{
    UNUSED_PARAMETER( event_flags );
    UNUSED_PARAMETER( flags_to_set );
    check_string( 0!=0, "Unsupported\r\n" );
    return kUnsupportedErr;
}

OSStatus mico_rtos_deinit_event_flags( mico_event_flags_t* event_flags )
{
    UNUSED_PARAMETER( event_flags );
    check_string( 0!=0, "Unsupported\r\n" );
    return kUnsupportedErr;
}


#if 0
void vApplicationStackOverflowHook( xTaskHandle *pxTask, signed portCHAR *pcTaskName )
{
    /*@-noeffect@*/
    UNUSED_PARAMETER( pxTask );
    UNUSED_PARAMETER( pcTaskName ); /* unused parameter in release build */
    /*@+noeffect@*/

    mico_rtos_stack_overflow((char*)pcTaskName);
}

void vApplicationMallocFailedHook( void )
{
    //WPRINT_RTOS_DEBUG(("Heap is out of memory during malloc\r\n"));
}

#endif

