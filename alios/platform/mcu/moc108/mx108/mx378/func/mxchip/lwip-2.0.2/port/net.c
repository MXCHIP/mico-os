#include "include.h"
#include <stdio.h>
#include <string.h>

#include <lwip/inet.h>

#include "netif/etharp.h"
#include "lwip/netif.h"
#include <lwip/netifapi.h>
#include <lwip/tcpip.h>
#include <lwip/dns.h>
#include <lwip/dhcp.h>
#include "lwip/prot/dhcp.h"

#include <lwip/sockets.h>
#include "ethernetif.h"


#include "sa_station.h"

#include "drv_model_pub.h"
#include "mem_pub.h"
#include "common.h"

#include "rw_pub.h"

#include "mxchip_netif_address.h"
#include "rtos_pub.h"
#include <hal/base.h>
#include <hal/wifi.h>


struct ipv4_config sta_ip_settings;
struct ipv4_config uap_ip_settings;
void *net_get_sta_handle(void);
void *net_get_uap_handle(void);
static int up_iface = 1;
uint32_t sta_ip_start_flag = 0;

#ifdef CONFIG_IPV6
#define IPV6_ADDR_STATE_TENTATIVE       "Tentative"
#define IPV6_ADDR_STATE_PREFERRED       "Preferred"
#define IPV6_ADDR_STATE_INVALID         "Invalid"
#define IPV6_ADDR_STATE_VALID           "Valid"
#define IPV6_ADDR_STATE_DEPRECATED      "Deprecated"
#define IPV6_ADDR_TYPE_LINKLOCAL	"Link-Local"
#define IPV6_ADDR_TYPE_GLOBAL		"Global"
#define IPV6_ADDR_TYPE_UNIQUELOCAL	"Unique-Local"
#define IPV6_ADDR_TYPE_SITELOCAL	"Site-Local"
#define IPV6_ADDR_UNKNOWN		"Unknown"
#endif


#define net_e 
#define net_d 

struct interface {
	struct netif netif;
	struct ip4_addr ipaddr;
	struct ip4_addr nmask;
	struct ip4_addr gw;
};

static struct interface g_mlan;
static struct interface g_uap;

err_t lwip_netif_init(struct netif *netif);
err_t lwip_netif_uap_init(struct netif *netif);
void handle_data_packet(const u8_t interface, const u8_t *rcvdata,
			const u16_t datalen);

extern void stats_udp_bcast_display();

#ifdef CONFIG_IPV6
char *ipv6_addr_state_to_desc(unsigned char addr_state)
{
	if (ip6_addr_istentative(addr_state))
		return IPV6_ADDR_STATE_TENTATIVE;
	else if (ip6_addr_ispreferred(addr_state))
		return IPV6_ADDR_STATE_PREFERRED;
	else if (ip6_addr_isinvalid(addr_state))
		return IPV6_ADDR_STATE_INVALID;
	else if (ip6_addr_isvalid(addr_state))
		return IPV6_ADDR_STATE_VALID;
	else if (ip6_addr_isdeprecated(addr_state))
		return IPV6_ADDR_STATE_DEPRECATED;
	else
		return IPV6_ADDR_UNKNOWN;

}

char *ipv6_addr_type_to_desc(struct ipv6_config *ipv6_conf)
{
	if (ip6_addr_islinklocal((ip6_addr_t *)ipv6_conf->address))
		return IPV6_ADDR_TYPE_LINKLOCAL;
	else if (ip6_addr_isglobal((ip6_addr_t *)ipv6_conf->address))
		return IPV6_ADDR_TYPE_GLOBAL;
	else if (ip6_addr_isuniquelocal((ip6_addr_t *)ipv6_conf->address))
		return IPV6_ADDR_TYPE_UNIQUELOCAL;
	else if (ip6_addr_issitelocal((ip6_addr_t *)ipv6_conf->address))
		return IPV6_ADDR_TYPE_SITELOCAL;
	else
		return IPV6_ADDR_UNKNOWN;
}
#endif /* CONFIG_IPV6 */

int net_dhcp_hostname_set(char *hostname)
{
	netif_set_hostname(&g_mlan.netif, hostname);
	return 0;
}

