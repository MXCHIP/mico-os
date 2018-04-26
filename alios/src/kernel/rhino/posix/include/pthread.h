/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef PTHREAD_H
#define PTHREAD_H

#include <k_api.h>
#include <time.h>

typedef ktask_t *pthread_t;

#define PTHREAD_SCOPE_PROCESS      0
#define PTHREAD_SCOPE_SYSTEM       1
#define PTHREAD_INHERIT_SCHED      1
#define PTHREAD_EXPLICIT_SCHED     2
#define PTHREAD_CREATE_DETACHED    0
#define PTHREAD_CREATE_JOINABLE    1

#define SCHED_OTHER                0
#define SCHED_FIFO                 1
#define SCHED_RR                   2

#define RH_LOW_PRI         RHINO_CONFIG_PRI_MAX     /* low priority rhino numbering */
#define RH_HIGH_PRI        0                        /* high priority rhino numbering */
#define POSIX_LOW_PRI      0                        /* low priority POSIX numbering */
#define POSIX_HIGH_PRI     RHINO_CONFIG_PRI_MAX     /* high priority POSIX numbering */

/* conversion pri between POSIX and RHINO */
#define PRI_CONVERT_PX_RH(prior) (POSIX_HIGH_PRI - prior)

#define DEFAULT_THREAD_STACK_SIZE  2048
#define DEFAULT_THREAD_PRIORITY    30

enum {
    PTHREAD_CANCEL_ASYNCHRONOUS,
    PTHREAD_CANCEL_ENABLE,
    PTHREAD_CANCEL_DEFERRED,
    PTHREAD_CANCEL_DISABLE,
    PTHREAD_CANCELED
};

typedef int pid_t;

struct sched_param {
    int sched_priority;           /* Process execution scheduling priority */
};

typedef struct {
    int is_initialized;
    void *stackaddr;
    int stacksize;
    int contentionscope;
    int inheritsched;
    int schedpolicy;
    struct sched_param schedparam;
    size_t guardsize;
    int  detachstate;
    size_t affinitysetsize;
} pthread_attr_t;

#define PTHREAD_MAGIC   0x11223344

struct _pthread_cleanup
{
    void (*cleanup_func)(void *parameter);
    void *para;

    struct _pthread_cleanup *next;
};
typedef struct _pthread_cleanup _pthread_cleanup_t;


struct _pthread_data{
    uint32_t magic;
    pthread_attr_t attr;
    ktask_t *tid;

    void *(*thread_entry)(void *para);
    void *thread_para;

    void *tet;

    ksem_t *join_sem;

    uint8_t cancel_stat;
    volatile uint8_t cancel_type;
    volatile uint8_t canceled;

    _pthread_cleanup_t *cleanup;
    void **tls;
};


typedef _pthread_data _pthread_data_t;

RHINO_INLINE _pthread_data_t *_pthread_get_data(pthread_t thread)
{
    _pthread_data_t *ptd;
    ptd = (_pthread_data_t *)thread->user_info[0];
    return ptd;
}

int     pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg);
void    pthread_exit(void *retval);
int     pthread_detach(pthread_t thread);
int     pthread_join(pthread_t thread, void **retval);
int     pthread_cancel(pthread_t thread);
void    pthread_testcancel(void);
int     pthread_setcancelstate(int state, int *oldstate);
int     pthread_setcanceltype(int type, int *oldtype);
int     pthread_kill(pthread_t thread, int sig);
int     pthread_equal(pthread_t t1, pthread_t t2);
int     pthread_setschedparam
        (
        pthread_t   thread,                 /* thread               */
        int         policy,                 /* new policy           */
        const struct sched_param * pParam   /* new parameters       */
        );

int     sched_yield(void);
int     sched_get_priority_min(int policy);
int     sched_get_priority_max(int policy);
int     sched_setscheduler(pid_t pid, int policy);

typedef struct {
  int   is_initialized;
  int   recursive;
} pthread_mutexattr_t;

typedef struct {
	kmutex_t *	    mutex;
    int             initted;
	//pthread_mutexattr_t	mutexAttr;
	} pthread_mutex_t;

#define PTHREAD_INITIALIZED_OBJ		0xABCDEFEF
#define PTHREAD_UNUSED_YET_OBJ		-1

#define PTHREAD_MUTEX_INITIALIZER	{NULL, PTHREAD_UNUSED_YET_OBJ}

int  pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr);
int  pthread_mutex_destroy(pthread_mutex_t *mutex);
int  pthread_mutex_lock(pthread_mutex_t *mutex);
int  pthread_mutex_unlock(pthread_mutex_t *mutex);
int  pthread_mutex_trylock(pthread_mutex_t *mutex);

int  pthread_mutexattr_init(pthread_mutexattr_t *attr);
int  pthread_mutexattr_destroy(pthread_mutexattr_t *attr);
int  pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type);
int  pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type);
int  pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int  pshared);
int  pthread_mutexattr_getpshared(pthread_mutexattr_t *attr, int *pshared);

void pthread_cleanup_pop(int execute);
void pthread_cleanup_push(void (*routine)(void *), void *arg);


typedef struct new_cond
{
    kmutex_t *lock;
    int       waiting;
    int       signals;
    ksem_t   *wait_sem;
    ksem_t   *wait_done;
} pthread_cond_t;

typedef struct
{
    int __dummy;
} pthread_condattr_t;

int     pthread_condattr_init(pthread_condattr_t *attr);
int     pthread_condattr_destroy(pthread_condattr_t *attr);
int     pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr);
int     pthread_cond_destroy(pthread_cond_t *cond);
int     pthread_cond_broadcast(pthread_cond_t *cond);
int     pthread_cond_signal(pthread_cond_t *cond);

int     pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
int     pthread_cond_timedwait(pthread_cond_t        *cond,
                           pthread_mutex_t       *mutex,
                           const struct timespec *abstime);

#endif

