#include "include.h"
#include "common.h"

#include "wlan_ui_pub.h"
#include "rw_pub.h"
#include "vif_mgmt.h"

#include "sa_station.h"
#include "param_config.h"
#include "common/ieee802_11_defs.h"
#include "driver_beken.h"
#include "mac_ie.h"
#include "sa_ap.h"
#include "main_none.h"
#include "sm.h"
#include "mac.h"
#include "scan_task.h"
#include "hal_machw.h"

monitor_data_cb_t g_monitor_cb = 0;
#ifdef CONFIG_AOS_MESH
monitor_data_cb_t g_mesh_monitor_cb = 0;
uint8_t g_mesh_bssid[6];
#endif

static monitor_data_cb_t g_mgnt_cb = 0;

extern int connect_flag;
extern struct assoc_ap_info assoc_ap;

static void rwnx_remove_added_interface(void)
{
    struct vif_info_tag *p_vif_entry = vif_mgmt_first_used();

    while (p_vif_entry != NULL)
    {
        mt_msg_dispatch(MM_REMOVE_IF_REQ, &p_vif_entry->index);
        p_vif_entry = vif_mgmt_next(p_vif_entry);
    }
}

void bk_wlan_connection_loss(void)
{
    struct vif_info_tag *p_vif_entry = vif_mgmt_first_used();

    while (p_vif_entry != NULL)
    {
        os_printf("bk_wlan_connection_loss\r\n");
        sta_ip_down();

        mt_msg_dispatch(MM_CONNECTION_LOSS_IND, &p_vif_entry->index);
        p_vif_entry = vif_mgmt_next(p_vif_entry);
    }
}

static int ip_aton(const char *cp, UINT32 *addr)
{
    char c;
    UINT8 base;
    UINT32 val;
    UINT32 parts[4];
    UINT32 *pp = parts;

    c = *cp;
    for (;;)
    {
        if (!isdigit(c))
            return (0);
        val = 0;
        base = 10;
        if (c == '0')
        {
            c = *++cp;
            if (c == 'x' || c == 'X')
            {
                base = 16;
                c = *++cp;
            }
            else
                base = 8;
        }
        for (;;)
        {
            if (isdigit(c))
            {
                val = (val * base) + (int)(c - '0');
                c = *++cp;
            }
            else if (base == 16 && isxdigit(c))
            {
                val = (val << 4) | (int)(c + 10 - (islower(c) ? 'a' : 'A'));
                c = *++cp;
            }
            else
                break;
        }
        if (c == '.')
        {
            if (pp >= parts + 3)
            {
                return (0);
            }
            *pp++ = val;
            c = *++cp;
        }
        else
            break;
    }
    if (c != '\0' && !isspace(c))
    {
        return (0);
    }
    switch (pp - parts + 1)
    {

    case 0:
        return (0);

    case 1:
        break;

    case 2:
        if (val > 0xffffffUL)
        {
            return (0);
        }
        val |= parts[0] << 24;
        break;

    case 3:
        if (val > 0xffff)
        {
            return (0);
        }
        val |= (parts[0] << 24) | (parts[1] << 16);
        break;

    case 4:
        if (val > 0xff)
        {
            return (0);
        }
        val |= (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8);
        break;
    default:
        break;
    }

    *addr = val;
    return (1);
}

static char *ip_ntoa(UINT32 addr, char *buf, int buflen)
{
    UINT32 s_addr;
    char inv[3];
    char *rp;
    UINT8 *ap;
    UINT8 rem;
    UINT8 n;
    UINT8 i;
    int len = 0;

    s_addr = addr;

    rp = buf;
    ap = (UINT8 *)&s_addr;
    for(n = 0; n < 4; n++)
    {
        i = 0;
        do
        {
            rem = *ap % (UINT8)10;
            *ap /= (UINT8)10;
            inv[i++] = '0' + rem;
        }
        while(*ap);
        while(i--)
        {
            if (len++ >= buflen)
            {
                return NULL;
            }
            *rp++ = inv[i];
        }
        if (len++ >= buflen)
        {
            return NULL;
        }
        *rp++ = '.';
        ap++;
    }
    *--rp = 0;
    return buf;
}

