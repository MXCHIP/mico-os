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

//TODO type should convert not equal
OSStatus micoWlanStart(network_InitTypeDef_st* inNetworkInitParaAdv)
{
    return hal_wifi_start_adv(aos_wlan, (hal_wifi_init_type_adv_t *)inNetworkInitParaAdv);
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


