/**
 ******************************************************************************
 * @file    BkDriverFlash.h
 * @brief   This file provides all the headers of Flash operation functions.
 ******************************************************************************
 *
 *  The MIT License
 *  Copyright (c) 2017 BEKEN Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 */
#include "include.h"
#include "rtos_pub.h"
#include "BkDriverFlash.h"
#include "flash_pub.h"
#include "drv_model_pub.h"
#include "ll.h"

/* Logic partition on flash devices */
const bk_logic_partition_t bk7231_partitions[] =
{
    [BK_PARTITION_BOOTLOADER] =
    {
        .partition_owner           = BK_FLASH_EMBEDDED,
        .partition_description     = "Bootloader",
        .partition_start_addr      = 0x00000000,
        .partition_length          = 0x20000,         //128k bytes
        .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [BK_PARTITION_APPLICATION] =
    {
        .partition_owner           = BK_FLASH_EMBEDDED,
        .partition_description     = "Application",
        .partition_start_addr      = 0x20000,
        .partition_length          =    0,           //0 bytes
        .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    },
    [BK_PARTITION_RF_FIRMWARE] =
    {
        .partition_owner           = BK_FLASH_EMBEDDED,
        .partition_description     = "RF Firmware",
        .partition_start_addr      = 0xFA000,// just for test flash  cmd interface
        .partition_length          = 0x4000,
        .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
    }
};

static void BkFlashPartitionAssert( bk_partition_t inPartition )
{
    ASSERT(BK_PARTITION_BOOTLOADER < BK_PARTITION_MAX);
}

static uint32_t BkFlashPartitionIsValid( bk_partition_t inPartition )
{
    if((BK_PARTITION_BOOTLOADER == inPartition)
            || (BK_PARTITION_RF_FIRMWARE == inPartition))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

bk_logic_partition_t *bk_flash_get_info( bk_partition_t inPartition )
{
    bk_logic_partition_t *pt = NULL;

    BkFlashPartitionAssert(inPartition);

    if(BkFlashPartitionIsValid(inPartition))
    {
        pt = (bk_logic_partition_t *)&bk7231_partitions[inPartition];
    }

    return pt;
}

static OSStatus BkFlashInit(void)
{
    /* flash has initialized at dd model*/
}

static OSStatus BkFlashUninit(void)
{
}

OSStatus bk_flash_erase(bk_partition_t inPartition, uint32_t off_set, uint32_t size)
{
    uint32_t i;
    uint32_t param;
    uint32_t status;
    DD_HANDLE flash_hdl;
    uint32_t start_sector, end_sector;
    bk_logic_partition_t *partition_info;
    GLOBAL_INT_DECLARATION();

    partition_info = bk_flash_get_info(inPartition);
    start_sector = off_set >> 12;
    end_sector = (off_set + size - 1) >> 12;

    flash_hdl = ddev_open(FLASH_DEV_NAME, &status, 0);
    ASSERT(DD_HANDLE_UNVALID != flash_hdl);
    for(i = start_sector; i <= end_sector; i ++)
    {
        param = partition_info->partition_start_addr + (i << 12);
        GLOBAL_INT_DISABLE();
        ddev_control(flash_hdl, CMD_FLASH_ERASE_SECTOR, (void *)&param);
        GLOBAL_INT_RESTORE();
    }

    return kNoErr;
}

OSStatus bk_flash_write( bk_partition_t inPartition, volatile uint32_t *off_set, uint8_t *inBuffer , uint32_t inBufferLength)
{
    uint32_t status;
    uint32_t address;
    uint8_t *user_buf;
    DD_HANDLE flash_hdl;
    uint32_t start_addr;
    bk_logic_partition_t *partition_info;
    GLOBAL_INT_DECLARATION();

    ASSERT(off_set);
    ASSERT(inBuffer);

    partition_info = bk_flash_get_info(inPartition);
    start_addr = partition_info->partition_start_addr + *off_set;

    flash_hdl = ddev_open(FLASH_DEV_NAME, &status, 0);
    ASSERT(DD_HANDLE_UNVALID != flash_hdl);

    GLOBAL_INT_DISABLE();
    ddev_write(flash_hdl, inBuffer, inBufferLength, start_addr);
    GLOBAL_INT_RESTORE();

    return 0;
}

OSStatus bk_flash_read( bk_partition_t inPartition, volatile uint32_t *off_set, uint8_t *outBuffer, uint32_t inBufferLength)
{
    uint32_t status;
    uint32_t start_addr;
    DD_HANDLE flash_hdl;
    bk_logic_partition_t *partition_info;
    GLOBAL_INT_DECLARATION();

    ASSERT(off_set);
    ASSERT(outBuffer);

    partition_info = bk_flash_get_info(inPartition);
    start_addr = partition_info->partition_start_addr + *off_set;

    flash_hdl = ddev_open(FLASH_DEV_NAME, &status, 0);
    ASSERT(DD_HANDLE_UNVALID != flash_hdl);

    GLOBAL_INT_DISABLE();
    ddev_read(flash_hdl, outBuffer, inBufferLength, start_addr);
    GLOBAL_INT_RESTORE();

    return 0;
}

OSStatus bk_flash_enable_security( bk_partition_t partition, uint32_t off_set, uint32_t size )
{
    return 0;
}


// EOF

