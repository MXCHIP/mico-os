/**
 ******************************************************************************
 * @file    mico_lwip_ethif.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-Aug-2018
 * @brief   This file provide the lwip ethernet interface layer functions.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "mico_common.h"

#include "mico_eth.h"

#include "lwip/opt.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/opt.h"
#include "lwip/dns.h"
#include "lwip/tcpip.h"
#include "lwip/netif.h"
#include "netif/etharp.h"

#include "mico_lwip_ethif_logging.h"
#include "platform_peripheral.h"

#if PLATFORM_ETH_ENABLE

/******************************************************
 *                      Macros
 ******************************************************/

#define PREF_IPV4                   1
#define PREF_IPV6                   2

#if MICO_CONFIG_IP_VER_PREF == 4
#define IP_VERSION_PREF             PREF_IPV4
#endif
#if MICO_CONFIG_IP_VER_PREF == 6
#define IP_VERSION_PREF             PREF_IPV6
#endif
#ifndef IP_VERSION_PREF
#error "Either IPv4 or IPv6 must be preferred."
#endif

#define DHCP_TIMEOUT_CHECK_TIME (30*1000)

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
 *               Function Declarations
 ******************************************************/

static void mico_eth_set_mac_address(void);
static void mico_lwip_netif_link_irq(struct netif *lwip_netif);
static void mico_lwip_netif_status_irq(struct netif *lwip_netif);

void mico_eth_add_dns_addr();
const ip_addr_t *mico_eth_get_ip_addr(bool any_addr);
static const ip_addr_t *mico_eth_get_ipv6_addr(void);

extern OSStatus mxchipInit( void );

extern err_t eth_arch_enetif_init(struct netif *netif);
extern void eth_arch_enable_interrupts(void);
extern void WifiStatusHandler(notify_netif_status_t status);

/******************************************************
 *               Variables Definitions
 ******************************************************/

/* TCP/IP and Network Interface Initialisation */
static struct netif lwip_netif;
static bool lwip_dhcp = false;
static char lwip_mac_address[NSAPI_MAC_SIZE];


/******************************************************
 *               Function Definitions
 ******************************************************/

/* This function is called after void MicoInit(void) */
OSStatus mico_eth_init( void )
{

    // Check if we've already brought up lwip
    if (!mico_eth_get_mac_address()) {
        // Set up network
        mico_eth_set_mac_address();

        if (mico_tcpip_stack_is_inited() == MICO_FALSE) {
            mxchipInit();
        }

        memset(&lwip_netif, 0, sizeof lwip_netif);

        if (!netif_add(&lwip_netif,
#if LWIP_IPV4
                IP_ADDR_ANY, IP_ADDR_ANY, IP_ADDR_ANY,
#endif
                NULL, eth_arch_enetif_init, tcpip_input)) {
            return kUnexpectedErr;
        }

        netif_set_link_callback(&lwip_netif, mico_lwip_netif_link_irq);
        netif_set_status_callback(&lwip_netif, mico_lwip_netif_status_irq);

        eth_arch_enable_interrupts();
    }

    return kNoErr;
}

static bool lwip_connected = false;

/* If ethernet interface can't get IP address for 30 seconds, start autoip. */
static void dhcp_timeout_check(struct netif *netif)
{
    if ( netif->dhcp->state == DHCP_BOUND ) {
        return;
    }
    dhcp_stop(netif);
    autoip_start(netif);
}

