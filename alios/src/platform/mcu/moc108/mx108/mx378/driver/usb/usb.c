#include "include.h"
#include "typedef.h"
#include "arm_arch.h"

#include "usb_pub.h"
#include "usb.h"

#include "drv_model_pub.h"
#include "icu_pub.h"
#include "sys_ctrl_pub.h"
#include "usb_msd.h"
#include "target_util_pub.h"
#include "intc_pub.h"
#include "board.h"
#include "ps.h"
#include "brd_cnf.h"
#include "uart_pub.h"

#if CFG_SUPPORT_MSD
#include "usb_msd.h"
#elif CFG_SUPPORT_HID
#include "usb_hid.h"
#elif CFG_SUPPORT_UVC
#include "usb_uvc.h"
#else
#endif

#if CFG_USB
PROCESS(usb_background_process, "usb_bg");
AUTOSTART_PROCESSES(&usb_background_process);

static DD_OPERATIONS usb_op = {
			usb_open,
			usb_close,
			usb_read,
			usb_write,
            usb_ctrl
};

/*******************************************************************/
void usb_init(void)
{
    intc_service_register(IRQ_USB, PRI_IRQ_USB, usb_isr);
	ddev_register_dev(USB_DEV_NAME, &usb_op);
    
    USB_PRT("usb_init\r\n");
}

void usb_exit(void)
{
	ddev_unregister_dev(USB_DEV_NAME);
    
    USB_PRT("usb_exit\r\n");
}

UINT32 usb_open (UINT32 op_flag)
{
	UINT32 param;
    
    USB_PRT("usb_open\r\n");
    
	/*step 0.0: reset usb module*/
    param = 0;
    sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_USB_SUBSYS_RESET, &param);    
    
	/*step0.1: open clock*/
	param = BLK_BIT_DPLL_480M;
	sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_BLK_ENABLE, &param);

	param = MCLK_SELECT_DPLL;
	sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_MCLK_SELECT, &param);
	
	param = USB_DPLL_DIVISION;
	sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_MCLK_DIVISION, &param);	
	
	/*step1: config clock power down for peripheral unit*/
	param = PWD_USB_CLK_BIT;
	sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_UP, &param);	
    
	/*step1.0: power up usb subsystem*/
    param = 0;
	sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_USB_POWERUP, &param);	    

    VREG_USB_TEST_MODE = 0x01;
    
    #ifdef MUSB_FORCE_FULLSPEED
    VREG_USB_POWER = 0x01; 
    #else
    VREG_USB_POWER |= 0x21; 
    #endif 
    
    VREG_USB_FADDR = 0;		
    VREG_USB_DEVCTL = 0x01;    
	
    #if CFG_USB
	usb_sw_init();
    #endif 

    process_start(&usb_background_process, NULL);  
    
	/*step2: interrupt setting*/
	param = IRQ_USB_BIT;
	sddev_control(ICU_DEV_NAME, CMD_ICU_INT_ENABLE, &param);	
	param = GINTR_IRQ_BIT;
	sddev_control(ICU_DEV_NAME, CMD_ICU_GLOBAL_INT_ENABLE, &param);
    
	return USB_SUCCESS;
}

UINT32 usb_close (void)
{
	UINT32 param;
    
    USB_PRT("usb_close\r\n");
	
	param = IRQ_USB_BIT;
	sddev_control(ICU_DEV_NAME, CMD_ICU_INT_DISABLE, &param);
	
	param = PWD_USB_CLK_BIT;
	sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_DOWN, &param);	

    #if CFG_USB
	usb_sw_uninit();
    #endif
	
	return USB_SUCCESS;
}

UINT32 usb_read (char *user_buf, UINT32 count, UINT32 op_flag)
{
    USB_PRT("usb_read\r\n");
	return USB_SUCCESS;
}

UINT32 usb_write (char *user_buf, UINT32 count, UINT32 op_flag)
{
    USB_PRT("usb_write\r\n");
	return USB_SUCCESS;
}
	
UINT32 usb_ctrl(UINT32 cmd, void *param)
{
	UINT32 ret;

	ret = USB_SUCCESS;
	
	switch(cmd)
	{		
		case UCMD_RESET:
			break;
            
        #if CFG_SUPPORT_MSD            
        case UCMD_MSC_REGISTER_FIDDLE_CB:
            MGC_RegisterCBTransferComplete(param);
            break;
        #endif // CFG_SUPPORT_MSD

        #if CFG_SUPPORT_UVC
        case UCMD_UVC_SET_PARAM:
            {
                UINT32 resolution_id;
                UINT32 fps;

                if(0 == param)
                {
                    return USB_FAILURE;
                }

                fps = UVC_DEMUX_FPS(*((UINT32*)param));
                resolution_id = UVC_DEMUX_ID(*((UINT32*)param));
                MGC_UvcSetParameter(resolution_id, fps);
            }
            break;

        case UCMD_UVC_ENABLE_H264:
            MGC_UvcEnableH264();
            break;

        case UCMD_UVC_ENABLE_MJPEG:
            MGC_UvcEnableMjpeg();
            break;
            
        case UCMD_UVC_REGISTER_RX_VSTREAM_BUF_PTR:
            MGC_UvcRegisterRxVstreamBufPtr(param);
            break;
            
        case UCMD_UVC_REGISTER_RX_VSTREAM_BUF_LEN:
            MGC_UvcRegisterRxVstreamBufLen((*(uint32_t *)param));
            break;
            
        case UCMD_UVC_RECEIVE_VSTREAM:
            ret = MGC_UvcReceiveVideoStream();
            break;
            
        case UCMD_UVC_REGISTER_CONFIG_NOTIFY_CB:
            MGC_UvcRegisterConfiguredCallback((FUNCPTR)param);
            break;
            
        case UCMD_UVC_REGISTER_RX_VSTREAM_CB:
            MGC_UvcRegisterVSRxedCallback((FUNCPTR)param);
            break;
            
        case UCMD_UVC_START_STREAM:
            ret = MGC_UvcStartStream();
            break;
            
        case UCMD_UVC_STOP_STREAM:
            ret = MGC_UvcStopStream();
            break;

        case UCMD_UVC_GET_CONNECT_STATUS:
            ret = MGC_UvcGetConnectStatus();
            break;
        #endif // CFG_SUPPORT_UVC
            
		default:
			break;
	}
	
    return ret;
}

void usb_isr(void)
{
    MGC_AfsUdsIsr();	
}

void usb_event_post(void)
{
    int ret;
    ret = process_post(&usb_background_process, PROCESS_EVENT_POLL, NULL);
    ASSERT(0 == ret);
}

PROCESS_THREAD(usb_background_process, ev, data)
{	
	PROCESS_BEGIN();

	while(1)
	{
		PROCESS_YIELD();
        
		MUSB_NoneRunBackground();		
	}

	PROCESS_END();
}
#endif
// EOF
