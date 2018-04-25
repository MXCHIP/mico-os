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

#include <stdlib.h>
#include "os.h"
#include "enrollee.h"
#include "utils.h"
#include "zconfig_utils.h"
#include "passwd.h"
#include "platform/platform.h"
#include "awss_notify.h"
#include "json_parser.h"
#include "awss_cmp.h"
#include "awss_wifimgr.h"
#include "work_queue.h"
#include "awss_main.h"

#if defined(__cplusplus)  /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

#define AWSS_NOTIFY_PORT     (5683)
#define AWSS_NOTIFY_HOST     "255.255.255.255"
#define AWSS_DEV_NOTIFY_FMT  "{\"id\":\"%u\",\"version\":\"1.0\",\"method\":\"%s\",\"params\":{%s}}"

static unsigned char g_notify_id;
static unsigned short g_notify_msg_id;
char awss_notify_resp[AWSS_NOTIFY_MAX] = {0};

int awss_connectap_notify();
int awss_devinfo_notify();

static struct work_struct awss_connectap_notify_work = {
    .func = (work_func_t)&awss_connectap_notify,
    .prio = 1, /* smaller digit means higher priority */
    .name = "connectap",
};

static struct work_struct awss_devinfo_notify_work = {
    .func = (work_func_t)&awss_devinfo_notify,
    .prio = 1, /* smaller digit means higher priority */
    .name = "devinfo",
};

int awss_devinfo_notify_resp(void *context, int result, void *userdata, void *remote, void *message)
{
    awss_debug("%s\n", __func__);

    if (message == NULL)
        return -1;

    int len = 0;
    if (awss_cmp_get_coap_code(message) >= 0x60)
        return 0;

    if (awss_cmp_get_coap_payload(message, &len) == NULL || len > 40 || len == 0)
        return 0;

    awss_notify_resp[AWSS_NOTIFY_DEV_RAND] = 1;
    return 0;
}

/*
 * {
 *  "id": "123",
 *  "code": 200,
 *  "data": {}
 * }
 */
int awss_connectap_notify_resp(void *context, int result, void *userdata, void *remote, void *message)
{
    awss_debug("%s\n", __func__);

    if (message == NULL)
        return -1;

    int len = 0;
    if (awss_cmp_get_coap_code(message) >= 0x60)
        return 0;

    if (awss_cmp_get_coap_payload(message, &len) == NULL || len > 40 || len == 0)
        return 0;

    awss_notify_resp[AWSS_NOTIFY_DEV_TOKEN] = 1;
    return 0;
}

int awss_notify_dev_info(int type, int count)
{
    char *buf = NULL;
    char *dev_info = NULL;
    int i;

    do {
        buf = os_zalloc(DEV_INFO_LEN_MAX);
        dev_info = os_zalloc(DEV_INFO_LEN_MAX);
        if (buf == NULL || dev_info == NULL)
            break;

        platform_netaddr_t notify_sa = {0};

        memcpy(notify_sa.host, AWSS_NOTIFY_HOST, strlen(AWSS_NOTIFY_HOST));
        notify_sa.port = AWSS_NOTIFY_PORT;

        awss_build_dev_info(type, dev_info, DEV_INFO_LEN_MAX);
        char *method = (type == AWSS_NOTIFY_DEV_TOKEN ? METHOD_DEV_INFO_NOTIFY : METHOD_AWSS_DEV_INFO_NOTIFY);
        char *topic = (type == AWSS_NOTIFY_DEV_TOKEN ? TOPIC_NOTIFY : TOPIC_AWSS_NOTIFY);
        snprintf(buf, DEV_INFO_LEN_MAX - 1, AWSS_DEV_NOTIFY_FMT, ++ g_notify_id, method, dev_info);
        void *cb = (type == AWSS_NOTIFY_DEV_TOKEN ? awss_connectap_notify_resp : awss_devinfo_notify_resp);

        awss_debug("topic:%s, %s\n", topic, buf);
        for (i = 0; i < count; i ++) {
            awss_cmp_coap_send(buf, strlen(buf), &notify_sa, topic, cb, &g_notify_msg_id);
            if (count > 1) os_msleep(200 + 100 * i);
            if (awss_notify_resp[type])
                break;
        }
    } while (0);

    if (buf) os_free(buf);
    if (dev_info) os_free(dev_info);

    return awss_notify_resp[type];
}

#define AWSS_NOTIFY_CNT_MAX    (50)

int awss_connectap_notify_stop()
{
    cancel_work(&awss_connectap_notify_work);
    return 0;
}

