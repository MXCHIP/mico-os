#include "include.h"
#include "rw_msg_tx.h"
#include "ke_msg.h"
#include "rw_ieee80211.h"
#include "rw_pub.h"
#include "mem_pub.h"
#include "common.h"

#include "scanu_task.h"
#include "scan_task.h"
#include "mm_task.h"
#include "sm_task.h"
#include "me_task.h"
#include "apm_task.h"
#include "vif_mgmt.h"
#include "str_pub.h"
#include "mac_frame.h"
#include "lwip/def.h"
#include "scanu_task.h"
#include "wlan_ui_pub.h"

#if CFG_MODE_SWITCH
#include "param_config.h"
#endif

void mt_msg_send(void const *param_ptr)
{
	bmsg_ioctl_sender(param_ptr);
}

/*MM_RESET_REQ*/
void mt_reset(void)
{
    void *no_param = ke_msg_alloc(MM_RESET_REQ, TASK_MM, TASK_API, 0);
    mt_msg_send(no_param);
}

/*ME_CONFIG_REQ*/
void mt_me_config(void)
{
    struct me_config_req *me_cfg_ptr;

#if CFG_IEEE80211N
    WIPHY_T *wiphy = &g_wiphy;
    struct ieee80211_sta_ht_cap *ht_cap = &wiphy->bands[IEEE80211_BAND_5GHZ].ht_cap;
    struct ieee80211_sta_vht_cap *vht_cap = &wiphy->bands[IEEE80211_BAND_5GHZ].vht_cap;
    uint8_t *ht_mcs = (uint8_t *)&ht_cap->mcs;
    int i;
#endif // CFG_IEEE80211N    

    me_cfg_ptr = KE_MSG_ALLOC(ME_CONFIG_REQ, TASK_ME, TASK_API, me_config_req);

#if CFG_IEEE80211N
    me_cfg_ptr->ht_supp = ht_cap->ht_supported;
    me_cfg_ptr->vht_supp = vht_cap->vht_supported;
    me_cfg_ptr->ht_cap.ht_capa_info = cpu_to_le16(ht_cap->cap);
    me_cfg_ptr->ht_cap.a_mpdu_param = ht_cap->ampdu_factor |
                                      (ht_cap->ampdu_density <<
                                       IEEE80211_HT_AMPDU_PARM_DENSITY_SHIFT);

    for (i = 0; i < sizeof(ht_cap->mcs); i++)
    {
        me_cfg_ptr->ht_cap.mcs_rate[i] = ht_mcs[i];
    }

    me_cfg_ptr->ht_cap.ht_extended_capa = 0;
    me_cfg_ptr->ht_cap.tx_beamforming_capa = 0x64000000;
    me_cfg_ptr->ht_cap.asel_capa = 0x1;

    me_cfg_ptr->vht_cap.vht_capa_info = cpu_to_le32(vht_cap->cap);
    me_cfg_ptr->vht_cap.rx_highest = cpu_to_le16(vht_cap->vht_mcs.rx_highest);
    me_cfg_ptr->vht_cap.rx_mcs_map = cpu_to_le16(vht_cap->vht_mcs.rx_mcs_map);
    me_cfg_ptr->vht_cap.tx_highest = cpu_to_le16(vht_cap->vht_mcs.tx_highest);
    me_cfg_ptr->vht_cap.tx_mcs_map = cpu_to_le16(vht_cap->vht_mcs.tx_mcs_map);
#endif // CFG_IEEE80211N

    mt_msg_send(me_cfg_ptr);
}

