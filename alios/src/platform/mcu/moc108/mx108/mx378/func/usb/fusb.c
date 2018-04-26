#include "include.h"
#include "uart_pub.h"
#include "fusb_pub.h"
#include "fusb.h"
#include "usb_pub.h"
#include "drv_model_pub.h"

#if CFG_USB
#if (defined(FMSC_TEST) || defined(FUVC_TEST))
#include "ps.h"
#include "schedule_pub.h"
#endif

#ifdef FMSC_TEST
PROCESS(fmsc_test, "msc_test");
#elif defined(FUVC_TEST)
PROCESS(fuvc_test, "uvc_test");
#endif

UINT32 fusb_init(void)
{	
	UINT32 ret;

	ret = FUSB_SUCCESS;

    #ifdef FMSC_TEST
        fmsc_test_init();
    #elif defined(FUVC_TEST)
        //fuvc_test_init();
    #else   
    #endif
    
	return ret;
}

#ifdef FMSC_TEST
#define TEST_BUFFER_SIZE        512
#define FIRST_BLOCK             1
#define BLOCK_COUNT             1

UINT8 test_buff[TEST_BUFFER_SIZE] = {0};

void fmsc_test_init(void)
{   
    UINT32 status;
    void *parameter;
    DD_HANDLE usb_hdl;

    usb_hdl = ddev_open(USB_DEV_NAME, &status, 0);
    if(DD_HANDLE_UNVALID == usb_hdl)
    {
        goto init_failed;
    }

    parameter = (void *)fmsc_fiddle_process;
    ddev_control(usb_hdl, UCMD_MSC_REGISTER_FIDDLE_CB, parameter);
    ddev_close(usb_hdl);
    
    process_start(&fmsc_test, NULL);
    
init_failed:
    return;
}

void fmsc_fiddle_process(void)
{
    process_post(&fmsc_test, PROCESS_EVENT_POLL, NULL);
}

PROCESS_THREAD(fmsc_test, ev, data)
{
    UINT32 ret;
    UINT32 total_blk_cnt;
    UINT32 i = FIRST_BLOCK;
    
    PROCESS_BEGIN();

    while(41)
    {
        total_blk_cnt = 100;// MSC_HfiMedium_size();
        
        PROCESS_YIELD();

        if(MUSB_GetConnect_Flag())
        {            
            ret = MUSB_HfiRead(i, BLOCK_COUNT, test_buff);             
            //ret = MUSB_HfiWrite(i, BLOCK_COUNT, test_buff);    
            if(total_blk_cnt == i)
            {
                i = 0;
            }
        }
        
		(void)ret;
    }

    PROCESS_END();
}
#endif // FMSC_TEST

#ifdef FUVC_TEST
#include "schedule_pub.h"

#define TEST_BUFFER_SIZE        1024 * 16
UINT8 test_buff[TEST_BUFFER_SIZE] = {0};

void fuvc_test_init(void)
{    
    UINT32 param;
    UINT32 status;
    void *parameter;
    DD_HANDLE usb_hdl;

    usb_hdl = ddev_open(USB_DEV_NAME, &status, 0);
    if(DD_HANDLE_UNVALID == usb_hdl)
    {
        goto init_error;
    }

    parameter = (void *)fuvc_notify_uvc_configed;
    ddev_control(usb_hdl, UCMD_UVC_REGISTER_CONFIG_NOTIFY_CB, parameter);
    parameter = (void *)fuvc_fiddle_rx_vs;
    ddev_control(usb_hdl, UCMD_UVC_REGISTER_RX_VSTREAM_CB, parameter);
    
    parameter = (void *)test_buff;
    ddev_control(usb_hdl, UCMD_UVC_REGISTER_RX_VSTREAM_BUF_PTR, parameter);
    param = TEST_BUFFER_SIZE;
    ddev_control(usb_hdl, UCMD_UVC_REGISTER_RX_VSTREAM_BUF_LEN, &param);

    #ifdef UVC_DEMO_SUPPORT100
    param = UVC_MUX_PARAM(U1_FRAME_640_480, FPS_30);
    ddev_control(usb_hdl, UCMD_UVC_SET_PARAM, &param);
    #endif
    
    process_start(&fuvc_test, NULL);
    
init_error:
    return;
}

void fuvc_notify_uvc_configed(void)
{
    process_post(&fuvc_test, PROCESS_EVENT_MSG, NULL);
}

void fuvc_fiddle_rx_vs(void)
{
    process_post(&fuvc_test, PROCESS_EVENT_POLL, NULL);
}

PROCESS_THREAD(fuvc_test, ev, data)
{    
    UINT32 status;
    static DD_HANDLE usb_hdl;
    
    PROCESS_BEGIN();

    while(51)
    {        
        usb_hdl = ddev_open(USB_DEV_NAME, &status, 0);
        ASSERT(DD_HANDLE_UNVALID != usb_hdl);
        
        PROCESS_WAIT_EVENT();

        if(PROCESS_EVENT_MSG == ev)
        {
            #ifdef UVC_DEMO_SUPPORT102            
            UINT32 param;

            ddev_control(usb_hdl, UCMD_UVC_ENABLE_H264, 0);
            param = UVC_MUX_PARAM(UVC_FRAME_640_480, FPS_30);
            ddev_control(usb_hdl, UCMD_UVC_SET_PARAM, &param);
            #endif
            
            ddev_control(usb_hdl, UCMD_UVC_START_STREAM, 0);
        }
        else if(PROCESS_EVENT_POLL == ev)
        {
            ddev_control(usb_hdl, UCMD_UVC_RECEIVE_VSTREAM, 0);
        }
        else if(PROCESS_EVENT_EXIT == ev)
        {
            ddev_close(usb_hdl);
            usb_hdl = 0;
        }
        else
        {
        }
    }

    PROCESS_END();
}

#endif // FUVC_TEST
#endif // CFG_USB

// eof

