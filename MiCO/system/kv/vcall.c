#include <stdlib.h>
#include <stdio.h>
#include "vcall.h"
#include "mico.h"

void aos_free(void *mem)
{
    free(mem);
}

void *aos_malloc(unsigned int size)
{
    return malloc(size);
}

int32_t hal_flash_erase(hal_partition_t in_partition, uint32_t off_set, uint32_t size)
{
    return MicoFlashErase(MICO_PARTITION_USER, off_set, size);
}

int32_t hal_flash_write(hal_partition_t in_partition, uint32_t *off_set, const void *in_buf, uint32_t in_buf_len)
{
    return MicoFlashWrite(MICO_PARTITION_USER, off_set, in_buf, in_buf_len);
}

int32_t hal_flash_read(hal_partition_t in_partition, uint32_t *off_set, void *out_buf, uint32_t out_buf_len)
{
    return MicoFlashRead(MICO_PARTITION_USER, off_set, out_buf, out_buf_len);
}