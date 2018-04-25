#include "include.h"
#include "hostapd_intf.h"
#include "sk_intf.h"
#include "mem_pub.h"
#include "me_task.h"
#include "ke_event.h"

#include "uart_pub.h"
#include "ieee802_11_defs.h"
#include "driver.h"
#include "driver_beken.h"
#include "hostapd_cfg.h"
#include "errno-base.h"
#include "drv_model_pub.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "mm.h"
#include "rw_pub.h"
#include "sa_station.h"
#include "sa_ap.h"
#include "wlan_ui_pub.h"

#if CFG_MODE_SWITCH
#include "param_config.h"
#include "flash_pub.h"
#endif

#if CFG_WIFI_STATION_MODE
struct scanu_rst_upload *scan_rstup_ptr;
extern struct assoc_ap_info assoc_ap;
#endif

int hapd_intf_sta_add(struct prism2_hostapd_param *param, int len)
{
    UINT32 param_len;
    struct ke_msg *kmsg_dst;
    struct me_sta_add_req *sta_add_ptr;

    param_len = sizeof(struct me_sta_add_req);
    kmsg_dst = (struct ke_msg *)os_malloc(sizeof(struct ke_msg)
                                          + param_len);
    ASSERT(kmsg_dst);
    os_memset(kmsg_dst, 0, (sizeof(struct ke_msg) + param_len));

    kmsg_dst->id = ME_STA_ADD_REQ;
    kmsg_dst->dest_id = TASK_ME;
    kmsg_dst->src_id  = DRV_TASK_ID;
    kmsg_dst->param_len = param_len;

    sta_add_ptr = (struct me_sta_add_req *)kmsg_dst->param;

    os_memcpy((void *)&sta_add_ptr->mac_addr, param->sta_addr, ETH_ALEN);

    sta_add_ptr->vif_idx = 0;
    sta_add_ptr->aid = param->u.add_sta.aid;
    sta_add_ptr->flags = 0x01; // 1:STA_QOS_CAPA 2: STA_HT_CAPA

    WPAS_PRT("hapd_intf_sta_add:%d\r\n", sta_add_ptr->aid);

    sta_add_ptr->rate_set.length = 12;
    sta_add_ptr->rate_set.array[0] = 130;
    sta_add_ptr->rate_set.array[1] = 132;
    sta_add_ptr->rate_set.array[2] = 139;
    sta_add_ptr->rate_set.array[3] = 150;
    sta_add_ptr->rate_set.array[4] = 36;
    sta_add_ptr->rate_set.array[5] = 48;
    sta_add_ptr->rate_set.array[6] = 72;
    sta_add_ptr->rate_set.array[7] = 108;
    sta_add_ptr->rate_set.array[8] = 12;
    sta_add_ptr->rate_set.array[9] = 18;
    sta_add_ptr->rate_set.array[10] = 24;
    sta_add_ptr->rate_set.array[11] = 96;

    sta_add_ptr->ht_cap.mcs_rate[0] = 255;

    ke_msg_send(ke_msg2param(kmsg_dst));

    return 0;
}