uint32_t bk_wlan_is_ap(void)
{
    ASSERT(g_wlan_general_param);
    return (CONFIG_ROLE_AP == g_wlan_general_param->role);
}

uint32_t bk_wlan_is_sta(void)
{
    ASSERT(g_wlan_general_param);
    return (CONFIG_ROLE_STA == g_wlan_general_param->role);
}

uint32_t bk_sta_cipher_is_open(void)
{
    ASSERT(g_sta_param_ptr);
    return (SECURITY_TYPE_NONE == g_sta_param_ptr->security);
}

uint32_t bk_sta_cipher_is_wep(void)
{
    ASSERT(g_sta_param_ptr);
    return (SECURITY_TYPE_WEP == g_sta_param_ptr->security);
}

 void bk_wlan_ap_init(hal_wifi_init_type_t *inNetworkInitPara)
{
    os_printf("Soft_AP_start\r\n");

    if(!g_ap_param_ptr)
    {
        g_ap_param_ptr = (ap_param_t *)os_zalloc(sizeof(ap_param_t));
        ASSERT(g_ap_param_ptr);
    }

    if(MAC_ADDR_NULL((u8 *)&g_ap_param_ptr->bssid))
    {
        wifi_get_mac_address((u8 *)&g_ap_param_ptr->bssid);
    }

    g_ap_param_ptr->chann = CFG_CHANNEL_AP;

    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_zalloc(sizeof(general_param_t));
        ASSERT(g_wlan_general_param);
    }
    g_wlan_general_param->role = CONFIG_ROLE_AP;

    if(inNetworkInitPara)
    {
        g_ap_param_ptr->ssid.length = os_strlen(inNetworkInitPara->wifi_ssid);
		if (g_ap_param_ptr->ssid.length > 32)
			g_ap_param_ptr->ssid.length = 32;
		
        os_memcpy(g_ap_param_ptr->ssid.array, inNetworkInitPara->wifi_ssid, g_ap_param_ptr->ssid.length);
        g_ap_param_ptr->key_len = os_strlen(inNetworkInitPara->wifi_key);
		if (g_ap_param_ptr->key_len >= 64)
			g_ap_param_ptr->key_len = 0;
		
        if(g_ap_param_ptr->key_len < 8)
        {
            g_ap_param_ptr->cipher_suite = CONFIG_CIPHER_OPEN;
        }
        else
        {
            g_ap_param_ptr->cipher_suite = CONFIG_CIPHER_CCMP;
            os_memcpy(g_ap_param_ptr->key, inNetworkInitPara->wifi_key, g_ap_param_ptr->key_len);
        }

        if(inNetworkInitPara->dhcp_mode == DHCP_SERVER)
        {
            g_wlan_general_param->dhcp_enable = 1;
        }
        else
        {
            g_wlan_general_param->dhcp_enable = 0;
        }
        ip_aton(inNetworkInitPara->local_ip_addr, &(g_wlan_general_param->ip_addr));
        ip_aton(inNetworkInitPara->net_mask, &(g_wlan_general_param->ip_mask));
        ip_aton(inNetworkInitPara->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
    }

    sa_ap_init();
}
 