void net_ipv4stack_init()
{
	static bool tcpip_init_done = 0;
	if (tcpip_init_done)
		return;

	net_d("Initializing TCP/IP stack\r\n");
	tcpip_init(NULL, NULL);
	tcpip_init_done = true;
}

#ifdef CONFIG_IPV6
void net_ipv6stack_init(struct netif *netif)
{
	netif->flags |= NETIF_IPV6_FLAG_UP;

	netif_create_ip6_linklocal_address(netif, 1);
	netif->ip6_autoconfig_enabled = 1;
}

#endif /* CONFIG_IPV6 */

void net_wlan_init(void)
{
	static int wlan_init_done = 0;
	int ret;
	if (!wlan_init_done) {
		net_ipv4stack_init();
		g_mlan.ipaddr.addr = INADDR_ANY;
		ret = netifapi_netif_add(&g_mlan.netif,
                     &g_mlan.ipaddr, &g_mlan.ipaddr, &g_mlan.ipaddr, NULL,
					 lwip_netif_init, tcpip_input);
		if (ret) {
			/*FIXME: Handle the error case cleanly */
			net_e("MLAN interface add failed");
		}
#ifdef CONFIG_IPV6
		net_ipv6stack_init(&g_mlan.netif);
#endif /* CONFIG_IPV6 */
		netifapi_netif_set_default(&g_mlan.netif);
#ifdef CONFIG_SOFTAP 
		ret = netifapi_netif_add(&g_uap.netif, &g_uap.ipaddr,
					 &g_uap.ipaddr, &g_uap.ipaddr, NULL,
					 lwip_netif_uap_init, tcpip_input);
		if (ret) {
			/*FIXME: Handle the error case cleanly */
			net_e("UAP interface add failed");
		}
#endif
		wlan_init_done = 1;
	}

	return;
}

static void dhcp_up(uint32_t ip, uint32_t netmask, uint32_t gateway, uint32_t dnsServer)
{
    hal_wifi_ip_stat_t netpara;
    hal_wifi_ip_stat_t *pnetpara = &netpara;
    unsigned char mac[6];
	char macstr[14];
    struct in_addr addr;
	
	memset(pnetpara, 0, sizeof(hal_wifi_ip_stat_t));
	wifi_get_mac_address(mac);

	sprintf(macstr, "%02x%02x%02x%02x%02x%02x", mac[0],
			mac[1], mac[2], mac[3], mac[4], mac[5]);
	memcpy(pnetpara->mac, macstr, 12);

	addr.s_addr = ip;
	strcpy(pnetpara->ip, inet_ntoa(addr));
	addr.s_addr = netmask;
	strcpy(pnetpara->mask, inet_ntoa(addr));
	addr.s_addr = gateway;
	strcpy(pnetpara->gate, inet_ntoa(addr));
	addr.s_addr = dnsServer;
	strcpy(pnetpara->dns, inet_ntoa(addr));
    pnetpara->dhcp = 1;
    sprintf(pnetpara->broadcastip, "255.255.255.255");
    NetCallback(pnetpara);
}

static void wifi_station_changed(int connected)
{
	static int last_state = 0;

	if (connected == last_state)
		return;
	
	last_state = connected;
	if (connected) {
		hal_wifi_ap_info_adv_t ap_info;
		uint8_t *key;
		int key_len;

		wpa_get_ap_security(&ap_info, &key, &key_len);
		connected_ap_info(&ap_info, key, key_len);
	
		WifiStatusHandler(1);
	} else
		WifiStatusHandler(2);
}

void wifi_uap_changed(int connected)
{
	static int last_state = 0;

	if (connected == last_state)
		return;
	
	last_state = connected;
	if (connected) {
		WifiStatusHandler(3);
	} else
		WifiStatusHandler(4);
}

const ip_addr_t* dns_getserver(u8_t numdns);
#ifdef CONFIG_IPV6
/** check if ipv6 address supplied netif->ip_addr
 *
 * @param netif the netif to check
 * @return 1 if valid ipv6 addrss supplied netif->ip_addr (state valid),
 *         0 otherwise
 */
u8_t
ipv6_supplied_address(const struct netif *netif)
{
	s8_t i;
	/* Skip link-local address at index 0 */
  	for (i = 1; i < LWIP_IPV6_NUM_ADDRESSES; i++) { 
    	if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i))) {
      		return 1;
    	}
  	}
  return 0;
}

