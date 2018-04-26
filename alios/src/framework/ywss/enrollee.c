/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "aws_lib.h"
#include <stdlib.h>
#include "digest_algorithm.h"
#include "os.h"
#include "aos/aos.h"
#include "enrollee.h"

#ifndef AWSS_DISABLE_ENROLLEE
#define MODULE_NAME "enrollee"

const uint8_t probe_req_frame[64] = {
    0x40, 0x00,//mgnt type, frame control
    0x00, 0x00,//duration
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,//DA
    0x28, 0xC2, 0xDD, 0x61, 0x68, 0x83,//SA, to be replaced with wifi mac
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,//BSSID
    0xC0, 0x79,//seq
    0x00, 0x12, 0x61, 0x6C, 0x69, 0x62, 0x61, 0x62, 0x61, 0x2D, 0x74, 0x65, 0x73, 0x74, 0x2D, 0x37, 0x32, 0x36, 0x30, 0x36,//ssid
    0x01, 0x08, 0x82, 0x84, 0x8B, 0x96, 0x8C, 0x92, 0x98, 0xA4,//supported rates
    0x32, 0x04, 0xB0, 0x48, 0x60, 0x6C,//extended supported rates
    0x3F, 0x84, 0x10, 0x9E//FCS
};

const uint8_t iv[32] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static uint8_t *g_devid; /* pointer to devid_len start pos */
static uint8_t *g_model; /* pointer to model_len start pos */
static uint8_t *enrollee_frame;
static int enrollee_frame_len;


uint32_t get_random_digital(void);
static int decrypt_ssid_passwd(uint8_t *ie, uint8_t ie_len,
                               uint8_t out_ssid[OS_MAX_SSID_LEN],
                               uint8_t out_passwd[OS_MAX_PASSWD_LEN],
                               uint8_t out_bssid[ETH_ALEN]);

void awss_calc_sign(uint32_t rand,
                    char devid[OS_PRODUCT_SN_LEN],
                    char model[OS_PRODUCT_MODEL_LEN],
                    char secret[OS_PRODUCT_SECRET_LEN],
                    char sign[ENROLLEE_SIGN_SIZE])
{
    char *text;
    int text_len, devid_len, model_len, secret_len;

    if (!devid || !model || !secret || !sign) {
        return;
    }

    devid_len = strlen(devid);
    model_len = strlen(model);
    secret_len = strlen(secret);

    LOGD(MODULE_NAME,
         "dump rand(%d)+devid(%d)+model(%d)+secret(%d): %d %s %s %s",
         sizeof(uint32_t), devid_len, model_len,
         secret_len, rand, devid, model, secret);

    /* calc sign */
    text_len = sizeof(uint32_t) + devid_len + model_len;
    text = os_malloc(text_len + 1); /* +1 for string print */
    OS_CHECK_MALLOC(text);
    memset(text, 0, text_len + 1);

    memcpy(text, &rand, sizeof(uint32_t));
    memcpy(text + sizeof(uint32_t), devid, devid_len);
    memcpy(text + sizeof(uint32_t) + devid_len, model, model_len);

    digest_hmac(DIGEST_TYPE_MD5,
                (const unsigned char *)text, text_len,
                (const unsigned char *)secret, secret_len, sign);

    os_free(text);
}

