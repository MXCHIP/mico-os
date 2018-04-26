#include "include.h"

#if CFG_WIFI_STATION_MODE
#include "schedule_pub.h"

#include "sa_station.h"
#include "drv_model_pub.h"
#include "uart_pub.h"
#include "scanu_task.h"
#include "scan_task.h"
#include "rxu_task.h"
#include "mm_task.h"
#include "me_task.h"
#include "sm_task.h"
#include "rw_msg_tx.h"
#include "mac_ie.h"
#include "vif_mgmt.h"

#include "arm_arch.h"
#include "mem_pub.h"
#include "rw_pub.h"
#include "common.h"

#if CFG_USE_LWIP_NETSTACK
#include "lwip_intf.h"
#include "app_lwip_udp.h"
#include "app_lwip_tcp.h"
#endif

#if CFG_USE_TEMPERATURE_DETECT
#include "temp_detect_pub.h"
#endif
#if CFG_MODE_SWITCH
#include "param_config.h"
#endif

#include "mico_rtos.h"

extern struct mac_scan_result *scanu_search_by_ssid(struct mac_ssid const *ssid);

static uint32_t sa_sta_rsp_word = 0;
static int sta_inited = 0;

int sa_sta_inited()
{
	return sta_inited;
}

void sa_sta_clear_rsp_word(void)
{
	sa_sta_rsp_word = 0;
}

void sa_sta_set_rsp_word(uint32_t val)
{
	sa_sta_rsp_word = val;
}

/*---------------------------------------------------------------------------*/
int sa_station_send_associate_cmd(CONNECT_PARAM_T *connect_param)
{
#if 0 /* yhb changed, connect directly. the wpa scan results has the channel infomation. */
    struct ke_msg *msg;
    struct sm_connect_indication *conn_ind_ptr;
    struct mac_scan_result *desired_ap_ptr;

	if(g_sta_param_ptr->fast_connect_set)
	{
		g_sta_param_ptr->fast_connect_set = 0;
		connect_param->chan.freq = rw_ieee80211_get_centre_frequency(g_sta_param_ptr->fast_connect.chann);
		connect_param->chan.band = 0;
		connect_param->chan.flags = 0;
		connect_param->chan.tx_power = 10;
	}
	else
	{
	    desired_ap_ptr = scanu_search_by_ssid((void *)&connect_param->ssid);
	    if(NULL == desired_ap_ptr)
	    {
	        return -1;
	    }
	    connect_param->chan = *(desired_ap_ptr->chan);
		if(0 == connect_param->chan.tx_power)
		{
			connect_param->chan.tx_power = 10;
		}
	}
#endif
    mt_msg_dispatch(SM_CONNECT_REQ, connect_param);

    return 0;
}
/*---------------------------------------------------------------------------*/
int sa_station_send_disassociate_cmd(DISCONNECT_PARAM_T *disconnect_param)
{
	mt_msg_dispatch(SM_DISCONNECT_REQ, disconnect_param);

	return 0;
}

/*---------------------------------------------------------------------------*/
void sa_station_send_and_wait_rsp(UINT16 tx_cmd, void *param, UINT16 rx_rsp)
{
    mt_msg_dispatch(tx_cmd, param);

    while(1)
    {
        if(rx_rsp != sa_sta_rsp_word)
        {
        	mico_rtos_delay_milliseconds(10);
        }
		else
		{
			sa_sta_clear_rsp_word();
			break;
		}
    }
}

/*---------------------------------------------------------------------------*/
static void sa_station_cfg80211_init(void)
{
    UINT32 intf_type = VIF_STA;

    SASTA_PRT("[sa_sta]MM_RESET_REQ\r\n");
    sa_station_send_and_wait_rsp(MM_RESET_REQ, 0, MM_RESET_CFM);

    SASTA_PRT("[sa_sta]ME_CONFIG_REQ\r\n");
    sa_station_send_and_wait_rsp(ME_CONFIG_REQ, 0, ME_CONFIG_CFM);

    SASTA_PRT("[sa_sta]ME_CHAN_CONFIG_REQ\r\n");
    sa_station_send_and_wait_rsp(ME_CHAN_CONFIG_REQ, 0, ME_CHAN_CONFIG_CFM);

    SASTA_PRT("[sa_sta]MM_START_REQ\r\n");
    sa_station_send_and_wait_rsp(MM_START_REQ, 0, MM_START_CFM);

    SASTA_PRT("[sa_sta]MM_ADD_IF_REQ\r\n");
    sa_station_send_and_wait_rsp(MM_ADD_IF_REQ, &intf_type, MM_ADD_IF_CFM);
}

#ifndef DISABLE_RECONNECT

mico_thread_t  reconnect_thread_handle = NULL;
uint32_t  reconnect_stack_size = 2000;

void sa_reconnect_main(void *arg)
{  
	sa_station_init();     
	os_printf("sa_reconnect_main\r\n");

	mico_rtos_delete_thread( NULL );
	reconnect_thread_handle = NULL;
}

void sa_reconnect_init(void)
{
    OSStatus ret; 
return; // try it;
	if(NULL == reconnect_thread_handle)
	{
	    ret = mico_rtos_create_thread(&reconnect_thread_handle, 
	            THD_RECONNECT_PRIORITY,
	            "reconnect_thread", 
	            sa_reconnect_main, 
	            (unsigned short)reconnect_stack_size, 
	            0);
	    ASSERT(kNoErr == ret);
	}
	else
	{   
		os_printf("sa_reconnect_init_strange\r\n");
	}
}
#endif

void sa_station_init(void)
{
    sa_station_cfg80211_init();
	sta_inited = 1;
}

void sa_station_uninit(void)
{
}
/*---------------------------------------------------------------------------*/
#endif // CFG_WIFI_STATION_MODE

// eof

