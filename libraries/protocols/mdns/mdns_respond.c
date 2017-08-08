/**
 ******************************************************************************
 * @file    mdns_port.c
 * @author  William Xu
 * @version V1.0.0
 * @date    8-Aug-2017
 * @brief   All functions in this file are implementation of mDNS APIs exposed 
 *          to the application. All functions sends control message to mDNS responder
 *          thread.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include <errno.h>
#include "mdns_port.h"
#include "mdns_private.h"


/* Function first validates the service and then sends control message to
 * mdns responder thread for announcing service on the specified n/w interface.
 */
int mdns_announce_service(struct mdns_service *service, netif_t iface)
{
	int ret;
	struct mdns_service *service_arr[2];
	mdns_ctrl_data msg;

	if (service == NULL)
		return ERR_MDNS_INVAL;

	service_arr[0] = service;
	service_arr[1] = NULL;

	ret = mdns_verify_service(service_arr, iface);
	if (ret != MICO_SUCCESS)
		return ret;

	msg.cmd = MDNS_CTRL_ANNOUNCE_SERVICE;
	msg.iface = iface;
	msg.service = service;
	ret = mdns_send_ctrl_msg(MDNS_CTRL_RESPONDER, &msg);

	return ret;
}

/* Function sends control message to mdns responder thread for deannouncing
 * service over the given n/w interface. */
int mdns_deannounce_service(struct mdns_service *service, netif_t iface)
{
	int ret;
	mdns_ctrl_data msg;

	if (service == NULL)
		return ERR_MDNS_INVAL;

	msg.cmd = MDNS_CTRL_DEANNOUNCE_SERVICE;
	msg.iface = iface;
	msg.service = service;
	ret = mdns_send_ctrl_msg(MDNS_CTRL_RESPONDER, &msg);

	return ret;
}

/* Function validates set of mdns services and sends a control message to mdns
 * responder thread to announce all services simultaneously on the specified
 * n/w interface. */
int mdns_announce_service_arr(struct mdns_service *services[], netif_t iface)
{
	int ret;
	mdns_ctrl_data msg;

	ret = mdns_verify_service(services, iface);
	if (ret != MICO_SUCCESS)
		return ret;

	msg.cmd = MDNS_CTRL_ANNOUNCE_SERVICE_ARR;
	msg.iface = iface;
	msg.service = services;
	ret = mdns_send_ctrl_msg(MDNS_CTRL_RESPONDER, &msg);
	return ret;
}

/* Function sends out a control message to mdns responder thread for
 * deannouncing given set of services on the specified n/w interface. */
int mdns_deannounce_service_arr(struct mdns_service *services[], netif_t iface)
{
	int ret;
	mdns_ctrl_data msg;

	msg.cmd = MDNS_CTRL_DEANNOUNCE_SERVICE_ARR;
	msg.iface = iface;
	msg.service = services;
	ret = mdns_send_ctrl_msg(MDNS_CTRL_RESPONDER, &msg);
	return ret;
}

/* Function sends a control message to mdns responder thread to deannounce all
 * mdns services previously announced on a specified n/w interface. */
int mdns_deannounce_service_all(netif_t iface)
{
	int ret;
	mdns_ctrl_data msg;

	if (iface == INTERFACE_NONE)
		return ERR_MDNS_INVAL;

	msg.cmd = MDNS_CTRL_DEANNOUNE_ALL;
	msg.iface = iface;
	msg.service = NULL;
	ret = mdns_send_ctrl_msg(MDNS_CTRL_RESPONDER, &msg);
	return ret;
}

