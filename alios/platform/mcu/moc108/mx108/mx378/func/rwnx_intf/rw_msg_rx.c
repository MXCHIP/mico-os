#include "include.h"
#include "rw_msg_rx.h"
#include "rw_pub.h"
#include "ke_msg.h"
#include "mem_pub.h"
#include "mac_common.h"
#include "scanu_task.h"
#include "sa_station.h"
#include "apm_task.h"
#include "me_task.h"
#include "sm_task.h"
#include "hostapd_intf_pub.h"
#include "mac_ie.h"
#include "ieee802_11_defs.h"

SCAN_RST_UPLOAD_T *scan_rst_set_ptr = 0;
IND_CALLBACK_T scan_cfm_cb = {0};
IND_CALLBACK_T assoc_cfm_cb = {0};
IND_CALLBACK_T deassoc_evt_cb = {0};
IND_CALLBACK_T deauth_evt_cb = {0};
struct co_list rw_msg_rx_head;
uint8_t *ind_buf_ptr = 0;
uint32_t scan_cfm = 0;
int connect_flag = 0;

extern void sa_ap_set_rsp_word(uint32_t val);
extern void sa_sta_set_rsp_word(uint32_t val);

int beken_get_sm_connect_flag(void)
{
    return connect_flag;
}

#if 1 /* scan result*/
UINT8 *sr_malloc_result_item(UINT32 vies_len)
{
	return os_zalloc(vies_len + sizeof(struct sta_scan_res));
}

void sr_free_result_item(UINT8 *item_ptr)
{
	os_free(item_ptr);
}

UINT8 *sr_malloc_shell(void)
{
	UINT8 *ptr;
	UINT32 layer1_space_len;
	UINT32 layer2_space_len;

	layer1_space_len = sizeof(SCAN_RST_UPLOAD_T);
	layer2_space_len = MAX_BSS_LIST * sizeof(struct sta_scan_res *);
	ptr = os_zalloc(layer1_space_len + layer2_space_len);

	ASSERT(ptr);
	
	return ptr;
}

void sr_free_shell(UINT8 *shell_ptr)
{
	os_free(shell_ptr);
}

void sr_free_all(SCAN_RST_UPLOAD_T *scan_rst)
{
	UINT32 i;

	for(i = 0; i < scan_rst->scanu_num; i ++)
	{
		sr_free_result_item((UINT8 *)scan_rst->res[i]);
		scan_rst->res[i] = 0;
	}
	scan_rst->scanu_num = 0;
	
	sr_free_shell((UINT8 *)scan_rst);
}

void *sr_get_scan_results(void)
{
	return scan_rst_set_ptr;
}

void sr_release_scan_results(SCAN_RST_UPLOAD_PTR ptr)
{
	if(ptr)
	{
		sr_free_all(ptr);
	}	
	scan_rst_set_ptr = 0;
}
#endif

#if 2
UINT32 mr_kmsg_fwd(struct ke_msg *msg)
{
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
    co_list_push_back(&rw_msg_rx_head, &msg->hdr);
	GLOBAL_INT_RESTORE();
	
	app_set_sema();
	
    return 0;
}

void mr_kmsg_flush(void)
{
    while(mr_kmsg_fuzzy_handle())
    {
        ;
    }
}

UINT32 mr_kmsg_fuzzy_handle(void)
{
    UINT32 ret = 0;
    struct ke_msg *msg;
    struct co_list_hdr *node;

	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
    node = co_list_pop_front(&rw_msg_rx_head);
	GLOBAL_INT_RESTORE();
	
    if(node)
    {
        msg = (struct ke_msg *)node;
        ke_msg_free(msg);

        ret = 1;
    }

    return ret;
}

UINT32 mr_kmsg_exact_handle(UINT16 rsp)
{
    UINT32 ret = 0;
    struct ke_msg *msg;
    struct co_list_hdr *node;

	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
    node = co_list_pop_front(&rw_msg_rx_head);
	GLOBAL_INT_RESTORE();
	
    if(node)
    {
        msg = (struct ke_msg *)node;
        if(rsp == msg->id)
        {
            ret = 1;
        }
        ke_msg_free(msg);
    }

    return ret;
}