/*ME_CHAN_CONFIG_REQ*/
void mt_channel_config(void)
{
    int i;
    WIPHY_T *wiphy = &g_wiphy;
    struct me_chan_config_req *req;

    req = KE_MSG_ALLOC(ME_CHAN_CONFIG_REQ, TASK_ME, TASK_API, me_chan_config_req);

    req->chan2G4_cnt =  0;
    if (wiphy->bands[IEEE80211_BAND_2GHZ].num_channels)
    {
        struct ieee80211_supported_band *b = &wiphy->bands[IEEE80211_BAND_2GHZ];
        for (i = 0; i < b->num_channels; i++)
        {
            req->chan2G4[req->chan2G4_cnt].flags = 0;

            if (b->channels[i].flags & IEEE80211_CHAN_DISABLED)
                req->chan2G4[req->chan2G4_cnt].flags |= SCAN_DISABLED_BIT;

            if (b->channels[i].flags & IEEE80211_CHAN_NO_IR)
                req->chan2G4[req->chan2G4_cnt].flags |= SCAN_PASSIVE_BIT;

            req->chan2G4[req->chan2G4_cnt].band = IEEE80211_BAND_2GHZ;
            req->chan2G4[req->chan2G4_cnt].freq = b->channels[i].center_freq;
            req->chan2G4_cnt++;

            if (req->chan2G4_cnt == SCAN_CHANNEL_2G4)
                break;
        }
    }

    req->chan5G_cnt = 0;
    if (wiphy->bands[IEEE80211_BAND_5GHZ].num_channels)
    {
        struct ieee80211_supported_band *b = &wiphy->bands[IEEE80211_BAND_5GHZ];
        for (i = 0; i < b->num_channels; i++)
        {
            req->chan5G[req->chan5G_cnt].flags = 0;

            if (b->channels[i].flags & IEEE80211_CHAN_DISABLED)
                req->chan5G[req->chan5G_cnt].flags |= SCAN_DISABLED_BIT;

            if (b->channels[i].flags & IEEE80211_CHAN_NO_IR)
                req->chan5G[req->chan5G_cnt].flags |= SCAN_PASSIVE_BIT;

            req->chan5G[req->chan5G_cnt].band = IEEE80211_BAND_5GHZ;
            req->chan5G[req->chan5G_cnt].freq = b->channels[i].center_freq;
            req->chan5G_cnt++;
            if (req->chan5G_cnt == SCAN_CHANNEL_5G)
                break;
        }
    }

    mt_msg_send(req);
}

/*MM_START_REQ*/
void mt_start(void)
{
    struct mm_start_req *start_ptr;

    start_ptr = KE_MSG_ALLOC(MM_START_REQ, TASK_MM, TASK_API, mm_start_req);
    start_ptr->phy_cfg.parameters[0] = 1;
    start_ptr->phy_cfg.parameters[1] = 0;
    start_ptr->uapsd_timeout = 300;
    start_ptr->lp_clk_accuracy = 20;

    mt_msg_send(start_ptr);
}

/*MM_ADD_IF_REQ*/
void mt_add_if(UINT32 type)
{
    extern int hwaddr_aton(const char * txt, u8 * addr);
    struct mm_add_if_req *add_if_ptr;

    ASSERT((VIF_AP == type) || (VIF_STA == type));

    add_if_ptr = KE_MSG_ALLOC(MM_ADD_IF_REQ, TASK_MM, TASK_API, mm_add_if_req);
    add_if_ptr->type = type;
	wifi_get_mac_address((u8 *)&add_if_ptr->addr);
    mt_msg_send(add_if_ptr);
}

/*APM_START_REQ*/
#if CFG_WIFI_AP_MODE
void mt_apm_start(void)
{
    struct apm_start_req *start_req_ptr;

    start_req_ptr = KE_MSG_ALLOC(APM_START_REQ, TASK_APM, TASK_API, apm_start_req);

    start_req_ptr->basic_rates.length = 4;
    start_req_ptr->basic_rates.array[0] = 130;
    start_req_ptr->basic_rates.array[1] = 132;
    start_req_ptr->basic_rates.array[2] = 139;
    start_req_ptr->basic_rates.array[3] = 150;

    start_req_ptr->chan.band = 0;
    start_req_ptr->chan.flags = 0;
#if CFG_MODE_SWITCH
	start_req_ptr->chan.freq = rw_ieee80211_get_centre_frequency(g_ap_param_ptr->chann);
	start_req_ptr->center_freq1 = rw_ieee80211_get_centre_frequency(g_ap_param_ptr->chann);
#else
    start_req_ptr->chan.freq = rw_ieee80211_get_centre_frequency(CFG_CHANNEL_AP);
    start_req_ptr->center_freq1 = rw_ieee80211_get_centre_frequency(CFG_CHANNEL_AP);
#endif
    start_req_ptr->center_freq2 = 0;
    start_req_ptr->ch_width = 0;

    start_req_ptr->bcn_addr = (UINT32)beacon;
    start_req_ptr->bcn_len = sizeof(beacon);
    start_req_ptr->tim_oft = 56;
    start_req_ptr->tim_len = 6;
    start_req_ptr->bcn_int = BEACON_INTERVAL;

#if CFG_MODE_SWITCH
	if(g_ap_param_ptr->cipher_suite > CONFIG_CIPHER_WEP){
		start_req_ptr->flags = WPA_WPA2_IN_USE;
	}else{
		start_req_ptr->flags = 0;
	}
#else
#if CFG_WIFI_WPA
    start_req_ptr->flags = WPA_WPA2_IN_USE;
#else
    start_req_ptr->flags = 0;
#endif
#endif
    start_req_ptr->ctrl_port_ethertype = 36488;
    start_req_ptr->vif_idx = 0;

    mt_msg_send(start_req_ptr);
}
#endif

