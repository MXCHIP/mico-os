/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <umesh_pal.h>
#include <aos/list.h>

typedef struct {
    dlist_t next;
    int user_ms;
    uint32_t ms;
    void (*handler)(void *);
    void *arg;
} loop_task_t;

static struct {
    pal_sem_hdl_t task_counter;
    pal_sem_hdl_t task_mtx;
    dlist_t task_list;
    dlist_t timer_list;
} loop_stat;

static void dump_timer_list(void)
{
    loop_task_t *timer;
    printf("----start----\n");
    dlist_for_each_entry(&loop_stat.timer_list, timer, loop_task_t, next) {
        printf("%d %d %p\n", timer->user_ms, timer->ms, timer->handler);
    }
    printf("----end----\n");
}

static void check_timer_list(void)
{
    loop_task_t *timer;
    uint32_t cur = 0;
    dlist_for_each_entry(&loop_stat.timer_list, timer, loop_task_t, next) {
        if (cur > timer->ms) {
            dump_timer_list();
            *(int *)0 = 0;
        }
        cur = timer->ms;
    }
}

int umesh_pal_post_delayed_action(int ms, void (*handler)(void *arg), void *arg)
{
    loop_task_t *task = umesh_pal_malloc(sizeof *task);

    if (!task)
        return -1;

    task->user_ms = ms;
    task->ms = umesh_pal_now_ms() + ms;
    task->handler = handler;
    task->arg = arg;

    umesh_pal_sem_wait(&loop_stat.task_mtx, -1);

    loop_task_t *timer;
    dlist_for_each_entry(&loop_stat.timer_list, timer, loop_task_t, next) {
        if ((int)(timer->ms - task->ms) <= 0) {
            continue;
        }

        break;
    }

    dlist_add_tail(&task->next, &timer->next);
    check_timer_list();

    umesh_pal_sem_signal(&loop_stat.task_mtx);

    return 0;
}

void umesh_pal_cancel_delayed_action(int ms, void (*handler)(void *arg), void *arg)
{
    loop_task_t *timer;
    umesh_pal_sem_wait(&loop_stat.task_mtx, -1);
    dlist_for_each_entry(&loop_stat.timer_list, timer, loop_task_t, next) {
        if (timer->handler != handler)
            continue;
        if (timer->arg != arg)
            continue;
        if (ms >= 0 && ms != timer->user_ms)
            continue;

        dlist_del(&timer->next);
        break;
    }
    umesh_pal_sem_signal(&loop_stat.task_mtx);
}

int umesh_pal_schedule_call(void (*handler)(void *), void *arg)
{
    loop_task_t *task = umesh_pal_malloc(sizeof *task);
 
    if (!task)
        return -1;

    task->handler = handler;
    task->arg = arg;

    umesh_pal_sem_wait(&loop_stat.task_mtx, -1);
    dlist_add_tail(&task->next, &loop_stat.task_list);
    umesh_pal_sem_signal(&loop_stat.task_mtx);

    umesh_pal_sem_signal(&loop_stat.task_counter);

    return 0;
}

static void run_timer_list(dlist_t *timer_list)
{
    uint32_t now = umesh_pal_now_ms();

    while (!dlist_empty(timer_list)) {
        loop_task_t *task = dlist_first_entry(timer_list, loop_task_t, next);

        if ((int)(task->ms - now) > 0)
            break;
       
        dlist_del(&task->next);

        umesh_pal_sem_signal(&loop_stat.task_mtx);
        task->handler(task->arg);
        umesh_pal_sem_wait(&loop_stat.task_mtx, -1);

        umesh_pal_free(task);
    }
}

static void run_task_list(dlist_t *task_list)
{
    while (!dlist_empty(task_list)) {
        loop_task_t *task = dlist_first_entry(task_list, loop_task_t, next);
        dlist_del(&task->next);

        umesh_pal_sem_signal(&loop_stat.task_mtx);
        task->handler(task->arg);
        umesh_pal_sem_wait(&loop_stat.task_mtx, -1);

        umesh_pal_free(task);
    }
}

void umesh_pal_task_entry(void)
{
    dlist_t *timer_list = &loop_stat.timer_list;
    dlist_t *task_list = &loop_stat.task_list;

    while (1) {
        int min_tmo = -1;
        uint32_t now = umesh_pal_now_ms();

        umesh_pal_sem_wait(&loop_stat.task_mtx, -1);
        if (!dlist_empty(timer_list)) {
            loop_task_t *timer = dlist_first_entry(timer_list, loop_task_t, next);
            int delta = now - timer->ms;
            min_tmo = delta < 0 ? 0 : delta;
        }
        umesh_pal_sem_signal(&loop_stat.task_mtx);

        umesh_pal_sem_wait(&loop_stat.task_counter, min_tmo);

        umesh_pal_sem_wait(&loop_stat.task_mtx, -1);
        run_task_list(task_list);
        run_timer_list(timer_list);
        umesh_pal_sem_signal(&loop_stat.task_mtx);
    }
}

void umesh_pal_task_init(void)
{
    umesh_pal_sem_new(&loop_stat.task_counter, 0);
    umesh_pal_sem_new(&loop_stat.task_mtx, 1);
    dlist_init(&loop_stat.task_list);
    dlist_init(&loop_stat.timer_list);
}