UINT32 mr_kmsg_fetch(UINT8 *buf, UINT32 len)
{
    UINT32 ret = 0;
    UINT32 count;
    struct ke_msg *msg;
    struct co_list_hdr *node;

	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
    node = co_list_pop_front(&rw_msg_rx_head);
	GLOBAL_INT_RESTORE();
	
    if(node)
    {
        msg = (struct ke_msg *)node;
        count = sizeof(struct ke_msg) + msg->param_len - sizeof(msg->param[0]);
        count = MIN(count, len);
        os_memcpy(buf, msg, count);

        ret = count;
        ke_msg_free(msg);
    }

    return ret;
}
#endif

void mhdr_assoc_cfm_cb(FUNC_1PARAM_PTR ind_cb, void *ctxt)
{
	assoc_cfm_cb.cb = ind_cb;
	assoc_cfm_cb.ctxt_arg = ctxt;
}

void mhdr_scanu_reg_cb(FUNC_1PARAM_PTR ind_cb, void *ctxt)
{
	scan_cfm_cb.cb = ind_cb;
	scan_cfm_cb.ctxt_arg = ctxt;
}

void mhdr_deauth_evt_cb(FUNC_1PARAM_PTR ind_cb, void *ctxt)
{
	deauth_evt_cb.cb = ind_cb;
	deauth_evt_cb.ctxt_arg = ctxt;
}

void mhdr_deassoc_evt_cb(FUNC_1PARAM_PTR ind_cb, void *ctxt)
{
	deassoc_evt_cb.cb = ind_cb;
	deassoc_evt_cb.ctxt_arg = ctxt;
}

void mhdr_disconnect_ind(void)
{	
	connect_flag = 0;

	sa_reconnect_init();

	if(deassoc_evt_cb.cb)
	{
		(*deassoc_evt_cb.cb)(deassoc_evt_cb.ctxt_arg);
	}
}

void mhdr_connect_ind(void *msg, UINT32 len)
{		
    struct ke_msg *msg_ptr;
    struct sm_connect_indication *conn_ind_ptr;

	msg_ptr = (struct ke_msg *)msg;
    conn_ind_ptr = (struct sm_connect_indication *)msg_ptr->param;
    if(0 == conn_ind_ptr->status_code)
    {
		os_printf("---------SM_CONNECT_IND_ok\r\n");
		connect_flag = 1;
		
		if(assoc_cfm_cb.cb)
		{
			(*assoc_cfm_cb.cb)(assoc_cfm_cb.ctxt_arg);
		}	
		wpa_enable_traffic_port_at_open_wep();
    }
	else
	{
		os_printf("---------SM_CONNECT_IND_fail\r\n");
		connect_flag = 0;
		
		sa_reconnect_init();

		if(deassoc_evt_cb.cb)
		{
			(*deassoc_evt_cb.cb)(deassoc_evt_cb.ctxt_arg);
		}
	}
}

static void sort_scan_result(SCAN_RST_UPLOAD_T *ap_list)
{
	int i, j;
	struct sta_scan_res *tmp;
	
	if (ap_list->scanu_num == 0)
		return;
	
	for(i=0; i < (ap_list->scanu_num-1); i++) {
		for(j=i+1; j < ap_list->scanu_num; j++) {
			if (ap_list->res[j]->level > ap_list->res[i]->level) {
				tmp = ap_list->res[j];
				ap_list->res[j] = ap_list->res[i];
				ap_list->res[i] = tmp;
			}
		}
	}
}

UINT32 mhdr_scanu_start_cfm(SCAN_RST_UPLOAD_T *ap_list)
{
	UINT32 i;

	if(ap_list)
	{
		sort_scan_result(ap_list);
#if 0
		os_printf("\r\n got %d AP\r\n", ap_list->scanu_num);
		for(i = 0; i < ap_list->scanu_num; i++)
		{
			os_printf("ap%d: name = %s	| strenth=%d | channel=%d\r\n",
					  i, ap_list->res[i]->ssid, 
					  ap_list->res[i]->level,
					  ap_list->res[i]->channel);
		}
#endif
		wpa_buffer_scan_results();
	}

	if(scan_cfm_cb.cb)
	{
		(*scan_cfm_cb.cb)(scan_cfm_cb.ctxt_arg);
	}	
	
	return RW_SUCCESS;
}


