/* MiCO Team
 * Copyright (c) 2017 MXCHIP Information Tech. Co.,Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "mico_debug.h"

#include "rt_TypeDef.h"
#include "rt_System.h"
#include "rt_Time.h"

#include "mico_rtos.h"
#include "cmsis_os.h"


#include "mico_board_conf.h"
#include "platform_peripheral.h"

/******************************************************
 *                      Macros
 ******************************************************/


/******************************************************
 *                    Constants
 ******************************************************/

#ifndef STDIO_BUFFER_SIZE
#define STDIO_BUFFER_SIZE   64
#endif

const osPriority _priority_remappingp[] = {
    [0]=osPriorityRealtime,
    [1]=osPriorityRealtime,
    [2]=osPriorityHigh,
    [3]=osPriorityHigh,
    [4]=osPriorityHigh,
    [5]=osPriorityAboveNormal,
    [6]=osPriorityAboveNormal,
    [7]=osPriorityNormal,
    [8]=osPriorityBelowNormal,
    [9]=osPriorityIdle,
};

/******************************************************
 *                   Enumerations
 ******************************************************/

enum {
    MICO_TIMER_UNUSED,
    MICO_TIMER_INIT,
    MICO_TIMER_START,
    MICO_TIMER_STOP,
};

/******************************************************
 *                 Type Definitions
 ******************************************************/
typedef struct _thread_t {
	osThreadId			id;
	osThreadDef_t		def;
	void				*stack;
	struct _thread_t	*next;
} thread_t;

typedef struct {
	osSemaphoreId		id;
	osSemaphoreDef_t	def;
	uint32_t			buf[2];
} semaphore_t;

typedef struct {
	osMutexId		id;
	osMutexDef_t	def;
	uint8_t			buf[16];
} mutex_t;

typedef struct {
	osMailQId		id;
	int 			state;
	osMailQDef_t	queue_def;
	uint32_t		queue_size;
	uint32_t		count;
	uint32_t		item_size;
	uint8_t         buf[1];
} queue_t;

typedef struct {
    osTimerId       id;
    uint32_t        time_ms;
    void            *buf;
    int             state;
    osTimerDef_t    timer_def;
} ostimer_t;

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

extern void mico_rtos_stack_overflow(char *taskname);
extern void mico_main(void);

/******************************************************
 *               Variables Definitions
 ******************************************************/
 static void check_mico_thread(void const*n);

static thread_t *p_thread_list = NULL;
static osMutexId mico_thread_mutex;
static int mico_inited = 0;
static osMutexDef(mico_thread_mutex);
static osTimerDef(mico_thread_timer, check_mico_thread);
static osTimerId mico_thread_timer;

static mico_thread_t* cur_thread;

/******************************************************
 *               Function Definitions
 ******************************************************/

void mbed_main( void )
{
    /* Initialize MICO_STDIO_UART and Printf Mutex. */
    mico_main();
}

static int cmsis_status_to_mico_status(osStatus status)
{
	switch(status) {
	case osOK:
		return kNoErr;
	case osEventTimeout:
	case osErrorTimeoutResource:
		return kTimeoutErr;
	case osErrorParameter:
		return kParamErr;
	case osErrorResource:
		return kNoResourcesErr;
	case osErrorNoMemory:
		return kNoMemoryErr;
	default:
		return kGeneralErr;
	}
}

static void insert_mico_thread(thread_t* thread)
{
	thread_t *p;
	
	osMutexWait(mico_thread_mutex, osWaitForever);
	if (p_thread_list == NULL) {
		osTimerStart(mico_thread_timer, 1000); // new thread, start timer 
		p_thread_list = thread;
	} else {
		p = p_thread_list; 
		while(p->next != NULL)
			p = p->next;
		p->next = thread;
	}
	osMutexRelease(mico_thread_mutex);
}

static int is_thread_alive(osThreadId id)
{
	osThreadEnumId enumid;
    osThreadId threadid;
	int found = 0;

	enumid = _osThreadsEnumStart();
	while ((threadid = _osThreadEnumNext(enumid))) {
		if (id == threadid) {
			found = 1;
			break;
		}
	}
	_osThreadEnumFree(enumid);
	return found;
}

