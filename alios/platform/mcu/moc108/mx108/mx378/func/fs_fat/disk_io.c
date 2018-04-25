#include "include.h"
#include "arm_arch.h"

#if CFG_USE_SDCARD_HOST

#include "sdio_driver.h"
#include "sdcard.h"
#include "sdcard_pub.h"

#include "drv_model_pub.h"
#include "sys_ctrl_pub.h"
#include "mem_pub.h"
#include "icu_pub.h"
#include "target_util_pub.h"

#include "Diskio.h"
#include "ff.h"

static DD_HANDLE sdcard_hdl;
DSTATUS disk_initialize (uint8 pdrv)
{
    int cnt = 5;
    UINT32 status;

    if(pdrv == DISK_TYPE_SD){
        while(cnt--){
            sdcard_hdl = ddev_open(SDCARD_DEV_NAME, &status, 0);
            if(DD_HANDLE_UNVALID != sdcard_hdl){
                return RES_OK;
            }    
            FAT_WARN("SD init retry cnt = %d\r\n",cnt);
        }
    } else if(pdrv == DISK_TYPE_SD) {
        FAT_WARN("UDISK initialise without code!!!\r\n");
        return RES_OK;
    }
    return STA_NOINIT;
}


DSTATUS disk_status (uint8 pdrv)
{
	//test 
	return RES_OK;
}

DRESULT disk_read (
	uint8 pdrv,
	uint8* buff, 
	uint32 start_sector, 
	uint32 sector_cnt 
)
{
    uint32 err;

    if( pdrv == DISK_TYPE_SD) {
        err = ddev_read(sdcard_hdl, (char *)buff, sector_cnt, start_sector);
        if(err != SD_OK )
            return RES_ERROR;
    } 
    else if(pdrv == DISK_TYPE_SD) {
        //if(udisk_rd_blk_sync(start_sector,sector_cnt,buff)!= USB_RET_OK)
        FAT_WARN("UDISK initialise without code!!!\r\n");
        return RES_ERROR;
    }
    return RES_OK;
}


DSTATUS disk_close(void)
{   
    ddev_close(sdcard_hdl);

    sdcard_hdl = DD_HANDLE_UNVALID;
    return RES_OK;
}

#endif