void bk_wlan_sta_init(hal_wifi_init_type_t *inNetworkInitPara)
{
    if(!g_sta_param_ptr)
    {
        g_sta_param_ptr = (sta_param_t *)os_zalloc(sizeof(sta_param_t));
        ASSERT(g_sta_param_ptr);
    }

    wifi_get_mac_address((u8 *)&g_sta_param_ptr->own_mac);
    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_zalloc(sizeof(general_param_t));
        ASSERT(g_wlan_general_param);
    }
    g_wlan_general_param->role = CONFIG_ROLE_STA;

    if(inNetworkInitPara)
    {
        g_sta_param_ptr->ssid.length = os_strlen(inNetworkInitPara->wifi_ssid);
		if (g_sta_param_ptr->ssid.length > 32)
			g_sta_param_ptr->ssid.length = 32;
		
        os_memcpy(g_sta_param_ptr->ssid.array,
                  inNetworkInitPara->wifi_ssid,
                  g_sta_param_ptr->ssid.length);


        g_sta_param_ptr->key_len = os_strlen(inNetworkInitPara->wifi_key);
		if (g_sta_param_ptr->key_len > 64)
			g_sta_param_ptr->key_len = 64;
		
		os_memcpy(g_sta_param_ptr->key, inNetworkInitPara->wifi_key, g_sta_param_ptr->key_len);

        if(inNetworkInitPara->dhcp_mode == DHCP_CLIENT)
        {
            g_wlan_general_param->dhcp_enable = 1;
        }
        else
        {
            g_wlan_general_param->dhcp_enable = 0;
            ip_aton(inNetworkInitPara->local_ip_addr, &(g_wlan_general_param->ip_addr));
            ip_aton(inNetworkInitPara->net_mask, &(g_wlan_general_param->ip_mask));
            ip_aton(inNetworkInitPara->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
        }
    }

    sa_station_init();
}

OSStatus bk_wlan_start(hal_wifi_init_type_t *inNetworkInitPara)
{
    if(inNetworkInitPara->wifi_mode == SOFT_AP)
    {
#ifdef CONFIG_SOFTAP    
    	hostapd_thread_stop();
		supplicant_main_exit();
        bk_wlan_ap_init(inNetworkInitPara);

        os_printf("lwip_intf_initial\r\n");
        ip_address_set(SOFT_AP, 0, inNetworkInitPara->local_ip_addr,
                       inNetworkInitPara->net_mask,
                       inNetworkInitPara->gateway_ip_addr,
                       inNetworkInitPara->dns_server_ip_addr);
        

        os_printf("wpa_main_entry\r\n");
        wpa_main_entry(2, 0);

        sm_build_broadcast_deauthenticate();
		uap_ip_start();
#else
        return kUnsupportedErr;
#endif
    }
    else if(inNetworkInitPara->wifi_mode == STATION)
    {
#ifdef CONFIG_SOFTAP     
    	hostapd_thread_stop();
#endif
        supplicant_main_exit();
        sta_ip_down();
        ip_address_set(STATION, inNetworkInitPara->dhcp_mode, inNetworkInitPara->local_ip_addr,
                       inNetworkInitPara->net_mask, inNetworkInitPara->gateway_ip_addr,
                       inNetworkInitPara->dns_server_ip_addr);
        bk_wlan_sta_init(inNetworkInitPara);
        supplicant_main_entry(inNetworkInitPara->wifi_ssid);
    }

    return 0;
}

extern int sa_ap_inited();
extern int sa_sta_inited();

static int wlan_monitor_enabled = 0;

static void scan_cb(void *ctxt, void *user)
{
	struct scanu_rst_upload *scan_rst;
	hal_wifi_scan_result_t apList;
	int i, j;

	apList.ap_list = NULL;
	scan_rst = sr_get_scan_results();
	if (scan_rst == NULL) {
		apList.ap_num = 0;
	} else {
		apList.ap_num = scan_rst->scanu_num;
	}
	if (apList.ap_num > 0) {
		apList.ap_list = os_malloc(sizeof(*apList.ap_list)*apList.ap_num);
		for(i=0; i<scan_rst->scanu_num; i++) {
			os_memcpy(apList.ap_list[i].ssid, scan_rst->res[i]->ssid, 32);
			apList.ap_list[i].ap_power = scan_rst->res[i]->level;
		}
	}
	if (apList.ap_list == NULL) {
		apList.ap_num = 0;
	}
	ApListCallback(&apList);
	if (apList.ap_list != NULL) {
        os_free(apList.ap_list);
		apList.ap_list = NULL;
	}
	sr_release_scan_results(scan_rst);
}

