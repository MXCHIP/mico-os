/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "iot_import.h"
#include "iot_export.h"
#include "aos/log.h"
#include "aos/yloop.h"
#include "aos/network.h"
#include <netmgr.h>
#include <aos/kernel.h>
#include <k_err.h>
#include <netmgr.h>
#include <aos/aos.h>
#include <aos/cli.h>
#include <aos/cloud.h>
#include "soc_init.h"
#include "k_types.h"
#include "lite-log.h"
#if defined(MQTT_ID2_AUTH) && defined(TEST_ID2_DAILY)

#else
    #define PRODUCT_KEY             "GJ2uoVqx7ka"
    #define DEVICE_NAME             "test_dev_st_01"
    #define DEVICE_SECRET           "G0trudPo6lFDnpQ94T9M5viHLQrnTpxH"

#endif

// These are pre-defined topics
#define TOPIC_UPDATE            "/"PRODUCT_KEY"/"DEVICE_NAME"/update"
#define TOPIC_ERROR             "/"PRODUCT_KEY"/"DEVICE_NAME"/update/error"
#define TOPIC_GET               "/"PRODUCT_KEY"/"DEVICE_NAME"/get"
#define TOPIC_DATA              "/"PRODUCT_KEY"/"DEVICE_NAME"/data"


#define MSG_LEN_MAX             (2048)

