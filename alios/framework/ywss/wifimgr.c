/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "wifimgr.h"
#include "alink_export.h"
#include "os/platform/platform.h"
#include "aos/aos.h"
#include "awss.h"
#include "alink_protocol.h"
#include "json_parser.h"
#include "ywss_utils.h"
#include "enrollee.h"
#include "accs.h"

#define ALINK_AUTHOR_RETRANS_INTERVAL 1

#define METHOD_WIFILIST "getWifiList"
#define METHOD_SWITCHAP "switchAp"
#define METHOD_GETDEVICEINFO "getDeviceInfo"
#define METHOD_WIFILIST_ACK "getWifiListResult"
#define METHOD_SWITCHAP_ACK "switchApResult"
#define METHOD_GETDEVICEINFO_ACK "getDeviceInfoResult"
typedef struct {
    char *methStr;
    int methStrLen;
} alink_ap_setup_msg_t;

typedef struct _ap_info_ {
    char ssid[PLATFORM_MAX_SSID_LEN];
    uint8_t bssid[ETH_ALEN];
    enum AWSS_AUTH_TYPE auth;
    enum AWSS_ENC_TYPE encry;
    uint8_t channel;
    char rssi;
    struct _ap_info_ *next;
} alink_ap_info_t;

#define MAX_AP_NUM_IN_MSG 5
#define WIFI_APINFO_LIST_LEN    (MAX_AP_NUM_IN_MSG * 256 +128)

ap_info_t oneAP;

extern void *udpFd;

int isUTF8(const char *ansiStr, int length)
{
    int i = 0;
    int isUTF8 = 1;
    while (i < length) {
        if ((0x80 & ansiStr[i]) == 0) { // ASCII
            i++;
            continue;
        } else if ((0xE0 & ansiStr[i]) == 0xC0) { // 110xxxxx
            if (ansiStr[i + 1] == '\0') {
                isUTF8 = 0;
                break;
            }
            if ((0xC0 & ansiStr[i + 1]) == 0x80) { // 10xxxxxx
                i += 2;
                continue;
            } else {
                isUTF8 = 0;
                break;
            }
        } else if ((0xF0 & ansiStr[i]) == 0xE0) { // 1110xxxx
            if (ansiStr[i + 1] == '\0') {
                isUTF8 = 0;
                break;
            }
            if (ansiStr[i + 2] == '\0') {
                isUTF8 = 0;
                break;
            }
            if (((0xC0 & ansiStr[i + 1]) == 0x80) && ((0xC0 & ansiStr[i + 2]) == 0x80)) { // 10xxxxxx 10xxxxxx
                i += 3;
                continue;
            } else {
                isUTF8 = 0;
                break;
            }
        } else {
            isUTF8 = 0;
            break;
        }
    }
    return isUTF8;
}

int getShubSecurityLevel(void)
{
    //int ret = 0;
    //if (os_aes_cbc_128_supported()) ret = 2;
    //if (os_aes_cbc_256_supported()) ret = 1;
    return 0;
}

#define WIFI_LIST_REQUEST_ID_LEN 16
static alink_ap_info_t *ap_info_list;

int cbScan(const char ssid[PLATFORM_MAX_SSID_LEN],
           const uint8_t bssid[ETH_ALEN],
           enum AWSS_AUTH_TYPE auth,
           enum AWSS_ENC_TYPE encry,
           uint8_t channel, char rssi,
           int isLastAP)
{
    alink_ap_info_t *p, *q;

    p = os_zalloc(sizeof(alink_ap_info_t));
    if (p == NULL) {
        return 0;
    }

    if (ssid != NULL) {
        memcpy(p->ssid, ssid, PLATFORM_MAX_SSID_LEN);
    }
    memcpy(p->bssid, bssid, ETH_ALEN);
    p->auth = auth;
    p->encry = encry;
    p->channel = channel;
    p->rssi = rssi;
    p->next = NULL;
    if (ap_info_list == NULL) {
        ap_info_list = p;
    } else {
        q = ap_info_list;
        while (q->next != NULL) {
            q = q->next;
        }
        q->next = p;
    }

    return 0;
}

void testSendWifiList(char *str, pplatform_netaddr_t sa)
{
    char *msgToSend;

    msgToSend = os_malloc(500);
    snprintf(msgToSend, 500,
             "{\"method\":\"getWifiListResult\", \"code\":\"0\", \"list\":[{\"ssid\":\"alibaba-guest\", \"rssi\":-70, \"auth\":\"4\"}],\"id\":\"%s",
             str);
    LOGI("[wifimgr]", "sending message to app: %s", msgToSend);

    if (0 > os_udp_sendto(udpFd, msgToSend, strlen(msgToSend), sa)) {
        LOGI("[wifimgr]", "sending failed.");
    }

    if (msgToSend) {
        os_free(msgToSend);
    }
}

