/*
 * Copyright (c) 2015 - 2017, Alibaba
 * 
 * All rights reserved.
 * 
 * Use in source and binary forms, redistribution in binary form only, with
 * or without modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions in binary form, except as embedded into a Alibaba in a 
 *    product or a software update for such product, must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the distribution.
 * 
 * 2. Neither the name of Alibaba nor the names of its contributors may be used
 *    to endorse or promote products derived from this software without specific
 *    prior written permission.
 * 
 * 3. This software, with or without modification, must only be used with a Alibaba.
 * 
 * 4. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "blefi_config.h"
#include "zconfig_lib.h"
#include "zconfig_utils.h"
#include <stdint.h>
#include <string.h>

#define ARRAY_SIZE(a)    (sizeof(a) / sizeof(a[0]))

enum blefi_status_t {
    BLEFI_GOT_SSID = 0x01,
    BLEFI_GOT_PASSWD = 0x02,
    BLEFI_GOT_BSSID = 0x04,
    BLEFI_GOT_ALL = BLEFI_GOT_SSID | BLEFI_GOT_PASSWD | BLEFI_GOT_BSSID,
};

extern uint8_t aws_result_ssid[ZC_MAX_SSID_LEN]; 
extern uint8_t aws_result_passwd[ZC_MAX_PASSWD_LEN]; 
extern uint8_t aws_result_bssid[ETH_ALEN]; 
extern uint8_t zconfig_callback_over(uint8_t *ssid, uint8_t *passwd, uint8_t *bssid);

static uint8_t m_ali_blefi_status = 0;

static void ali_blefi_ssid_handler(void *config);
static void ali_blefi_passwd_handler(void *config);
static void ali_blefi_bssid_handler(void *config);

static ali_dev_config_ctx_t m_ali_config_ctx;
static const config_handler_t m_config_handler_list[] = {
    ali_blefi_ssid_handler,
    ali_blefi_passwd_handler,
    ali_blefi_bssid_handler,
}; 

inline static int ali_ble_dev_valid_config(void *config)
{
    if (!config)
        return 0;

    ali_dev_config_t cmd;
    memcpy(&cmd, config, sizeof(cmd));
    ali_dev_config_t *config_cmd = (ali_dev_config_t *)&cmd;
    if (config_cmd->ctyp > ALINK_CONFIG_NONE &&
        config_cmd->ctyp < ALINK_CONFIG_CMD_CNT)
        return 1;

    return 0;
}

static void ali_blefi_ssid_handler(void *config)
{
    if (!config)
        return;

    ali_dev_config_t *config_cmd = (ali_dev_config_t *)config;

    if (config_cmd->ctyp != ALINK_CONFIG_SSID_CMD)
        return;

    strncpy((char *)aws_result_ssid, (const char *)config_cmd->cval, config_cmd->clen);
    m_ali_blefi_status |= BLEFI_GOT_SSID;

    if ((m_ali_blefi_status & BLEFI_GOT_ALL) != BLEFI_GOT_ALL)
        return;
    zconfig_callback_over(aws_result_ssid, aws_result_passwd, aws_result_bssid);
}

static void ali_blefi_passwd_handler(void *config)
{
    if (!config)
        return;

    ali_dev_config_t *config_cmd = (ali_dev_config_t *)config;

    if (config_cmd->ctyp != ALINK_CONFIG_PASSWD_CMD)
        return;
    strncpy((char *)aws_result_passwd, (const char *)config_cmd->cval, config_cmd->clen);
    m_ali_blefi_status |= BLEFI_GOT_PASSWD;

    if ((m_ali_blefi_status & BLEFI_GOT_ALL) != BLEFI_GOT_ALL)
        return;
    zconfig_callback_over(aws_result_ssid, aws_result_passwd, aws_result_bssid);
}

static void ali_blefi_bssid_handler(void *config)
{
    if (!config)
        return;

    ali_dev_config_t *config_cmd = (ali_dev_config_t *)config;

    if (config_cmd->ctyp != ALINK_CONFIG_BSSID_CMD)
        return;
    memcpy(aws_result_bssid, config_cmd->cval, ETH_ALEN);
    m_ali_blefi_status |= BLEFI_GOT_BSSID;
#if 0
    info("blefi, bssid:%02X:%02X:%02X:%02X:%02X:%02X\n", aws_result_bssid[0],
        aws_result_bssid[1], aws_result_bssid[2], aws_result_bssid[3],
        aws_result_bssid[4], aws_result_bssid[5]);
#endif
    if ((m_ali_blefi_status & BLEFI_GOT_ALL) != BLEFI_GOT_ALL)
        return;
    zconfig_callback_over(aws_result_ssid, aws_result_passwd, aws_result_bssid);
}

void ali_blefi_config_init()
{
    enum alink_config_t ctyp = ALINK_CONFIG_SSID_CMD;

    if (m_ali_config_ctx.init == 1)
        return;

    memset(&m_ali_config_ctx, 0, sizeof(m_ali_config_ctx));
    memset(aws_result_ssid, 0, sizeof(aws_result_ssid));
    memset(aws_result_passwd, 0, sizeof(aws_result_passwd));
    for (uint8_t i = 0; i < ARRAY_SIZE(m_config_handler_list); i ++) {
        m_ali_config_ctx.ctyp[i] = ctyp + i;
        m_ali_config_ctx.handler[i] = m_config_handler_list[i];
    }
}

void ali_blefi_config_handler(void *buf, int length)
{
    int idx = 0;
    ali_dev_config_t *cmd = NULL;

    if (!buf || length < sizeof(*cmd))
        return;

    while (idx < length) {
        cmd = (ali_dev_config_t *)((uint8_t *)buf + idx);
        if (!ali_ble_dev_valid_config(cmd))
            break;
        for (uint16_t i = 0; i < ARRAY_SIZE(m_ali_config_ctx.ctyp); i ++) {
            if (m_ali_config_ctx.ctyp[i] == cmd->ctyp &&
                m_ali_config_ctx.handler[i])
                m_ali_config_ctx.handler[i]((char *)buf + idx);
        }
        idx += cmd->clen + sizeof(*cmd);
    }
}

void ali_blefi_config_deinit()
{
    m_ali_blefi_status = 0;
}
