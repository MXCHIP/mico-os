#include "lite-utils.h"
#include "iot_import.h"

#include "thread.h"

#ifndef PLATFORM_WAIT_INFINITE 
#define PLATFORM_WAIT_INFINITE (~0)
#endif

struct lk_thread_s {
    void *tid;
    void *sem;

    void *stack;
    int stack_size;

    void *(*thread)(void *arg);
    void *arg;
};

static void *lk_thread_wraper(void *arg)
{
    lk_thread_t *thr = arg;

    void *ret = thr->thread(thr->arg);

    HAL_SemaphorePost(thr->sem);

    return ret;
}

lk_thread_t *lk_thread_create(char *name, int stack_size, void *(*thread)(void *arg), void *arg)
{
    if (stack_size <= 0 || !thread)
        return NULL;

    lk_thread_t *thr = LITE_malloc(sizeof(lk_thread_t));
    if (!thr)
        return NULL;
    memset(thr, 0, sizeof(lk_thread_t));

    thr->thread = thread;
    thr->arg = arg;
    thr->stack_size = stack_size;

    thr->stack = LITE_malloc(stack_size);
    if (!thr->stack) {
        LITE_free(thr);
        return NULL;
    }

    thr->sem = HAL_SemaphoreCreate();
    if (!thr->sem) {
        LITE_free(thr->stack);
        LITE_free(thr);
        return NULL;
    }

    hal_os_thread_param_t threadParams = {
        .priority = os_thread_priority_normal,
        .stack_addr = thr->stack,
        .stack_size = thr->stack_size,
        .detach_state = 0,
        .name = name,
    };

    int stack_used = 0;

    if (HAL_ThreadCreate(&thr->tid, lk_thread_wraper, thr, &threadParams, &stack_used) < 0) {
        HAL_SemaphoreDestroy(thr->sem);
        LITE_free(thr->stack);
        LITE_free(thr);
        return NULL;
    }

    HAL_ThreadDetach(thr->tid);

    if (stack_used == 0) {
        LITE_free(thr->stack);
        thr->stack = NULL;
        thr->stack_size = 0;
    }

    return thr;
}

int lk_thread_join(lk_thread_t *thr)
{
    /* wait thread to exit */
    if (thr->sem) {
        HAL_SemaphoreWait(thr->sem, PLATFORM_WAIT_INFINITE);
        HAL_SemaphoreDestroy(thr->sem);
    }

    if (thr->stack)
        LITE_free(thr->stack);

    LITE_free(thr);

    return 0;
}
