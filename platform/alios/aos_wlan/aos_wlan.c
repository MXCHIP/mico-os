/**
 ******************************************************************************
 * @file    aos_wlan.c
 * @author  William Xu
 * @version V1.0.0
 * @date    30-Apr-2018
 * @brief   This file provide wlan driver functions.
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2018 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#ifdef ALIOS_DEV_WLAN

#include "mico_common.h"
#include "mico_wlan.h"

#include <hal/wifi.h>

/******************************************************
*                    Constants
******************************************************/

/******************************************************
*                   Enumerations
******************************************************/

/******************************************************
*                 Type Definitions
******************************************************/

/******************************************************
*                    Structures
******************************************************/

/******************************************************
*               Variables Definitions
******************************************************/

hal_wifi_module_t *aos_wlan = NULL;

/******************************************************
*               Function Declarations
******************************************************/

/******************************************************
*               Function Definitions
******************************************************/

MICO_WEAK void mxchipInit(void)
{
    aos_wlan = hal_wifi_get_default_module();
}

OSStatus micoWlanStartAdv(network_InitTypeDef_adv_st* inNetworkInitParaAdv)
{
    return hal_wifi_start_adv(aos_wlan, (hal_wifi_init_type_adv_t *)inNetworkInitParaAdv);
}

OSStatus micoWlanStart(network_InitTypeDef_st* inNetworkInitParaAdv)
{
    hal_wifi_init_type_t conf;

    memset(&conf, 0x0, sizeof(hal_wifi_init_type_t));

    conf.dhcp_mode = inNetworkInitParaAdv->dhcpMode;
    conf.wifi_mode = inNetworkInitParaAdv->wifi_mode;
    memcpy(conf.wifi_ssid, inNetworkInitParaAdv->wifi_ssid, 32);
    memcpy(conf.wifi_key, inNetworkInitParaAdv->wifi_key, 64);
    memcpy(conf.wifi_ssid, inNetworkInitParaAdv->wifi_ssid, 32);
    memcpy(conf.local_ip_addr, inNetworkInitParaAdv->local_ip_addr, 16);
    memcpy(conf.net_mask, inNetworkInitParaAdv->net_mask, 16);
    memcpy(conf.gateway_ip_addr, inNetworkInitParaAdv->gateway_ip_addr, 16);
    memcpy(conf.dns_server_ip_addr, inNetworkInitParaAdv->dnsServer_ip_addr, 16);
    conf.wifi_retry_interval = inNetworkInitParaAdv->wifi_retry_interval;

    return hal_wifi_start(aos_wlan, &conf);
}

void wlan_get_mac_address( uint8_t *mac )
{
    hal_wifi_get_mac_addr(aos_wlan, mac);
}

OSStatus sta_disconnect(void)
{
    return hal_wifi_suspend_station(aos_wlan);
}

void mxchipStartScan(void)
{
    return hal_wifi_start_scan(aos_wlan);
}

OSStatus wifi_power_up(void)
{
    return hal_wifi_power_on(aos_wlan);
}

OSStatus wifi_power_down(void)
{
    return hal_wifi_power_off(aos_wlan);
}

OSStatus getNetPara(IPStatusTypedef *outNetpara, netif_t inInterface)
{
    return hal_wifi_get_ip_stat(aos_wlan, (hal_wifi_ip_stat_t *)outNetpara, (hal_wifi_type_t)inInterface);
}

OSStatus micoWlanSuspend(void)
{
    return hal_wifi_suspend_station(aos_wlan);
}

void ps_enable(void)
{

}

void ps_disable(void)
{

}

void mico_wlan_monitor_no_easylink(void)
{
    hal_wifi_start_wifi_monitor(aos_wlan);
}

int mico_wlan_stop_monitor(void)
{
    hal_wifi_stop_wifi_monitor(aos_wlan);
    return kNoErr;
}


int mico_wlan_monitor_set_channel( uint8_t channel )
{
    return hal_wifi_set_channel(aos_wlan, channel);
}

int mico_wlan_start_monitor(void)
{
    hal_wifi_start_wifi_monitor(aos_wlan);
    return kNoErr;
}

void mico_wlan_register_monitor_cb(monitor_cb_t fn)
{
    hal_wifi_register_monitor_cb(aos_wlan, (monitor_data_cb_t)fn);
}

int mxchip_active_scan(char*ssid, int is_adv)
{
    return 0;
}

int micoWlanGetLinkStatus(LinkStatusTypeDef *outStatus)
{
    return hal_wifi_get_link_stat(aos_wlan, (hal_wifi_link_stat_t *)outStatus);
}

#endif