/* find any dead thread in the thread list, free and remove it. */
static void check_mico_thread(void const *n)
{
	thread_t *pre, *next, tmp;

	if (p_thread_list == NULL)
		return;
	
	osMutexWait(mico_thread_mutex, osWaitForever);
	
	next = p_thread_list;
	pre = &tmp;
	pre->next = next;
	while( next != NULL) {
		if (is_thread_alive(next->id) == 0) {
			pre->next = next->next;
			free(next->stack);
			free(next);
			next = pre->next;
		} else {
			next = next->next;
			pre = pre->next;
		}
	}
	if (tmp.next != p_thread_list)
		p_thread_list = tmp.next;

	if (p_thread_list == NULL) // no more thread, stop the timer.
		osTimerStop(mico_thread_timer);
	
	osMutexRelease(mico_thread_mutex);

	
}

OSStatus mico_rtos_create_thread( mico_thread_t* thread, uint8_t priority, const char* name, mico_thread_function_t function, uint32_t stack_size, uint32_t arg )
{
	osThreadId thread_id;
	osThreadDef_t *p_def;
	thread_t *p_thread;

	if (mico_inited == 0) {
		mico_inited = 1;
		mico_thread_mutex = osMutexCreate(osMutex(mico_thread_mutex));
		mico_thread_timer = osTimerCreate(osTimer(mico_thread_timer), osTimerPeriodic, (void *)0);
	} else {
		check_mico_thread(NULL);
	}
	
	p_thread = (thread_t*)malloc(sizeof(thread_t));
	if (p_thread == NULL)
		return kNoMemoryErr;

	p_thread->next = NULL;
	p_def = &p_thread->def;
#ifdef __MBED_CMSIS_RTOS_CM	
	void *stack = malloc(stack_size);
	if (stack == NULL)
		return kNoMemoryErr;
	p_def->stack_pointer = stack;
#endif
	p_def->instances = 1;
	p_def->pthread = (os_pthread)function;
	p_def->stacksize = stack_size;
	p_def->tpriority = _priority_remappingp[priority];
	
    thread_id = osThreadCreate(p_def, (void*)arg);
	if (thread_id == NULL)
		return kNoMemoryErr;

	p_thread->id = thread_id;
	p_thread->stack = stack;
	if (thread) {
		*thread = (mico_thread_t)p_thread;
	}
	insert_mico_thread(p_thread);
	return kNoErr;
}

OSStatus mico_rtos_delete_thread( mico_thread_t* thread )
{
	int ret;
	osThreadId     id;
	thread_t *pthread = (thread_t*)*thread;
	
    if (thread == NULL) { // terminate current thread
		id = osThreadGetId();
	} else {
		id = pthread->id;
	}

	ret = osThreadTerminate(id);
	
	return cmsis_status_to_mico_status(ret);
}

OSStatus mico_rtos_thread_join( mico_thread_t* thread )
{
	osThreadId thread_id;
	thread_t *pthread = (thread_t*)*thread;
	
    if (thread == NULL)
		return kParamErr;
	
	thread_id = (osThreadId)pthread->id;
	while(0 != osThreadGetState(thread_id)) {
		osDelay(10);
	}

	return kNoErr;
}

void mico_rtos_thread_yield( void )
{
    osThreadYield();
}

bool mico_rtos_is_current_thread( mico_thread_t* thread )
{
	osThreadId thread_id;
	thread_t *pthread = (thread_t*)*thread;
	
	if (thread == NULL)
		return false;
	
    thread_id = osThreadGetId();
	if (pthread->id == thread_id)
		return true;
	else
		return false;
}

mico_thread_t* mico_rtos_get_current_thread( void )
{
    osThreadId thread_id;
	thread_t *p;

	thread_id = osThreadGetId();
	osMutexWait(mico_thread_mutex, osWaitForever);
	p = p_thread_list;
	while(p != NULL) {
		if (p->id == thread_id)
			break;
		p = p->next;
	}
	osMutexRelease(mico_thread_mutex);
	
	*cur_thread = (mico_thread_t)p;

	return cur_thread;
}

