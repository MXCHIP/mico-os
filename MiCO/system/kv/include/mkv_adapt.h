/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef MKV_ADAPT_H
#define MKV_ADAPT_H

#ifdef __cplusplus
extern "C" {
#endif

void *mkv_queue_new(uint32_t n);
void mkv_queue_pop(void *queue, void *msg);
void mkv_queue_push(void *queue, void *msg);

void *mkv_sem_new(void);
void mkv_sem_acquire(void *sem);
void mkv_sem_release(void *sem);
void mkv_sem_delete(void *sem);

int mkv_thread_new(void(*func)(void));

#ifdef __cplusplus
}
#endif

#endif  /* MKV_ADAPT_H */

