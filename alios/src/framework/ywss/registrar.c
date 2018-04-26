/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdlib.h>
#include "service_manager.h"
#include "alink_protocol.h"
#include "json_parser.h"
#include "enrollee.h"
#include "awss.h"
#include "wsf.h"
#include "os.h"
#include "aos/aos.h"

static int registrar_event(int type, void *data, int dlen, void *result, int *rlen);
static void awss_wifi_mgnt_frame_callback(uint8_t *buffer, int length, char rssi, int buffer_type);
static void registrar_raw_frame_init(struct enrollee_info *enr);
static void registrar_raw_frame_send(void);
static void registrar_raw_frame_destroy(void);

static void enrollee_report(void *);
static void enrollee_checkin(void *);
static int enrollee_enable_somebody_checkin(int dev_type, char *token, char *key, char *devid);

#define REGISTRAR_SEND_PKT_INTERVAL     (160)
#define MODULE_NAME_ENROLLEE "awss.enrollee"

static const uint8_t alibaba_oui[3] = { 0xD8, 0x96, 0xE0 };
static int registrar_inited = 0;
void awss_registrar_init(void)
{
    if (registrar_inited) {
        return;
    }
    registrar_inited = 1;
    os_wifi_enable_mgnt_frame_filter(
        FRAME_BEACON_MASK | FRAME_PROBE_REQ_MASK,
        (uint8_t *)alibaba_oui, awss_wifi_mgnt_frame_callback);
    sm_attach_service("accs", &registrar_event);
}
AOS_EXPORT(void, awss_registrar_init, void);

void awss_registrar_exit(void)
{
    os_wifi_enable_mgnt_frame_filter(
        FRAME_BEACON_MASK | FRAME_PROBE_REQ_MASK,
        (uint8_t *)alibaba_oui, NULL);

    sm_detach_service("accs", &registrar_event);
}

#if 0
"params" :
{
    "deviceType" : 0,
"traceToken" : "0ABCDEF"
    ,
"key" : "11918f561fabde1cffb9e7b3178743aa"
    ,
"deviceId" : "AA:BB:CC:DD:11:EE:22"
    ,
"algorithm" : "r2h5"
}
#endif
int alink_enrollee_checkin(char *params)
{
    int dev_type;
    char *token = NULL, *key = NULL, *devid = NULL;
    char *value;
    int attr_len, ret = -1;

    token = aos_zalloc(MAX_TOKEN_LEN + 1);
    devid = aos_zalloc(MAX_DEVID_LEN + 1);
    key = aos_zalloc(MAX_KEY_LEN * 2 + 1);

    if (!token || !devid || !key) {
        goto out;
    }


    value = json_get_value_by_name(params, strlen(params),
                                   "deviceType", &attr_len, NULL);
    if (value) {
        dev_type = atoi(value);
        if (dev_type) { /* alink device type = 0 */
            LOGW(MODULE_NAME_ENROLLEE, "dev type:%d, not supported", dev_type);
            goto out;
        }
    }

    value = json_get_value_by_name(params, strlen(params),
                                   "traceToken", &attr_len, NULL);
    if (value) {
        memcpy(token, value, attr_len);
    } else {
        goto out;
    }

    value = json_get_value_by_name(params, strlen(params),
                                   "deviceId", &attr_len, NULL);
    if (value) {
        memcpy(devid, value, attr_len);
    } else {
        goto out;
    }

    value = json_get_value_by_name(params, strlen(params),
                                   "key", &attr_len, NULL);
    if (value) {
        memcpy(key, value, attr_len);
    } else {
        goto out;
    }

    value = json_get_value_by_name(params, strlen(params),
                                   "algorithm", &attr_len, NULL);
    if (!value || strncmp(value, "aesh5", strlen("aesh5"))) {
        goto out;    //algorithm not supported
    }

    ret = enrollee_enable_somebody_checkin(dev_type, token, key, devid);


out:
    if (token) {
        aos_free(token);
    }
    if (devid) {
        aos_free(devid);
    }
    if (key) {
        aos_free(key);
    }

    if (ret > 0) {
        return 0;
    } else {
        LOGW(MODULE_NAME_ENROLLEE, "alink checkin failed");
        return -1;
    }
    return -1;
}

