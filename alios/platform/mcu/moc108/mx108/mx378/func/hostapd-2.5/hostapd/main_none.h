#ifndef _MAIN_NONE_H_
#define _MAIN_NONE_H_

#define CFG_CONFIG_FNAME          "beken_cfg_fname"
#define CFG_BSS_CONFIG            "wangzhilei_config:bss_fname"
#define CFG_AP_IFACE_CONFIG       "config= wlan0"
#define WEP40_KEY_LENGTH           10

struct hapd_global {
	void **drv_priv;
	size_t drv_count;
};

extern char *bss_iface;

static void hostapd_thread_start(void);

#if CFG_WIFI_AP_MODE
extern int wpa_main_entry(int argc, char *argv[]);
#endif

#if CFG_WIFI_STATION_MODE
extern int supplicant_main_entry(char *oob_ssid);
void wpa_supplicant_poll(void *param);
#endif

#endif // _MAIN_NONE_H_
// eof