int hapd_intf_add_key(struct prism2_hostapd_param *param, int len)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);
    struct sta_info_tag *sta;
    KEY_PARAM_T key_param;

    if(os_strcmp((char *)param->u.crypt.alg, "WEP40") == 0)
	{
    	WPAS_PRT("add WEP40\r\n");
        key_param.cipher_suite = MAC_RSNIE_CIPHER_WEP40;
	}
    else if(os_strcmp((char *)param->u.crypt.alg, "WEP104") == 0)
    {
    	WPAS_PRT("add WEP104\r\n");
        key_param.cipher_suite = MAC_RSNIE_CIPHER_WEP104;
    }
    else if(os_strcmp((char *)param->u.crypt.alg, "TKIP") == 0)
    {
    	WPAS_PRT("add TKIP\r\n");
        key_param.cipher_suite = MAC_RSNIE_CIPHER_TKIP;
    }
    else if(os_strcmp((char *)param->u.crypt.alg, "CCMP") == 0)
    {
    	WPAS_PRT("hapd_intf_add_key CCMP\r\n");
        key_param.cipher_suite = MAC_RSNIE_CIPHER_CCMP;
    }

    if(is_broadcast_ether_addr(param->sta_addr))
    {
    	WPAS_PRT("add is_broadcast_ether_addr\r\n");
        key_param.sta_idx = 0xFF;
        key_param.inst_nbr = vif->index;

		wpa_enable_traffic_port_at_NonOpensystem();
    }
    else
    {
    	wpa_enable_traffic_port_at_ApNonOpensystem();
		
    	WPAS_PRT("add sta_mgmt_get_sta\r\n");
        sta = sta_mgmt_get_sta(param->sta_addr);
        if(sta == NULL)
        {
            return -1;
        }
        key_param.sta_idx = sta->staid;
        key_param.inst_nbr = sta->inst_nbr;
    }

    key_param.key_idx = param->u.crypt.idx;
    key_param.key.length = param->u.crypt.key_len;
    os_memcpy((u8 *) & (key_param.key.array[0]), (u8 *)&param[1], key_param.key.length);

#if CFG_WIFI_AP_MODE
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_AP)
#endif
	{
    	sa_ap_send_and_wait_rsp(MM_KEY_ADD_REQ, (void *)&key_param, MM_KEY_ADD_CFM);
	}
#endif

#if CFG_WIFI_STATION_MODE
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_STA)
#endif
	{
	    sa_station_send_and_wait_rsp(MM_KEY_ADD_REQ, (void *)&key_param, MM_KEY_ADD_CFM);
	}
#endif

    return 0;
}


int hapd_intf_del_key(struct prism2_hostapd_param *param, int len)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);
    struct key_info_tag *key;
    struct sta_info_tag *sta;
    KEY_PARAM_T key_param;

    if(is_broadcast_ether_addr(param->sta_addr))
    {
        key = vif->default_key;
    }
    else
    {
        sta = sta_mgmt_get_sta(param->sta_addr);
        if(sta == NULL)
        {
            return -1;
        }
        key = *sta->sta_sec_info.cur_key;
    }

    if(key)
    {
        key_param.hw_key_idx = key->hw_key_idx;
		mm_sec_machwkey_del(key->hw_key_idx);
    }

    return 0;
}

#if CFG_WIFI_STATION_MODE
void wpa_buffer_scan_results(void)
{
	scan_rstup_ptr = sr_get_scan_results();
}

int wpa_reg_assoc_cfm_callback(struct prism2_hostapd_param *param, int len)
{
	ASSERT(param);
	mhdr_assoc_cfm_cb(param->u.reg_assoc_cfm.cb, 
							param->u.reg_assoc_cfm.arg);
}

int wpa_reg_scan_cfm_callback(struct prism2_hostapd_param *param, int len)
{
	ASSERT(param);
	mhdr_scanu_reg_cb(param->u.reg_scan_cfm.cb, 
							param->u.reg_scan_cfm.arg);
}

int wpa_reg_deassoc_evt_callback(struct prism2_hostapd_param *param, int len)
{
	ASSERT(param);
	mhdr_deassoc_evt_cb(param->u.reg_disassoc_evt.cb, 
							param->u.reg_disassoc_evt.arg);
}

int wpa_reg_deauth_evt_callback(struct prism2_hostapd_param *param, int len)
{
	ASSERT(param);
	mhdr_deauth_evt_cb(param->u.reg_deauth_evt.cb, 
							param->u.reg_deauth_evt.arg);
}