int alink_clear_apinfo(char *params)
{
    char ssid[OS_MAX_SSID_LEN] = { 0 };
    char passwd[OS_MAX_PASSWD_LEN] = { 0 };
    uint8_t bssid[OS_ETH_ALEN] = { 0 };
    int ret;

    //TODO: if device got ssid,passwd A from zeroconfig, but use ssid,passwd B
    //to connect, here we may mis-clean ssid,passwd B
    ret = os_wifi_get_ap_info(ssid, passwd, bssid);
    if (!ret) {
        LOGI(MODULE_NAME_ENROLLEE, "clean ssid %s credential");
        //os_wifi_set_ap_info(ssid, "", bssid);//TODO:
        os_sys_reboot();
        return 0;
    } else {
        LOG();
        return -1;
    }
}

int alink_set_apinfo(char *params)
{
    char *value;
    char ssid[OS_MAX_SSID_LEN] = { 0 }, passwd[OS_MAX_PASSWD_LEN] = { 0 };
    int ret, attr_len;

    value = json_get_value_by_name(params, strlen(params),
                                   "ssid", &attr_len, NULL);
    if (value) {
        memcpy(ssid, value, attr_len);
    } else {
        goto out;
    }

    value = json_get_value_by_name(params, strlen(params),
                                   "passwd", &attr_len, NULL);
    if (value) {
        memcpy(passwd, value, attr_len);
    } else {
        goto out;
    }

    {
        char orig_ssid[OS_MAX_SSID_LEN] = { 0 }, orig_passwd[OS_MAX_PASSWD_LEN] = { 0 };
        uint8_t orig_bssid[OS_ETH_ALEN] = { 0 };

        enum AWSS_AUTH_TYPE auth = AWSS_AUTH_TYPE_INVALID;
        enum AWSS_ENC_TYPE encry = AWSS_ENC_TYPE_INVALID;
        uint8_t bssid[OS_ETH_ALEN] = { 0 };
        uint8_t channel = 0;

        os_wifi_get_ap_info(orig_ssid, orig_passwd, orig_bssid);

        ret = os_awss_connect_ap(WLAN_CONNECTION_TIMEOUT_MS, ssid, passwd,
                                 auth, encry, bssid, channel);
        if (ret) {
            LOGW(MODULE_NAME_ENROLLEE, "set ap info ssid:%s passwd:%s fail", ssid, passwd);
            ret = os_awss_connect_ap(WLAN_CONNECTION_TIMEOUT_MS, orig_ssid, orig_passwd,
                                     auth, encry, orig_bssid, channel);
        }
    }

    return ret;
out:
    return -1;
}

//TODO: real id?
#define enrolleeCheckInFailed   "{\"id\":-1,\"result\":{\"msg\":\"enrollee not found\",\"code\":2001}}"
#define enrolleeSetWifiInfoFailed   "{\"id\":-1,\"result\":{\"msg\":\"set apinfo failed\",\"code\":2001}}"
static int registrar_event(int type, void *data, int dlen, void *result, int *rlen)
{
    if (type == SERVICE_DATA) {
        alink_data_t *p = (alink_data_t *)data;
        char *method = p->method;
        char *params = p->data;
        int ret = 0;

        if (!strcmp(method, "device.enrolleeCheckIn")) {
            ret = alink_enrollee_checkin(params);
            if (ret) {
                strcpy(result, enrolleeCheckInFailed);
            }
        } else if (!strcmp(method, "device.enrolleeRollback")) {
            ret = alink_clear_apinfo(params);
            if (ret) {
                strcpy(result, enrolleeSetWifiInfoFailed);
            }
        } else if (!strcmp(method, "device.setWifiInfo")) {
            ret = alink_set_apinfo(params);
            if (ret) {
                strcpy(result, enrolleeSetWifiInfoFailed);
            }
        } else {
            return EVENT_IGNORE;
        }

        if (result != NULL) {
            *rlen = strlen(result);
        }
        return EVENT_CONSUMED;
    }

    return EVENT_IGNORE;
}