OSStatus mico_eth_bringup(bool dhcp, const char *ip, const char *netmask, const char *gw)
{
    // Check if we've already connected
    if (lwip_connected) {
        return kAlreadyInUseErr;
    }

    mico_eth_power_up();

    if(mico_eth_init() != kNoErr) {
        return kNotPreparedErr;
    }

#if LWIP_IPV6
    netif_create_ip6_linklocal_address(&lwip_netif, 1/*from MAC*/);
#if LWIP_IPV6_MLD
  /*
   * For hardware/netifs that implement MAC filtering.
   * All-nodes link-local is handled by default, so we must let the hardware know
   * to allow multicast packets in.
   * Should set mld_mac_filter previously. */
  if (lwip_netif.mld_mac_filter != NULL) {
    ip6_addr_t ip6_allnodes_ll;
    ip6_addr_set_allnodes_linklocal(&ip6_allnodes_ll);
    lwip_netif.mld_mac_filter(&lwip_netif, &ip6_allnodes_ll, NETIF_ADD_MAC_FILTER);
  }
#endif /* LWIP_IPV6_MLD */

#if LWIP_IPV6_AUTOCONFIG
    /* IPv6 address autoconfiguration not enabled by default */
  lwip_netif.ip6_autoconfig_enabled = 1;
#endif /* LWIP_IPV6_AUTOCONFIG */

#endif

    u32_t ret;

#if LWIP_IPV4
    ip_addr_t ip_addr;
    ip_addr_t netmask_addr;
    ip_addr_t gw_addr;
    
    if (!dhcp) {
        if (!inet_aton(ip, &ip_addr) ||
            !inet_aton(netmask, &netmask_addr) ||
            !inet_aton(gw, &gw_addr)) {
            return kParamErr;
        }
    } else {
        ip_addr_set_zero(&ip_addr);
        ip_addr_set_zero(&netmask_addr);
        ip_addr_set_zero(&gw_addr);
    }

    netif_set_addr(&lwip_netif, &ip_addr, &netmask_addr, &gw_addr);
#endif

#if LWIP_IPV4
    // Connect to the network
    lwip_dhcp = dhcp;

    if (netif_is_link_up(&lwip_netif)) {
        if (lwip_dhcp) {
            eth_log("Start DHCP...");
            autoip_stop(&lwip_netif);
            dhcp_start(&lwip_netif);
            tcpip_untimeout(dhcp_timeout_check, &lwip_netif);
            tcpip_timeout(DHCP_TIMEOUT_CHECK_TIME, dhcp_timeout_check, &lwip_netif);
        } else {
            netif_set_up(&lwip_netif);
        }
    }

#endif

    lwip_connected = true;
    return 0;
}

#if LWIP_IPV6
void mbed_lwip_clear_ipv6_addresses(struct netif *lwip_netif)
{
    for (u8_t i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        netif_ip6_addr_set_state(lwip_netif, i, IP6_ADDR_INVALID);
    }
}
#endif

OSStatus mico_eth_bringdown(void)
{
    // Check if we've connected
    if (!lwip_connected) {
        return kNotInUseErr;
    }
    eth_log("bring down");
#if LWIP_IPV4
    // Disconnect from the network
    if (lwip_dhcp) {
        dhcp_release(&lwip_netif);
        dhcp_stop(&lwip_netif);
        lwip_dhcp = false;
        autoip_stop(&lwip_netif);
    }
#endif

    netif_set_down(&lwip_netif);

#if LWIP_IPV6
    mbed_lwip_clear_ipv6_addresses(&lwip_netif);
#endif

    lwip_connected = false;
    return kNoErr;
}

void mico_eth_set_default_interface(void)
{
    netif_set_default(&lwip_netif);
}


static const ip_addr_t *mico_eth_get_ipv4_addr(void)
{
#if LWIP_IPV4
    if (!netif_is_up(&lwip_netif)) {
        return NULL;
    }

    if (!ip_addr_isany(netif_ip4_addr(&lwip_netif))) {
        return netif_ip_addr4(&lwip_netif);
    }
#endif

    return NULL;
}

static const ip_addr_t *mico_eth_get_ipv6_addr(void)
{
#if LWIP_IPV6
    if (!netif_is_up(netif)) {
        return NULL;
    }

    for (int i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) {
        if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i)) &&
                !ip6_addr_islinklocal(netif_ip6_addr(netif, i))) {
            return netif_ip_addr6(netif, i);
        }
    }
#endif

    return NULL;

}

const ip_addr_t *mico_eth_get_ip_addr(bool any_addr)
{
    const ip_addr_t *pref_ip_addr = 0;
    const ip_addr_t *npref_ip_addr = 0;

#if IP_VERSION_PREF == PREF_IPV4
    pref_ip_addr = mico_eth_get_ipv4_addr();
    npref_ip_addr = mico_eth_get_ipv6_addr();
#else
    pref_ip_addr = mico_eth_get_ipv6_addr();
    npref_ip_addr = mico_eth_get_ipv4_addr();
#endif

    if (pref_ip_addr) {
        return pref_ip_addr;
    } else if (npref_ip_addr && any_addr) {
        return npref_ip_addr;
    }

    return NULL;
}