u8_t
ipv6_show_address(const struct netif *netif)
{
	s8_t i;
	/* Skip link-local address at index 0 */
  	for (i = 0; i < LWIP_IPV6_NUM_ADDRESSES; i++) { 
    	if (ip6_addr_isvalid(netif_ip6_addr_state(netif, i))) {
      		printf("ipv6 address(%d): %s\r\n", i, ip6addr_ntoa(netif_ip6_addr(netif, i)));
    	}
  	}
  return 0;
}

#endif
static void netif_status_callback(struct netif *n)
{
	ip_addr_t *dns_server;
	uint32_t sed = (uint32_t)aos_now_ms();

	/* use current system time to set the start tcp/udp port number RANDOMIZE */
	srand(sed);
	tcp_init();
	udp_init();
	
	if (n->flags & NETIF_FLAG_UP) {
		struct dhcp *dhcp = netif_dhcp_data(n);
		if(dhcp != NULL){ 
			if (dhcp->state == DHCP_STATE_BOUND) {
				// dhcp success
				dns_server = dns_getserver(0);
				dhcp_up(ip_addr_get_ip4_u32(&n->ip_addr), 
                    ip_addr_get_ip4_u32(&n->netmask), 
                    ip_addr_get_ip4_u32(&n->gw),
					ip_addr_get_ip4_u32(dns_server));
				wifi_station_changed(1);
			} else {
				// dhcp fail
			}
		} 
#ifdef CONFIG_IPV6
        if (ipv6_supplied_address(n)) {
            wifi_station_changed(1);
            ipv6_show_address(n);
        }
#endif
		
	} else {
		// dhcp fail;
	}
}


static int check_iface_mask(void *handle, uint32_t ipaddr)
{
	uint32_t interface_ip, interface_mask;
	net_get_if_ip_addr(&interface_ip, handle);
	net_get_if_ip_mask(&interface_mask, handle);
	if (interface_ip > 0)
		if ((interface_ip & interface_mask) ==
		    (ipaddr & interface_mask))
			return 0;
	return -1;
}

void *net_ip_to_interface(uint32_t ipaddr)
{
	int ret;
	void *handle;
	/* Check mlan handle */
	handle = net_get_sta_handle();
	ret = check_iface_mask(handle, ipaddr);
	if (ret == 0)
		return handle;

	/* Check uap handle */
	handle = net_get_uap_handle();
	ret = check_iface_mask(handle, ipaddr);
	if (ret == 0)
		return handle;

	/* If more interfaces are added then above check needs to done for
	 * those newly added interfaces
	 */
	return NULL;
}

void *net_sock_to_interface(int sock)
{
	struct sockaddr_in peer;
	unsigned long peerlen = sizeof(struct sockaddr_in);
	void *req_iface = NULL;

	getpeername(sock, (struct sockaddr *)&peer, &peerlen);
	req_iface = net_ip_to_interface(peer.sin_addr.s_addr);
	return req_iface;
}

void *net_get_sta_handle(void)
{
	return &g_mlan;
}

void *net_get_uap_handle(void)
{
	return &g_uap;
}

void *net_get_netif_handle(void)
{
	if (up_iface == 1)
		return &g_mlan.netif;
	else
		return &g_uap.netif;
}

void net_interface_up(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	netifapi_netif_set_up(&if_handle->netif);
}

void net_interface_down(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	netifapi_netif_set_down(&if_handle->netif);
}

#ifdef CONFIG_IPV6
void net_interface_deregister_ipv6_callback(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	if (intrfc_handle == &g_mlan)
		netif_set_ipv6_status_callback(&if_handle->netif, NULL);
}
#endif

void net_interface_dhcp_stop(void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	netifapi_dhcp_stop(&if_handle->netif);
	netif_set_status_callback(&if_handle->netif,
				NULL);
}

void sta_ip_down(void)
{
	if(sta_ip_start_flag)
	{
		os_printf("sta_ip_down\r\n");
		wifi_station_changed(0);
		sta_ip_start_flag = 0;

		netifapi_netif_set_down(&g_mlan.netif);
		netif_set_status_callback(&g_mlan.netif, NULL);
		netifapi_dhcp_stop(&g_mlan.netif);
	}
}