static void scan_adv_cb(void *ctxt, void *user)
{
	struct scanu_rst_upload *scan_rst;
	hal_wifi_scan_result_adv_t apList;
	int i, j;

	apList.ap_list = NULL;
	scan_rst = sr_get_scan_results();
	if (scan_rst == NULL) {
		apList.ap_num = 0;
	} else {
		apList.ap_num = scan_rst->scanu_num;
	}
	if (apList.ap_num > 0) {
		apList.ap_list = os_malloc(sizeof(*apList.ap_list)*apList.ap_num);
		for(i=0; i<scan_rst->scanu_num; i++) {
			os_memcpy(apList.ap_list[i].ssid, scan_rst->res[i]->ssid, 32);
			apList.ap_list[i].ap_power = scan_rst->res[i]->level;
			os_memcpy(apList.ap_list[i].bssid, scan_rst->res[i]->bssid, 6);
			apList.ap_list[i].channel = scan_rst->res[i]->channel;
			apList.ap_list[i].security = scan_rst->res[i]->security;
		}
	}
	if (apList.ap_list == NULL) {
		apList.ap_num = 0;
	}
	ApListAdvCallback(&apList);
	if (apList.ap_list != NULL) {
        os_free(apList.ap_list);
		apList.ap_list = NULL;
	}
	sr_release_scan_results(scan_rst);
}


void bk_wlan_start_scan(void)
{
    SCAN_PARAM_T scan_param = {0};

	mhdr_scanu_reg_cb(scan_cb, 0);
	if ((sa_ap_inited() == 0) && (sa_sta_inited() == 0))
    	bk_wlan_sta_init(0);

    os_memset(&scan_param.bssid, 0xff, ETH_ALEN);
    mt_msg_dispatch(SCANU_START_REQ, &scan_param);
}

void bk_wlan_start_adv_scan(void)
{
    SCAN_PARAM_T scan_param = {0};

	mhdr_scanu_reg_cb(scan_adv_cb, 0);
	if ((sa_ap_inited() == 0) && (sa_sta_inited() == 0))
    	bk_wlan_sta_init(0);

    os_memset(&scan_param.bssid, 0xff, ETH_ALEN);
    mt_msg_dispatch(SCANU_START_REQ, &scan_param);
}