OSStatus mico_rtos_print_thread_status( char* pcWriteBuffer, int xWriteBufferLen )
{
#if MBED_STACK_STATS_ENABLED && MBED_CONF_RTOS_PRESENT
    osThreadEnumId enumid = _osThreadsEnumStart();
    osThreadId threadid;
	uint32_t stack_max, stack_size, state, entry_point;

	cmd_printf("EntryAddress State  StackMax StackSize\r\n");
    while ((threadid = _osThreadEnumNext(enumid))) {
        osEvent e;

		e = _osThreadGetInfo(threadid, osThreadInfoEntry);
		if (e.status != osOK) {
	        break;
	    }
		entry_point = (uint32_t)e.value.p;
		
        e = _osThreadGetInfo(threadid, osThreadInfoStackMax);
        if (e.status != osOK) {
	        break;
	    }
		stack_max = (uint32_t)e.value.p;
	
        e = _osThreadGetInfo(threadid, osThreadInfoStackSize);
        if (e.status != osOK) {
	        break;
	    }
		stack_size = (uint32_t)e.value.p;

		state = osThreadGetState(threadid);

		cmd_printf("%p %d %d %d\r\n", entry_point, stack_max, stack_size, state);
    }
	_osThreadEnumFree(enumid);
#endif
    return kNoErr;
}


OSStatus mico_rtos_check_stack( void )
{
    rt_stk_check();
    return kNoErr;
}

OSStatus mico_rtos_thread_force_awake( mico_thread_t* thread )
{
    return kUnsupportedErr;
}

mico_time_t mico_time_offset = 0;

OSStatus mico_time_get_time(mico_time_t* time_ptr)
{
    *time_ptr = (mico_time_t) rt_time_get() + mico_time_offset;
	
	return kNoErr;
}

OSStatus mico_time_set_time(mico_time_t* time_ptr)
{
    mico_time_offset = *time_ptr - (mico_time_t) rt_time_get();
    return kNoErr;
}

OSStatus mico_rtos_init_semaphore( mico_semaphore_t* semaphore, int count )
{
	osSemaphoreId       id;
	semaphore_t   *p_sem;
	if (semaphore == NULL)
		return kParamErr;

	p_sem = (semaphore_t*)malloc(sizeof(semaphore_t));
	if (p_sem == NULL)
		return kNoMemoryErr;
	
	memset(p_sem->buf, 0, sizeof(p_sem->buf));
	p_sem->def.semaphore = (void*)p_sem->buf;
	id = osSemaphoreCreate(&p_sem->def, 0 ); // id equal p_sem;
	if (id == NULL) {
		return kGeneralErr;
	}
	p_sem->id = id;

	*semaphore = (mico_semaphore_t)p_sem;
    return kNoErr;
}

OSStatus mico_rtos_get_semaphore( mico_semaphore_t* semaphore, uint32_t timeout_ms )
{
	int ret;
	semaphore_t *p_sem;
	if (semaphore == NULL)
		return kParamErr;

	p_sem = (semaphore_t*)*semaphore;
    ret = osSemaphoreWait(p_sem->id, timeout_ms);
	if (ret == -1)
		return kGeneralErr;
	
    if (ret == 0)
        return kTimeoutErr;

	return kNoErr;
}

int mico_rtos_set_semaphore( mico_semaphore_t* semaphore )
{
	int ret;
	semaphore_t *p_sem;
	
	if (semaphore == NULL)
		return kParamErr;

	p_sem = (semaphore_t*)*semaphore;
    ret = osSemaphoreRelease(p_sem->id);

	return cmsis_status_to_mico_status(ret);
}

OSStatus mico_rtos_deinit_semaphore( mico_semaphore_t* semaphore )
{
	semaphore_t *p_sem;
	
	if (semaphore == NULL)
		return kParamErr;

	p_sem = (semaphore_t*)*semaphore;
    osSemaphoreDelete(p_sem->id);
	free(p_sem);
	*semaphore = NULL;
    return kNoErr;
}

