/*
 * Copyright (c) 2014-2016 Alibaba Group. All rights reserved.
 *
 * Alibaba Group retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Alibaba Group., you are not
 * authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and
 * compilation into object code), and you must immediately destroy or
 * return to Alibaba Group all copies of this computer program.  If you
 * are licensed by Alibaba Group, your rights to utilize this computer
 * program are limited by the terms of that license.  To obtain a license,
 * please contact Alibaba Group.
 *
 * This computer program contains trade secrets owned by Alibaba Group.
 * and, unless unauthorized by Alibaba Group in writing, you agree to
 * maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related
 * information to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
 * Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NONINFRINGEMENT.
 */

#include <stdio.h>
#include "os.h"
#include "CoAPExport.h"
#include "CoAPServer.h"
#include "awss_cmp.h"
#include "awss_wifimgr.h"
#include "awss_notify.h"

#if defined(__cplusplus)  /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

static void *g_coap_ctx = NULL;

unsigned char awss_cmp_get_coap_code(void *request)
{
    if (request == NULL)
        return 0x60;
    struct CoAPMessage *msg = (struct CoAPMessage *)request;
    return msg->header.code;
}

char *awss_cmp_get_coap_payload(void *request, int *payload_len)
{
    if (request == NULL)
        return NULL;

    struct CoAPMessage *msg = (struct CoAPMessage *)request;
    if (payload_len)
        *payload_len = msg->payloadlen;
    return (char *)msg->payload;
}

int awss_cmp_coap_register_cb(char *topic, void* cb)
{
    if (g_coap_ctx == NULL)
        g_coap_ctx = CoAPServer_init();

    if (g_coap_ctx == NULL)
        return -1;
    if (topic == NULL)
        return -1;

    CoAPServer_register(g_coap_ctx, (const char *)topic, (CoAPRecvMsgHandler)cb);
    return 0;
}

int awss_cmp_coap_loop(void *param)
{
    if (g_coap_ctx == NULL) g_coap_ctx = CoAPServer_init();
#ifndef COAP_WITH_YLOOP
    awss_debug("create thread\r\n");
    CoAPServer_loop(g_coap_ctx);
#endif
    return 0;
}

int awss_cmp_coap_send(void *buf, unsigned int len, void *sa, const char *uri, void *cb, unsigned short *msgid)
{
    if (g_coap_ctx == NULL) {
        g_coap_ctx = CoAPServer_init();
    } else {
        CoAPMessageId_cancel(g_coap_ctx, *msgid);
    }
    return CoAPServerMultiCast_send(g_coap_ctx, (NetworkAddr *)sa, uri, (unsigned char *)buf,
                                    (unsigned short)len, (CoAPSendMsgHandler)cb, msgid);
}

int awss_cmp_coap_send_resp(void *buf, unsigned int len, void *sa, const char *uri, void *req)
{
    if (g_coap_ctx == NULL) g_coap_ctx = CoAPServer_init();

    return CoAPServerResp_send(g_coap_ctx, (NetworkAddr *)sa, (unsigned char *)buf, (unsigned short)len, req, uri);
}

int awss_cmp_coap_ob_send(void *buf, unsigned int len, void *sa, const char *uri, void *cb)
{
    if (g_coap_ctx == NULL) g_coap_ctx = CoAPServer_init();

    return CoAPObsServer_notify(g_coap_ctx, uri, (unsigned char *)buf, (unsigned short)len, cb);
}

int awss_cmp_coap_deinit()
{
    if (g_coap_ctx) CoAPServer_deinit(g_coap_ctx);
    g_coap_ctx = NULL;
    return 0;
}

const struct awss_cmp_couple awss_local_couple[] = {
    {TOPIC_AWSS_SWITCHAP,            wifimgr_process_switch_ap_request},
    {TOPIC_AWSS_WIFILIST,            wifimgr_process_get_wifilist_request},
    {TOPIC_AWSS_GETDEVICEINFO_MCAST, wifimgr_process_mcast_get_device_info},
    {TOPIC_AWSS_GETDEVICEINFO_UCAST, wifimgr_process_ucast_get_device_info},
#ifndef AWSS_DISABLE_REGISTRAR
    {TOPIC_NOTIFY,                   online_connectap_monitor},
#endif
    {TOPIC_GETDEVICEINFO_MCAST,      online_mcast_get_device_info},
    {TOPIC_GETDEVICEINFO_UCAST,      online_ucast_get_device_info}
};

int awss_cmp_local_init()
{
    char topic[TOPIC_LEN_MAX] = {0};
    int i;

    for (i = 0; i < sizeof(awss_local_couple) / sizeof(awss_local_couple[0]); i ++) {
        memset(topic, 0, sizeof(topic));
        awss_build_topic(awss_local_couple[i].topic, topic, TOPIC_LEN_MAX);
        awss_cmp_coap_register_cb(topic, awss_local_couple[i].cb);
    }

    awss_cmp_coap_loop(NULL);

    return 0;
}

int awss_cmp_local_deinit()
{
    if (g_coap_ctx == NULL)
        return 0;
    awss_devinfo_notify_stop();
    awss_cmp_coap_deinit();

    return 0;
}
#if defined(__cplusplus)  /* If this is a C++ compiler, use C linkage */
}
#endif