void bk_wlan_sta_init_adv(hal_wifi_init_type_adv_t *inNetworkInitParaAdv)
{
	int valid_ap = 1;
	
    if(!g_sta_param_ptr)
    {
        g_sta_param_ptr = (sta_param_t *)os_malloc(sizeof(sta_param_t));
        ASSERT(g_sta_param_ptr);
    }

    if(MAC_ADDR_NULL((u8 *)&g_sta_param_ptr->own_mac))
    {
        wifi_get_mac_address((char *)&g_sta_param_ptr->own_mac);
    }

	if (MAC_ADDR_NULL(inNetworkInitParaAdv->ap_info.bssid)) {
		valid_ap = 0;
	}

	if (((inNetworkInitParaAdv->ap_info.channel <= 0)) || 
		 (inNetworkInitParaAdv->ap_info.channel > 13)) {
		valid_ap = 0;
	}
	
    g_sta_param_ptr->ssid.length = os_strlen(inNetworkInitParaAdv->ap_info.ssid);
    os_memcpy(g_sta_param_ptr->ssid.array, inNetworkInitParaAdv->ap_info.ssid, g_sta_param_ptr->ssid.length);

    g_sta_param_ptr->security = inNetworkInitParaAdv->ap_info.security;
    if ((SECURITY_TYPE_AUTO < inNetworkInitParaAdv->ap_info.security) ||
		(SECURITY_TYPE_NONE > inNetworkInitParaAdv->ap_info.security)) {
		bk_printf("security %d\r\n", inNetworkInitParaAdv->ap_info.security);
		valid_ap = 0;
    }

	if (valid_ap) {
    g_sta_param_ptr->fast_connect_set = 1;
    g_sta_param_ptr->fast_connect.chann = inNetworkInitParaAdv->ap_info.channel;
    os_memcpy(g_sta_param_ptr->fast_connect.bssid, inNetworkInitParaAdv->ap_info.bssid, ETH_ALEN);
	}
    g_sta_param_ptr->key_len = inNetworkInitParaAdv->key_len;
    os_memcpy((uint8_t *)g_sta_param_ptr->key, inNetworkInitParaAdv->key, inNetworkInitParaAdv->key_len);

    if(!g_wlan_general_param)
    {
        g_wlan_general_param = (general_param_t *)os_malloc(sizeof(general_param_t));
    }
    g_wlan_general_param->role = CONFIG_ROLE_STA;
    if(inNetworkInitParaAdv->dhcp_mode == DHCP_CLIENT)
    {
        g_wlan_general_param->dhcp_enable = 1;
    }
    else
    {
        g_wlan_general_param->dhcp_enable = 0;
        ip_aton(inNetworkInitParaAdv->local_ip_addr, &(g_wlan_general_param->ip_addr));
        ip_aton(inNetworkInitParaAdv->net_mask, &(g_wlan_general_param->ip_mask));
        ip_aton(inNetworkInitParaAdv->gateway_ip_addr, &(g_wlan_general_param->ip_gw));
    }
    sa_station_init();
}

OSStatus bk_wlan_start_adv(hal_wifi_init_type_adv_t *inNetworkInitParaAdv)
{
#ifdef CONFIG_SOFTAP 
    hostapd_thread_stop();
#endif
    supplicant_main_exit();
    sta_ip_down();
    ip_address_set(STATION, inNetworkInitParaAdv->dhcp_mode,
                   inNetworkInitParaAdv->local_ip_addr,
                   inNetworkInitParaAdv->net_mask,
                   inNetworkInitParaAdv->gateway_ip_addr,
                   inNetworkInitParaAdv->dns_server_ip_addr);

    bk_wlan_sta_init_adv(inNetworkInitParaAdv);
    supplicant_main_entry(inNetworkInitParaAdv->ap_info.ssid);

    return 0;
}

OSStatus bk_wlan_get_ip_status(hal_wifi_ip_stat_t *outNetpara, hal_wifi_type_t inInterface)
{
    uint8_t mac[6];
	
    if(g_wlan_general_param->dhcp_enable)
    {
        outNetpara->dhcp = DHCP_SERVER;
    }
    else
    {
        outNetpara->dhcp = DHCP_DISABLE;
    }
    ip_ntoa(g_wlan_general_param->ip_addr, outNetpara->ip, 16);
    ip_ntoa(g_wlan_general_param->ip_gw, outNetpara->gate, 16);
    ip_ntoa(g_wlan_general_param->ip_mask, outNetpara->mask, 16);

    wifi_get_mac_address(mac);
	sprintf(outNetpara->mac, "%02x%02x%02x%02x%02x%02x", mac[0],
			mac[1], mac[2], mac[3], mac[4], mac[5]);

    return 0;
}

OSStatus bk_wlan_get_link_status(hal_wifi_link_stat_t *outStatus)
{
    if(g_wlan_general_param->role == CONFIG_ROLE_AP)
    {
        return -1;
    }
    outStatus->is_connected = connect_flag;
    os_memcpy(outStatus->bssid, assoc_ap.bssid, 6);
    os_memcpy(outStatus->ssid, assoc_ap.ssid, assoc_ap.ssid_len);
    outStatus->channel = assoc_ap.chann;
    outStatus->wifi_strength = assoc_ap.level;

    return 0;
}