UINT32 mhdr_scanu_result_ind(SCAN_RST_UPLOAD_T *scan_rst, void *msg, UINT32 len)
{
    UINT32 ret, chann;
    UINT8 *elmt_addr;
    UINT32 vies_len;
    UINT8 *var_part_addr;	
    struct ke_msg *msg_ptr;
    SCAN_RST_ITEM_PTR item = NULL;
    SCAN_RST_UPLOAD_PTR result_ptr;
    SCAN_IND_PTR scanu_ret_ptr;
    IEEE802_11_PROBE_RSP_PTR probe_rsp_ieee80211_ptr;
	char on_channel;
	UINT8 ssid_len = 0, *pssid;
	
	ret = RW_SUCCESS;
	result_ptr = scan_rst;

	if (result_ptr->scanu_num >= MAX_BSS_LIST)
		goto scan_rst_exit;
	
	msg_ptr = (struct ke_msg *)msg;	
	scanu_ret_ptr = (SCAN_IND_PTR)msg_ptr->param;
	probe_rsp_ieee80211_ptr =  (IEEE802_11_PROBE_RSP_PTR)scanu_ret_ptr->payload;
	vies_len = scanu_ret_ptr->length - MAC_BEACON_VARIABLE_PART_OFT;
	var_part_addr = probe_rsp_ieee80211_ptr->rsp.variable;

	elmt_addr = (UINT8 *)mac_ie_find((UINT32)var_part_addr, (UINT16)vies_len, MAC_ELTID_DS);
	if(elmt_addr) // adjust channel
	{
		chann = *(elmt_addr + MAC_DS_CHANNEL_OFT);
		if (chann == rw_ieee80211_get_chan_id(scanu_ret_ptr->center_freq))
			on_channel = 1;
		else
			on_channel = 0;
	} else {
		chann = rw_ieee80211_get_chan_id(scanu_ret_ptr->center_freq);
		on_channel = 0;
	}

	elmt_addr = (UINT8 *)mac_ie_find((UINT32)var_part_addr, 
										(UINT16)vies_len, 
										MAC_ELTID_SSID);
    if(elmt_addr)
    {
        ssid_len = *(elmt_addr + MAC_SSID_LEN_OFT);

        if (ssid_len > MAC_SSID_LEN) { // invalid ssid length
            goto scan_rst_exit;
        }
		pssid = elmt_addr + MAC_SSID_SSID_OFT;
    }
	
	/* check the duplicate bssid*/
	do
	{
		int i, count;

		for(i = 0; i < result_ptr->scanu_num; i ++)
		{
			if(!os_memcmp(probe_rsp_ieee80211_ptr->bssid, result_ptr->res[i]->bssid, ETH_ALEN))
			{
				if ((result_ptr->res[i]->ssid[0] == '\0') && 
					(ssid_len > 0)){
					os_memcpy(result_ptr->res[i]->ssid, pssid, ssid_len);
				}
				if ((result_ptr->res[i]->on_channel == 1) || (on_channel == 0)) {
					goto scan_rst_exit;
				} else {
					item = result_ptr->res[i]; // should replace it.
					item->level = scanu_ret_ptr->rssi;
					item->channel = chann;
					item->on_channel = 1;
					goto scan_rst_exit;
				}
			}
		}
	}while(0);

	item = (SCAN_RST_ITEM_PTR)sr_malloc_result_item(vies_len);
	if (item == NULL)
		goto scan_rst_exit;

	os_memset(item->ssid, 0, sizeof(item->ssid));
	os_memcpy(item->ssid, pssid, ssid_len);
	
	os_memcpy(item->bssid, probe_rsp_ieee80211_ptr->bssid, ETH_ALEN);
	item->channel = chann;
	item->beacon_int = probe_rsp_ieee80211_ptr->rsp.beacon_int;
	item->caps = probe_rsp_ieee80211_ptr->rsp.capab_info;
	item->level = scanu_ret_ptr->rssi;
	item->on_channel = on_channel;
	
	os_memcpy(item->tsf, probe_rsp_ieee80211_ptr->rsp.timestamp, 8);
	
	item->ie_len = vies_len;
	os_memcpy(item + 1, var_part_addr, vies_len);

	item->security = get_security_type_from_ie((u8 *)var_part_addr, vies_len, item->caps);
	
	result_ptr->res[result_ptr->scanu_num] = item;
	result_ptr->scanu_num ++;
	
scan_rst_exit:
	return ret;
}


