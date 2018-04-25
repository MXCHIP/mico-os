#include "include.h"
#include "rtos_pub.h"
#include "MiCODriverFlash.h"

mico_logic_partition_t* MicoFlashGetInfo( mico_partition_t inPartition )
{
    return bk_flash_get_info(inPartition);
}

OSStatus MicoFlashErase(mico_partition_t inPartition, uint32_t off_set, uint32_t size)
{
    return bk_flash_erase(inPartition,off_set,size);
}

OSStatus MicoFlashWrite( mico_partition_t inPartition, volatile uint32_t* off_set, uint8_t* inBuffer ,uint32_t inBufferLength)
{
    return bk_flash_write(inPartition,off_set,inBuffer,inBufferLength);
}

OSStatus MicoFlashRead( mico_partition_t inPartition, volatile uint32_t* off_set, uint8_t* outBuffer, uint32_t inBufferLength)
{
    return bk_flash_read(inPartition,off_set,outBuffer,inBufferLength);
}

OSStatus MicoFlashEnableSecurity( mico_partition_t partition, uint32_t off_set, uint32_t size )
{
    return bk_flash_enable_security(partition,off_set,size);
}

// eof