/** @brief  Add the packet type which monitor should receive
 *
 *  @detail This function can be called many times to receive different wifi packets.
 */
int bk_wlan_monitor_rx_type(int type)
{
    unsigned int filter = 0;
    switch(type)
    {
    case WLAN_RX_BEACON:
        nxmac_accept_beacon_setf(1);
        break;
    case WLAN_RX_PROBE_REQ:
        nxmac_accept_probe_req_setf(1);
        break;
    case WLAN_RX_PROBE_RES:
        nxmac_accept_probe_resp_setf(1);
        break;
    case WLAN_RX_ACTION:
        break;
    case WLAN_RX_MANAGEMENT:
        nxmac_accept_other_mgmt_frames_setf(1);
        break;
    case WLAN_RX_DATA:
        nxmac_accept_other_data_frames_setf(1);
        nxmac_accept_qo_s_null_setf(1);
        nxmac_accept_qcfwo_data_setf(1);
        nxmac_accept_q_data_setf(1);
        nxmac_accept_cfwo_data_setf(1);
        nxmac_accept_data_setf(1);
        break;
    case WLAN_RX_MCAST_DATA:
        nxmac_accept_multicast_setf(1);
        nxmac_accept_broadcast_setf(1);
        break;
    case WLAN_RX_ALL:
        nxmac_rx_cntrl_set((uint32_t)0x7FFFFFFC);
        break;
    }

    mt_msg_dispatch(MM_SET_FILTER_REQ, &filter);
    return 0;
}

/** @brief  Start wifi monitor mode
 *
 *  @detail This function disconnect wifi station and softAP.
 *
 */
int bk_wlan_start_monitor(void)
{
    wlan_monitor_enabled = 1;
    lsig_init();
    bk_wlan_ap_init(0);
    rwnx_remove_added_interface();
    g_wlan_general_param->role = CONFIG_ROLE_NULL;
    return 0;
}

/** @brief  Stop wifi monitor mode
 *
 */
int bk_wlan_stop_monitor(void)
{
    if(wlan_monitor_enabled)
    {
        g_monitor_cb = 0;
        hal_machw_exit_monitor_mode();
        wlan_monitor_enabled = 0;
    }
    
    return 0;
}

int bk_wlan_monitor_enabled(void)
{
    if (wlan_monitor_enabled == 0)
        return 0;
    else
        return 1;
}
/** @brief  Set the monitor channel
 *
 *  @detail This function change the monitor channel (from 1~13).
 *       it can change the channel dynamically, don't need restart monitor mode.
 */
int bk_wlan_set_channel(int channel)
{
	rwnxl_reset_evt(0);
    mt_msg_dispatch(MM_SET_CHANNEL_REQ, &channel);
    return 0;
}

#define WLAN_OUI_MICROSOFT		(0x0050F2)
#define WLAN_OUI_WPS			(0x0050F2)
#define WLAN_OUI_TYPE_MICROSOFT_WPA	(1)
#define WLAN_OUI_TYPE_WPS		(4)
#define WLAN_EID_RSN 48
#define WLAN_EID_VENDOR_SPECIFIC    (221)

struct ieee80211_vendor_ie {
    u8 element_id;
    u8 len;
    u8 oui[3];
    u8 oui_type;
};

static u8 *wlan_find_ie(u8 eid, const u8 *ies, int len)
{
        while (len > 2 && ies[0] != eid) {
                len -= ies[1] + 2;
                ies += ies[1] + 2;
        }
        if (len < 2)
                return NULL;
        if (len < 2 + ies[1])
                return NULL;
        return ies;
}

