/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "umesh_utils.h"

typedef struct mem_info_s {
    ur_mem_stats_t stats;
    pal_sem_hdl_t mutex;
} mem_info_t;
static mem_info_t g_mem_info;

void *ur_mem_alloc(uint16_t size)
{
    void *mem = NULL;

    if (g_mem_info.stats.num + size > MEM_BUF_SIZE) {
        return mem;
    }

    mem = (void *)umesh_malloc((size_t)size);
    if (mem) {
        umesh_pal_sem_wait(&g_mem_info.mutex, -1);
        g_mem_info.stats.num += size;
        umesh_pal_sem_signal(&g_mem_info.mutex);
    }
    return mem;
}

void ur_mem_free(void *mem, uint16_t size)
{
    if (mem) {
        umesh_free(mem);
        umesh_pal_sem_wait(&g_mem_info.mutex, -1);
        g_mem_info.stats.num -= size;
        umesh_pal_sem_signal(&g_mem_info.mutex);
    }
}

void umesh_mem_init(void)
{
    bzero(&g_mem_info.stats, sizeof(g_mem_info.stats));
    umesh_pal_sem_new(&g_mem_info.mutex, 1);
}

void umesh_mem_deinit(void)
{
    umesh_pal_sem_free(&g_mem_info.mutex);
}

const ur_mem_stats_t *ur_mem_get_stats(void)
{
    return &g_mem_info.stats;
}
