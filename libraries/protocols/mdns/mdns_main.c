/*
 *  Copyright (C) 2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include "mico.h"
#include "mdns_port.h"
#include "mdns.h"
#include "mdns_private.h"

#define MDNS_QUERIER_THREAD_STACK (2048)
#define MDNS_RESPONDER_THREAD_STACK (1500)

mico_thread_t mdns_querier_thread;
mico_thread_t mdns_responder_thread;

static bool is_responder_started, is_querier_started;


void *mdns_thread_create(mdns_thread_entry entry, int id)
{
	void *ret = NULL;
    if (id == MDNS_THREAD_RESPONDER) {
        if ( mico_rtos_create_thread( &mdns_responder_thread, MICO_APPLICATION_PRIORITY, "mdns_resp_thread",
                                     (mico_thread_function_t)entry, MDNS_RESPONDER_THREAD_STACK, 0) == kNoErr ) {
            ret = &mdns_responder_thread;
        }
    } else if (id == MDNS_THREAD_QUERIER) {
        if ( mico_rtos_create_thread( &mdns_querier_thread, MICO_APPLICATION_PRIORITY, "mdns_querier_thread",
                                     (mico_thread_function_t)entry, MDNS_QUERIER_THREAD_STACK, 0) == kNoErr ) {
            ret = &mdns_querier_thread;
        }
    }
	return ret;
}

void mdns_thread_delete(void *t)
{
    mico_rtos_delete_thread( (mico_thread_t *) t );
}

void mdns_thread_yield(void *t)
{
	mico_rtos_thread_yield();
}

uint32_t mdns_time_ms(void)
{
    mico_time_t current;
    mico_time_get_time(&current);
	return current;
}

int mdns_rand_range(int n)
{
    int r;
    srand( mico_rtos_get_time() );
	r = rand();
	return r / (RAND_MAX / n + 1);
}

static void net_status_changed_delegate(WiFiEvent event, void *arg)
{
    UNUSED_PARAMETER(arg);
    switch ( event )
    {
        case NOTIFY_STATION_UP:
            mdns_iface_group_state_change(INTERFACE_STA, JOIN);
            mdns_iface_state_change(INTERFACE_STA, UP);
            break;
        case NOTIFY_AP_UP:
            mdns_iface_group_state_change(INTERFACE_UAP, JOIN);
            mdns_iface_state_change(INTERFACE_UAP, UP);
            break;
        default:
            break;
    }
}

static void system_will_poweroff_delegate( void *arg )
{
    UNUSED_PARAMETER(arg);
    mdns_iface_state_change(INTERFACE_STA, DOWN);
    mdns_iface_state_change(INTERFACE_UAP, DOWN);
}

int mdns_start(const char *domain, char *hostname)
{
    OSStatus err = kNoErr;

    err = mico_system_notify_register(mico_notify_WIFI_STATUS_CHANGED, (void *) net_status_changed_delegate, NULL);
    require_noerr(err, exit);
    err = mico_system_notify_register(mico_notify_SYS_WILL_POWER_OFF, (void *) system_will_poweroff_delegate, NULL);
    require_noerr(err, exit);

    if (hostname) {
        if (is_responder_started != true) {
            err = responder_launch(domain, hostname);
            require_noerr(err, exit);
            is_responder_started = true;
        } else {
            LOG("mdns responder already started");
        }
    }
    if (is_querier_started != true) {
        err = query_launch();
        require_noerr(err, exit);
        is_querier_started = true;
    } else {
        LOG("mdns querier already started");
    }

exit:
    return err;
}

void mdns_stop(void)
{
    LOG("Stopping mdns.\r\n");
    if (is_responder_started == true) {
        responder_halt();
        is_responder_started = false;
    } else {
        LOG("Can't stop mdns responder; responder not started");
    }
    if (is_querier_started == true) {
        query_halt();
        is_querier_started = false;
    } else {
        LOG("Can't stop mdns querier; querier not started");
    }

    mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)net_status_changed_delegate, NULL );
    mico_system_notify_register( mico_notify_SYS_WILL_POWER_OFF, (void *)system_will_poweroff_delegate, NULL );

}


int mdns_socket_mcast(uint32_t mcast_addr, uint16_t port)
{
	int sock;
	int yes = 1;
	unsigned char ttl = 255;
	//uint8_t mcast_mac[6];
	struct sockaddr_in in_addr;
	struct ip_mreq mc;
	memset(&in_addr, 0, sizeof(in_addr));
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = port;
	in_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	//wifi_get_ipv4_multicast_mac(ntohl(mcast_addr), mcast_mac);
	//wifi_add_mcast_filter(mcast_mac);
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		LOG("error: could not open multicast socket\n");
		return ERR_MDNS_FSOC;
	}
#ifdef SO_REUSEPORT
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&yes,
		       sizeof(yes)) < 0) {
		LOG("error: failed to set SO_REUSEPORT option\n");
		close(sock);
		return ERR_MDNS_FREUSE;
	}
#endif
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes)) < 0) {
        LOG("error: failed to set SO_REUSEADDR option\n");
        close(sock);
        return ERR_MDNS_FREUSE;
	}

	if (bind(sock, (struct sockaddr *)&in_addr, sizeof(in_addr))) {
		close(sock);
		return ERR_MDNS_FBIND;
	}

    /* join multicast group */
	mc.imr_multiaddr.s_addr = mcast_addr;
	mc.imr_interface.s_addr = htonl(INADDR_ANY);
	setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc, sizeof(mc));

	/* set other IP-level options */
	setsockopt(sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl));

	return sock;
}

