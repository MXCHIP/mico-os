/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef _vcall_h_
#define _vcall_h_

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

#include <stdint.h>

#define CONFIG_AOS_KV_PTN 0
#define AOS_WAIT_FOREVER 0xFFFFFFFF

typedef void* aos_sem_t;
typedef void* aos_mutex_t;
typedef uint8_t hal_partition_t;

void aos_free(void *mem);
void *aos_malloc(unsigned int size);
void kv_lock_init(void);
void kv_lock(void);
void kv_unlock(void);
int32_t hal_flash_erase(hal_partition_t in_partition, uint32_t off_set, uint32_t size);
int32_t hal_flash_write(hal_partition_t in_partition, uint32_t *off_set, const void *in_buf, uint32_t in_buf_len);
int32_t hal_flash_read(hal_partition_t in_partition, uint32_t *off_set, void *out_buf, uint32_t out_buf_len);

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif

#endif