/*
 * @desc: ????getWifiList??Ϣ
 *
 */
static int wifimgrProcessGetWifiListRequest(
    pplatform_netaddr_t sa, char *msg, int len)
{
    int strLen = 0;
    char *str = json_get_value_by_name(msg, len, "id", &strLen, 0);
    char *msgToSend;
    int msglen = 0;
    alink_ap_info_t *p, *q;
    char otherApinfo[64];
    char encodedSSID[OS_MAX_SSID_LEN * 2];
    uint8_t bssidConnected[ETH_ALEN];
    int apNumInMsg = 0;
    char wifiListRequestId[WIFI_LIST_REQUEST_ID_LEN + 1];

    msgToSend = os_malloc(WIFI_APINFO_LIST_LEN);
    if (msgToSend == NULL) {
        return SHUB_ERR;
    }

    memset(wifiListRequestId, 0, WIFI_LIST_REQUEST_ID_LEN);
    if (str && (strLen < WIFI_LIST_REQUEST_ID_LEN)) {
        memcpy(wifiListRequestId, str, strLen);
    }

    ap_info_list = NULL;
    os_wifi_get_ap_info(NULL, NULL, bssidConnected);

    os_wifi_scan(&cbScan);
    if (ap_info_list == NULL) {
        os_free(msgToSend);
        return SHUB_ERR;
    }

    p = ap_info_list;
    while (p != NULL) {
        if (0 == msglen) {
            msglen += snprintf(msgToSend + msglen, WIFI_APINFO_LIST_LEN - msglen,
                               "{\"method\":\"getWifiListResult\", \"code\":\"0\", \"id\":\"%s\", \"list\":[", wifiListRequestId);
        }

        if (p->ssid[0] != '\0') {
            if (memcmp(bssidConnected, p->bssid, ETH_ALEN) == 0) {
                snprintf(otherApinfo, 64, "\"auth\":\"%d\",\"connected\":\"1\"", p->auth);
            } else {
                snprintf(otherApinfo, 64, "\"auth\":\"%d\"", p->auth);
            }
            if (isUTF8(p->ssid, strlen(p->ssid))) {
                msglen += snprintf(msgToSend + msglen, WIFI_APINFO_LIST_LEN - msglen,
                                   "{\"ssid\":\"%s\", \"rssi\":\"%d\",%s},",
                                   p->ssid, p->rssi, otherApinfo);
            } else {
                utils_hex_to_str((const uint8_t *)oneAP.ssid, strlen(oneAP.ssid), encodedSSID, OS_MAX_SSID_LEN * 2);
                msglen += snprintf(msgToSend + msglen, WIFI_APINFO_LIST_LEN - msglen,
                                   "{\"xssid\":\"%s\", \"rssi\":\"%d\",%s},",
                                   encodedSSID, p->rssi, otherApinfo);
            }
            apNumInMsg++;
        }

        if ((p->next == NULL) || (MAX_AP_NUM_IN_MSG == apNumInMsg)) {
            if (msgToSend[msglen - 1] == ',') {
                msglen--;    /* eating the last ',' */
            }
            msglen += snprintf(msgToSend + msglen, WIFI_APINFO_LIST_LEN - msglen, "]}");
            LOGI("[wifimgr]", "sending message to app: %s", msgToSend);
            if (0 > os_udp_sendto(udpFd, msgToSend, strlen(msgToSend), sa)) {
                LOGI("[wifimgr]", "sending failed.");
            }
            msglen = 0;
            apNumInMsg = 0;
        }
        q = p;
        p = p->next;
        os_free(q);
    }

    os_free(msgToSend);

    return SHUB_OK;
}

extern void wifi_get_ip(char ips[16]);
static int wifimgrProcessGetDeviceInfo(pplatform_netaddr_t sa)
{
    char *buf;
    int len = 0;

    buf = os_malloc(256);
    if (!buf) {
        return SHUB_ERR;
    }

    char sIP[PLATFORM_IP_LEN];
    //os_wifi_get_ip(sIP);
    wifi_get_ip(sIP);

    char *product_get_sn(char *);
    char *product_get_model(char *);
    char sn[PRODUCT_SN_LEN];
    product_get_sn(sn);
    char model[PRODUCT_MODEL_LEN];
    product_get_model(model);
    len += snprintf(buf + len, WIFI_APINFO_LIST_LEN - len,
                    "{\"method\":\"getDeviceInfoResult\", \"code\":\"0\", \"sn\":\"%s\",\"model\":\"%s\",\"ip\":\"%s\",\"security\":\"%d\"}",
                    sn, model, sIP, getShubSecurityLevel());

    LOGI("[wifimgr]", "sending message to app: %s", buf);
    if (0 > os_udp_sendto(udpFd, buf, strlen(buf), sa)) {
        LOGI("[wifimgr]", "sending failed.");
    }

    if (buf) {
        os_free(buf);
    }

    return SHUB_OK;
}