int wpa_send_scan_req(struct prism2_hostapd_param *param, int len)
{
    UINT8 i;
    int ret = 0;
    SCAN_PARAM_T scan_param;

    WPAS_PRT("wpa_send_scan_req\r\n");
    scan_param.num_ssids = param->u.scan_req.ssids_num;
    for(i = 0; i < scan_param.num_ssids; i++)
    {
        scan_param.ssids[i].length = param->u.scan_req.ssids[i].ssid_len;
        os_memcpy(scan_param.ssids[i].array, param->u.scan_req.ssids[i].ssid, scan_param.ssids[i].length);
    }
    scan_param.bssid = mac_addr_bcst;

	mt_msg_dispatch(SCANU_START_REQ, &scan_param);

    return ret;
}

int wpa_get_scan_rst(struct prism2_hostapd_param *param, int len)
{
    struct wpa_scan_results *results = param->u.scan_rst;
    struct sta_scan_res *scan_rst_ptr;
    struct wpa_scan_res *r;
    int i, ret = 0;

    if(scan_rstup_ptr == NULL)
    {
	    WPAS_PRT("get_scan_rst_null\r\n");
        return -1;
    }
    
    WPAS_PRT("wpa_get_scan_rst:%d\r\n", scan_rstup_ptr->scanu_num);
    for(i = 0; i < scan_rstup_ptr->scanu_num; i++)
    {
        scan_rst_ptr = scan_rstup_ptr->res[i];
        r = os_zalloc(sizeof(*r) + scan_rst_ptr->ie_len);
        if (r == NULL)
        {
            ret = -1;
			WPAS_PRT("wpa_get_scan_rst break;\r\n");
            break;
        }

        os_memcpy(r->bssid, scan_rst_ptr->bssid, ETH_ALEN);
        r->level = scan_rst_ptr->level;
        r->freq = rw_ieee80211_get_centre_frequency(scan_rst_ptr->channel);
        r->beacon_int = scan_rst_ptr->beacon_int;
        r->caps = scan_rst_ptr->caps;
        r->tsf = WPA_GET_BE64(scan_rst_ptr->tsf);
        r->ie_len = scan_rst_ptr->ie_len;
        os_memcpy(r + 1, scan_rst_ptr + 1, scan_rst_ptr->ie_len);

        results->res[results->num++] = r;
    }

    return ret;
}

int wpa_send_assoc_req(struct prism2_hostapd_param *param, int len)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);
    CONNECT_PARAM_T connect_param = {0};
    int ret;

    os_memcpy((UINT8 *)&connect_param.bssid, param->u.assoc_req.bssid, ETH_ALEN);
    connect_param.flags = CONTROL_PORT_HOST;
    if(param->u.assoc_req.proto & (WPA_PROTO_WPA | WPA_PROTO_RSN))
    {
        connect_param.flags |= WPA_WPA2_IN_USE;
    }
	
    connect_param.vif_idx = vif->index;
    connect_param.ssid.length = param->u.assoc_req.ssid_len;
    os_memcpy(connect_param.ssid.array, param->u.assoc_req.ssid, connect_param.ssid.length);
    connect_param.ie_len = param->u.assoc_req.ie_len;
    os_memcpy((UINT8 *)connect_param.ie_buf, (UINT8 *)param->u.assoc_req.ie_buf, connect_param.ie_len);

	connect_param.chan.freq = rw_ieee80211_get_centre_frequency(param->u.assoc_req.chann);
	connect_param.chan.band = 0;
	connect_param.chan.flags = 0;
	connect_param.chan.tx_power = 10;
	connect_param.auth_type = param->u.assoc_req.auth_alg;
    ret = sa_station_send_associate_cmd(&connect_param);

	if(scan_rstup_ptr)
	{
	    sr_release_scan_results(scan_rstup_ptr);
	    scan_rstup_ptr = NULL;
	}
	
    return ret;
}

int wpa_send_disconnect_req(struct prism2_hostapd_param *param, int len)
{
    struct vif_info_tag *vif = (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);
	DISCONNECT_PARAM_T disconnect_param = {0};
	int ret = 0;

	disconnect_param.vif_idx = vif->index;
	disconnect_param.reason_code = param->u.disconnect_req.reason;
	
	ret = sa_station_send_disassociate_cmd(&disconnect_param); 

	return ret;
}