static struct enrollee_info tmp_enrollee, enrollee_info[MAX_ENROLLEE_NUM];

extern void *alink_base64_decode_alloc(const char *str, int *len);
extern void alink_base64_release(void *data);

static int enrollee_enable_somebody_checkin(int dev_type, char *token, char *key, char *devid)
{
    int i;

    LOGI(MODULE_NAME_ENROLLEE, "dev_type:%d, token:%s, key:%s, devid:%s", dev_type, token, key, devid);
    if (dev_type || strlen(token) > MAX_TOKEN_LEN || strlen(key) > (MAX_KEY_LEN * 2)) {
        goto out;
    }

    for (i = 0; i < MAX_ENROLLEE_NUM; i++) {
        if (enrollee_info[i].state == ENR_IN_QUEUE) {
            if (strlen(devid) == enrollee_info[i].devid_len
                && !memcmp(devid, enrollee_info[i].devid, enrollee_info[i].devid_len)) {

                int key_byte_len = 0;
                uint8_t *key_byte = aos_malloc(MAX_KEY_LEN);
                if (!key_byte) {
                    return 0;
                }
                memset(key_byte, 0, MAX_KEY_LEN);

                key_byte_len = utils_str_to_hex(key, strlen(key), key_byte, MAX_KEY_LEN);

                if (key_byte_len != AES_KEY_LEN) {
                    return 0;
                }

                memcpy((char *)&enrollee_info[i].key[0], key_byte, AES_KEY_LEN);
                strcpy((char *)&enrollee_info[i].token[0], token);

                aos_free(key_byte);

                LOGI(MODULE_NAME_ENROLLEE, "enrollee[%d] state %d->%d", i, enrollee_info[i].state,
                     ENR_CHECKIN_ENABLE);
                enrollee_info[i].state = ENR_CHECKIN_ENABLE;
                enrollee_info[i].checkin_priority = 1;//TODO: not implement yet
                aos_loop_schedule_work(0, enrollee_checkin, NULL, NULL, NULL);
                return 1;/* match */
            }
        }
        LOGI(MODULE_NAME_ENROLLEE, "enrollee[%d] state %d", i, enrollee_info[i].state);
    }

out:
    return 0;/* mismatch */
}

/* 1 -- checkin onging, 0 -- idle */
static void enrollee_checkin(void *arg)
{
    int i;
    int checkin_ongoing = -1, pri = 65536, checkin_new = -1;
    unsigned int timestamp = os_get_time_ms();

    for (i = 0; i < MAX_ENROLLEE_NUM; i++) {
        switch (enrollee_info[i].state) {
            case ENR_CHECKIN_ENABLE:
                if (pri > enrollee_info[i].checkin_priority) {
                    pri = enrollee_info[i].checkin_priority;
                    checkin_new = i;
                }
                break;
            case ENR_CHECKIN_ONGOING:
                checkin_ongoing = i;
                goto ongoing;
            default:
                break;
        }
    }

    if (checkin_ongoing == -1 && checkin_new == -1) {
        return;
    }

    //checkin_new:
    if (checkin_ongoing == -1) {
        LOGI(MODULE_NAME_ENROLLEE, "enrollee[%d] state %d->%d", checkin_new,
             enrollee_info[checkin_new].state, ENR_CHECKIN_ONGOING);
        enrollee_info[checkin_new].state = ENR_CHECKIN_ONGOING;

        enrollee_info[checkin_new].checkin_timestamp = os_get_time_ms();
        registrar_raw_frame_init(&enrollee_info[checkin_new]);

        i = checkin_new;
    }
ongoing:
    registrar_raw_frame_send();
    LOGI(MODULE_NAME_ENROLLEE, "registrar_raw_frame_send");
    if (time_elapsed_ms_since(enrollee_info[i].checkin_timestamp) > ENROLLEE_CHECKIN_PERIOD_MS) {
        LOGI(MODULE_NAME_ENROLLEE, "enrollee[%d] state %d->%d", i,
             enrollee_info[i].state, ENR_CHECKIN_END);
        enrollee_info[i].state = ENR_CHECKIN_END;//FIXME: remove this state?
        LOGI(MODULE_NAME_ENROLLEE, "enrollee[%d] state %d->%d", i,
             enrollee_info[i].state, ENR_FREE);
        enrollee_info[i].state = ENR_FREE;
        registrar_raw_frame_destroy();
    }
    aos_loop_schedule_work(REGISTRAR_SEND_PKT_INTERVAL, enrollee_checkin, NULL, NULL, NULL);
}

