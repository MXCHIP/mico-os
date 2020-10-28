#include "device_lock.h"
#include <wifi/wifi_conf.h>
#include <wifi/wifi_util.h>
#include <wifi/wifi_ind.h>
#include <lwip_netconf.h>
#include <lwip/sockets.h>
#include "lwip/netdb.h"
#include "mwifi.h"
#include "lota.h"
#include "flash_api.h"

#include "mos.h"
#include "mdebug.h"
#include "mhal/flash.h"

static mwifi_monitor_cb_t lota_monitor_cb = NULL;

extern unsigned char psk_essid[NET_IF_NUM][32+4];
extern unsigned char psk_passphrase[NET_IF_NUM][64 + 1];
extern unsigned char wpa_global_PSK[NET_IF_NUM][20 * 2];
extern unsigned char psk_passphrase64[64 + 1];
extern struct netif xnetif[NET_IF_NUM]; 

static void wifi_rx_cb(unsigned char *buf, unsigned int len, void* userdata)
{
    if (lota_monitor_cb == NULL)
        return;
    lota_monitor_cb(buf, len);
}

/* start wifi monitor */
void lota_monitor_start(mwifi_monitor_cb_t func, uint8_t channel)
{
	lota_monitor_cb = func;
	wifi_on(RTW_MODE_PROMISC);
    wifi_set_promisc(RTW_PROMISC_ENABLE_2, wifi_rx_cb, 0);
    wifi_set_channel((int)channel);
}

/* stop wifi monitor*/
void lota_monitor_stop(void)
{
    wifi_set_promisc(RTW_PROMISC_DISABLE, NULL, 0);
	wifi_off();
}

/* wifi initilize */
void lota_wifi_init(void)
{
    wlan_network();
}

static void set_netif_static_ip(mwifi_ip_attr_t *attr)
{
	struct ip_addr ipaddr;
	struct ip_addr netmask;
	struct ip_addr gw, dns;
    struct netif *pnetif = &xnetif[0];
    uint32_t ip;

    ip = inet_addr((char*)attr->localip);
    ip_addr_set_ip4_u32(&ipaddr, ip);
    ip = inet_addr((char*)attr->netmask);
    ip_addr_set_ip4_u32(&netmask, ip);
    ip = inet_addr((char*)attr->gateway);
    ip_addr_set_ip4_u32(&gw, ip);
    
#if LWIP_VERSION_MAJOR >= 2
	netif_set_addr(pnetif, ip_2_ip4(&ipaddr), ip_2_ip4(&netmask),ip_2_ip4(&gw));
#else
	netif_set_addr(pnetif, &ipaddr , &netmask, &gw);
#endif
    netif_set_up(pnetif); 
	netif_set_default(pnetif);
}


/* wifi connect, max wait 4 seconds
 * retval: 0=connected, other=fail.
 */
int lota_wifi_connect(char *ssid, char *psk, char*pass, mwifi_connect_attr_t *attr, mwifi_ip_attr_t *ip)
{
    int ret;
    char key[8];
    uint8_t     pscan_config;
    
    memcpy(wpa_global_PSK[0], psk, 40);
    strcpy(psk_essid[0], ssid);
    memset(key, 0, 8);
    pscan_config = PSCAN_ENABLE | PSCAN_FAST_SURVEY;
	wifi_set_pscan_chan((uint8_t *)&attr->channel, &pscan_config, 1);
    ret = wifi_connect_bssid(attr->bssid, ssid, RTW_SECURITY_WPA_WPA2_MIXED,  
		               key, 6, strlen(ssid), 8, 0, NULL);
    printf("wifi connect %s:%s, ret %d\r\n", ssid, pass, ret);
    if (ret == 0)
        set_netif_static_ip(ip);
    return ret;
}

int lota_wifi_disconnect(void)
{
    wifi_disconnect();
}

