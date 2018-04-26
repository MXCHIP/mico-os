#include "include.h"
#include "rwnx_config.h"
#include "app.h"

#if (NX_POWERSAVE)
#include "ps.h"
#endif //(NX_POWERSAVE)

#include "sa_ap.h"
#include "app_lwip_udp.h"
#include "sa_station.h"
#include "main_none.h"
#include "sm.h"

#if CFG_MODE_SWITCH
#include "param_config.h"
#endif

#include "mico_rtos.h"

#include "wlan_ui_pub.h"
#include "uart_pub.h"
#include "mem_pub.h"
#include "str_pub.h"
#include "drv_model_pub.h"
#include "flash_pub.h"
#include "ieee802_11_demo.h"

#if 1//CFG_TEST_SOFTAP
void test_softap_app_init(char *ap_ssid, char *ap_key)
{
    OSStatus err = kNoErr;
    hal_wifi_init_type_t wNetConfig;

    os_memset(&wNetConfig, 0x0, sizeof(hal_wifi_init_type_t));

    os_strcpy((char *)wNetConfig.wifi_ssid, ap_ssid);
    os_strcpy((char *)wNetConfig.wifi_key, ap_key);

    wNetConfig.wifi_mode = SOFT_AP;
    wNetConfig.dhcp_mode = DHCP_SERVER;
    wNetConfig.wifi_retry_interval = 100;
    os_strcpy((char *)wNetConfig.local_ip_addr, "192.168.0.1");
    os_strcpy((char *)wNetConfig.net_mask, "255.255.255.0");
    os_strcpy((char *)wNetConfig.dns_server_ip_addr, "192.168.0.1");

    os_printf("ssid:%s  key:%s\r\n", wNetConfig.wifi_ssid, wNetConfig.wifi_key);
    bk_wlan_start(&wNetConfig);
}
#endif

#if 2// CFG_TEST_SCAN
uint32_t scan_over_flag = 0;
void scan_cb(void *ctxt, void *user)
{
	scan_over_flag = 1;
}

void test_scan_app_init(void)
{
    /* Register user function when wlan scan is completed */
	while(1)
	{
	    os_printf("start scan mode, please wait...\r\n");
		mhdr_scanu_reg_cb(scan_cb, 0, 0);
	    micoWlanStartScan();
		while(0 == scan_over_flag)
		{
			mico_rtos_delay_milliseconds(200);
		}
		scan_over_flag = 0;
	}
}
#endif

#if 3// CFG_TEST_STATION
#include "rw_pub.h"

extern struct scanu_rst_upload *scan_rstup_ptr;
void test_sta_app_init(char *oob_ssid,char *connect_key)
{
#if 0
	hal_wifi_init_type_adv_t	wNetConfigAdv;

	os_memset( &wNetConfigAdv, 0x0, sizeof(wNetConfigAdv) );
	
	os_strcpy((char*)wNetConfigAdv.ap_info.ssid, oob_ssid);   /* wlan ssid string */
	hwaddr_aton("48:ee:0c:48:93:12", wNetConfigAdv.ap_info.bssid);
	wNetConfigAdv.ap_info.security = SECURITY_TYPE_WPA2_MIXED;		  /* wlan security mode */
	wNetConfigAdv.ap_info.channel = 6;							  /* Select channel automatically */
	
	os_strcpy((char*)wNetConfigAdv.key, connect_key);				  /* wlan key string or hex data in WEP mode */
	wNetConfigAdv.key_len = strlen(connect_key);			  /* wlan key length */
	wNetConfigAdv.dhcp_mode = DHCP_CLIENT;						  /* Fetch Ip address from DHCP server */
	wNetConfigAdv.wifi_retry_interval = 100;					  /* Retry interval after a failure connection */

	/* Connect Now! */
	micoWlanStartAdv(&wNetConfigAdv);
#else
	hal_wifi_init_type_t wNetConfig;

	os_memset(&wNetConfig, 0x0, sizeof(hal_wifi_init_type_t));

	os_strcpy((char *)wNetConfig.wifi_ssid, oob_ssid);
	os_strcpy((char *)wNetConfig.wifi_key, connect_key);

	wNetConfig.wifi_mode = STATION;
	wNetConfig.dhcp_mode = DHCP_CLIENT;
	wNetConfig.wifi_retry_interval = 100;
	os_strcpy((char *)wNetConfig.local_ip_addr, "192.168.0.1");
	os_strcpy((char *)wNetConfig.net_mask, "255.255.255.0");
	os_strcpy((char *)wNetConfig.dns_server_ip_addr, "192.168.0.1");

	os_printf("ssid:%s key:%s\r\n", wNetConfig.wifi_ssid, wNetConfig.wifi_key);
	bk_wlan_start(&wNetConfig);
#endif
}

#endif