void awss_init_enrollee_info(void)// void enrollee_raw_frame_init(void)
{
    uint8_t sign[ENROLLEE_SIGN_SIZE];
    uint32_t rand = get_random_digital();
    char *model, *devid, *secret;
    int devid_len, model_len, secret_len;
    int len, ie_len;

    if (enrollee_frame_len) {
        return;
    }

    model = os_malloc(OS_PRODUCT_MODEL_LEN);
    devid = os_malloc(OS_PRODUCT_SN_LEN);
    secret = os_malloc(OS_PRODUCT_SECRET_LEN);
    OS_CHECK_MALLOC(model && devid && secret);

    os_product_get_model(model);
    os_product_get_sn(devid);
    os_product_get_secret(secret);

    model_len = strlen(model);
    devid_len = strlen(devid);
    secret_len = strlen(secret);
    OS_ASSERT(model_len && devid_len && secret_len, "invalid len");

    awss_calc_sign(rand, devid, model, secret, sign);

    ie_len = model_len + devid_len + ENROLLEE_IE_FIX_LEN;
    enrollee_frame_len = sizeof(probe_req_frame) + ie_len;

    enrollee_frame = os_malloc(enrollee_frame_len);
    OS_CHECK_MALLOC(enrollee_frame);

    /* construct the enrollee frame right now */
    len = sizeof(probe_req_frame) - FCS_SIZE;
    memcpy(enrollee_frame, probe_req_frame, len);

    enrollee_frame[len++] = 221; //vendor ie
    enrollee_frame[len++] = ie_len - 2; /* exclude 221 & len */
    enrollee_frame[len++] = 0xD8;
    enrollee_frame[len++] = 0x96;
    enrollee_frame[len++] = 0xE0;
    enrollee_frame[len++] = 0xAA;/* OUI type */
    enrollee_frame[len++] = DEVICE_TYPE_VERSION;/* version & dev type */

    g_devid = &enrollee_frame[len];/* pointer to devid len, see decrypt func */
    enrollee_frame[len++] = devid_len;/* dev id len*/
    memcpy(&enrollee_frame[len], devid, devid_len);
    len += devid_len;

    enrollee_frame[len++] = ENROLLEE_FRAME_TYPE;/* frame type */

    g_model = &enrollee_frame[len]; /* pointer to model len, see decrypt func */
    enrollee_frame[len++] = model_len;
    memcpy(&enrollee_frame[len], model, model_len);
    len += model_len;

    memcpy(&enrollee_frame[len], &rand, sizeof(uint32_t));
    len += sizeof(uint32_t);

    memcpy(&enrollee_frame[len], sign, ENROLLEE_SIGN_SIZE);
    len += ENROLLEE_SIGN_SIZE;

    memcpy(&enrollee_frame[len],
           &probe_req_frame[sizeof(probe_req_frame) - FCS_SIZE], FCS_SIZE);

    /* update probe request frame src mac */
    {
        uint8_t mac[ETH_ALEN];
        os_wifi_get_mac(mac);

        memcpy(enrollee_frame + SA_POS, mac, ETH_ALEN);
    }

    vendor_decrypt_ssid_passwd = &decrypt_ssid_passwd;

    {
        //dump pkt info
        int i;

        platform_printf("dump enrollee info:\r\n");
        for (i = 0; i < enrollee_frame_len; i++) {
            platform_printf("%02x", enrollee_frame[i]);
        }
        platform_printf("\r\n");
    }

    os_free(model);
    os_free(devid);
    os_free(secret);
}

void awss_destroy_enrollee_info(void)
{
    if (enrollee_frame_len) {
        os_free(enrollee_frame);
        enrollee_frame_len = 0;
        enrollee_frame = NULL;
        g_devid = NULL;
        g_model = NULL;
    }
}

void awss_broadcast_enrollee_info(void)
{
    os_wifi_send_80211_raw_frame(FRAME_PROBE_REQ, enrollee_frame,
                                 enrollee_frame_len);

#if 0   //TODO: send beacon frame, so android device will be able to discover it
    os_wifi_send_80211_raw_frame(FRAME_BEACON, beacon_frame,
                                 sizeof(beacon_frame));
#endif
}