UINT32 mr_kmsg_background_handle(void)
{
    UINT8 *msg_ptr;
    UINT32 buf_len;
    UINT32 msg_len;
    struct ke_msg *msg;
	SCAN_RESULT_SET *p_ap_list;
	
    buf_len = sizeof(struct ke_msg) + SCANU_IND_PAYLOAD_LEN;
	if(0 == ind_buf_ptr)
	{
	    msg_ptr = (UINT8 *)os_zalloc(buf_len);
	    if(!msg_ptr)
	    {
	        return RW_FAILURE;
	    }

		ind_buf_ptr = msg_ptr;
	}
	else
	{
		msg_ptr = ind_buf_ptr;
		os_memset(msg_ptr, 0, buf_len);
	}

	while(1)
	{		
		msg_len = mr_kmsg_fetch(msg_ptr, buf_len);
		if(msg_len)
		{
			msg = (struct ke_msg *)msg_ptr;
			switch(msg->id)
			{
				case SCANU_START_CFM:
					scan_cfm = 1;
					mhdr_scanu_start_cfm(scan_rst_set_ptr);	
					break;
					
				case SCANU_RESULT_IND:
					if(scan_cfm && scan_rst_set_ptr)
					{
						sr_release_scan_results(scan_rst_set_ptr);
						scan_rst_set_ptr = 0;
						scan_cfm = 0;
					}
					
					if(0 == scan_rst_set_ptr)
					{						
						scan_rst_set_ptr = (SCAN_RST_UPLOAD_T *)sr_malloc_shell();
						ASSERT(scan_rst_set_ptr);
						scan_rst_set_ptr->scanu_num = 0;
						scan_rst_set_ptr->res = (SCAN_RST_UPLOAD_PTR*)&scan_rst_set_ptr[1];
					}

					mhdr_scanu_result_ind(scan_rst_set_ptr, msg, msg_len);
					
					break;
					
				case SM_CONNECT_IND:
					if(scan_rst_set_ptr)
					{
						sr_release_scan_results(scan_rst_set_ptr);
						scan_rst_set_ptr = 0;
					}
					
					mhdr_connect_ind(msg, msg_len);
					break;

				case SM_DISCONNECT_IND:
					os_printf("SM_DISCONNECT_IND\r\n");
					mhdr_disconnect_ind();
					break;
					
				case MM_KEY_ADD_CFM:
				case MM_KEY_DEL_CFM:
				case ME_SET_CONTROL_PORT_CFM:
				case MM_RESET_CFM:
				case ME_CONFIG_CFM:
				case ME_CHAN_CONFIG_CFM:
				case MM_START_CFM:
				case MM_ADD_IF_CFM:	
					sa_sta_set_rsp_word(msg->id);
					sa_ap_set_rsp_word(msg->id);
					break;

				case APM_START_CFM:
					sa_ap_set_rsp_word(msg->id);
					break;
					
				default:
					//os_printf("ind:%x\r\n", msg->id);
					break;
			}
		}
		else
		{
			break;
		}
	}

	#ifdef DYNAMIC_MSG_BUF
	if(msg_ptr)
	{
		os_free(msg_ptr);
		msg_ptr = 0;
	}
	#endif

    return RW_SUCCESS;
}

// eof