static int online_get_device_info(void *ctx, void *resource, void *remote, void *request, char is_mcast)
{
    char *buf = NULL;
    char *dev_info = NULL;
    int len = 0, id_len = 0;
    char *msg = NULL, *id = NULL;
    char req_msg_id[MSG_REQ_ID_LEN];

    extern char awss_report_token_flag;
    if (awss_report_token_flag == 0)
        goto DEV_INFO_ERR;

    buf = os_zalloc(DEV_INFO_LEN_MAX);
    if (buf == NULL)
        goto DEV_INFO_ERR;
    dev_info = os_zalloc(DEV_INFO_LEN_MAX);
    if (dev_info == NULL)
        goto DEV_INFO_ERR;
    msg = awss_cmp_get_coap_payload(request, &len);
    id = json_get_value_by_name(msg, len, "id", &id_len, 0);
    memset(req_msg_id, 0, sizeof(req_msg_id));
    memcpy(req_msg_id, id, id_len);

    produce_random(aes_random, sizeof(aes_random));
    awss_report_token();

    awss_build_dev_info(AWSS_NOTIFY_DEV_TOKEN, buf, DEV_INFO_LEN_MAX);
    snprintf(dev_info, DEV_INFO_LEN_MAX - 1, "{%s}", buf);
    awss_debug("dev_info:%s\r\n", dev_info);
    memset(buf, 0x00, DEV_INFO_LEN_MAX);
    snprintf(buf, DEV_INFO_LEN_MAX - 1, AWSS_ACK_FMT, req_msg_id, 200, dev_info);
    os_free(dev_info);

    awss_debug("sending message to app: %s", buf);
    char topic[TOPIC_LEN_MAX] = {0};
    if (is_mcast)
        awss_build_topic((const char *)TOPIC_GETDEVICEINFO_MCAST, topic, TOPIC_LEN_MAX);
    else
        awss_build_topic((const char *)TOPIC_GETDEVICEINFO_UCAST, topic, TOPIC_LEN_MAX);
    if (0 > awss_cmp_coap_send_resp(buf, strlen(buf), remote, topic, request)) {
        awss_debug("sending failed.");
    }
    os_free(buf);
    return 0;

DEV_INFO_ERR:
    if (buf) os_free(buf);
    if (dev_info) os_free(dev_info);
    return -1;
}

int online_mcast_get_device_info(void *ctx, void *resource, void *remote, void *request)
{
    return online_get_device_info(ctx, resource, remote, request, 1);
}

int online_ucast_get_device_info(void *ctx, void *resource, void *remote, void *request)
{
    return online_get_device_info(ctx, resource, remote, request, 0);
}

int awss_connectap_notify()
{
    static int connectap_interval = 300; 
    static char connectap_cnt = 0;

    do {
        if (awss_notify_resp[AWSS_NOTIFY_DEV_TOKEN] != 0)
            break;

        unsigned char i = 0;
        for (i = 0; i < RANDOM_MAX_LEN; i ++)
            if (aes_random[i] != 0x00)
                break;

        if (i >= RANDOM_MAX_LEN)
            produce_random(aes_random, sizeof(aes_random));

        awss_notify_dev_info(AWSS_NOTIFY_DEV_TOKEN, 1);

        connectap_interval += 100;
        if (connectap_cnt ++ < AWSS_NOTIFY_CNT_MAX &&
            awss_notify_resp[AWSS_NOTIFY_DEV_TOKEN] == 0) {
            queue_delayed_work(&awss_connectap_notify_work, connectap_interval);
            return 0;
        }
    } while (0);

    awss_notify_resp[AWSS_NOTIFY_DEV_TOKEN] = 0;
    connectap_interval = 0; 
    connectap_cnt = 0;
    return 1;
}

int awss_devinfo_notify_stop()
{
    cancel_work(&awss_devinfo_notify_work);
    return 0;
}

int awss_devinfo_notify()
{
    static int devinfo_interval = 0; 
    static char devinfo_cnt = 0;

    do {
        if (awss_notify_resp[AWSS_NOTIFY_DEV_RAND] != 0)
            break;

        awss_notify_dev_info(AWSS_NOTIFY_DEV_RAND, 1);

        devinfo_interval += 100;
        if (devinfo_cnt ++ < AWSS_NOTIFY_CNT_MAX &&
           awss_notify_resp[AWSS_NOTIFY_DEV_RAND] == 0) {
           queue_delayed_work(&awss_devinfo_notify_work, devinfo_interval);
           return 0;
        }
    } while (0);

    awss_notify_resp[AWSS_NOTIFY_DEV_RAND] = 0;
    devinfo_interval = 0; 
    devinfo_cnt = 0;
    return 1;
}

#if defined(__cplusplus)  /* If this is a C++ compiler, use C linkage */
}
#endif