void mico_rtos_enter_critical( void )
{
    //vPortEnterCritical();
}

void mico_rtos_exit_critical( void )
{
    //vPortExitCritical();
}


OSStatus mico_rtos_init_mutex( mico_mutex_t* mutex )
{
	osMutexId id;
	mutex_t *p_mux;

	if (mutex == NULL)
		return kParamErr;

	p_mux = (mutex_t*)malloc(sizeof(mutex_t));
	if (p_mux == NULL)
		return kNoMemoryErr;

	memset(p_mux->buf, 0, 16);
	p_mux->def.mutex = p_mux->buf;
	id = osMutexCreate(&p_mux->def); 
	if (id == NULL) {
		free(p_mux);
		return kGeneralErr;
	}
	p_mux->id = id;
	*mutex = (mico_mutex_t)p_mux;
    return kNoErr;
}

OSStatus mico_rtos_lock_mutex( mico_mutex_t* mutex )
{
	int ret;
	mutex_t *p_mux;
	
	if (mutex == NULL)
		return kParamErr;
	p_mux = (mutex_t*)*mutex;
    ret = osMutexWait(p_mux->id, osWaitForever);
	if (ret == -1)
		return kGeneralErr;
	
	return kNoErr;
}


OSStatus mico_rtos_unlock_mutex( mico_mutex_t* mutex )
{
	int ret;
	mutex_t *p_mux;
	
	if (mutex == NULL)
		return kParamErr;
	p_mux = (mutex_t*)*mutex;
    ret = osMutexRelease(p_mux->id);

	return cmsis_status_to_mico_status(ret);
}

OSStatus mico_rtos_deinit_mutex( mico_mutex_t* mutex )
{
	mutex_t *p_mux;
	
	if (mutex == NULL)
		return kParamErr;
	p_mux = (mutex_t*)*mutex;
    osMutexDelete(p_mux->id);
	free(p_mux);
	*mutex = NULL;
    return kNoErr;
}

OSStatus mico_rtos_init_queue( mico_queue_t* queue, const char* name, uint32_t message_size, uint32_t number_of_messages )
{
	osMailQDef_t *queue_def;
	osMailQId id;
	int pool_len;
	queue_t *p_queue;
	uint32_t *q, *m, *pool;
	
	if (queue == NULL)
		return kParamErr;



	pool_len = 4*2 + 4*(number_of_messages + 4); // ref cmsis_os.h MARCRO: osMailQDef
	pool_len += 4*(3+((message_size+3)/4)*(number_of_messages)); // 3 is for 4 alignment
	
	p_queue = malloc(sizeof(queue_t) + pool_len);
	if (p_queue == NULL)
		return kNoMemoryErr;

	memset(p_queue->buf, 0, pool_len);
	queue_def = &p_queue->queue_def;
	queue_def->queue_sz = number_of_messages;
	queue_def->item_sz = message_size;
	pool = (uint32_t*)p_queue->buf;
	q = (uint32_t*)&pool[2];
	m = (uint32_t*)&q[4+number_of_messages];
	pool[0] = (uint32_t)q;
	pool[1] = (uint32_t)m;
	queue_def->pool = (void*)pool;
	id = osMailCreate(queue_def, NULL); 
	if (id == NULL) {
		free(p_queue);
		return kGeneralErr;
	}

	p_queue->id = id;
	p_queue->item_size = message_size;
	p_queue->queue_size = number_of_messages;
	p_queue->count = 0;
	*queue = (mico_queue_t*)p_queue;
    return kNoErr;
}

OSStatus mico_rtos_push_to_queue( mico_queue_t* queue, void* message, uint32_t timeout_ms )
{
	osMailQId id;
    void *mail; 
	int ret;
	queue_t *p_queue;

	if (queue == NULL)
		return kParamErr;

	p_queue = (queue_t *)*queue;
	id = p_queue->id;
	mail = osMailAlloc(id, timeout_ms);
	if (mail == NULL)
		return kTimeoutErr;
	
	memcpy(mail, message, p_queue->item_size);
	ret = osMailPut(id, mail);
	if (ret == osOK)
		p_queue->count++;
    return ret;
}