void sta_ip_start(void)
{
	if (sta_ip_start_flag == 1) // IP already started
		return;
	os_printf("sta_ip_start\r\n");
	sta_ip_start_flag = 1;
	net_configure_address(&sta_ip_settings, net_get_sta_handle());
}

uint32_t sta_ip_is_start(void)
{
	return sta_ip_start_flag;
}

void uap_ip_start(void)
{
	net_configure_address(&uap_ip_settings, net_get_uap_handle());
}

void uap_ip_down(void)
{
	wifi_uap_changed(0);
	
	netifapi_netif_set_down(&g_uap.netif);
	netif_set_status_callback(&g_uap.netif, NULL);
}

#define DEF_UAP_IP	0xc0a80a01UL /* 192.168.10.1 */
static unsigned int uap_ip =  DEF_UAP_IP;

void ip_address_set(int iface, int dhcp, char *ip, char *mask, char*gw, char*dns)
{
	uint32_t tmp;
	struct ipv4_config addr;

	memset(&addr, 0, sizeof(struct ipv4_config));
	if (dhcp == 1) {
		addr.addr_type = ADDR_TYPE_DHCP;
	} else {
		addr.addr_type = ADDR_TYPE_STATIC;
	    tmp = inet_addr((char*)ip);
	    addr.address = (tmp);
	    tmp = inet_addr((char*)mask);
	    if (tmp == 0xFFFFFFFF)
	        tmp = 0x00FFFFFF;// if not set valid netmask, set as 255.255.255.0
	    addr.netmask= (tmp);
	    tmp = inet_addr((char*)gw);
	    addr.gw = (tmp);

	    tmp = inet_addr((char*)dns);
	    addr.dns1 = (tmp);
	}
	if (iface == 1) {// Station
	    up_iface = 1;
		memcpy(&sta_ip_settings, &addr, sizeof(addr));
	} else {
        up_iface = 0;
        memcpy(&uap_ip_settings, &addr, sizeof(addr));
    }
}

int net_configure_address(struct ipv4_config *addr, void *intrfc_handle)
{
	if (!intrfc_handle)
		return -1;

	struct interface *if_handle = (struct interface *)intrfc_handle;
	struct ip4_addr dns;
	int i = 0;

	net_d("\r\nconfiguring interface %s (with %s)",
		(if_handle == &g_mlan) ? "mlan" :"uap",
		(addr->addr_type == ADDR_TYPE_DHCP)
		? "DHCP client" : "Static IP");
	netifapi_netif_set_down(&if_handle->netif);

	/* De-register previously registered DHCP Callback for correct
	 * address configuration.
	 */
	netif_set_status_callback(&if_handle->netif,
					netif_status_callback);
	//if (if_handle == &g_mlan) //TODO: dual mode, only set station as the default interface
		netifapi_netif_set_default(&if_handle->netif);
	switch (addr->addr_type) {
	case ADDR_TYPE_STATIC:
		if_handle->ipaddr.addr = addr->address;
		if_handle->nmask.addr = addr->netmask;
		if_handle->gw.addr = addr->gw;
		netifapi_netif_set_addr(&if_handle->netif, &if_handle->ipaddr,
					&if_handle->nmask, &if_handle->gw);
		netifapi_netif_set_up(&if_handle->netif);
		if (addr->dns1 != 0) {
			dns.addr = addr->dns1;
			dns_setserver(i++, &dns);
		}
		if (addr->dns2 != 0) {
			dns.addr = addr->dns2;
			dns_setserver(i, &dns);
		}
		if (if_handle == &g_mlan) {
			wifi_station_changed(1);
		} else {
			wifi_uap_changed(1);
		}
		break;

	case ADDR_TYPE_DHCP:
		/* Reset the address since we might be transitioning from static to DHCP */
		memset(&if_handle->ipaddr, 0, sizeof(struct ip4_addr));
		memset(&if_handle->nmask, 0, sizeof(struct ip4_addr));
		memset(&if_handle->gw, 0, sizeof(struct ip4_addr));
		netifapi_netif_set_addr(&if_handle->netif, &if_handle->ipaddr,
				&if_handle->nmask, &if_handle->gw);

		netifapi_netif_set_up(&if_handle->netif);
		netifapi_dhcp_start(&if_handle->netif);
		break;

	default:
		break;
	}
	/* Finally this should send the following event. */
	if (if_handle == &g_mlan) {
		// static IP up;

		/* XXX For DHCP, the above event will only indicate that the
		 * DHCP address obtaining process has started. Once the DHCP
		 * address has been obtained, another event,
		 * WD_EVENT_NET_DHCP_CONFIG, should be sent to the wlcmgr.
		 */
	} else {
#ifdef CONFIG_SOFTAP 	
		// softap IP up, start dhcp server;
		dhcp_server_start(net_get_uap_handle());
#endif  
	}

	return 0;
}