/*MM_KEY_ADD_REQ*/
void mt_key_add(KEY_PARAM_T *key_param)
{
    struct mm_key_add_req *add_key_ptr;

    add_key_ptr = KE_MSG_ALLOC(MM_KEY_ADD_REQ, TASK_MM, TASK_API, mm_key_add_req);
    add_key_ptr->cipher_suite = key_param->cipher_suite;
    add_key_ptr->sta_idx = key_param->sta_idx;
    add_key_ptr->inst_nbr = key_param->inst_nbr;
    add_key_ptr->key.length = key_param->key.length;
    os_memcpy((uint8_t *)add_key_ptr->key.array, (uint8_t *)key_param->key.array, add_key_ptr->key.length);

    add_key_ptr->key_idx = key_param->key_idx;
    add_key_ptr->spp = 0;

    mt_msg_send(add_key_ptr);
}

/*MM_KEY_DEL_REQ*/
void mt_key_del(KEY_PARAM_T *key_param)
{
    struct mm_key_del_req *del_key_req;

    del_key_req = KE_MSG_ALLOC(MM_KEY_DEL_REQ, TASK_MM, TASK_API, mm_key_del_req);
    del_key_req->hw_key_idx = key_param->hw_key_idx;

    mt_msg_send(del_key_req);
}

/*ME_SET_CONTROL_PORT_REQ*/
void mt_set_ctrl_port(void)
{
	uint32_t ret;
	uint32_t staid = 0;
    struct me_set_control_port_req *set_port_ptr;	
	
	ret = vif_mgmt_first_used_staid(&staid);
	if(ret)
	{
		os_printf("trMsdu_Strange\r\n");
	}

    set_port_ptr = KE_MSG_ALLOC(ME_SET_CONTROL_PORT_REQ, TASK_ME, TASK_API, me_set_control_port_req);
    set_port_ptr->control_port_open = 1;
    set_port_ptr->sta_idx = staid;

    mt_msg_send(set_port_ptr);
}

void mt_disable_ctrl_port(void)
{
    struct me_set_control_port_req *set_port_ptr;

    set_port_ptr = KE_MSG_ALLOC(ME_SET_CONTROL_PORT_REQ, TASK_ME, TASK_API, me_set_control_port_req);
    set_port_ptr->control_port_open = 0;
    set_port_ptr->sta_idx = 0;

    mt_msg_send(set_port_ptr);
}

/*SCAN_CANCEL_REQ*/
void mt_scan_cancel(void)
{
    struct scan_cancel_req *req_ptr;

    req_ptr = KE_MSG_ALLOC(SCAN_CANCEL_REQ, TASK_SCAN, TASK_API, scan_cancel_req);
	
    mt_msg_send(req_ptr);
}

/*SCANU_FAST_REQ*/
void mt_scan_fast(FAST_SCAN_PARAM_T *fscan_param)
{
    struct scanu_fast_req *fscan_start_ptr;

    fscan_start_ptr = KE_MSG_ALLOC(SCANU_FAST_REQ, TASK_SCANU, TASK_API, scanu_fast_req);
	fscan_start_ptr->bssid = fscan_param->bssid;
	fscan_start_ptr->ch_nbr= fscan_param->ch_num;
	fscan_start_ptr->maxch_time = fscan_param->max_ch_time;
	fscan_start_ptr->minch_time = fscan_param->min_ch_time;
	fscan_start_ptr->probe_delay = fscan_param->probe_delay;
	fscan_start_ptr->ssid = fscan_param->ssid;
	
    mt_msg_send(fscan_start_ptr);
}