static const u8 *wlan_find_vendor_ie(
    unsigned int oui, u8 oui_type, const u8 *ies, int len)
{
    struct ieee80211_vendor_ie *ie;
    const u8 *pos = ies, *end = ies + len;
    int ie_oui;

    while (pos < end) {
        pos = wlan_find_ie(WLAN_EID_VENDOR_SPECIFIC, pos,
                               end - pos);
        if (!pos) {
            return NULL;
        }

        ie = (struct ieee80211_vendor_ie *)pos;

        if (ie->len < sizeof(*ie)) {
            goto cont;
        }

        ie_oui = ie->oui[0] << 16 | ie->oui[1] << 8 | ie->oui[2];
        if (ie_oui == oui && ie->oui_type == oui_type) {
            return pos;
        }
cont:
        pos += 2 + ie->len;
    }
    return NULL;
}

static inline int get_cipher_info(uint8_t *frame, int frame_len,
		uint8_t *pairwise_cipher_type)
{
	uint8_t cap = frame[24+10]; // 24 is mac header; 8 timestamp, 2 beacon interval; 
	u8 is_privacy = !!(cap & 0x10); // bit 4 = privacy
	const u8 *ie = frame + 36; // 24 + 12
	u16 ielen = frame_len - 36;
	const u8 *tmp;
	int ret = 0;

	tmp = wlan_find_ie(WLAN_EID_RSN, ie, ielen);
	if (tmp && tmp[1]) {
		*pairwise_cipher_type = ENC_CCMP;// TODO: maybe it is a CCMP&TKIP, set to CCMP to try try
	} else {
		tmp = wlan_find_vendor_ie(WLAN_OUI_MICROSOFT, WLAN_OUI_TYPE_MICROSOFT_WPA, ie, ielen);
		if (tmp) {
			*pairwise_cipher_type = ENC_TKIP;
		} else {
			if (is_privacy) {
				*pairwise_cipher_type = ENC_WEP;
			} else {
				*pairwise_cipher_type = ENC_OPEN;
			}
		}
	}

	return ret;
}

static void bk_monitor_callback(uint8_t *data, int len, hal_wifi_link_info_t *info)
{
    uint8_t enc_type;

    /* check the RTS packet */
    if ((data[0] == 0xB4) && (len == 16)) { // RTS
        rts_update(data, info->rssi, mico_rtos_get_time());
        return;
    }
    /* check beacon/probe reponse packet */
    if ((data[0] == 0x80) || (data[0] == 0x50)) {
        
        get_cipher_info(data, len, &enc_type);
        beacon_update(&data[16], enc_type);
    }

    
    if (g_monitor_cb)
        g_monitor_cb(data, len, info);
}

void upload_monitor_data(uint8_t *data, int len, hal_wifi_link_info_t *info)
{
    if (g_monitor_cb)
        g_monitor_cb(data, len, info);
}

/** @brief  Register the monitor callback function
 *        Once received a 802.11 packet call the registered function to return the packet.
 */
void bk_wlan_register_monitor_cb(monitor_data_cb_t fn)
{
    g_monitor_cb = fn;
}

monitor_data_cb_t bk_wlan_get_monitor_cb(void)
{
    if (g_monitor_cb)
        return bk_monitor_callback;
    else {
        return NULL;
    }
}

#include "mm.h"

static  uint32_t umac_rx_filter = 0;
/** @brief  Register the monitor callback function
 *        Once received a 802.11 packet call the registered function to return the packet.
 */
void bk_wlan_register_mgnt_monitor_cb(monitor_data_cb_t fn)
{
    g_mgnt_cb = fn;

	if (fn != NULL) {
		umac_rx_filter = mm_rx_filter_umac_get();
		mm_rx_filter_umac_set(umac_rx_filter | NXMAC_ACCEPT_PROBE_REQ_BIT);
	} else {
		if (umac_rx_filter != 0) {
			mm_rx_filter_umac_set(umac_rx_filter);
		}
	}
}

monitor_data_cb_t bk_wlan_get_mgnt_monitor_cb(void)
{
    return g_mgnt_cb;
}