#define EXAMPLE_TRACE(fmt, args...)  \
    do { \
        printf("%s|%03d :: ", __func__, __LINE__); \
        printf(fmt, ##args); \
        printf("%s", "\r\n"); \
    } while(0)

static int is_demo_started = 0;
void *pclient;
static uint32_t    send_cnt = 0;
static uint32_t    recv_cnt = 0;
static char msg_pub[MSG_LEN_MAX];
static int running;


int mqtt_client_example(void);

static void wifi_service_event(input_event_t *event, void *priv_data) {
    LOG("wifi_service_event!");
    if (event->type != EV_WIFI) {
        return;
    }

    if (event->code != CODE_WIFI_ON_GOT_IP) {
        return;
    }

    if (is_demo_started == 0) {
        is_demo_started = 1;
        mqtt_client_example();
    }
}


void event_handle_mqtt(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    uintptr_t packet_id = (uintptr_t)msg->msg;
    iotx_mqtt_topic_info_pt topic_info = (iotx_mqtt_topic_info_pt)msg->msg;

    switch (msg->event_type) {
        case IOTX_MQTT_EVENT_UNDEF:
            EXAMPLE_TRACE("undefined event occur.");
            break;

        case IOTX_MQTT_EVENT_DISCONNECT:
            EXAMPLE_TRACE("MQTT disconnect.");
            break;

        case IOTX_MQTT_EVENT_RECONNECT:
            EXAMPLE_TRACE("MQTT reconnect.");
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_SUCCESS:
            EXAMPLE_TRACE("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_TIMEOUT:
            EXAMPLE_TRACE("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_SUBCRIBE_NACK:
            EXAMPLE_TRACE("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_SUCCESS:
            EXAMPLE_TRACE("unsubscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
            EXAMPLE_TRACE("unsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_UNSUBCRIBE_NACK:
            EXAMPLE_TRACE("unsubscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_SUCCESS:
            EXAMPLE_TRACE("publish success, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_TIMEOUT:
            EXAMPLE_TRACE("publish timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_NACK:
            EXAMPLE_TRACE("publish nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case IOTX_MQTT_EVENT_PUBLISH_RECVEIVED:
            EXAMPLE_TRACE("topic message arrived but without any related handle: topic=%.*s, topic_msg=%.*s",
                          topic_info->topic_len,
                          topic_info->ptopic,
                          topic_info->payload_len,
                          topic_info->payload);
            break;

        default:
            EXAMPLE_TRACE("Should NOT arrive here.");
            break;
    }
}

static void publish_topic(iotx_mqtt_topic_info_t *topic_msg)
{
    int ret = 0;

    topic_msg->qos = IOTX_MQTT_QOS1;
    topic_msg->retain = 0;
    topic_msg->dup = 0;
    topic_msg->payload = (void *)msg_pub;
    topic_msg->payload_len = strlen(msg_pub);

    ret = IOT_MQTT_Publish(pclient, TOPIC_UPDATE, topic_msg);
    if (ret < 0) {
        running = 0;
        EXAMPLE_TRACE("Publish failed\n");
    } else {
        send_cnt++;
#ifdef MQTT_ID2_CRYPTO
        EXAMPLE_TRACE("publish count=%u, publish topic msg='0x%02x%02x%02x%02x'...",
                      (uint32_t)send_cnt,
                      msg_pub[0], msg_pub[1], msg_pub[2], msg_pub[3]
                     );
#else
        EXAMPLE_TRACE("publish count=%u, topic msg=%s\n", (uint32_t)send_cnt, msg_pub);
#endif
    }
}

static void _demo_message_arrive(void *pcontext, void *pclient, iotx_mqtt_event_msg_pt msg)
{
    iotx_mqtt_topic_info_pt ptopic_info = (iotx_mqtt_topic_info_pt) msg->msg;
    iotx_mqtt_topic_info_t topic_msg;

    recv_cnt++;
    memset(msg_pub, 0, sizeof(msg_pub));
    strncpy(msg_pub, ptopic_info->payload, ptopic_info->payload_len);
    EXAMPLE_TRACE("sub recv count: %u\n", recv_cnt);
    publish_topic(&topic_msg);
}

int mqtt_client_example(void)
{
    int rc = 0;
    iotx_conn_info_pt pconn_info;
    iotx_mqtt_param_t mqtt_params;
    char *msg_buf = NULL, *msg_readbuf = NULL;

    if (NULL == (msg_buf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }

    if (NULL == (msg_readbuf = (char *)HAL_Malloc(MSG_LEN_MAX))) {
        EXAMPLE_TRACE("not enough memory");
        rc = -1;
        goto do_exit;
    }
    
    /* Device AUTH */
    if (0 != IOT_SetupConnInfo(PRODUCT_KEY, DEVICE_NAME, DEVICE_SECRET, (void **)&pconn_info)) {
        EXAMPLE_TRACE("AUTH request failed!");
        rc = -1;
        goto do_exit;
    }

    /* Initialize MQTT parameter */
    memset(&mqtt_params, 0x0, sizeof(mqtt_params));

    mqtt_params.port = pconn_info->port;
    mqtt_params.host = pconn_info->host_name;
    mqtt_params.client_id = pconn_info->client_id;
    mqtt_params.username = pconn_info->username;
    mqtt_params.password = pconn_info->password;
    mqtt_params.pub_key = pconn_info->pub_key;

    mqtt_params.request_timeout_ms = 5000;
    mqtt_params.clean_session = 1;
    mqtt_params.keepalive_interval_ms = 60000;
    mqtt_params.pread_buf = msg_readbuf;
    mqtt_params.read_buf_size = MSG_LEN_MAX;
    mqtt_params.pwrite_buf = msg_buf;
    mqtt_params.write_buf_size = MSG_LEN_MAX;

    mqtt_params.handle_event.h_fp = event_handle_mqtt;
    mqtt_params.handle_event.pcontext = NULL;


    /* Construct a MQTT client with specify parameter */
    pclient = IOT_MQTT_Construct(&mqtt_params);
    if (NULL == pclient) {
        EXAMPLE_TRACE("MQTT construct failed");
        rc = -1;
        goto do_exit;
    }

    running = 1;
    /* Subscribe the specific topic */
    rc = IOT_MQTT_Subscribe(pclient, TOPIC_GET, IOTX_MQTT_QOS1, _demo_message_arrive, NULL);
    if (rc < 0) {
        IOT_MQTT_Destroy(&pclient);
        EXAMPLE_TRACE("IOT_MQTT_Subscribe() failed, rc = %d", rc);
        rc = -1;
        goto do_exit;
    }

    HAL_SleepMs(1000);

    while (running) {
        /* handle the MQTT packet received from TCP or SSL connection */
        if (0 != IOT_MQTT_Yield(pclient, 200)) {
            EXAMPLE_TRACE("IOT_MQTT_Yield failed!");
            break;
        }
    }

    IOT_MQTT_Unsubscribe(pclient, TOPIC_GET);

    HAL_SleepMs(200);

    IOT_MQTT_Destroy(&pclient);

do_exit:
    if (NULL != msg_buf) {
        HAL_Free(msg_buf);
    }

    if (NULL != msg_readbuf) {
        HAL_Free(msg_readbuf);
    }
    is_demo_started = 0;
    EXAMPLE_TRACE("mqtt example loop end!");

    HAL_SleepMs(500);
    SPI_WIFI_DeInit();
    SPI_WIFI_Init();
    aos_post_event(EV_WIFI, CODE_WIFI_CMD_RECONNECT, 0);

    return rc;
}
#ifdef CSP_LINUXHOST
static void mqtt_main( void *data )
{
    mqtt_client_example();
    aos_task_exit(0);
}
#endif
int application_start(int argc, char *argv[])
{
    aos_set_log_level(AOS_LL_DEBUG);

    aos_register_event_filter(EV_WIFI, wifi_service_event, NULL);

    netmgr_init();
    netmgr_start(false);
    LITE_openlog("mqttest");
    LITE_set_loglevel(LOG_WARNING_LEVEL);

#ifdef CSP_LINUXHOST
    int ret = aos_task_new("mqtttask", mqtt_main, 0, 1024*10);
    if (ret != SUCCESS_RETURN) {
        printf("Error: Failed to create cli thread: %d\r\n", ret);
    }
#endif
    aos_loop_run();
    return 0;
}