/* return 0 for success, -1 devid not match, otherwise return -2 */
static int decrypt_ssid_passwd(
    uint8_t *ie, uint8_t ie_len,
    uint8_t out_ssid[OS_MAX_SSID_LEN],
    uint8_t out_passwd[OS_MAX_PASSWD_LEN],
    uint8_t out_bssid[ETH_ALEN])
{
    uint8_t tmp_ssid[OS_MAX_SSID_LEN], key[MAX_KEY_LEN], tmp_passwd[OS_MAX_PASSWD_LEN];
    uint8_t *p_devid, *p_ssid, *p_passwd, *p_bssid, *p_token;
    uint8_t *orig_ie = ie;
    char *text = NULL, *secret;
    int text_len;

    /* ignore ie hdr: 221, len, oui[3], type(0xAB) */
#define REGISTRAR_IE_HDR    (6)
    ie += REGISTRAR_IE_HDR;
    if (ie[0] != DEVICE_TYPE_VERSION) {
        LOGW(MODULE_NAME, "registrar(devtype/ver=%d not supported!", ie[0]);
        return -1;
    }

    ie++;
    p_devid = ie;
    ie += ie[0] + 1; /* eating devid_len & devid[n] */

    if (ie[0] != REGISTRAR_FRAME_TYPE) {
        LOGW(MODULE_NAME, "registrar(frametype=%d not supported!", ie[0]);
        return -1;
    }

    ie[0] = '\0';/* for devid platform_printf */
    ie++; /* eating frame type */
    p_ssid = ie;
    if (ie[0] > OS_MAX_SSID_LEN) {
        LOGW(MODULE_NAME, "registrar(ssidlen=%d invalid!", ie[0]);
        return -1;
    }
    memcpy(tmp_ssid, &p_ssid[1], p_ssid[0]);
    tmp_ssid[p_ssid[0]] = '\0';
    LOGI(MODULE_NAME, "Registrar ssid:%s, devid:%s", tmp_ssid, &p_devid[1]);

    ie += ie[0] + 1; /* eating ssid_len & ssid[n] */

    p_passwd = ie;

    ie += ie[0] + 1; /* eating passwd_len & passwd */
    p_bssid = ie;

    ie += ETH_ALEN; /* eating bssid len */
    p_token = ie;
    if (p_token[0] > MAX_TOKEN_LEN) {
        LOGW(MODULE_NAME, "bad token len(%d)!", p_token[0]);
        return -2;
    }

    ie += p_token[0] + 1; /* eating token_len & token[n] */

    if (ie - orig_ie - 2 != ie_len) {
        LOGW(MODULE_NAME, "ie len not match %d != %d", ie - orig_ie - 2, ie_len);
        return -2;
    }

    //if (!enrollee_frame_len) return -2;
    if (!g_devid || memcmp(g_devid, p_devid, p_devid[0])) {
        LOGW(MODULE_NAME, "devid not match, expect %s!= recv %s", g_devid, p_devid);
        return -2;
    }

    secret = os_malloc(OS_PRODUCT_SECRET_LEN);
    if (!secret) {
        return -2;
    }
    os_product_get_secret(secret);

    text_len = strlen(secret) + p_devid[0];
    text = os_malloc(text_len);
    if (!text) {
        os_free(secret);
        return -2;
    }

    memcpy(text, secret, strlen(secret));
    memcpy(text + strlen(secret), (char *)&p_devid[1], p_devid[0]);

    /* decrypt key = hmac_md5[key:bssid, payload:secret+devid] */
    digest_hmac(DIGEST_TYPE_MD5,
                (const unsigned char *)text, text_len,
                (const unsigned char *)p_bssid, ETH_ALEN, (unsigned char *)key);

#if 0   //dump decrypt key
    int i;
    for (i = 0; i < 16; i++) {
        platform_printf("%02x", key[i]);
    }

    platform_printf("\r\n");
#endif

    {
        p_aes128_t aes = os_aes128_init(key, iv, PLATFORM_AES_DECRYPTION);
        os_aes128_cbc_decrypt(aes, &p_passwd[1], p_passwd[0] / AES_KEY_LEN, tmp_passwd);
        os_aes128_destroy(aes);

        LOGI(MODULE_NAME, "ssid:%s, passwd:%s\n", tmp_ssid, tmp_passwd);
    }

    strcpy((char *)out_ssid, (char *)tmp_ssid);
    strcpy((char *)out_passwd, (char *)tmp_passwd);
    memcpy((char *)out_bssid, (char *)p_bssid, ETH_ALEN);

    awss_set_enrollee_token((char *)&p_token[1], p_token[0]);

    if (secret) {
        os_free(secret);
    }
    if (text) {
        os_free(text);
    }

    return 0;/* success */
}

uint32_t get_random_digital(void)
{
    uint32_t seed = os_get_time_ms();
    uint8_t byte[4 + 1];
    uint32_t result;

    result = result; /* elimit warning */
    seed += result; /* add a random number */

    seed = seed % 9999;

    snprintf((char *)byte, sizeof(byte), "%04d", seed);

    memcpy(&result, byte, 4);

    return result;
}

#endif
char enrollee_token[MAX_TOKEN_LEN + 1];
char *awss_get_enrollee_token(void)
{
    char *token = enrollee_token;

    if (strlen(token) <= MAX_TOKEN_LEN) {
        return token;
    }

    return NULL;
}

void awss_clear_enrollee_token(void)
{
    memset(enrollee_token, 0, MAX_TOKEN_LEN + 1);
}

int awss_set_enrollee_token(char *token, int tokenLen)
{
    if (tokenLen > MAX_TOKEN_LEN) {
        return -1;
    }
    memcpy(enrollee_token, token, tokenLen);
    enrollee_token[tokenLen] = '\0';
    return 0;
}