/*SCANU_START_REQ*/
void mt_scan_start(SCAN_PARAM_T *scan_param)
{
    UINT32 i;
    UINT32 channel_id;
    struct scan_start_req *scan_start_ptr;

    scan_start_ptr = KE_MSG_ALLOC(SCANU_START_REQ, TASK_SCANU, TASK_API, scan_start_req);

    channel_id = 0;
    for(i = 0; i < g_wiphy.bands[IEEE80211_BAND_2GHZ].num_channels; i ++)
    {
        scan_start_ptr->chan[i].band = IEEE80211_BAND_2GHZ;
        scan_start_ptr->chan[i].flags = 0;
        scan_start_ptr->chan[i].freq = rw_ieee80211_get_centre_frequency(i + 1);

        channel_id ++;
    }

#ifdef ENABLE_5GHZ_IEEE80211
    for(i = 0; i < g_wiphy.bands[IEEE80211_BAND_5GHZ].num_channels; i ++)
    {
        scan_start_ptr->chan[i].band = IEEE80211_BAND_5GHZ;
        scan_start_ptr->chan[i].flags = 0;
        scan_start_ptr->chan[i].freq = rw_ieee80211_get_centre_frequency(i
                                       + SCAN_CHANNEL_2G4
                                       + 1);

        ASSERT(scan_start_ptr->chan[i].freq);
        channel_id ++;
    }
#endif

    scan_start_ptr->chan_cnt = channel_id;

    os_memcpy(&scan_start_ptr->bssid, &scan_param->bssid, sizeof(scan_start_ptr->bssid));
    scan_start_ptr->ssid_cnt = scan_param->num_ssids;
    for(i = 0; i < scan_start_ptr->ssid_cnt; i++)
    {
        scan_start_ptr->ssid[i].length = scan_param->ssids[i].length;
        os_memcpy(scan_start_ptr->ssid[i].array, scan_param->ssids[i].array, scan_start_ptr->ssid[i].length);
    }
	
    scan_start_ptr->add_ies = 0;
    scan_start_ptr->add_ie_len = 0;

    mt_msg_send(scan_start_ptr);
}

/*SM_CONNECT_REQ*/
void mt_sm_connect(CONNECT_PARAM_T *sme)
{
    struct sm_connect_req *sm_connect_req_ptr;

    ASSERT(sme);

    sm_connect_req_ptr = KE_MSG_ALLOC(SM_CONNECT_REQ, TASK_SM, TASK_API, sm_connect_req);

    sm_connect_req_ptr->ssid.length = sme->ssid.length;
    os_memcpy(sm_connect_req_ptr->ssid.array, sme->ssid.array, sme->ssid.length);
    os_memcpy(&sm_connect_req_ptr->bssid, &sme->bssid, sizeof(sme->bssid));

    sm_connect_req_ptr->vif_idx = sme->vif_idx;
    sm_connect_req_ptr->chan.band = sme->chan.band;
    sm_connect_req_ptr->chan.flags = sme->chan.flags;
    sm_connect_req_ptr->chan.freq = sme->chan.freq;
    sm_connect_req_ptr->flags = sme->flags;
    sm_connect_req_ptr->ctrl_port_ethertype = PP_HTONS(ETH_P_PAE);
    sm_connect_req_ptr->ie_len = sme->ie_len;
    sm_connect_req_ptr->auth_type = sme->auth_type;
    os_memcpy((UINT8 *)sm_connect_req_ptr->ie_buf, (UINT8 *)sme->ie_buf, sm_connect_req_ptr->ie_len);
	
    mt_msg_send(sm_connect_req_ptr);
}

void mt_sm_disconnect(DISCONNECT_PARAM_T *sme)
{
	struct sm_disconnect_req *sm_disconnect_req_ptr;
	
    ASSERT(sme);
	sm_disconnect_req_ptr = KE_MSG_ALLOC(SM_DISCONNECT_REQ, TASK_SM, TASK_API, sm_disconnect_req);

	sm_disconnect_req_ptr->vif_idx = sme->vif_idx;
	sm_disconnect_req_ptr->reason_code = sme->reason_code;
	
    mt_msg_send(sm_disconnect_req_ptr);
}
	

/*MM_REMOVE_IF_REQ*/
void mt_remove_if(UINT32 vif_index)
{
    struct mm_remove_if_req *remove_if_req;

    remove_if_req = KE_MSG_ALLOC(MM_REMOVE_IF_REQ, TASK_MM, TASK_API,
                                    mm_remove_if_req);
    ASSERT(remove_if_req);
    remove_if_req->inst_nbr = vif_index;
	
    mt_msg_send(remove_if_req);
}

