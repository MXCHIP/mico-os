/**
 ******************************************************************************
 * @file    mico_network.h
 * @author  William Xu
 * @version V1.0.0
 * @date    16-Sep-2014
 * @brief   This file provides all the headers of network interface management.
 ******************************************************************************
 *
 *  The MIT License
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 */

#ifndef __MICO_NETWORK_H__
#define __MICO_NETWORK_H__


#include "mico_opt.h"
#include "mico_common.h"


#ifdef __cplusplus
extern "C" {
#endif

/** Maximum size of MAC address representation
 */
#define NSAPI_MAC_SIZE 18

/**
 *  @brief  wlan network interface enumeration definition.
 */
typedef enum {
    Soft_AP,  /**< Act as an access point, and other station can connect, 4 stations Max*/
    Station,   /**< Act as a station which can connect to an access point*/
    INTERFACE_UAP = Soft_AP,
    INTERFACE_STA = Station,
    INTERFACE_ETH,
    INTERFACE_MAX,
    INTERFACE_NONE = 0xFF,
} wlanInterfaceTypedef;

typedef uint8_t wlan_if_t;
typedef uint8_t netif_t;

/**
 *  @brief  network interface status.
 */
enum {
    INTERFACE_STATUS_UP,
    INTERFACE_STATUS_DOWN,
};

typedef uint8_t wlan_if_status_t;
typedef uint8_t netif_status_t;

extern netif_status_t netif_status[INTERFACE_MAX];

#define IS_INTERFACE_UP(netif)   ((netif_status[netif] == INTERFACE_STATUS_UP)? 1:0)
#define IS_INTERFACE_DOWN(netif)   ((netif_status[netif] == INTERFACE_STATUS_DOWN)? 1:0)

/**
 *  @brief  Interface status change notification.
 */
enum {
    NOTIFY_STATION_UP = 1,
    NOTIFY_STATION_DOWN,

    NOTIFY_AP_UP,
    NOTIFY_AP_DOWN,

    NOTIFY_ETH_UP,
    NOTIFY_ETH_DOWN,
};

typedef uint8_t notify_wlan_t;
typedef uint8_t notify_netif_status_t;

/**@brief Set interface priorities used in interface auto switch @mico_network_switch_interface_auto
 *
 * @param[in]   priorities: Interface array, interface has lower array index has higher priority
 *              default priorities is INTERFACE_ETH > INTERFACE_STA > INTERFACE_UAP
 *
 * @return    None
 */
void mico_network_set_interface_priority( netif_t priorities[INTERFACE_MAX] );

/**
 * @brief Switch to a active interface according to interface priorities
 *
 * @return    None
 */
OSStatus mico_network_switch_interface_auto( void );

/**@brief Disable auto switch mode, and set the default interface manually
 *
 * @param[in]   interface : The dafault network interface
 *
 * @return    kNoErr on success
 */
OSStatus mico_network_switch_interface_manual( netif_t interface );


#ifdef __cplusplus
    }
#endif

#endif //__MICOWLAN_H__

/**
  * @}
  */

/**
  * @}
  */