#ifndef CONFIG_AOS_MESH
int wlan_is_mesh_monitor_mode(void)
{
    return FALSE;
}

bool rxu_mesh_monitor(struct rx_swdesc *swdesc)
{
    return false;
}

monitor_data_cb_t wlan_get_mesh_monitor_cb(void)
{
    return NULL;
}
#else
void wlan_register_mesh_monitor_cb(monitor_data_cb_t fn)
{
    g_mesh_monitor_cb = fn;
}

monitor_data_cb_t wlan_get_mesh_monitor_cb(void)
{
    return g_mesh_monitor_cb;
}

int wlan_is_mesh_monitor_mode(void)
{
    if (g_mesh_monitor_cb) {
        return TRUE;
    }
    return FALSE;
}

int wlan_set_mesh_bssid(uint8_t *bssid)
{
    if (bssid == NULL) {
        return -1;
    }
    memcpy(g_mesh_bssid, bssid, 6);
    return 0;
}

uint8_t *wlan_get_mesh_bssid(void)
{
    return g_mesh_bssid;
}

bool rxu_mesh_monitor(struct rx_swdesc *swdesc)
{
    struct rx_dmadesc *dma_hdrdesc = swdesc->dma_hdrdesc;
    struct rx_hd *rhd = &dma_hdrdesc->hd;
    struct rx_payloaddesc *payl_d = HW2CPU(rhd->first_pbd_ptr);
    struct rx_cntrl_rx_status *rx_status = &rxu_cntrl_env.rx_status;
    uint32_t *frame = payl_d->buffer;
    struct mac_hdr *hdr = (struct mac_hdr *)frame;
    uint8_t *local_bssid;
    uint8_t *bssid;

    if (wlan_is_mesh_monitor_mode() == FALSE) {
        return false;
    }

    if(MAC_FCTRL_DATA_T == (hdr->fctl & MAC_FCTRL_TYPE_MASK)) {
        local_bssid = wlan_get_mesh_bssid();
        bssid = (uint8_t *)hdr->addr3.array;
        if (memcmp(local_bssid, bssid, 6) == 0) {
            return true;
        }
    } else if (MAC_FCTRL_ACK == (hdr->fctl & MAC_FCTRL_TYPESUBTYPE_MASK)) {
        uint16_t local_addr[3];
        uint16_t *addr;
        uint32_t addr_low;
        uint16_t addr_high;

        addr = (uint16_t *)hdr->addr1.array;
        addr_low = nxmac_mac_addr_low_get();
        local_addr[0] = addr_low;
        local_addr[1] = (addr_low & 0xffff0000) >> 16;
        local_addr[2] = nxmac_mac_addr_high_getf();

        if (memcmp(local_addr, addr, 6) == 0) {
            return true;
        }
    }

    return false;
}

#endif

int bk_wlan_send_80211_raw_frame(uint8_t *buffer, int len)
{
    uint8_t *pkt;
    int ret;

    pkt = aos_malloc(len);
    if (pkt == NULL) {
        return -1;
    }

    memcpy(pkt, buffer, len);
    ret = bmsg_tx_raw_sender(pkt, len);
    return ret;
}

int bk_wlan_is_monitor_mode(void)
{
    return (0 == g_monitor_cb) ? FALSE : TRUE;
}

int bk_wlan_power_off(void)
{
	return 0;
}

int bk_wlan_power_on(void)
{
	return 0;
}

int bk_wlan_suspend(void)
{
	supplicant_main_exit();
#ifdef CONFIG_SOFTAP 
    hostapd_thread_stop();
#endif
	return 0;
}

int bk_wlan_suspend_station(void)
{
    supplicant_main_exit();

	return 0;
}

int bk_wlan_suspend_softap(void)
{
#ifdef CONFIG_SOFTAP 
    hostapd_thread_stop();
#endif
	return 0;
}

// eof