unsigned int enrollee_report_period_ms = 30 * 1000;

int alink_report_enrollee(int dev_type, unsigned char *devid, int devid_len,
                          unsigned char *payload, int payload_len, int rssi)
{
    char *payload_str = NULL, *devid_str = NULL;
    int len = 0, ret = 0, i;
    alink_data_t data;
    char *buf = NULL;

    char ssid[OS_MAX_SSID_LEN] = { 0 };
    char passwd[OS_MAX_PASSWD_LEN] = { 0 };
    uint8_t bssid[OS_ETH_ALEN] = { 0 };

    os_wifi_get_ap_info(ssid, passwd, bssid);
    OS_CHECK_PARAMS(devid && devid_len && payload && payload_len);

    devid_str = os_malloc(devid_len + 1);
    payload_str = os_malloc(payload_len * 2 + 1);
    OS_CHECK_MALLOC(devid_str && payload_str);

    memcpy(devid_str, devid, devid_len);
    devid_str[devid_len] = '\0';
    for (i = 0; i < payload_len; i++) {
        sprintf(&payload_str[i * 2], "%02X", payload[i]);
    }
    payload_str[payload_len * 2] = '\0'; /* sprintf not add '\0' in the end of string in qcom */

    buf = os_malloc(ALINK_BUF_SIZE);
    OS_CHECK_MALLOC(buf);

    len += sprintf(&buf[len], "{");
    len += sprintf(&buf[len], "\"deviceType\":%d,", dev_type);
    len += sprintf(&buf[len], "\"deviceId\":\"%s\",", devid_str);
    len += sprintf(&buf[len], "\"payload\":\"%s\",", payload_str);
    len += sprintf(&buf[len], "\"ssid\":\"%s\",", ssid);
    len += sprintf(&buf[len], "\"bssid\":\"%02X%02X%02X%02X%02X%02X\",",
                   bssid[0], bssid[1], bssid[2],
                   bssid[3], bssid[4], bssid[5]);
    len += sprintf(&buf[len], "\"rssi\":\"%d\"", rssi);
    len += sprintf(&buf[len], "}");
    buf[len] = '\0';

    data.method = "device.enrolleeFound";
    data.data = buf;

    LOGD("registart", "To put data: %s", data.data);

    ret = sm_get_service("accs")->put((void *)&data, sizeof(data));//accs_put
    //TODO: parse the result: report period
    ret = 10;//report enrollee info duration

    if (buf) {
        os_free(buf);
    }
    if (devid_str) {
        os_free(devid_str);
    }
    if (payload_str) {
        os_free(payload_str);
    }

    return ret;
}