OSStatus mico_rtos_push_to_queue_front( mico_queue_t* queue, void* message, uint32_t timeout_ms )
{
    return kUnsupportedErr;
}


OSStatus mico_rtos_pop_from_queue( mico_queue_t* queue, void* message, uint32_t timeout_ms )
{
    osMailQId id;
    void *mail; 
	osEvent evt ;
	queue_t *p_queue;

	if (queue == NULL)
		return kParamErr;

	p_queue = (queue_t *)*queue;
	id = p_queue->id;

	evt = osMailGet(id, timeout_ms);
	if (evt.status != osEventMail)
		return kTimeoutErr;
	mail = evt.value.p;
	memcpy(message, mail, p_queue->item_size);
	osMailFree(id, mail);
	p_queue->count--;
	
    return kNoErr;
}


OSStatus mico_rtos_deinit_queue( mico_queue_t* queue )
{
	queue_t *p_queue;
	
	if (queue == NULL)
		return kParamErr;

	p_queue = (queue_t *)*queue;
	free(p_queue);
	*queue = NULL;
    return kNoErr;
}

bool mico_rtos_is_queue_empty( mico_queue_t* queue )
{
	queue_t *p_queue;
	if (queue == NULL)
		return true;

	p_queue = (queue_t *)*queue;
	if (p_queue->count == 0)
		return true;

	return false;
}

bool mico_rtos_is_queue_full( mico_queue_t* queue )
{
	queue_t *p_queue;
	if (queue == NULL)
		return false;

	p_queue = (queue_t *)*queue;
	if (p_queue->queue_size == p_queue->count)
		return true;

	return false;
}
#if 0
OSStatus mico_rtos_init_timer( mico_timer_t* timer, uint32_t time_ms, timer_handler_t function, void* arg )
{
    void *p_timer;
	osTimerId id;

	if (timer == NULL)
		return kParamErr;
	
	p_timer = malloc(24);
	if (p_timer == NULL)
		return kNoMemoryErr;

	memset(p_timer, 0, 24);
	timer->timer_def.timer = (void*)p_timer;
	timer->timer_def.ptimer = (os_ptimer)function;
	id = osTimerCreate(&timer->timer_def, osTimerPeriodic, arg); // id equal p_timer;
	if (id == NULL) {
		free(p_timer);
		return kGeneralErr;
	}
	timer->buf = p_timer;
	timer->id = id;
	timer->time_ms = time_ms;
	timer->state = MICO_TIMER_INIT;
    return kNoErr;
}
#endif


OSStatus mico_rtos_init_timer( mico_timer_t* timer, uint32_t time_ms, timer_handler_t function, void* arg )
{
    OSStatus err = kNoErr;
    void *p_timer = NULL;
    ostimer_t *timer_handle = NULL;
    osTimerId id;

    check_string(timer != NULL, "Bad args");

    timer_handle = malloc( sizeof(ostimer_t));
    require_action(timer_handle != NULL, exit, err = kNoMemoryErr);

    p_timer = malloc(24);
    require_action(p_timer != NULL, exit, err = kNoMemoryErr);

    memset(p_timer, 0, 24);
    timer_handle->timer_def.timer = (void*)p_timer;
    timer_handle->timer_def.ptimer = (os_ptimer)function;
    id = osTimerCreate(&timer_handle->timer_def, osTimerPeriodic, arg); // id equal p_timer;
    require_action(id != NULL, exit, err = kGeneralErr);

    timer_handle->buf = p_timer;
    timer_handle->id = id;
    timer_handle->time_ms = time_ms;
    timer_handle->state = MICO_TIMER_INIT;


    timer->function = function;
    timer->arg      = arg;
    timer->handle   = timer_handle;

exit:
    if( err != kNoErr )
    {
        if(timer_handle) free(timer_handle);
        if(p_timer) free(p_timer);
    }
    return err;
}