/*MM_REMOVE_IF_REQ*/
void mt_set_filter(UINT32 filter)
{
    struct mm_set_filter_req *set_filter_req;

    set_filter_req = KE_MSG_ALLOC(MM_SET_FILTER_REQ, TASK_MM, TASK_API,
                                    mm_set_filter_req);
    ASSERT(set_filter_req);
    set_filter_req->filter = filter;
	
    mt_msg_send(set_filter_req);
}

/*MM_REMOVE_IF_REQ*/
void mt_set_channel(UINT32 channel)
{
    struct mm_set_channel_req *set_channel_req;

    set_channel_req = KE_MSG_ALLOC(MM_SET_CHANNEL_REQ, TASK_MM, TASK_API,
                                    mm_set_channel_req);
    ASSERT(set_channel_req);

    set_channel_req->band = PHY_BAND_2G4;
    set_channel_req->type = PHY_CHNL_BW_20;
    set_channel_req->center1_freq = set_channel_req->center2_freq = 
        rw_ieee80211_get_centre_frequency(channel);
    set_channel_req->index = PHY_PRIM;
    set_channel_req->tx_power = 0;
	set_channel_req->index = PHY_SEC;
    
    mt_msg_send(set_channel_req);
}

void mt_remove_intf(UINT8 vif_id)
{
	struct mm_remove_if_req *remove_if_req;

    remove_if_req = KE_MSG_ALLOC(MM_REMOVE_IF_REQ, TASK_MM, TASK_API,
									mm_remove_if_req);
	ASSERT(remove_if_req);

	/* Set parameters for the MM_REMOVE_IF_REQ message */
	remove_if_req->inst_nbr = vif_id;

	/* Send the MM_REMOVE_IF_REQ message to LMAC FW */
    mt_msg_send(remove_if_req);
}

void mt_connection_loss(UINT8 vif_id)
{
    struct mm_connection_loss_ind *ind =
        KE_MSG_ALLOC(MM_CONNECTION_LOSS_IND, TASK_SM, TASK_API, mm_connection_loss_ind);

    // Fill-in the indication message parameters
    ind->inst_nbr = vif_id;

    // Send the indication to the upper layers
    ke_msg_send(ind);
}

void mt_msg_dispatch(UINT16 cmd, void *param)
{
    switch(cmd)
    {
    case MM_REMOVE_IF_REQ:
		ASSERT(param);
		mt_remove_intf(*(UINT8 *)param);
		break;
	
	case MM_SET_FILTER_REQ:
		mt_set_filter(*(UINT32 *)param);
		break;
	
	case MM_SET_CHANNEL_REQ:
		mt_set_channel(*(UINT32 *)param);
		break;		 
		
    case MM_RESET_REQ:
        mt_reset();
        break;

    case ME_CONFIG_REQ:
        mt_me_config();
        break;

    case ME_CHAN_CONFIG_REQ:
        mt_channel_config();
        break;

    case MM_START_REQ:
        mt_start();
        break;

    case MM_ADD_IF_REQ:
        ASSERT(param);
        mt_add_if(*(UINT32 *)param);
        break;

#if CFG_WIFI_AP_MODE
    case APM_START_REQ:
        mt_apm_start();
        break;
#endif

    case MM_KEY_ADD_REQ:
        mt_key_add((KEY_PARAM_T *)param);
        break;

    case MM_KEY_DEL_REQ:
        mt_key_del((KEY_PARAM_T *)param);
        break;

    case ME_SET_CONTROL_PORT_REQ:
        mt_set_ctrl_port();
        break;

    case SM_CONNECT_REQ:
        mt_sm_connect((CONNECT_PARAM_T *)param);
        break;
			
	case SCANU_START_REQ:
		mt_scan_start((SCAN_PARAM_T *)param);
		break;
		
    case SCANU_FAST_REQ:
        mt_scan_fast((FAST_SCAN_PARAM_T *)param);
        break;

	case SCAN_CANCEL_REQ:
		mt_scan_cancel();
		break;
		
	case MM_CONNECTION_LOSS_IND:
		ASSERT(param);
		mt_connection_loss(*(UINT8 *)param);
		break;

	case SM_DISCONNECT_REQ:
		mt_sm_disconnect((DISCONNECT_PARAM_T *)param);
		break;

    default:
        break;
    }
}

// eof