/* consumer */
static void enrollee_report(void *arg)
{
    int i;
    char ssid[OS_MAX_SSID_LEN] = { 0 };

    os_wifi_get_ap_info(ssid, NULL, NULL);
    if (!strcmp(ssid, DEFAULT_SSID)) {
        return;/* ignore enrollee in 'aha' mode */
    }


    /* evict timeout enrollee */
    for (i = 0; i < MAX_ENROLLEE_NUM; i++) {
        if (enrollee_info[i].state) {
            if (time_elapsed_ms_since(enrollee_info[i].timestamp)
                > ENROLLEE_EVICT_PERIOD_MS) {
                LOGI(MODULE_NAME_ENROLLEE, "enrollee[%d](state %d) evict:%s, %x->%x", i,
                     enrollee_info[i].state,
                     enrollee_info[i].devid,
                     enrollee_info[i].timestamp, os_get_time_ms());
                enrollee_info[i].state = ENR_FREE;
            } else {
                struct enrollee_info *enrollee = &enrollee_info[i];

                if (time_elapsed_ms_since(enrollee->report_timestamp) > enrollee_report_period_ms) {
                    int payload_len = 1 + enrollee->model_len + sizeof(uint32_t) + ENROLLEE_SIGN_SIZE;
                    uint8_t *payload = os_malloc(payload_len);
                    if (!payload) {
                        return;
                    }

                    payload[0] = enrollee->model_len;
                    memcpy(&payload[1], enrollee->model, enrollee->model_len);
                    memcpy(&payload[1 + enrollee->model_len], &enrollee->random,
                           sizeof(uint32_t) + ENROLLEE_SIGN_SIZE);

                    int report_period = alink_report_enrollee(
                                            enrollee->dev_type_ver,
                                            enrollee->devid, enrollee->devid_len,
                                            payload, payload_len, enrollee->rssi);
                    if (report_period > 0 && report_period < 65536) {
                        enrollee_report_period_ms = report_period * 1000;
                    }

                    LOGI(MODULE_NAME_ENROLLEE, "enrollee report result:%s, period:%dms\n",
                         report_period > 0 ? "success" : "failed",
                         enrollee_report_period_ms);

                    enrollee->report_timestamp = os_get_time_ms();
                    if (payload) {
                        os_free(payload);
                    }
                }
            }
        }
    }
}

int enrollee_put(struct enrollee_info *in);

int process_enrollee_ie(const uint8_t *ie, int rssi)
{
    /* suppose enrollee_ie is complete */
#define ENROLLEE_IE_HDR        (6)
#define ENROLLEE_DEV_TYPE_VER   (0)
#define ENROLLEE_FRAME_TYPE (0)
    /* copy to tmp_enrollee */
    ie += ENROLLEE_IE_HDR;
    memset(&tmp_enrollee, 0, sizeof(tmp_enrollee));

    if (ie[0] != DEVICE_TYPE_VERSION) {
        LOGW(MODULE_NAME_ENROLLEE, "enrollee(devtype/ver=%d not supported!", ie[0]);
        return -1;
    }
    tmp_enrollee.dev_type_ver = ie[0];
    ie++;/* eating dev_type_ver */

    if (ie[0] > MAX_DEVID_LEN) {
        LOGW(MODULE_NAME_ENROLLEE, "enrollee(devid_len=%d out of range!\r\n", ie[0]);
        return -1;
    }
    tmp_enrollee.devid_len = ie[0];
    memcpy(tmp_enrollee.devid, &ie[1], ie[0]);
    ie += ie[0] + 1; /* eating devid[n], devid_len */

    if (ie[0] != ENROLLEE_FRAME_TYPE) {
        LOGW(MODULE_NAME_ENROLLEE, "enrollee(frametype=%d not supported!\r\n", ie[0]);
        return -1;
    }
    tmp_enrollee.frame_type = ie[0];
    ie++;/* eating frame type */

    if (ie[0] > MAX_MODEL_LEN) {
        LOGW(MODULE_NAME_ENROLLEE, "enrollee(modle_len=%d out of range!\r\n", ie[0]);
        return -1;
    }
    tmp_enrollee.model_len = ie[0];
    memcpy(tmp_enrollee.model, &ie[1], ie[0]);
    ie += ie[0] + 1; /* eating model[n], model_len */

    memcpy(tmp_enrollee.random, ie, 4 + 16);

    tmp_enrollee.rssi = rssi;

    enrollee_put(&tmp_enrollee);

    return 0;
}

/* producer */
/*
 * 1: already saved, update timestamp
 * 0: new saved
 * -1: no slot to save, drop
 */