//int canLogout = 0;

void aesDecryptString(char *cipherText, char *plainText, int blockNum32B, int securityLevel)
{
    //uint8_t key[32];  /* AES256 size */
    uint8_t key[] = {
        0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
        0x88, 0x99, 0xa1, 0xb2, 0xc3, 0xd4, 0xe5, 0xf6,
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f
    };
    /* decrypt using the key/iv */
    uint8_t iv[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };

    //uint8_t encoded[128];
    //memset(encoded, 0, 128);
    //strncpy(encoded, cipherText, 128);

    int len = strlen(cipherText);
    uint8_t *decoded = os_zalloc(len);
    if (!decoded) {
        return;
    }

    utils_str_to_hex(cipherText, len, decoded, len);

    p_aes128_t aes = 0;
    switch (securityLevel) {
        case 1:
            //os_aes_cbc_descrypt_256(key, iv, decoded, blockNum32B, plainText );
            break;
        case 2:
            //os_aes_cbc_descrypt_128(key, iv, decoded, blockNum32B*2, plainText );
            aes = os_aes128_init(key, iv, PLATFORM_AES_DECRYPTION);
            os_aes128_cbc_decrypt(aes, decoded, blockNum32B * 2, plainText);
            os_aes128_destroy(aes);
            break;
        default:
            LOGI("[wifimgr]", "wrong security level: %d\n", securityLevel);
    }
    LOGI("[wifimgr]", "descrypted '%s'\n", plainText);

    if (decoded) {
        os_free(decoded);
    }
}

#define WLAN_CONNECTION_TIMEOUT     (30 * 1000) //30 seconds
int switchApDone = 0;

/*
 * @desc: ????switchAp??Ϣ
 *
 */
