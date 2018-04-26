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

#include "sdcard_test.h"

static DD_HANDLE sdcard_hdl;

static SDIO_Error sdcard_test_open(void)
{   
    UINT32 status;

    sdcard_hdl = ddev_open(SDCARD_DEV_NAME, &status, 0);
    if(DD_HANDLE_UNVALID == sdcard_hdl){
        return (SDIO_Error)status;
    }
        
    return SD_OK;
}

static void sdcard_test_close(void)
{   
    ddev_close(sdcard_hdl);
}

UINT32 sdcard_intf_test(void)
{
    SDIO_Error ret = SD_OK;
    UINT32 err = SDCARD_SUCCESS;
    
    ret = sdcard_test_open();
    if(ret != SD_OK){
        SDCARD_FATAL("Can't open sdcard, please check\r\n");
        goto err_out;
    }
    SDCARD_PRT("sdcard_open is ok \r\n");
    
    return err;
    
err_out:
    SDCARD_FATAL("sdcard_test err, ret:%d\r\n", ret);
    return SDCARD_FAILURE;
        
}

void sdcard_intf_close(void)
{
    sdcard_test_close();        
}

#endif