int net_get_if_addr(struct wlan_ip_config *addr, void *intrfc_handle)
{
	ip_addr_t *tmp;
	struct interface *if_handle = (struct interface *)intrfc_handle;

	addr->ipv4.address = ip_addr_get_ip4_u32(&if_handle->netif.ip_addr);
	addr->ipv4.netmask = ip_addr_get_ip4_u32(&if_handle->netif.netmask);
	addr->ipv4.gw = ip_addr_get_ip4_u32(&if_handle->netif.gw);

	tmp = dns_getserver(0);
	addr->ipv4.dns1 = ip_addr_get_ip4_u32(tmp);
	tmp = dns_getserver(1);
	addr->ipv4.dns2 = ip_addr_get_ip4_u32(tmp);

	return 0;
}

#ifdef CONFIG_IPV6
int net_get_if_ipv6_addr(struct wlan_ip_config *addr, void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;
	int i;

	for (i = 0; i < MAX_IPV6_ADDRESSES; i++) {
		memcpy(addr->ipv6[i].address,
			if_handle->netif.ip6_addr[i].u_addr.ip6.addr, 16);
		addr->ipv6[i].addr_state = if_handle->netif.ip6_addr_state[i];
	}
	/* TODO carry out more processing based on IPv6 fields in netif */
	return 0;
}

int net_get_if_ipv6_pref_addr(struct wlan_ip_config *addr, void *intrfc_handle)
{
	int i, ret = 0;
	struct interface *if_handle = (struct interface *)intrfc_handle;

	for (i = 0; i < MAX_IPV6_ADDRESSES; i++) {
		if (if_handle->netif.ip6_addr_state[i] == IP6_ADDR_PREFERRED) {
			memcpy(addr->ipv6[ret++].address,
				if_handle->netif.ip6_addr[i].u_addr.ip6.addr, 16);
		}
	}
	return ret;
}
#endif /* CONFIG_IPV6 */

int net_get_if_ip_addr(uint32_t *ip, void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;

	*ip = ip_addr_get_ip4_u32(&if_handle->netif.ip_addr);
	return 0;
}

int net_get_if_ip_mask(uint32_t *nm, void *intrfc_handle)
{
	struct interface *if_handle = (struct interface *)intrfc_handle;

	*nm = ip_addr_get_ip4_u32(&if_handle->netif.netmask);
	return 0;
}

void net_configure_dns(struct wlan_ip_config *ip)
{
	ip_addr_t tmp;

	if (ip->ipv4.addr_type == ADDR_TYPE_STATIC) {

		if (ip->ipv4.dns1 == 0)
			ip->ipv4.dns1 = ip->ipv4.gw;
		if (ip->ipv4.dns2 == 0)
			ip->ipv4.dns2 = ip->ipv4.dns1;
        ip_addr_set_ip4_u32(&tmp, ip->ipv4.dns1);
		dns_setserver(0, &tmp);
		ip_addr_set_ip4_u32(&tmp, ip->ipv4.dns2);
		dns_setserver(1, &tmp);
	}

	/* DNS MAX Retries should be configured in lwip/dns.c to 3/4 */
	/* DNS Cache size of about 4 is sufficient */
}


uint32_t ipv4_addr_aton(char *ipstr)
{
	ip4_addr_t ip;
	uint32_t ret;
	
	ret = ip4addr_aton(ipstr, &ip);

	if (ret == 0) // fail
		return 0;
	else
		ret = ip.addr;

	return ret;
}

