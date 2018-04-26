/*
 * WPA Supplicant - Layer2 packet handling example with dummy functions
 * Copyright (c) 2003-2005, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 *
 * This file can be used as a starting point for layer2 packet implementation.
 */

#include "includes.h"

#include "common.h"
#include "eloop.h"
#include "l2_packet.h"
#include "socket.h"
#include "mac.h"

#include "uart_pub.h"

struct l2_packet_data {
	char ifname[17];
	u8 own_addr[ETH_ALEN];
	void (*rx_callback)(void *ctx, const u8 *src_addr,
			    const u8 *buf, size_t len);
	void *rx_callback_ctx;
	int l2_hdr; /* whether to include layer 2 (Ethernet) header data
		     * buffers */
	int fd;
};


int l2_packet_get_own_addr(struct l2_packet_data *l2, u8 *addr)
{
	os_memcpy(addr, l2->own_addr, ETH_ALEN);
	return 0;
}


int l2_packet_send(struct l2_packet_data *l2, const u8 *dst_addr, u16 proto,
		   const u8 *buf, size_t len)
{
#define TMP_BUF_LEN     512

	int data_len, ret = 0;
	struct l2_ethhdr *eth;
	unsigned char *data_buf;

	data_len = len + sizeof(struct l2_ethhdr);
	data_buf = os_zalloc(data_len);
	if(data_buf == NULL){
		ret = -1;
		goto send_exit;
	}

	eth = (struct l2_ethhdr *) data_buf;
	os_memcpy(eth->h_dest, dst_addr, ETH_ALEN);
	os_memcpy(eth->h_source, l2->own_addr, ETH_ALEN);
	eth->h_proto = host_to_be16(proto);
	os_memcpy(eth+1, buf, len);
	
	bk_send(l2->fd, data_buf, data_len, HOSTAPD_DATA);

send_exit:	
	if(data_buf){
		os_free(data_buf);
	}
	return ret;
}


static void l2_packet_receive(int sock, void *eloop_ctx, void *sock_ctx)
{
#define TMP_BUF_LEN     512

	int len;
	unsigned char *buf;
	struct l2_ethhdr *hdr;
	struct l2_packet_data *l2 = eloop_ctx;

	buf = os_malloc(TMP_BUF_LEN);
	ASSERT(buf);
	
	len = bk_recv(sock, buf, TMP_BUF_LEN, 0);
	if (len < 0) {
		wpa_printf(MSG_ERROR, "recv: %s", strerror(errno));
		goto recv_exit;
	}
	
	hdr = (struct l2_ethhdr *) buf;
	l2->rx_callback(l2->rx_callback_ctx, 
						hdr->h_source, 
						buf + sizeof(struct l2_ethhdr), 
						len - sizeof(struct l2_ethhdr));

recv_exit:
	if(buf)
	{
		os_free(buf);
	}
	
	return;
}


struct l2_packet_data * l2_packet_init(
	const char *ifname, const u8 *own_addr, unsigned short protocol,
	void (*rx_callback)(void *ctx, const u8 *src_addr,
			    const u8 *buf, size_t len),
	void *rx_callback_ctx, int l2_hdr)
{
	struct l2_packet_data *l2;

	l2 = os_zalloc(sizeof(struct l2_packet_data));
	if (l2 == NULL)
		return NULL;
	os_strlcpy(l2->ifname, ifname, sizeof(l2->ifname));
	os_memcpy(l2->own_addr, own_addr, ETH_ALEN);
	l2->rx_callback = rx_callback;
	l2->rx_callback_ctx = rx_callback_ctx;
	l2->l2_hdr = l2_hdr;

	/*
	 * TODO: open connection for receiving frames
	 */
	l2->fd = bk_socket(PF_PACKET, SOCK_RAW, protocol);
	if (l2->fd >= 0)
		eloop_register_read_sock(l2->fd, l2_packet_receive, l2, NULL);

	return l2;
}


struct l2_packet_data * l2_packet_init_bridge(
	const char *br_ifname, const char *ifname, const u8 *own_addr,
	unsigned short protocol,
	void (*rx_callback)(void *ctx, const u8 *src_addr,
			    const u8 *buf, size_t len),
	void *rx_callback_ctx, int l2_hdr)
{
	return l2_packet_init(br_ifname, own_addr, protocol, rx_callback,
			      rx_callback_ctx, l2_hdr);
}


void l2_packet_deinit(struct l2_packet_data *l2)
{
	if (l2 == NULL)
		return;

	if (l2->fd >= 0) {
		eloop_unregister_read_sock(l2->fd);
		/* TODO: close connection */
		bk_close(l2->fd);
	}
		
	os_free(l2);
}


int l2_packet_get_ip_addr(struct l2_packet_data *l2, char *buf, size_t len)
{
	/* TODO: get interface IP address */
	return -1;
}


void l2_packet_notify_auth_start(struct l2_packet_data *l2)
{
	/* This function can be left empty */
}


int l2_packet_set_packet_filter(struct l2_packet_data *l2,
				enum l2_packet_filter_type type)
{
	return -1;
}
