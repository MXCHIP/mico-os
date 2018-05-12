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


#endif