void mico_eth_add_dns_addr()
{
    // Do nothing if not brought up
    const ip_addr_t *ip_addr = mico_eth_get_ip_addr(true);
    ip_addr_t dns_ip_addr;

    if (!ip_addr) {
        return;
    }

    // Check for existing dns server
    for (char numdns = 0; numdns < DNS_MAX_SERVERS; numdns++) {
        dns_ip_addr = dns_getserver(numdns);
        if (dns_ip_addr.addr != IP_ADDR_ANY->addr) {
            return;
        }
    }

#if LWIP_IPV6
    if (IP_IS_V6(ip_addr)) {
        /* 2001:4860:4860::8888 google */
        ip_addr_t ipv6_dns_addr = IPADDR6_INIT(
                PP_HTONL(0x20014860UL),
                PP_HTONL(0x48600000UL),
                PP_HTONL(0x00000000UL),
                PP_HTONL(0x00008888UL));
        dns_setserver(0, &ipv6_dns_addr);
    }
#endif

#if LWIP_IPV4
    if (IP_IS_V4(ip_addr)) {
        /* 8.8.8.8 google */
        ip_addr_t ipv4_dns_addr = IPADDR4_INIT(0x08080808);
        dns_setserver(0, &ipv4_dns_addr);
    }
#endif
}

static void mico_lwip_netif_link_irq(struct netif *lwip_netif)
{
    if (netif_is_link_up(lwip_netif)) {
        eth_log("Ethernet link up");
        if (lwip_dhcp) {
            ip_addr_t ip_addr;

            ip_addr_set_zero(&ip_addr);
            netif_set_addr(lwip_netif, &ip_addr, &ip_addr, &ip_addr);
            autoip_stop(lwip_netif);
            dhcp_start(lwip_netif);
            tcpip_untimeout(dhcp_timeout_check, lwip_netif);
            tcpip_timeout(DHCP_TIMEOUT_CHECK_TIME, dhcp_timeout_check, lwip_netif);
        } else {
            netif_set_up(lwip_netif);
        }
    } else {
        eth_log("Ethernet link down");
        netif_set_down(lwip_netif);
        if (lwip_dhcp) {
            dhcp_stop(lwip_netif);
        }
    }
}

static OSStatus notify_app_ethif_status_changed(void *arg)
{
    uint32_t if_status = (uint32_t)arg;
    WifiStatusHandler(if_status);
    return kNoErr;
}

static void mico_lwip_netif_status_irq(struct netif *lwip_netif)
{
    static bool any_addr = true;
    const ip_addr_t *addr;

    if (netif_is_up(lwip_netif)) {
        eth_log("Ethernet interface up");

        // Indicates that has address
        addr = mico_eth_get_ip_addr(true);
        if (any_addr == true && addr) {
            eth_log("Ethernet interface has address: %s", ipaddr_ntoa(addr));
            any_addr = false;
            mico_eth_add_dns_addr();
            mico_rtos_send_asynchronous_event( MICO_NETWORKING_WORKER_THREAD, notify_app_ethif_status_changed, (void *)NOTIFY_ETH_UP );

            return;
        }

        // Indicates that has preferred address
        addr = mico_eth_get_ip_addr(false);
        if (addr) {
            eth_log("Ethernet interface has preferred address: %s", ipaddr_ntoa(addr));

            mico_eth_add_dns_addr();
            mico_rtos_send_asynchronous_event( MICO_NETWORKING_WORKER_THREAD, notify_app_ethif_status_changed, (void *)NOTIFY_ETH_UP );

        }

    } else {
        any_addr = true;
        mico_rtos_send_asynchronous_event( MICO_NETWORKING_WORKER_THREAD, notify_app_ethif_status_changed, (void *)NOTIFY_ETH_DOWN );
        eth_log("Ethernet interface down");
    }
}


static void mico_eth_set_mac_address(void)
{
    char mac[6];
    platform_eth_mac_address(mac);
    snprintf(lwip_mac_address, NSAPI_MAC_SIZE, "%02x:%02x:%02x:%02x:%02x:%02x",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    eth_log("Ethernet MAC address %s", lwip_mac_address);
}

/* LWIP interface implementation */
const char *mico_eth_get_mac_address(void)
{
    return lwip_mac_address[0] ? lwip_mac_address : 0;
}

struct netif *mico_eth_get_if_handle(void)
{
    return &lwip_netif;
}

#endif