int enrollee_put(struct enrollee_info *in)
{
    int i, empty_slot = -1;

    for (i = 0; i < MAX_ENROLLEE_NUM; i++) {
        if (enrollee_info[i].state) {
            if (!memcmp(in, &enrollee_info[i], ENROLLEE_INFO_HDR_SIZE)) {
                /* update timestamp */
                enrollee_info[i].timestamp = os_get_time_ms();
                enrollee_info[i].rssi = (2 * enrollee_info[i].rssi + in->rssi) / 3;
                /*
                 * under certain conditions(state was clear
                 * to 0 at the same time by consumer),
                 * this enrollee req will lost, but it's ok.
                 * next time it will find its slot.
                 */
                //A_PRINTF("enrollee[%d] devid:%s time:%x\n",
                //      i, enrollee_info[i].devid,
                //      enrollee_info[i].timestamp);
                return 1;/* already saved */
            }
        } else {
            if (empty_slot == -1) {
                empty_slot = i;
            }
        }
    }

    if (empty_slot == -1) {
        return -1;    /* no slot to save */
    }

    memset(&enrollee_info[empty_slot], 0, sizeof(struct enrollee_info));
    memcpy(&enrollee_info[empty_slot], in, ENROLLEE_INFO_HDR_SIZE);
    enrollee_info[empty_slot].rssi = in->rssi;
    enrollee_info[empty_slot].timestamp = os_get_time_ms();
    enrollee_info[empty_slot].state = ENR_IN_QUEUE;
    LOGI(MODULE_NAME_ENROLLEE, "new enrollee[%d] devid:%s time:%x",
         empty_slot, in->devid, enrollee_info[empty_slot].timestamp);

    aos_loop_schedule_work(0, enrollee_report, NULL, NULL, NULL);

    return 0;
}

extern const unsigned char *cfg80211_find_vendor_ie(
    unsigned int oui, unsigned char oui_type,
    const unsigned char *ies, int len);
/**
 * @brief management frame handler
 *
 * @param[in] buffer @n 80211 raw frame or ie(information element) buffer
 * @param[in] len @n buffer length
 * @param[in] buffer_type @n 0 when buffer is a 80211 frame,
 *                          1 when buffer only contain IE info
 * @return None.
 * @see None.
 * @note None.
 */
void awss_wifi_mgnt_frame_callback(uint8_t *buffer, int length, char rssi, int buffer_type)
{
#define MGMT_BEACON (0x80)
#define MGMT_PROBE_REQ  (0x40)
#define MGMT_PROBE_RESP (0x50)

    /* fc(2) + dur(2) + da(6) + sa(6) + bssid(6) + seq(2) */
#define MGMT_HDR_LEN    (24)

    int type = buffer[0], len = 0, eid;
    const uint8_t *ie;

    if (buffer_type) {
        ie = buffer;
        goto ie_handler;
    }

    switch (type) {
        case MGMT_BEACON:
            //LOGI(MODULE_NAME_ENROLLEE,"beacon");
            buffer += MGMT_HDR_LEN + 12;/* hdr(24) + 12(timestamp, beacon_interval, cap) */
            length -= MGMT_HDR_LEN + 12;

            eid = buffer[0];
            len = buffer[1];
            if (buffer[0] != 0) {
                LOGW(MODULE_NAME_ENROLLEE, "error eid, should be 0, now is :%d!", eid);
                return;
            }

            buffer += 2;
            length -= 2;
            buffer += len;
            length -= len;
            goto find_ie;
            break;
        case MGMT_PROBE_REQ:
            //LOGI(MODULE_NAME_ENROLLEE,"probe req\n");
            buffer += MGMT_HDR_LEN;
            length -= MGMT_HDR_LEN;

find_ie:
            ie = cfg80211_find_vendor_ie((unsigned int)WLAN_OUI_ALIBABA,
                                         (unsigned char)WLAN_OUI_TYPE_ENROLLEE,
                                         (const unsigned char *)buffer, (int)length);
            if (ie) {
ie_handler:
                process_enrollee_ie(ie, rssi);
            }
            break;
        case MGMT_PROBE_RESP:
            //LOGI(MODULE_NAME_ENROLLEE,"probe resp");
            break;
        default:
            //LOGI(MODULE_NAME_ENROLLEE,"frame (%d): %02x \n", length, type);
            break;
    }
}

static uint8_t *registrar_frame;
static int registrar_frame_len;

