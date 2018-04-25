/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdlib.h>
#include "aos/aos.h"
#include "aws_lib.h"
#include "zconfig_lib.h"
#include "awss.h"
#include "os.h"
#include "enrollee.h"
#include "alink_export.h"

#define SHUB_UDP_PORT 65128

static void awss_notify(void *);

#define MODULE_NAME "awss"
static int delay_ms = 0;  /* start from 200ms */

int stopAwssConnecting = 0;
int awssFinished = 0;
void *udpFd;

extern int switchApDone;

#define RECV_BUFFER_SIZE 512
extern int wifimgrProcessRequest(
    pplatform_netaddr_t sa, char *buf, unsigned int len);
static int handleSocketPacket()
{
    char *buf;
    int len, ret = 0;
    char host[OS_IP_LEN];
    platform_netaddr_t addr = {&host[0], 0};

    buf = (char *)os_malloc(RECV_BUFFER_SIZE);
    if (!buf) {
        return 0;
    }
    memset(buf, 0, RECV_BUFFER_SIZE);
    len = os_udp_recvfrom(udpFd, buf, RECV_BUFFER_SIZE - 1, &addr);
    if (len < 0) {
        LOGE("[awss]", "shub socket recvfrom error\n");
        ret = -1;
        goto end;
    }
    buf[len] = '\0';
    wifimgrProcessRequest(&addr, buf, len);

    LOGD("[awss]", "recv msg len:%d, ip:%s, port:%d", len, addr.host, addr.port);

end:
    os_free(buf);

    return ret;
}

#define AWSS_CB_MAX 5
void *awss_cb_func[AWSS_CB_MAX] = {NULL};
int awss_register_callback(unsigned char cb_type, void *cb_func)
{
    if (cb_type >= AWSS_CB_MAX) {
        LOGE("[awss]", "Error: %s failed, wrong cb_type (%d)", __func__, cb_type);
        return -1;
    }

    if (awss_cb_func[cb_type] != NULL) {
        LOGE("[awss]", "Same type cb registered.");
    }

    awss_cb_func[cb_type] = cb_func;

    return 0;
}
AOS_EXPORT(int, awss_register_callback, unsigned char, void *);

#define HOTSPOT_TIMEOUT (2*60*10) // 10 mins
int awss_start(void)
{
    awss_set_enrollee_token("default", strlen("default"));

    char ssid[OS_MAX_SSID_LEN] = { 0 }, passwd[OS_MAX_PASSWD_LEN] = { 0 };
    enum AWSS_AUTH_TYPE auth = AWSS_AUTH_TYPE_INVALID;
    enum AWSS_ENC_TYPE encry = AWSS_ENC_TYPE_INVALID;
    uint8_t bssid[OS_ETH_ALEN] = { 0 };
    uint8_t channel = 0;
    int ret;

    LOGI(MODULE_NAME, "awss version: %s", zconfig_lib_version());

    /* these params is useless, keep it for compatible reason */
    aws_start(NULL, NULL, NULL, NULL);

    ret = aws_get_ssid_passwd(&ssid[0], &passwd[0], &bssid[0],
                              (char *)&auth, (char *)&encry, &channel);
    if (!ret) {
        LOGW(MODULE_NAME, "awss timeout!");
    }

    aws_destroy();

    int awssNotifyNeeded = 1;
    uint32_t startAwssConnectingTimestamp = aos_now() / 1000000;
    int tryCount = 0;
    do {
        if (stopAwssConnecting) {
            break;
        }
        if (strcmp(ssid, DEFAULT_SSID) == 0) {
            uint32_t now = aos_now() / 1000000;
            if ((0 != os_awss_get_connect_default_ssid_timeout_interval_ms()) &&
                (now - startAwssConnectingTimestamp > os_awss_get_connect_default_ssid_timeout_interval_ms())) {
                break;
            }
            awssNotifyNeeded = 0;
        }

        if (ssid[0]) {
            ret = os_awss_connect_ap(WLAN_CONNECTION_TIMEOUT_MS, ssid, passwd,
                                     auth, encry, bssid, channel);

            if (ret == 0 && strcmp(ssid, DEFAULT_SSID) == 0) { // hotspot mode
                int hotspotCnt = 0;
                LOGI("[awss]", "hotspot mode detected");
                if (awss_cb_func[AWSS_HOTSPOT_CONNECTED]) {
                    void (*fn)(void) = awss_cb_func[AWSS_HOTSPOT_CONNECTED];
                    fn();
                }
                udpFd = os_udp_server_create(SHUB_UDP_PORT);
                OS_ASSERT(udpFd >= 0, "shub create socket failed!\r\n");
                while (hotspotCnt < HOTSPOT_TIMEOUT) {
                    handleSocketPacket(udpFd);
                    if (switchApDone) {
                        LOGI("[awss]", "switchApDone");
                        if (awss_cb_func[AWSS_HOTSPOT_SWITCH_AP_DONE]) {
                            void (*fn)(void) = awss_cb_func[AWSS_HOTSPOT_SWITCH_AP_DONE];
                            fn();
                        }
                        break;
                    }
                    aos_msleep(500);
                    hotspotCnt++;
                }
                if (hotspotCnt >= HOTSPOT_TIMEOUT) {
                    ret = -1;
                } else {
                    ret = 0;
                }
                os_udp_close(udpFd);
            }

            if (tryCount < 9999) {
                tryCount++;
            }
            if (!ret) {
                LOGI(MODULE_NAME, "awss connect ssid:%s success", ssid);
                delay_ms = 0;
                aos_loop_schedule_work(0, awss_notify, NULL, NULL, NULL);
                goto end;
            } else {
                LOGW(MODULE_NAME, "awss connect ssid:%s passwd:%s fail", ssid, passwd);
            }
        } else {
            strncpy(ssid, DEFAULT_SSID, sizeof(ssid));
            strncpy(passwd, DEFAULT_PASSWD, sizeof(passwd));
            awssNotifyNeeded = 0;
        }

        if (1 == tryCount) {
            strncpy(ssid, DEFAULT_SSID, sizeof(ssid));
            strncpy(passwd, DEFAULT_PASSWD, sizeof(passwd));
            awssNotifyNeeded = 0;
        }
    } while (1);

end:
    awssFinished = 1;
    /* never reach here */
    return 0;
}

int awss_stop(void)
{
    //LOGT();

    stopAwssConnecting = 1;
    aws_destroy();
    //stop_awss_work();

    while (1) {
        if (awssFinished) {
            break;
        }
        aos_msleep(100);
    }
    return 0;
}


extern int aws_notify_app_nonblock(void);

#define AWSS_NOTIFY_TIMES   (50)
static void awss_notify(void *arg)
{
    int ret = aws_notify_app_nonblock();

    delay_ms += 100;

    if (!ret) {
        aos_loop_schedule_work(delay_ms, awss_notify, NULL, NULL, NULL);
    }
}
