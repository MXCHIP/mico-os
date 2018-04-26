/*
 *  Copyright (C) 2008-2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

/** dhcp-server-main.c: CLI based APIs for the DHCP Server
 */
#include <string.h>

#include "mico_rtos.h"

#include "dhcp-priv.h"

static mico_thread_t dhcpd_thread;
static bool dhcpd_running;   
/*
 * API
 */

int dhcp_server_start(void *intrfc_handle)
{
	int ret;

	dhcp_d("DHCP server start request ");
	if (dhcpd_running || dhcp_server_init(intrfc_handle))
		return -1;

	ret = mico_rtos_create_thread(&dhcpd_thread, MICO_APPLICATION_PRIORITY, "dhcp-server", 
				dhcp_server, 1024, 0);
	if (ret) {
		dhcp_free_allocations();
		return -1;
	}

	dhcpd_running = 1;
	return 0;
}

void dhcp_server_stop(void)
{
	dhcp_d("DHCP server stop request");
	if (dhcpd_running) {
		if (dhcp_send_halt() != 0) {
			dhcp_w("failed to send halt to DHCP thread");
			return;
		}
		if (mico_rtos_delete_thread(&dhcpd_thread) != 0)
			dhcp_w("failed to delete thread");
		dhcpd_running = 0;
	} else {
		dhcp_w("server not dhcpd_running.");
	}
}

