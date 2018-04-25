/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <pthread.h>

void pthread_cleanup_pop(int execute)
{
    CPSR_ALLOC();

    _pthread_data_t *ptd;
    _pthread_cleanup_t *cleanup;

    ptd = _pthread_get_data(krhino_cur_task_get());

    if (execute > 0) {
        RHINO_CRITICAL_ENTER();
        cleanup = ptd->cleanup;
        if (cleanup) {
            ptd->cleanup = cleanup->next;
        }
        RHINO_CRITICAL_EXIT();

        if (cleanup != 0) {
            cleanup->cleanup_func(cleanup->parameter);
            krhino_mm_free(cleanup);
        }
    }
}

void pthread_cleanup_push(void (*routine)(void *), void *arg)
{
    CPSR_ALLOC();

    _pthread_data_t *ptd;
    _pthread_cleanup_t *cleanup;

    ptd = _pthread_get_data(krhino_cur_task_get());

    cleanup = (_pthread_cleanup_t *)krhino_mm_alloc(sizeof(_pthread_cleanup_t));
    if (cleanup != 0) {
        cleanup->cleanup_func = routine;
        cleanup->parameter = arg;

        RHINO_CRITICAL_ENTER();
        cleanup->next = ptd->cleanup;
        ptd->cleanup = cleanup;
        RHINO_CRITICAL_EXIT();
    }
}

int pthread_detach(pthread_t thread)
{
    kstat_t ret = 0;
    _pthread_data_t *ptd;

    ptd = thread->user_info[0];
    ptd->attr.detachstate == PTHREAD_CREATE_DETACHED

    return 0;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                          void *(*start_routine) (void *), void *arg)
{
    kstat_t ret = 0;

    int result;
    void *stack;
    _pthread_data_t *ptd;

    ptd = (_pthread_data_t *)krhino_mm_alloc(sizeof(_pthread_data_t));
    if (ptd == 0) {
        return -1;
    }

    memset(ptd, 0, sizeof(_pthread_data_t));

    ptd->canceled = 0;
    ptd->cancelstate = PTHREAD_CANCEL_DISABLE;
    ptd->canceltype = PTHREAD_CANCEL_DEFERRED;
    ptd->magic = PTHREAD_MAGIC;

    if (attr != 0) {
        ptd->attr = *attr;
    }
    else {
        pthread_attr_init(&ptd->attr);
    }

    if (ptd->attr.stackaddr == 0) {
        stack = (void *)krhino_mm_alloc(ptd->attr.stacksize);
    }
    else {
        stack = (void *)(ptd->attr.stackaddr);
    }

    if (stack == 0) {
        krhino_mm_free(ptd);
        return -1;
    }

    ptd->tid = krhino_mm_alloc(sizeof(ktask_t));
    if (ptd->tid == 0) {
        if (ptd->attr.stack_base == 0) {
            krhino_mm_free(stack);
        }
        krhino_mm_free(ptd);
        return -1;
    }

    if (ptd->attr.detachstate == PTHREAD_CREATE_JOINABLE) {
        krhino_sem_dyn_create(&ptd->join_sem, "join_sem", 0);
        if (ret != RHINO_SUCCESS) {
            if (ptd->attr.stack_base != 0) {
                krhino_mm_free(stack);
            }
            krhino_mm_free(ptd);
            return -1;
        }
    }
    else {
        ptd->join_sem = 0;
    }

    ptd->thread_entry = start_routine;
    ptd->thread_para  = arg;

    ret = krhino_task_create(ptd->tid, "task", arg,
                           ptd->attr.schedparam.sched_priority, 0, stack,
                           ptd->attr.stacksize, (task_entry_t)start_routine, 0);

    if (ret != RHINO_SUCCESS) {
        if (ptd->attr.stack_base == 0) {
            krhino_mm_free(stack);
        }

        if (ptd->join_sem != 0) {
            krhino_sem_dyn_del(ptd->join_sem);
        }
        krhino_mm_free(ptd);
        return -1;
    }

    *tid = ptd->tid;

    (*tid)->user_info[0] = ptd;

    ret = krhino_task_resume(ptd->tid);
    if (ret == RHINO_SUCCESS) {
        return RHINO_SUCCESS;
    }

    if (ptd->attr.stack_base == 0) {
        krhino_mm_free(stack);
    }

    krhino_mm_free(ptd);

    return -1;
}

void pthread_exit(void *value)
{
    CPSR_ALLOC();

    _pthread_data_t *ptd;
    _pthread_cleanup_t *cleanup;

    ptd = _pthread_get_data(krhino_cur_task_get());

    RHINO_CRITICAL_ENTER();
    ptd->cancelstate = PTHREAD_CANCEL_DISABLE;
    ptd->return_value = value;
    RHINO_CRITICAL_EXIT();

    /* ptd->join_sem semaphore should be released by krhino_task_del_hook
     * if ptd->attr.detachstate is PTHREAD_CREATE_DETACHED, task stack and task structure
     * should be release by krhino_task_del_hook
     */
    krhino_task_del(krhino_cur_task_get());
}

int pthread_join(pthread_t thread, void **retval)
{
    kstat_t ret = 0;
    _pthread_data_t *ptd;

    if (thread == krhino_cur_task_get()) {
        return -1;
    }

    ptd = thread->user_info[0];
    if (ptd->attr.detachstate == PTHREAD_CREATE_DETACHED) {
        return -1;
    }

    ret = krhino_sem_take(ptd->join_sem, RHINO_WAIT_FOREVER);
    if (ret == RHINO_SUCCESS) {
        if (retval != 0) {
            *retval = ptd->return_value;
        }

        krhino_sem_dyn_del(ptd->join_sem);

        if (ptd->attr.stack_base == 0) {
            krhino_mm_free(ptd->tid->task_stack_base);
        }

        krhino_mm_free(ptd->tid);
        krhino_mm_free(ptd);
    }
    else {
        return -1;
    }

    return 0;
}

int pthread_cancel(pthread_t thread)
{
    return 0;
}

void pthread_testcancel(void)
{
    return;
}

int pthread_setcancelstate(int state, int *oldstate)
{
    return 0;
}

int pthread_setcanceltype(int type, int *oldtype)
{
    return 0;
}

int pthread_kill(pthread_t thread, int sig)
{
    /* This api should not be used, and will not be supported */
    return 0;
}

int pthread_equal(pthread_t t1, pthread_t t2)
{
    return (int)(t1 == t2);
}

int pthread_setschedparam
    (
    pthread_t   thread,                 /* thread               */
    int         policy,                 /* new policy           */
    const struct sched_param * param   /* new parameters       */
    )
{
    kstat_t stat = RHINO_SUCCESS;
    uint8_t old_pri;

    if (policy == SCHED_FIFO)
        {
        stat = krhino_sched_policy_set((ktask_t *)thread, KSCHED_FIFO);
        }
    else if (policy == SCHED_RR)
        {
        stat = krhino_sched_policy_set((ktask_t *)thread, KSCHED_RR);
        }        

    if (RHINO_SUCCESS != stat) {
        return -1;
    }

    /* change the priority of pthread */

    if (param != 0)
        {
        stat = krhino_task_pri_change ((ktask_t *) thread,
                PRI_CONVERT_PX_RH(param->sched_priority), &old_pri);
        if (stat == RHINO_SUCCESS)
            return 0;
        else
            return 1;
        }

    return 0;
}