void wpa_enable_traffic_port_at_ApNonOpensystem(void)
{
	if(bk_wlan_is_ap())
	{
		WPAS_PRT("ME_SET_CONTROL_PORT_REQ\r\n");
		mt_msg_dispatch(ME_SET_CONTROL_PORT_REQ, 0);
	}	
}

void wpa_enable_traffic_port_at_NonOpensystem(void)
{
	WPAS_PRT("ME_SET_CONTROL_PORT_REQ\r\n");
	mt_msg_dispatch(ME_SET_CONTROL_PORT_REQ, 0);
}

void wpa_enable_traffic_port_at_open_wep(void)
{
	if(bk_sta_cipher_is_open() || bk_sta_cipher_is_wep())
	{
		mt_msg_dispatch(ME_SET_CONTROL_PORT_REQ, 0);
	}
}

#endif

int hapd_intf_ioctl(unsigned long arg)
{
    u32 cmd;
    int len, ret = 0;
    struct iwreq *iwr_ptr;
    struct prism2_hostapd_param *param;

    if(0 == arg)
    {
        ret = EINVAL;
        goto ioctl_exit;
    }

    iwr_ptr = (struct iwreq *)arg;
    len = iwr_ptr->u.data.length;
    param = iwr_ptr->u.data.pointer;

    cmd = param->cmd;
    switch(cmd)
    {
    case PRISM2_HOSTAPD_SET_FLAGS_STA:
        break;

    case PRISM2_SET_ENCRYPTION:
        if(os_strcmp((const char *)param->u.crypt.alg, "NONE") == 0)
        {
            ret = hapd_intf_del_key(param, len);
        }
        else
        {
            ret = hapd_intf_add_key(param, len);
        }
        break;

    case PRISM2_GET_ENCRYPTION:
        break;

    case PRISM2_HOSTAPD_FLUSH:
        break;

    case PRISM2_HOSTAPD_ADD_STA:
        ret = hapd_intf_sta_add(param, len);
        break;

    case PRISM2_HOSTAPD_REMOVE_STA:
        break;

    case PRISM2_HOSTAPD_GET_INFO_STA:
        break;

    case PRISM2_HOSTAPD_STA_CLEAR_STATS:
        break;

    case PRISM2_HOSTAPD_SET_GENERIC_ELEMENT:
        break;

#if CFG_WIFI_STATION_MODE
	case PRISM2_HOSTAPD_REG_SCAN_CALLBACK:
		wpa_reg_scan_cfm_callback(param, len);
		break;
		
	case PRISM2_HOSTAPD_REG_ASSOC_CALLBACK:
		wpa_reg_assoc_cfm_callback(param, len);
		break;
    case PRISM2_HOSTAPD_SCAN_REQ:
        ret = wpa_send_scan_req(param, len);
        break;

    case PRISM2_HOSTAPD_GET_SCAN_RESULT:
        ret = wpa_get_scan_rst(param, len);
        break;

    case PRISM2_HOSTAPD_ASSOC_REQ:
        ret = wpa_send_assoc_req(param, len);
        break;

    case PRISM2_HOSTAPD_ASSOC_ACK:		
		//WPAS_PRT("\r\n PRISM2_HOSTAPD_ASSOC_ACK\r\n");
        break;
		
	case PRISM2_HOSTAPD_REG_DISASSOC_CALLBACK:
		wpa_reg_deassoc_evt_callback(param, len);
		break;
		
	case PRISM2_HOSTAPD_REG_DEAUTH_CALLBACK:
		wpa_reg_deauth_evt_callback(param, len);
		break;
	case PRISM2_HOSTAPD_REG_APP_START:
		// net_address_config
		break;
	case PRISM2_HOSTAPD_DISCONN_REQ:
		wpa_send_disconnect_req(param, len);
		break;
#endif

    default:
        break;
    }

ioctl_exit:
    return ret;
}

