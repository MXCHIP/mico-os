#ifndef _RW_MSG_TX_H_
#define _RW_MSG_TX_H_

#include "include.h"
#include "scan.h"
#include "mac.h"

#if CFG_WIFI_AP_MODE
#define BEACON_INTERVAL                       (100)
#endif

struct scan_cancel_req
{
};

typedef struct cfg80211_connect_params
{
    uint32_t flags;
    uint32_t vif_idx;
    uint8_t auth_type;
    struct mac_addr bssid;
    struct mac_ssid ssid;
    struct scan_chan_tag chan;
    uint16_t ie_len;
    uint32_t ie_buf[64];
} CONNECT_PARAM_T;

typedef struct cfg80211_scan_params
{
    uint8_t num_ssids;
    struct mac_ssid ssids[SCAN_SSID_MAX];
    struct mac_addr bssid;
} SCAN_PARAM_T;

typedef struct cfg80211_key_params
{
    uint8_t cipher_suite;
    uint8_t sta_idx;
    uint8_t inst_nbr;
    uint8_t key_idx;
    uint8_t hw_key_idx;
    struct mac_sec_key key;
} KEY_PARAM_T;

typedef struct cfg80211_fast_scan_params
{
    struct mac_ssid ssid;
    struct mac_addr bssid;
	
    uint16_t probe_delay;
    uint16_t min_ch_time;
    uint16_t max_ch_time;
    uint16_t ch_num;
}FAST_SCAN_PARAM_T;

typedef struct cfg80211_disconnect_params
{
    uint16_t reason_code;
    uint8_t vif_idx;
}DISCONNECT_PARAM_T;

extern void mt_reset(void);
extern void mt_me_config(void);
extern void mt_channel_config(void);
extern void mt_start(void);
extern void mt_add_if(UINT32 type);
#if CFG_WIFI_AP_MODE
extern void mt_apm_start(void);
#endif
extern void mt_key_add(KEY_PARAM_T *key_param);
extern void mt_key_del(KEY_PARAM_T *key_param);
extern void mt_set_ctrl_port(void);
extern void mt_sm_connect(CONNECT_PARAM_T *sme);
extern void mt_scan_start(SCAN_PARAM_T *scan_param);
#endif // _RW_MSG_TX_H_
// eof