#ifdef CONFIG_IPV6
int mdns6_socket_mcast(ip6_addr_t mdns_ipv6_addr, uint16_t port)
{
	int sock;
	int yes = 1;
	uint8_t mcast_mac[MLAN_MAC_ADDR_LENGTH];
	struct sockaddr_in6 in_addr;
	struct ip_mreq mc;
	uint32_t mcast_addr = mdns_ipv6_addr.addr[3];

	memset(&in_addr, 0, sizeof(in_addr));
	in_addr.sin6_family = AF_INET6;
	in_addr.sin6_port = port;
	memcpy(in_addr.sin6_addr.s6_addr, IP6_ADDR_ANY,
		sizeof(struct in6_addr));
	wifi_get_ipv6_multicast_mac(ntohl(mcast_addr), mcast_mac);
	wifi_add_mcast_filter(mcast_mac);

	sock = socket(AF_INET6, SOCK_DGRAM, 0);
	if (sock < 0) {
		LOG("error: could not open multicast socket\n");
		return -WM_E_MDNS_FSOC;
	}
#ifdef SO_REUSEPORT
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&yes,
		       sizeof(yes)) < 0) {
		LOG("error: failed to set SO_REUSEPORT option\n");
		close(sock);
		return -WM_E_MDNS_FREUSE;
	}
#endif

	if (bind(sock, (struct sockaddr *)&in_addr, sizeof(in_addr))) {
		close(sock);
		return -WM_E_MDNS_FBIND;
	}
	/* join multicast group */
	mc.imr_multiaddr.s_addr = mcast_addr;
	mc.imr_interface.s_addr = htonl(INADDR_ANY);
	if (setsockopt(sock, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&mc,
		       sizeof(mc)) < 0) {
		LOG("error: failed to join multicast group");
		close(sock);
		return -WM_E_MDNS_FMCAST_JOIN;
	}

	if (net_set_mcast_interface() < 0) {
		LOG("Setting mcast interface failed: %d",
		net_get_sock_error(sock));
	}
	return sock;
}
#endif /* CONFIG_IPV6 */

#ifdef CONFIG_DNSSD_QUERY
bool dns_socket_used;
int dns_socket_ucast(uint16_t port)
{
	int sock;
	int yes = 1;
	struct sockaddr_in in_addr;

	if (dns_socket_used) {
		LOG("error: unicast-socket is already used.");
		return -WM_E_MDNS_INUSE;
	}

	memset(&in_addr, 0, sizeof(in_addr));
	in_addr.sin_family = AF_INET;
	in_addr.sin_port = port;
	in_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) {
		LOG("error: could not open unicast socket\n");
		return -WM_E_MDNS_FSOC;
	}
#ifdef SO_REUSEPORT
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char *)&yes,
		       sizeof(yes)) < 0) {
		LOG("error: failed to set SO_REUSEPORT option\n");
		close(sock);
		return -WM_E_MDNS_FREUSE;
	}
#endif
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));

	if (bind(sock, (struct sockaddr *)&in_addr, sizeof(in_addr))) {
		close(sock);
		return -WM_E_MDNS_FBIND;
	}
	dns_socket_used = true;
	return sock;
}

int dns_socket_close(int s)
{
	dns_socket_used = false;
	return close(s);
}
#endif

mico_queue_t mdns_ctrl_responder_queue[2] = {NULL, NULL};
int mdns_ctrl_socks[2] = {-1, -1};


int mdns_socket_loopback(uint8_t id, mico_queue_t **queue)
{
    int ret = 0;
    uint32_t msg_size;

    if( mdns_ctrl_socks[id] < 0) {
#if MDNS_QUERY_API
        msg_size = (id == MDNS_CTRL_RESPONDER)? sizeof(mdns_ctrl_data) : sizeof(query_ctrl_msg);
#else
        msg_size = sizeof(mdns_ctrl_data);
#endif
        ret = mico_rtos_init_queue( &mdns_ctrl_responder_queue[id], "CTRL_RESPONDER", msg_size, 8);
        if( ret != 0 ) return -1;
        mdns_ctrl_socks[id] = mico_rtos_init_event_fd(mdns_ctrl_responder_queue[id]);
    }

    if ( queue != NULL ) {
        *queue = &mdns_ctrl_responder_queue[id];
    }

    return mdns_ctrl_socks[id];

//	int s, one = 1, addr_len, ret;
//	struct sockaddr_in addr;
//
//	s = socket(PF_INET, SOCK_DGRAM, 0);
//	if (s < 0) {
//		LOG("error: Failed to create loopback socket.");
//		return ERR_MDNS_FSOC;
//	}
//
//	if (listen) {
//		/* bind loopback socket */
//		memset((char *)&addr, 0, sizeof(addr));
//		addr.sin_family = PF_INET;
//		addr.sin_port = port;
//		addr.sin_addr.s_addr = inet_addr("127.0.0.1");
//		addr_len = sizeof(struct sockaddr_in);
//		setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one,
//			   sizeof(one));
//		ret = bind(s, (struct sockaddr *)&addr, addr_len);
//		if (ret < 0) {
//			LOG("Failed to bind control socket");
//			return ERR_MDNS_FBIND;
//		}
//	}
//	return s;
}


int mdns_socket_close(int *s)
{
    if(*s == mdns_ctrl_socks[MDNS_CTRL_RESPONDER])
        mico_rtos_deinit_queue( &mdns_ctrl_responder_queue[MDNS_CTRL_RESPONDER] );

    if(*s == mdns_ctrl_socks[MDNS_CTRL_QUERIER])
        mico_rtos_deinit_queue( &mdns_ctrl_responder_queue[MDNS_CTRL_QUERIER] );

	close(*s);
	*s = -1;
	return 0;
}

