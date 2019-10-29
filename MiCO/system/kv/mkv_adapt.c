/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "mkv.h"
#include "mico.h"

void *mkv_queue_new(uint32_t n)
{
    mico_queue_t queue;
    if (mico_rtos_init_queue(&queue, NULL, sizeof(void *), n) != kNoErr)
    {
        return NULL;
    }
    return queue;
}

void mkv_queue_pop(void *queue, void *msg)
{
    mico_rtos_pop_from_queue((mico_queue_t *)&queue, msg, MICO_WAIT_FOREVER);
}

void mkv_queue_push(void *queue, void *msg)
{
    mico_rtos_push_to_queue((mico_queue_t *)&queue, msg, MICO_WAIT_FOREVER);
}

void *mkv_sem_new(void)
{
    mico_semaphore_t sem;
    if (mico_rtos_init_semaphore(&sem, 1) != kNoErr)
    {
        return NULL;
    }
    return sem;
}

void mkv_sem_acquire(void *sem)
{
    mico_rtos_get_semaphore((mico_semaphore_t *)&sem, MICO_WAIT_FOREVER);
}

void mkv_sem_release(void *sem)
{
    mico_rtos_set_semaphore((mico_semaphore_t *)&sem);
}

void mkv_sem_delete(void *sem)
{
    mico_rtos_deinit_semaphore((mico_semaphore_t *)&sem);
}

int mkv_thread_new(void (*func)(void))
{
    return mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "mkv deamon", (void (*)(uint32_t))func, 2048, 0);
}

int32_t kv_flash_read(uint32_t offset, void *buf, uint32_t nbytes)
{
#ifdef CONFIG_CPU_MX1290
    offset += 0x4000;
#endif
    return MicoFlashRead(MICO_PARTITION_KV, &offset, buf, nbytes);
}

int32_t kv_flash_write(uint32_t offset, void *buf, uint32_t nbytes)
{
#ifdef CONFIG_CPU_MX1290
    offset += 0x4000;
#endif
    return MicoFlashWrite(MICO_PARTITION_KV, &offset, buf, nbytes);
}

int32_t kv_flash_erase(uint32_t offset, uint32_t size)
{
#ifdef CONFIG_CPU_MX1290
    offset += 0x4000;
#endif
    return MicoFlashErase(MICO_PARTITION_KV, offset, size);
}

void *kv_malloc(uint32_t size)
{
    return malloc(size);
}

void kv_free(void *ptr)
{
    free(ptr);
}