static void registrar_raw_frame_init(struct enrollee_info *enr)
{
    int len, ie_len;
    int devid_len = enr->devid_len;
    unsigned char *devid = enr->devid;

    unsigned char *token = &enr->token[0];
    int token_len = strlen((char *)token);

    char ssid[OS_MAX_SSID_LEN] = { 0 };
    char passwd[OS_MAX_PASSWD_LEN] = { 0 };
    uint8_t bssid[OS_ETH_ALEN] = { 0 };
    int ssid_len, passwd_len;

    os_wifi_get_ap_info(ssid, passwd, bssid);
    ssid_len = strlen(ssid);
    passwd_len = strlen(passwd);

    /* padding 0, padding passwd/passwd_len to key block length */
    passwd_len = (passwd_len + AES_KEY_LEN - 1) & (~(AES_KEY_LEN - 1));

    ie_len = devid_len + ssid_len + passwd_len + token_len + REGISTRAR_IE_FIX_LEN;
    registrar_frame_len = sizeof(probe_req_frame) + ie_len;

    registrar_frame = os_malloc(registrar_frame_len);
    if (!registrar_frame) {
        LOGE(MODULE_NAME_ENROLLEE, "error: os_malloc size %d faild\r\n", registrar_frame_len);
        return;
    }

    /* construct the registrar frame right now */
    len = sizeof(probe_req_frame) - FCS_SIZE;
    memcpy(registrar_frame, probe_req_frame, len);

    registrar_frame[len++] = 221; //vendor ie
    registrar_frame[len++] = ie_len - 2; /* exclude 221 & len */
    registrar_frame[len++] = 0xD8;
    registrar_frame[len++] = 0x96;
    registrar_frame[len++] = 0xE0;
    registrar_frame[len++] = 0xAB;/* OUI type */
    registrar_frame[len++] = DEVICE_TYPE_VERSION;/* version & dev type */
    registrar_frame[len++] = devid_len;/* dev id len*/
    memcpy(&registrar_frame[len], devid, devid_len);
    len += devid_len;
    registrar_frame[len++] = REGISTRAR_FRAME_TYPE;/* frame type */

    registrar_frame[len++] = ssid_len;
    memcpy(&registrar_frame[len], ssid, ssid_len);
    len += strlen(ssid);

    registrar_frame[len++] = passwd_len;

    {
        p_aes128_t aes = os_aes128_init(&enr->key[0], iv, PLATFORM_AES_ENCRYPTION);
        //passwd was padding by 0
        os_aes128_cbc_encrypt(aes, (uint8_t *)passwd, passwd_len / AES_KEY_LEN,
                              (uint8_t *)&registrar_frame[len]);
        os_aes128_destroy(aes);
    }

    len += passwd_len;

    memcpy(&registrar_frame[len], bssid, ETH_ALEN);
    len += ETH_ALEN;

    registrar_frame[len++] = token_len;
    memcpy(&registrar_frame[len], token, token_len);
    len += token_len;

    memcpy(&registrar_frame[len],
           &probe_req_frame[sizeof(probe_req_frame) - FCS_SIZE], FCS_SIZE);

    /* update probe request frame src mac */
    {
        uint8_t mac[OS_ETH_ALEN];

        os_wifi_get_mac(mac);

        memcpy(registrar_frame + SA_POS, mac, ETH_ALEN);
    }

    {
        //dump registrar info
        int i;
        LOGI(MODULE_NAME_ENROLLEE, "dump registrar info:");
        for (i = 0; i < registrar_frame_len; i++) {
            platform_printf("%02x", registrar_frame[i]);
        }
        platform_printf("\r\n");
    }
}

static void registrar_raw_frame_destroy(void)
{
    if (registrar_frame_len) {
        os_free(registrar_frame);
        registrar_frame = NULL;
        registrar_frame_len = 0;
    }
}

static void registrar_raw_frame_send(void)
{
    /* suppose registrar_frame was ready
     * @see enrollee_checkin()
     */
    int ret = os_wifi_send_80211_raw_frame(FRAME_PROBE_REQ, registrar_frame,
                                           registrar_frame_len);
    if (ret) {
        LOGW(MODULE_NAME_ENROLLEE, "send failed");
    }
}
