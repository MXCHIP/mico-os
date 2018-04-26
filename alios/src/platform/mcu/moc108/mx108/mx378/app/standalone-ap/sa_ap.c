#include "include.h"
#include "sa_ap.h"
#include "schedule_pub.h"
#include "drv_model_pub.h"
#include "uart_pub.h"
#include "rw_pub.h"
#include "rxu_task.h"
#include "mm_task.h"
#include "me_task.h"
#include "apm_task.h"
#include "vif_mgmt.h"

#if CFG_WIFI_AP_MODE
#include "hostapd_cfg.h"

#if CFG_USE_LWIP_NETSTACK
#include "lwip_intf.h"
#include "app_lwip_udp.h"
#include "app_lwip_tcp.h"
#endif

#if CFG_USE_TEMPERATURE_DETECT
#include "temp_detect_pub.h"
#endif

#if CFG_SUPPORT_TIANZHIHENG_DRONE
#include "app_led.h"
#endif

static int ap_inited = 0;

uint32_t sa_ap_rsp_word = 0;
void sa_ap_clear_rsp_word(void)
{
	sa_ap_rsp_word = 0;
}

void sa_ap_set_rsp_word(uint32_t val)
{
	sa_ap_rsp_word = val;
}

void sa_ap_send_and_wait_rsp(UINT16 tx_cmd, void *param, UINT16 rx_rsp)
{
    mt_msg_dispatch(tx_cmd, param);

    while(1)
    {
        if(rx_rsp != sa_ap_rsp_word)
        {
        	mico_rtos_delay_milliseconds(10);
        }
		else
		{
			sa_ap_clear_rsp_word();
			break;
		}
    }
}

void sa_ap_init(void)
{
    UINT32 intf_type = VIF_AP;

    SAAP_PRT("[saap]MM_RESET_REQ\r\n");
    sa_ap_send_and_wait_rsp(MM_RESET_REQ, 0, MM_RESET_CFM);

    SAAP_PRT("[saap]ME_CONFIG_REQ\r\n");
    sa_ap_send_and_wait_rsp(ME_CONFIG_REQ, 0, ME_CONFIG_CFM);

    SAAP_PRT("[saap]ME_CHAN_CONFIG_REQ\r\n");
    sa_ap_send_and_wait_rsp(ME_CHAN_CONFIG_REQ, 0, ME_CHAN_CONFIG_CFM);

    SAAP_PRT("[saap]MM_START_REQ\r\n");
    sa_ap_send_and_wait_rsp(MM_START_REQ, 0, MM_START_CFM);

    SAAP_PRT("[saap]MM_ADD_IF_REQ\r\n");
    sa_ap_send_and_wait_rsp(MM_ADD_IF_REQ, &intf_type, MM_ADD_IF_CFM);

    SAAP_PRT("[saap]APM_START_REQ\r\n");
    sa_ap_send_and_wait_rsp(APM_START_REQ, 0, APM_START_CFM);

	ap_inited = 1;
}

int sa_ap_inited()
{
	return ap_inited;
}

void sa_ap_uninit(void)
{

}
#endif // CFG_WIFI_AP_MODE
// eof

