/**
 ****************************************************************************************
 *
 * @file arch_config.c
 *
 *
 * Copyright (C) Beken Corp 2011-2016
 *
 ****************************************************************************************
 */
#include "include.h"
#include "mem_pub.h"
#include "drv_model_pub.h"
#include "flash_pub.h"
#include "mac.h"
#include "param_config.h"
#include "uart_pub.h"
#include <hal/base.h>
#include <hal/wifi.h>


general_param_t *g_wlan_general_param = NULL;
ap_param_t *g_ap_param_ptr = NULL;
sta_param_t *g_sta_param_ptr = NULL;
static void load_mac(void);

uint32_t cfg_param_init(void)
{
	if(NULL == g_wlan_general_param)
	{
		g_wlan_general_param = (general_param_t *)os_zalloc(sizeof(general_param_t));
		ASSERT(g_wlan_general_param);
	}
	if(NULL == g_ap_param_ptr)
	{
		g_ap_param_ptr = (ap_param_t *)os_zalloc(sizeof(ap_param_t));
		ASSERT(g_ap_param_ptr);
	}
	if(NULL == g_sta_param_ptr)
	{
		g_sta_param_ptr = (sta_param_t *)os_zalloc(sizeof(sta_param_t));
		ASSERT(g_sta_param_ptr);
	}
}

#if 1
#define SYSTEM_DATA_ADDR 0xF000
#define DEFAULT_MAC_ADDR "\xC8\x93\x48\x22\x22\x01"
static uint8_t system_mac[6] = DEFAULT_MAC_ADDR;

static void load_mac(void)
{
	UINT32 status;
	DD_HANDLE flash_handle;
	
    flash_handle = ddev_open(FLASH_DEV_NAME, &status, 0);
    ddev_read(flash_handle, (char *)system_mac, sizeof(system_mac), SYSTEM_DATA_ADDR);
	
	if (system_mac[0] == 0xFF) 
	{
		os_memcpy(system_mac, DEFAULT_MAC_ADDR, 6);
	}
}

/* yhb added, save MAC to address 0xFE000 */
void wifi_get_mac_address(char *mac)
{
	static int mac_inited = 0;
	
	if (mac_inited == 0) 
	{
		load_mac();
		mac_inited = 1;
	}
	memcpy(mac, system_mac, 6);
}

int wifi_set_mac_address(char *mac)
{
    DD_HANDLE flash_handle;
	UINT32 status;

	memcpy(system_mac, mac, 6);
    flash_handle = ddev_open(FLASH_DEV_NAME, &status, 0);
    ddev_write(flash_handle, (char *)system_mac, sizeof(system_mac), SYSTEM_DATA_ADDR);
}
#endif

int wpa_get_ap_security(hal_wifi_ap_info_adv_t *ap, uint8_t **key, int *key_len)
{
	ap->security = g_sta_param_ptr->security;
	wpa_driver_get_ssid(NULL, ap->ssid);
	wpa_driver_get_bssid(NULL, ap->bssid);
	ap->channel = wpa_driver_get_bssid_channel(NULL);

	g_sta_param_ptr->psk_set = 1;
	if (g_sta_param_ptr->psk_set == 1) {
		*key = g_sta_param_ptr->psk;
		*key_len = 64;
	} else {
	*key = g_sta_param_ptr->key;
	*key_len = g_sta_param_ptr->key_len;
	}
	return 0;
}