int hapd_intf_set_ap(void *beacon, int bcn_len, int head_len)
{
    char *bcn_buf;
    UINT32 param_len;
    struct ke_msg *kmsg_dst;
    struct mm_bcn_change_req *bcn_change_ptr;

    param_len = sizeof(struct mm_bcn_change_req);
    kmsg_dst = (struct ke_msg *)os_malloc(sizeof(struct ke_msg)
                                          + param_len);
    ASSERT(kmsg_dst);
    os_memset(kmsg_dst, 0, (sizeof(struct ke_msg) + param_len));

    kmsg_dst->id = MM_BCN_CHANGE_REQ;
    kmsg_dst->dest_id = TASK_MM;
    kmsg_dst->src_id  = DRV_TASK_ID;
    kmsg_dst->param_len = param_len;

    bcn_buf = (char *)os_malloc(bcn_len);
    ASSERT(bcn_buf);
    os_memcpy(bcn_buf, beacon, bcn_len);

    bcn_change_ptr = (struct mm_bcn_change_req *)kmsg_dst->param;
    bcn_change_ptr->bcn_ptr = (UINT32)bcn_buf;
    bcn_change_ptr->bcn_len = bcn_len;
    bcn_change_ptr->tim_len = 6;
    bcn_change_ptr->tim_oft = head_len;
    bcn_change_ptr->bcn_malloc_flag = 1;

    ke_msg_send(ke_msg2param(kmsg_dst));

    return 0;
}

void hapd_intf_ke_rx_handle(int dummy)
{
    int payload_size;
    unsigned char *buf;

    if(dummy == HOSTAPD_MGMT)
    {
        int param_len;
        struct ke_msg *kmsg_dst;
        struct me_mgmt_tx_req *mgmt_tx_ptr;
		
        payload_size = ke_mgmt_peek_rxed_next_payload_size();
        ASSERT(payload_size);
        buf = (unsigned char *)os_malloc(payload_size);
        if(0 == buf)
        {
            goto exit;
        }
        ke_mgmt_packet_rx(buf, payload_size, 0);
        param_len = sizeof(struct me_mgmt_tx_req);
        kmsg_dst = (struct ke_msg *)os_malloc(sizeof(struct ke_msg)
                                              + param_len);
        if(0 == kmsg_dst)
        {
            goto kmsg_malloc_fail;
        }
        os_memset(kmsg_dst, 0, (sizeof(struct ke_msg) + param_len));

        kmsg_dst->id = ME_MGMT_TX_REQ;
        kmsg_dst->dest_id = TASK_ME;
        kmsg_dst->src_id  = 100;   // DRV_TASK_ID
        kmsg_dst->param_len = param_len;

        mgmt_tx_ptr = (struct me_mgmt_tx_req *)kmsg_dst->param;
        mgmt_tx_ptr->addr = (UINT32)buf;
        mgmt_tx_ptr->hostid = (UINT32)buf;
        mgmt_tx_ptr->len = payload_size;

        mgmt_tx_ptr->req_frm_new = 1;

        ke_msg_send(ke_msg2param(kmsg_dst));
    }
    else if(dummy == HOSTAPD_DATA)
    {
        UINT8 *pd_ptr;
		
        payload_size = ke_data_peek_rxed_next_payload_size();
        pd_ptr = (UINT8 *)os_malloc(payload_size);
        if(NULL == pd_ptr)
        {
            os_printf("hapd_intf_tx_error\r\n");
            goto exit;
        }

        ke_l2_packet_rx(pd_ptr, payload_size, 0);
        rwm_transfer(pd_ptr, payload_size);

        os_free(pd_ptr);
    }
    return;

kmsg_malloc_fail:
    os_free(buf);

exit:
    return;
}

// eof