OSStatus mico_rtos_start_timer( mico_timer_t* timer )
{
	int ret;
	
	if (timer == NULL)
		return kParamErr;

	ret = osTimerStart( ((ostimer_t *)timer->handle)->id, ((ostimer_t *)timer->handle)->time_ms);
	((ostimer_t *)timer->handle)->state = MICO_TIMER_START;
	
    return ret;
}

OSStatus mico_rtos_stop_timer( mico_timer_t* timer )
{
	int ret;
	
	if (timer == NULL)
		return kParamErr;

	ret = osTimerStop(((ostimer_t *)timer->handle)->id);
	((ostimer_t *)timer->handle)->state = MICO_TIMER_STOP;
	
    return ret;
}

OSStatus mico_rtos_reload_timer( mico_timer_t* timer )
{
    int ret;
	
	if (timer == NULL)
		return kParamErr;

	ret = osTimerStop(((ostimer_t *)timer->handle)->id);
	if (ret != osOK)
		return ret;
	
	((ostimer_t *)timer->handle)->state = MICO_TIMER_STOP;
	ret = osTimerStart(((ostimer_t *)timer->handle)->id, ((ostimer_t *)timer->handle)->time_ms);
	((ostimer_t *)timer->handle)->state = MICO_TIMER_START;
	
    return ret;
}

OSStatus mico_rtos_deinit_timer( mico_timer_t* timer )
{
	int ret;
	
	if (timer == NULL)
		return kParamErr;

	ret = osTimerDelete(((ostimer_t *)timer->handle)->id);
	((ostimer_t *)timer->handle)->state = MICO_TIMER_UNUSED;
	free(((ostimer_t *)timer->handle)->buf);
	free((ostimer_t *)timer->handle);

	timer->arg = NULL;
	timer->function = NULL;
	timer->handle = NULL;
    return ret;
}


bool mico_rtos_is_timer_running( mico_timer_t* timer )
{
	if (timer == NULL)
		return false;

	if (((ostimer_t *)timer->handle)->state == MICO_TIMER_START)
		return true;
	
	return false;
}

OSStatus mico_rtos_init_event_flags( mico_event_flags_t* event_flags )
{
    UNUSED_PARAMETER( event_flags );
    //check_string( 0!=0, "Unsupported\r\n" );
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
    //check_string( 0!=0, "Unsupported\r\n" );
    return kUnsupportedErr;
}

OSStatus mico_rtos_set_event_flags( mico_event_flags_t* event_flags, uint32_t flags_to_set )
{
    UNUSED_PARAMETER( event_flags );
    UNUSED_PARAMETER( flags_to_set );
    //check_string( 0!=0, "Unsupported\r\n" );
    return kUnsupportedErr;
}

OSStatus mico_rtos_deinit_event_flags( mico_event_flags_t* event_flags )
{
    UNUSED_PARAMETER( event_flags );
    //check_string( 0!=0, "Unsupported\r\n" );
    return kUnsupportedErr;
}


void mico_rtos_suspend_thread(mico_thread_t* thread)
{

}

void mico_rtos_resume_thread(mico_thread_t* thread)
{

}

/**
 * Gets time in milliseconds since RTOS start
 *
 * @Note: since this is only 32 bits, it will roll over every 49 days, 17 hours.
 *
 * @returns Time in milliseconds since RTOS started.
 */
mico_time_t mico_rtos_get_time( void )
{
    return (mico_time_t) rt_time_get();
}

/**
 * Delay for a number of milliseconds
 *
 * Processing of this function depends on the minimum sleep
 * time resolution of the RTOS.
 * The current thread sleeps for the longest period possible which
 * is less than the delay required, then makes up the difference
 * with a tight loop
 *
 * @return OSStatus : kNoErr if delay was successful
 *
 */
OSStatus mico_rtos_delay_milliseconds( uint32_t num_ms )
{
    osDelay(num_ms);

    return kNoErr;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


void *mico_malloc( size_t xWantedSize )
{
	return malloc(xWantedSize);
}

void mico_free( void *pv )
{
	free(pv);
}

void *mico_realloc( void *pv, size_t xWantedSize )
{
	return realloc(pv, xWantedSize);
}


//#endif