static int wifimgrProcessSwitchApRequest(
    pplatform_netaddr_t sa, char *buf, int len)
{
    char ssid[PLATFORM_MAX_SSID_LEN * 2] = { 0 }, passwd[PLATFORM_MAX_PASSWD_LEN] = { 0 };
    char msg[128];
    char *str;
    int str_len = 0, success = 1;

    snprintf(msg, sizeof(msg), "{\"method\":\"switchApResult\",\"code\":\"0\",\"msg\":\"success\"}");

    int ssidFound = 0;
    str = json_get_value_by_name(buf, len, "ssid", &str_len, 0);
    if (str && (str_len < PLATFORM_MAX_SSID_LEN)) {
        memcpy(ssid, str, str_len);
        ssidFound = 1;
    } else {
        snprintf(msg, sizeof(msg), "{\"method\":\"switchApResult\",\"code\":\"-1\",\"msg\":\"ssid error\"}");
        success = 0;
    }

    if (!ssidFound) {
        str = json_get_value_by_name(buf, len, "xssid", &str_len, 0);
        if (str && (str_len < PLATFORM_MAX_SSID_LEN * 2 - 1)) {
            memcpy(ssid, str, str_len);
            uint8_t decoded[OS_MAX_SSID_LEN];
            int len = utils_str_to_hex(ssid, str_len, decoded, OS_MAX_SSID_LEN);
            memcpy(ssid, (const char *)decoded, len);
            ssid[len] = '\0';
            success = 1;
        } else {
            snprintf(msg, sizeof(msg), "{\"method\":\"switchApResult\",\"code\":\"-1\",\"msg\":\"ssid error\"}");
            success = 0;
        }
    }

    int encrypted = 0;
    str = json_get_value_by_name(buf, len, "encrypted", &str_len, 0);
    if (strncmp(str, "0", str_len) == 0) {
        encrypted = 0;
    } else {
        encrypted = 1;
    }

    str = json_get_value_by_name(buf, len, "passwd", &str_len, 0);
    //TODO: empty passwd is allow? json parse "passwd":"" result is NULL?
    if (0 == encrypted) {
        if (str_len <= PLATFORM_MAX_PASSWD_LEN) {
            memcpy(passwd, str, str_len);
        } else {
            snprintf(msg, sizeof(msg), "{\"method\":\"switchApResult\",\"code\":\"-2\",\"msg\":\"passwd error\"}");
            success = 0;
        }
    } else if (0 == getShubSecurityLevel()) {
        snprintf(msg, sizeof(msg), "{\"method\":\"switchApResult\",\"code\":\"-4\",\"msg\":\"security level error\"}");
        success = 0;
    } else {
        if (str_len <= (PLATFORM_MAX_PASSWD_LEN * 2)) {
            char encoded[256];
            memset(encoded, 0 , 256);
            memcpy(encoded, str, str_len);
            aesDecryptString(encoded, passwd, 2, getShubSecurityLevel()); //64bytes=2x32bytes
        } else {
            snprintf(msg, sizeof(msg), "{\"method\":\"switchApResult\",\"code\":\"-3\",\"msg\":\"passwd error\"}");
            success = 0;
        }
    }

    //if (success){
    //log_debug("logout", msg);
    // if (1 == canLogout) alink_logout();
    //}

    int i = 0;
    //TODO: NO login in default AP.
    //tz/*
    uint32_t timeStart = os_get_time_ms();
    LOGI("[wifimgr]", "logout start: %u", timeStart);
    for (i = 0; i < 3; i++) {
        if (!cloud_is_connected()) {
            break;
        }
        int ret = alink_logout();
        if ((ALINK_CODE_SUCCESS == ret) || (ALINK_CODE_ERROR_DEV_NOT_LOGIN == ret)) {
            break;
        } else {
            LOGI("[wifimgr]", "alink_logout() result: %d", ret);
        }
    }
    uint32_t timeEnd = os_get_time_ms();
    LOGI("[wifimgr]", "logout end: %u ; total time: %d", timeEnd, timeEnd - timeStart);
    //tz*/

    LOGI("[wifimgr]", "Sending message to app: %s", msg);
    for (i = 0; i < 3; i++) {
        if (0 > os_udp_sendto(udpFd, msg, strlen(msg), sa)) {
            LOGI("[wifimgr]", "sending failed.");
        } else {
            LOGI("[wifimgr]", "sending succeeded.");
        }
    }

    os_msleep(1000);
    //TODO: what if app fail recv?

    LOGI("[wifimgr]", "connect '%s' '%s'", ssid, passwd);
    //vTaskDelay(100 / portTICK_RATE_MS);
    if (success) {
        if (0 != os_awss_connect_ap(WLAN_CONNECTION_TIMEOUT,
                                    ssid, passwd,
                                    AWSS_AUTH_TYPE_INVALID,
                                    AWSS_ENC_TYPE_INVALID,
                                    NULL, 0)) {
            while (1) {
                if (0 == os_awss_connect_ap(WLAN_CONNECTION_TIMEOUT,
                                            DEFAULT_SSID, DEFAULT_PASSWD,
                                            AWSS_AUTH_TYPE_INVALID,
                                            AWSS_ENC_TYPE_INVALID,
                                            NULL, 0)) {
                    break;
                }
                os_msleep(2000);
            }
        } else {
            switchApDone = 1;
            awss_set_enrollee_token("default", strlen("default"));
        }
    }

    return SHUB_OK;
}

/*
 * @desc: ap_setup??Ϣ????????
 *
 */
int wifimgrProcessRequest(
    pplatform_netaddr_t sa, char *buf, unsigned int len)
{
    alink_ap_setup_msg_t msg = { 0 };
    LOGI("wifimgr", "in wifimgrProcessRequest, host addr: %s", sa->host);

    msg.methStr = json_get_value_by_name(buf, len, "method", &msg.methStrLen, 0);

    if (!msg.methStr) {
        LOGE("[awss]", "No \"method\" value.");
        return SHUB_ERR;
    }

    LOGI("[wifimgr]", "wifimgr request: %s", buf);

    if (!strncmp(msg.methStr, METHOD_WIFILIST, strlen(METHOD_WIFILIST))) {
        return wifimgrProcessGetWifiListRequest(sa, buf, len);
    }

    if (!strncmp(msg.methStr, METHOD_SWITCHAP, strlen(METHOD_SWITCHAP))) {
        return wifimgrProcessSwitchApRequest(sa, buf, len);
    }

    if (!strncmp(msg.methStr, METHOD_GETDEVICEINFO, strlen(METHOD_GETDEVICEINFO))) {
        return wifimgrProcessGetDeviceInfo(sa);
    }

    LOGE("[awss]", "Invalid message type.");
    return SHUB_ERR;
}


