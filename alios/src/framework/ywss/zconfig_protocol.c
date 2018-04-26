/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdlib.h>
#include <string.h>             // strcpy, memset()
#include <ctype.h>              // isascii()
#include "os.h"             // uint32_t
#include <stdio.h>              // snprintf
#include "zconfig_utils.h"
#include "zconfig_ieee80211.h"
#include "zconfig_config.h"
#include "zconfig_protocol.h"

/* broadcast mac address */
const u8 br_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
/* all zero mac address */
const u8 zero_mac[] = {0, 0, 0, 0, 0, 0};

/* invalid channel, neither 0, 0xff, nor 1-13 */
#define INVALID_CHANNEL     (20)

/* which channel lock at */
u8 zconfig_channel_locked = INVALID_CHANNEL;/* locked channel */
/*
 * avoid zconfig_callback_over() was called twice,
 * once from tods, once from fromds
 */
u8 zconfig_finished;

/* global data structure, which hold all broadcast data */
struct zconfig_data *zconfig_data;

//////////////////////////////////////////////////////////////////////////////////
//following is broadcast protocol related code
static inline u8 is_start_frame(u16 len)
{
    return (len == START_FRAME);
}

static inline u8 is_group_frame(u16 len)
{
    /* is group frame? */
    return (len > GROUP_FRAME && len < GROUP_FRAME_END);
}

static inline u8 is_data_frame(u16 len)
{
    u8 group_frame, index;
    /* is start frame */
    if (is_start_frame(len)) {
        return 0;
    }

    /* is group frame? */
    group_frame = is_group_frame(len);
    if (group_frame) {
        return 0;
    }

    index = (len >> 7) & 0xF;
    return (index >= 2 && index <= 9);
}

static inline u8 get_group_index(u16 len)
{
    if (is_start_frame(len)) {
        return 0;
    }
    return (len - GROUP_FRAME) * GROUP_NUMBER;
}

static inline u8 get_data_index(u16 len)
{
    u8 index = (len >> 7) & 0xF;
    return index - 1;       /* from 1 to 8 */
}

#define sn_minus(a,b)   (((a) - (b)) & 0xfff)

/* a, b must be serial seq number */
static inline int sn_compare(u16 a, u16 b)
{
    /*
        case1: sn = 3, sn_prev = 5;         a < b
        case2: sn = 0, sn_prev = 0xfff;     a > b
        case3: sn = 4, sn_prev = 3;         a > b
    */
    u16 res = sn_minus(a, b);

    if (!res) {
        return 0;    /* equal */
    }

    if (res < 1000) {
        return 1;
    } else {
        return -1;
    }
}

/*
 * zconfig_get_data_len()
 *  here we guess the total_len of protocl message,
 *  base on the score of tods and fromds side.
 */
static inline int zconfig_get_data_len(void)
{
    u8 len; /* total len, include len(1B) & crc(2B) */
    u8 score;

    /* tods > fromds */
    if (zconfig_data->pkg[1][1].score > zconfig_data->pkg[0][1].score) {
        len = zconfig_data->pkg[1][1].len & 0x7F;
        score = zconfig_data->pkg[1][1].score;
    } else {
        len = zconfig_data->pkg[0][1].len & 0x7F;
        score = zconfig_data->pkg[0][1].score;
    }

    if (len && score > score_mid) {
        //info("zconfig_get_data_len = %d\r\n", len);
        goto out;
    }

    if (zconfig_data->data[1].max_pos > zconfig_data->data[0].max_pos) {
        len = zconfig_data->data[1].max_pos;
    } else {
        len = zconfig_data->data[0].max_pos;
    }
out:
    if (len < GROUP_NUMBER) {
        return GROUP_NUMBER;
    } else {
        return len >= MAX_PKG_NUMS ? (MAX_PKG_NUMS - GROUP_NUMBER - 1) : len;
    }
}

/*
 * 8bit -> 6bit
 *
 * serialize chinese char from 8bit to 6bit
 */
static inline void encode_chinese(u8 *in, u8 in_len, u8 *out, u8 *out_len)
{
    u8 bit[ZC_MAX_SSID_LEN * 8] = { 0 };
    u8 i, j;
    u8 output_len = ((in_len * 8) + 5) / 6;

    //char to bit stream
    for (i = 0; i < in_len; i++) {
        for (j = 0; j < 8; j++) {
            bit[i * 8 + j] = (in[i] >> j) & 0x01;
        }
    }

    out[output_len] = '\0'; /* NULL-terminated */
    for (i = 0; i < output_len; i++) {
        for (j = 0, out[i] = 0; j < 6; j++) {
            out[i] |= bit[i * 6 + j] << j;
            //info("%02x ", out[i]);
        }
    }

    if (out_len) {
        *out_len = output_len;
    }
}

/* 6bit -> 8bit */
static inline void decode_chinese(u8 *in, u8 in_len, u8 *out, u8 *out_len)
{
    u8 bit[ZC_MAX_SSID_LEN * 8];
    u8 i, j;
    u8 output_len = (in_len * 6) / 8;

    //char to bit stream
    for (i = 0; i < in_len; i++) {
        for (j = 0; j < 6; j++) {
            bit[i * 6 + j] = (in[i] >> j) & 0x01;
        }
    }

    out[output_len] = '\0'; /* NULL-terminated */
    for (i = 0; i < output_len; i++) {
        for (j = 0, out[i] = 0; j < 8; j++) {
            out[i] |= bit[i * 8 + j] << j;
            //info("%02x ", out[i]);
        }
    }

    if (out_len) {
        *out_len = output_len;
    }
}

/* check recv completed or not */
static inline int zconfig_recv_completed(u8 tods)
{
    int i;
    u8 len, flag, ssid_len, passwd_len;
    /*
        byte:   0   1       2   3       4       5   6
        name:       total_len   flag    ssid_len    passwd_len  ssid    ...
        index:      0x100       0x180   0x200       0x280       0x300   0x380
    */
    len = pkg_len(1) & 0x7F;/* total len, include len(1B) & crc(2B) */
    flag = pkg_len(2) & 0x7F;
    if (flag & 0x01) {/* ssid flag */
        ssid_len = pkg_len(3) & 0x7F;
        passwd_len = pkg_len(4) & 0x7F;
    } else {
        ssid_len = 0;
        passwd_len = pkg_len(3) & 0x7F;
    }

    if (!len || pkg_score(1) <= score_min) {
        //log("len=%d, pkg_score(1)=%d\r\n", len, pkg_score(1));
        return 0;
    }

#ifndef DISABLE_SSID_AUTO_COMPLETE
#define SSID_AUTO_COMPLETE_SCORE    (score_max + 1)
    /* ssid atuo-completion */
    if (zc_ssid[0] != '\0' && (flag == 0x01 || flag == 0x21)
        && pkg_score(2) < SSID_AUTO_COMPLETE_SCORE
        && pkg_score(3) > score_mid
        && !zc_ssid_auto_complete_disable) {

        /* over-written ssid_len here */
        ssid_len = strlen((char const *)zc_ssid);

        if (flag == 0x01) {/* include ssid */
            if ((ssid_len | 0x200) != pkg_len(3)) {
                warn_on(1, "ssid len not match! ssid:%s != %d\r\n",
                        zc_ssid, (pkg_len(3) & ~0x200));
                zc_ssid_auto_complete_disable = 1;
                goto skip_ssid_auto_complete;
            }

            info("ssid auto-complete: %s\r\n", zc_ssid);
            pkg_score(2) = SSID_AUTO_COMPLETE_SCORE;

            pkg_len(3) = ssid_len | 0x200;  /* 0x200 is the index 3 */
            pkg_score(3) = SSID_AUTO_COMPLETE_SCORE;

            for (i = 5; i < ssid_len + 5; i++) {
                pkg_len(i) = (zc_ssid[i - 5] - 32) | (0x80 + 0x80 * (i % GROUP_NUMBER));
                pkg_score(i) =  SSID_AUTO_COMPLETE_SCORE;
            }
        } else if (flag == 0x21) { /* include chinese ssid */
            u8 *buf, buf_len = 0;

            u8 ssid_encode_len = (ssid_len * 8 + 5) / 6;

            /*
             * for GBK encoded chinese, ssid auto-completion lead to crc error.
             * because Android APP may send utf8 encoded ssid.
             */
            if ((ssid_encode_len | 0x200) != pkg_len(3)) {
                zc_ssid_is_gbk = 1;
                zc_ssid_auto_complete_disable = 1;
                goto skip_ssid_auto_complete;
            } else {
                zc_ssid_is_gbk = 0;
            }

            buf = zconfig_malloc(256);
            bug_on(!buf, "os_malloc failed!\r\n");

            info("chinese ssid auto-complete: %s\r\n", zc_ssid);
            encode_chinese(zc_ssid, ssid_len, buf, &buf_len);

            pkg_score(2) = SSID_AUTO_COMPLETE_SCORE;

            pkg_len(3) = buf_len | 0x200;   /* 0x200 is the index 3 */
            pkg_score(3) =  SSID_AUTO_COMPLETE_SCORE;

            for (i = 5; i < buf_len + 5; i++) {
                pkg_len(i) = buf[i - 5] | (0x80 + 0x80 * (i % GROUP_NUMBER));
                pkg_score(i) =  SSID_AUTO_COMPLETE_SCORE;
            }
            zconfig_free(buf);
        }
    }
#endif

skip_ssid_auto_complete:
    //log("expect len = %d, max len = %d\r\n", len, zc_max_pos);
    if (zc_max_pos < len - 1) {
        return 0;    /* -1 for crc bug */
    }

    for (i = 1; i <= len - 1; i++) {/* -1 for crc bug */
        if (pkg_score(i) <= score_min) {
            return 0;
        }
    }

    /* 4 for total_len, flag, ssid_len, passwd_len, 2 for crc */
    if (flag & 0x01) { /* ssid exist */
        if (len != ssid_len + passwd_len + 4 + 2) {
            return 0;
        }
    } else if (len != passwd_len + 3 + 2) {
        return 0;
    }

    return 1;
}
/*
 * bugfix:
 *  crc字节跟起始、分组帧冲突问题
 * rootcause：
 *  英文ssid, passwd取值范围在[0, 0x60], 但crc字节取值范围会超过0x60,
 *  0x60 + 数据帧的index(0x480, 0x380)，
 *  有可能会跟起始帧(0x4e0)、分组帧(0x3e1-0x3ef)冲突，
 *  收包时误把crc当做分组帧处理，引起后续bug。
 * workround:
 *  起始帧：0x4e0
 *  分组帧：0x3e1, 0x3e2, ... 0x3ef
 *
 *  数据帧序号：0x100, 0x180, 0x200, 0x280, 0x300, 0x380, 0x400, 0x480
 *
 *  1) crc[0]不可能跟0x4e0冲突
 *     按英文实际情况看，最大sum应该为
 *     (32+64)*0x60+(32+64+4)+0x21+32+64=9445=0x24E5 => max crc = 0x4965
 *     按中文编码来看，最大sum应该为
 *     (37*127)+64*0x60+(37+64+4)+0x21+37+64=11080=0x2B48 => max crc=0x5648
 *     crc[0]不会超过0x60, 因此crc[0]不可能跟0x4e0冲突
 *  2) crc[0]也不可能跟分组帧0x3e1, 0x3e2, ... 0x3ef冲突
 *     基于1)的推断，crc[0]最大为0x56,
 *     因此不管处在序号帧哪个位置(0x380)都不会跟0x3e1等序号帧冲突
 *  3) crc[1]可能跟0x4e0冲突，此时需满足如下条件
 *     crc[1]处在数据序号8的位置，即数据序号为0x480, 且数据域为0x60
 *  4) crc[1]可能跟0x3e1, 0x3e2, ... 0x3ef冲突
 *     crc[1]处在数据序号6的位置，即数据序号为0x380，且数据域为0x61~0x6f
 * 具体如下：
 *  满足任一条即发生了冲突
 *  1) total_len % 8 = 0, 且计算出来的crc[1] = 0x60,
 *     也就是0x480 + 0x60 = 0x4e0
 *  2)total_len % 8 = 6, 且计算出来的crc[1] = [0x61, 0x6f],
 *    也就是0x380 + [0x61, 0x6f] = [0x3e1, 0x3ef]
 * bug fix, see the following func:
 *  1) zconfig_recv_completed
 *  2) zconfig_get_ssid_passwd
 *
 * TEST CASES:
 *  1) len = 8*n + 6, crc[1] = 0x60, not collision
 *  SSID: [极路由中文测试]
 *  PASSWD: [fhbccxxxxxxu]      //0x60, 0x3e0

 *  2) len = 8*n + 6, crc[1] = 0x61, collision
 *  crc collosion: crc[1] = 0x61
 *  SSID: [极路由中文测试]
 *  PASSWD: [fhbccxxxxxxv]      //0x61, 0x3e1

 *  3) len = 8*n + 6, crc[1] = 0x6f, collision
 *  crc collosion: crc[1] = 0x6f
 *  SSID: [极路由中文测试]
 *  PASSWD: [fhbckxxxxxx|]      //0x6f, 0x3ef

 *  4) len = 8*n + 6, crc[1] = 0x70, not collision
 *  SSID: [极路由中文测试]
 *  PASSWD: [fhbckxxxxxx}]      //0x70, 0x3f0

 *  5) len = 8*n, crc[1] = 0x60, collision
 *  SSID: [极路由中文测试]              //0x60, 0x4e0
 *  PASSWD: [fhbcv~~~~~~~~~]

 *  6) len = 8*n, crc[1] = 0x61, not collision
 *  SSID: [极路由中文测试]              //0x61, 0x4e1
 *  PASSWD: [fhbcw~~~~~~~~~]
 */
static inline int is_crc_collosion(u8 total_len, u16 recv_crc, u16 calc_crc)
{
#define CRC_LSB(x)  (x & 0xFF)
#define CRC_MSB(x)  (x & 0xFF00)

    if (CRC_MSB(recv_crc) != CRC_MSB(calc_crc)) { //MSB of crc, cannot collosion with start/group frame
        return 0;
    }

    if (0x60 == CRC_LSB(calc_crc) && ((total_len % GROUP_NUMBER) == 0)) {
        return 1;
    } else if ((GROUP_FRAME - 0x380 < CRC_LSB(calc_crc)
                && CRC_LSB(calc_crc) < GROUP_FRAME_END - 0x380)
               && (total_len % GROUP_NUMBER == 6)) {
        return 1;
    }

    return 0;
}

static inline int zconfig_get_ssid_passwd(u8 tods)
{
    int i, ssid_len, package_num, passwd_len, ret;
    u8 *buf, *pbuf, *tmp, flag;
    u8 data, score = score;
    u16 crc, cal_crc;

    if (!zconfig_recv_completed(tods)) {
        return -1;
    }

    buf = zconfig_malloc(256);
    tmp = zconfig_malloc(128);
    bug_on(!buf || !tmp, "os_malloc failed!\r\n");

    /* package num */
    package_num = pkg_len(1) & 0x7F;/* total len, include len(1B) & crc(2B) */

    for (i = 1; i <= package_num; i++) {
        data = pkg_len(i);
        score = pkg_score(i);
        buf[i - 1] = data & 0x7F;
        info("%02x %s", score, (i % GROUP_NUMBER == 0) ? "  " : "");
    }
    info("\r\n");

    dump_hex(&buf[0], package_num, GROUP_NUMBER);

    crc = os_get_unaligned_be16(&buf[package_num - 2]);

    cal_crc = zconfig_checksum(buf, package_num - 2);
    if (crc != cal_crc) {
        if (is_crc_collosion(package_num, crc, cal_crc)) {
            warn("crc collosion: total_len=%d, crc[1]=0x%x\r\n", package_num, cal_crc & 0xFF);
        } else {
            error("crc check error: recv 0x%x != 0x%x\r\n", crc, cal_crc);
            ret = -1;
            goto exit;
        }
    }

    pbuf = &buf[0]; /* len @ [0] */

    flag = pbuf[1]; /* flag @ [1] */
    pbuf += 2;      /* 2B for total_len, flag */

    if (flag & 0x01) {/* ssid exist */
        ssid_len = pbuf[0];
        passwd_len = pbuf[1];
        pbuf += 2; /* 2B for ssid_len, passwd_len */

        /* CAN'T use snprintf here, because of SPACE char */
        memcpy((char *)tmp, pbuf, ssid_len);
        tmp[ssid_len] = '\0';

        if (!(flag & (1 << 5))) {//ascii
            for (i = 0; i < ssid_len; i++) {
                tmp[i] += 32;
            }
        } else {//chinese format
            decode_chinese(pbuf, ssid_len, tmp, NULL);
            /* make sure 'tmp' is null-terminated */
        }
        pbuf += ssid_len; /* ssid */

        if (zc_ssid[0] == '\0' || zc_ssid_auto_complete_disable) {
            strncpy((char *)zc_ssid, (const char *)tmp, ZC_MAX_SSID_LEN);
            info("SSID: [%s]\r\n", zc_ssid);
        } else {
            if (!strncmp((const char *)tmp, (char *)zc_ssid, ZC_MAX_SSID_LEN)) {
                info("SSID: [%s]\r\n", zc_ssid);
            } else {
                if (zc_ssid_is_gbk) {
                    info("gbk SSID: [%s]\r\n", zc_ssid);
                } else {
                    info("gbk? SSID: [%s]\r\n", zc_ssid);
                }
            }
        }
    } else {
        passwd_len = pbuf[0];
        pbuf += 1; /* 1B for passwd_len */
    }

    /* CAN'T use snprintf here, because of SPACE char */
    memcpy((char *)tmp, pbuf, passwd_len);
    tmp[passwd_len] = '\0';
    for (i = 0; i < passwd_len; i++) {
        tmp[i] += 32;
    }

    strncpy((char *)zc_passwd, (const char *)tmp, ZC_MAX_PASSWD_LEN);
    info("PASSWD: [%s]\r\n", tmp);
    pbuf += passwd_len; /* passwd */

    ret = 0;
exit:
    zconfig_free(buf);
    zconfig_free(tmp);

    return ret;
}
////////////////////////////////////////////////////////////////////////////////
//following is p2p protocol

/*
 * 1. place 0 @ 0, because of java modified-UTF8
 * 2. translation follow utf8 stardard
 */
static const u8 decode_table[] = {
    0x0, 0xe, 0x6c, 0x3a, 0x6d, 0x44, 0x2a, 0x6f,
    0x4d, 0x5, 0x6b, 0x28, 0x8, 0x25, 0x5f, 0x2d,
    0x64, 0x76, 0x78, 0x37, 0x58, 0x60, 0x53, 0x31,
    0x36, 0x79, 0x43, 0x1a, 0x11, 0x72, 0x3, 0x59,
    0x50, 0x2, 0x71, 0x7c, 0x34, 0x3e, 0x23, 0x24,
    0x26, 0x5b, 0x73, 0xf, 0x5e, 0x12, 0x54, 0xb,
    0x61, 0x35, 0x3c, 0x57, 0x48, 0x55, 0x63, 0x4a,
    0x13, 0x75, 0x45, 0x70, 0x47, 0xc, 0x2f, 0x21,
    0x17, 0x2e, 0x62, 0x49, 0x4b, 0x5c, 0x19, 0x51,
    0x69, 0x3b, 0x7e, 0xd, 0x3d, 0x67, 0x2c, 0x22,
    0x14, 0x42, 0x5a, 0x7f, 0x32, 0x1, 0x7, 0x7b,
    0x15, 0x4f, 0x16, 0x29, 0x30, 0x27, 0x20, 0x18,
    0x65, 0x6, 0x1c, 0x3f, 0x68, 0x2b, 0x4c, 0xa,
    0x1e, 0x46, 0x5d, 0x1f, 0x10, 0x6e, 0x56, 0x7a,
    0x1b, 0x9, 0x52, 0x38, 0x66, 0x7d, 0x41, 0x40,
    0x4, 0x6a, 0x39, 0x77, 0x33, 0x1d, 0x74, 0x4e,
    0xaf, 0xa6, 0x8c, 0xbd, 0x89, 0xa2, 0xa9, 0x9e,
    0xa1, 0x91, 0xb9, 0xad, 0xbf, 0xb7, 0x95, 0xa8,
    0xa5, 0x82, 0xaa, 0xa3, 0x94, 0x92, 0xb8, 0x87,
    0x88, 0xb1, 0x93, 0xbc, 0x80, 0xb5, 0xba, 0x99,
    0xab, 0xbe, 0x90, 0x8e, 0x83, 0x9f, 0x9a, 0x86,
    0x85, 0x98, 0xa4, 0xa0, 0xac, 0x9c, 0x96, 0x81,
    0xb0, 0x8d, 0xbb, 0xb2, 0x9d, 0xae, 0x84, 0x9b,
    0xb4, 0x8b, 0x97, 0xa7, 0xb3, 0x8a, 0x8f, 0xb6,
    0xc5, 0xc0, 0xc8, 0xd7, 0xde, 0xc4, 0xd1, 0xd2,
    0xd9, 0xcb, 0xcd, 0xd5, 0xcc, 0xc7, 0xdb, 0xdf,
    0xdc, 0xdd, 0xcf, 0xc6, 0xda, 0xc2, 0xc3, 0xc9,
    0xc1, 0xca, 0xd6, 0xd8, 0xce, 0xd3, 0xd0, 0xd4,
    0xe9, 0xe5, 0xe8, 0xe2, 0xe6, 0xeb, 0xe3, 0xec,
    0xed, 0xe7, 0xe1, 0xe4, 0xea, 0xef, 0xee, 0xe0,
    0xf6, 0xf0, 0xf4, 0xf5, 0xf2, 0xf3, 0xf7, 0xf1,
    0xfb, 0xf9, 0xfa, 0xf8, 0xfc, 0xfd, 0xfe, 0xff
};

void decode(u8 *data, u8 len)
{
    int i;

    for (i = 0; i < len; i++) {
        data[i] = decode_table[data[i]];
    }
}

enum _GOT_RESULT_ {
    GOT_NOTHING = 0,
    GOT_CHN_LOCK = 1,
    GOT_SSID_PASSWD = 2,
};
/*
 * get_ssid_passwd_from_action()
 *
 * @Note: see zconfig ios protocol
 * @Return: _GOT_RESULT_
 */
static inline int get_ssid_passwd_from_a(u8 *in, int len)
{
    int i, ssid_len, passwd_len;
    u8 *data = in;
    /* used by service type, max len 15B */
    char *magic_service_type = "az09";
    /* used by display name, max len 63B */
    char *magic_display_name = "wjws";
    /* used by prepare frame */
    char *magic_ssid = "channel@lock";

    len -= strlen(magic_service_type);

    for (i = 0; i < len; i++, data++) {
        if (!memcmp(data, magic_service_type, strlen(magic_service_type))) {
            //log("get magic service type\r\n");
            break;
        }
    }

    len -= strlen(magic_display_name);

    for (; i < len; i++, data++) {
        if (!memcmp(data, magic_display_name, strlen(magic_display_name))) {
            /* magic(4), len(1), ssid, len(1), passwd, checksum(2) */
            u8 *payload = data + strlen(magic_display_name);
            u16 crc, cal_crc;

            ssid_len = payload[0];
            memset(tmp_ssid, 0, ZC_MAX_SSID_LEN);
            memcpy(tmp_ssid, (char *)&payload[1], ssid_len);

            payload += ssid_len + 1;
            passwd_len = payload[0];
            if (passwd_len) {
                memcpy(tmp_passwd, &payload[1], passwd_len);
            } else {
                memset(tmp_passwd, 0, ZC_MAX_PASSWD_LEN);
            }

            payload += passwd_len + 1;
            crc = os_get_unaligned_be16(payload);

            data += strlen(magic_display_name);
            cal_crc = zconfig_checksum(data, ssid_len + passwd_len + 2);
            if (crc != cal_crc) {
                error("crc check error: recv 0x%x != 0x%x\r\n", crc, cal_crc);
                return GOT_NOTHING;
            }
            decode(tmp_ssid, ssid_len);
            decode(tmp_passwd, passwd_len);

            if (!memcmp(tmp_ssid, magic_ssid, ssid_len)) {
                return GOT_CHN_LOCK;    //channel lock
            }

            /* final result */
            strcpy((char *)zc_ssid, (char const *)tmp_ssid);
            strcpy((char *)zc_passwd, (char const *)tmp_passwd);
            return GOT_SSID_PASSWD;
        }
    }

    return GOT_NOTHING;
}

static int is_ascii_string(u8 *str)
{
    int i = 0;
    while (str[i] != '\0') {
        if (str[i] < 128) {
            i++;
        } else {
            return 0;
        }
    }
    return 1;
}

/*
 * get_ssid_passwd_from_wps()
 *
 * @Note: see andriod zconfig protocol
 *  android sdk limit sizeof(passwd) <= 23
 *
 * @Return: _GOT_RESULT_
 *
 * use src mac to do ssid completion
 */
static inline int get_ssid_passwd_from_w(u8 *in, int total_len, u8 *src)
{
    int ssid_len, passwd_len, ssid_truncated = 0;
    u16 crc, cal_crc;
    /* used by prepare frame */
    char *magic_p_w_d = "zl&ws";//FIXME: Maybe it will be dangerous when opening source.

#define W_LEN       (32)    //total_len
#define EXTRA_LEN   (3) //ssid_len(1B) + checksum(2B)
    if (!in || total_len <= 4) {
        return GOT_NOTHING;
    }

    /* attr_id(2) + attr_len(2) = 4 */
    in += 4;
    total_len -= 4;

    warn_on(total_len > W_LEN, "ssid len > 32\r\n");

    /* total_len: ssid_len(1B), ssid, passwd, crc(2B) */
    ssid_len = in[0];
    passwd_len = total_len - ssid_len - EXTRA_LEN; /* ssid_len(1B), crc(2B) */
    if (ssid_len > W_LEN - EXTRA_LEN || passwd_len < 0) {
        return GOT_NOTHING;
    }

    crc = os_get_unaligned_be16(in + 1 + ssid_len + passwd_len);
    cal_crc = zconfig_checksum_v2(in, 1 + ssid_len + passwd_len);
    if (crc != cal_crc) {
        info("crc check error: recv 0x%x != 0x%x\r\n", crc, cal_crc);
        /*
         * use zconfig_checksum_v2() because
         * java modified UTF-8, U+C080 equal U+00,
         * ssid & passwd & len can not be 0, but checksum may result 0,
         * so add checksum_v2() which avoid use 0.
         *
         * also decode_table[] avoid use 0
         */
        return GOT_NOTHING;
    }

#define MAC_LOCAL_ADMINISTERED_BIT  (0x02)
    memcpy(zc_android_src, src, ETH_ALEN);
    if (zc_android_src[0] & MAC_LOCAL_ADMINISTERED_BIT) {
        zc_android_src[0] &= ~MAC_LOCAL_ADMINISTERED_BIT;
        //info("android src: %02x%02x%02x\r\n", zc_android_src[0], src[1], src[2]);
    } else
        warn_on(1, "local administered bit not set: %02x%02x%02x\r\n",
                src[0], src[1], src[2]);

    in += 1;/* eating ssid_len(1B) */
    memset(tmp_ssid, 0, ZC_MAX_SSID_LEN);
    memset(tmp_passwd, 0, ZC_MAX_PASSWD_LEN);

    memcpy(tmp_ssid, in, ssid_len);
    in += ssid_len;
    if (passwd_len) {
        memcpy(tmp_passwd, in, passwd_len);
    }

    decode(tmp_ssid, ssid_len);
    decode(tmp_passwd, passwd_len);
    //info("%s, %s, %d\r\n", tmp_ssid, tmp_passwd, total_len);

    if (passwd_len && !memcmp(tmp_passwd, magic_p_w_d, passwd_len)) {
        //Note: when v2 rollback to v1, zc_preapre_ssid will useless
        strcpy((char *)zc_pre_ssid, (char const *)tmp_ssid);
        return GOT_CHN_LOCK;
    }

#define P2P_NAME_LEN    (20)    //SAMSUNG S4 max name length = 22
    if (!is_ascii_string((u8 *)tmp_ssid)) { //chinese ssid
        ssid_truncated = 1;    //in case of gbk chinese
    } else if (total_len > P2P_NAME_LEN) {
        ssid_truncated = 1;
    }

    if (ssid_truncated) {
        struct ap_info *ap_info;
        u8 *best_ssid;
        int cur_ssid_len = strlen((const char *)tmp_ssid);  //current_ssid
        int pre_ssid_len = strlen((const char *)zc_pre_ssid);   //prepare_ssid
        if (pre_ssid_len && pre_ssid_len < cur_ssid_len) {
            /* should not happen */
            warn_on(1, "pre:%s < cur:%s\r\n", zc_pre_ssid, tmp_ssid);
            best_ssid = tmp_ssid;/* current ssid */
        } else if (pre_ssid_len) {
            best_ssid = zc_pre_ssid;    /* prepare ssid */
        } else {
            best_ssid = tmp_ssid;    /* default use current ssid */
        }

        //info("ssid truncated, best ssid: %s\r\n", best_ssid);

        ap_info = zconfig_get_apinfo_by_ssid_suffix(best_ssid);
        if (ap_info) {
            info("ssid truncated, got ssid from aplist:%s\r\n", best_ssid);
            strncpy((char *)zc_ssid, ap_info->ssid, ZC_MAX_SSID_LEN);
        } else {
            if (zc_android_ssid[0] != '\0') {
                strncpy((char *)zc_ssid, (char *)zc_android_ssid, ZC_MAX_SSID_LEN);
                memcpy(zc_bssid, zc_android_bssid, ETH_ALEN);
            } else {
                return GOT_NOTHING;
            }
        }
    } else {
        strcpy((char *)zc_ssid, (char const *)tmp_ssid);
    }

    strcpy((char *)zc_passwd, (char const *)tmp_passwd);

    return GOT_SSID_PASSWD;
}

/* debug code */
void dump_pkgs(u8 tods)
{
    u8 package_num, i, data, score = score;
    u8 buf[128];

    /* package num */
    package_num = pkg_len(1) & 0x7F;/* total len, include len(1B) & crc(2B) */

    for (i = 1; i <= package_num; i++) {
        data = pkg_len(i);
        score = pkg_score(i);
        buf[i - 1] = data & 0x7F;
        info("%02x %s", score, (i % GROUP_NUMBER == 0) ? "  " : "");
    }
    info("\r\n");

    dump_hex(&buf[0], package_num, GROUP_NUMBER);
}

static inline int package_cmp(u8 *package, u8 *src, u8 *dst, u8 tods, u16 len)
{
    struct package *pkg = (struct package *)package;

    //  if (memcmp(pkg->src, src, ETH_ALEN)
    //      || memcmp(pkg->dst, dst, ETH_ALEN)
    //      || (pkg->len != len))
    //      return 1;

    if (pkg->len != len) {
        return 1;
    }
    return 0;
}

static inline void package_save(u8 *package, u8 *src, u8 *dst, u8 tods, u16 len)
{
    struct package *pkg = (struct package *)package;

    //  memcpy(pkg->src, src, ETH_ALEN);
    //  memcpy(pkg->dst, dst, ETH_ALEN);
    pkg->len = len;
}

/* len -= (rth->it_len + hdrlen); see ieee80211.c */
u8 zconfig_fixed_offset[ZC_ENC_TYPE_MAX + 1][2] = {
    {
        //open, none   //ip(20) + udp(8) + 8(LLC)
        36, 36
    },
    {
        //wep      iv(4) + data + ICV(4)
        44, 44 //feixun, wep64(10byte), wep128(26byte)
    },
    {
        //tkip     iv/keyID(4) + Ext IV(4) + data + MIC(8) + ICV(4)
        56, 56 //tkip(10byte, 20byte), wpa2+tkip(20byte)
    },
    {
        //aes      ccmp header(8) + data + MIC(8) + ICV(4)
        52, 52 //
    },
    {
        //tkip-aes
        56, 52, //fromDs==tkip,toDs==aes
    }
};

u16 zconfig_hint_frame[] = {/* GROUP_FRAME is not used, gourp 0 - 7 */
    START_FRAME, GROUP_FRAME + 1, GROUP_FRAME + 2, GROUP_FRAME + 3, GROUP_FRAME + 4,
    GROUP_FRAME + 5, GROUP_FRAME + 6, GROUP_FRAME + 7,
    0   /* NULL terminated */
};

/*
 * is_hint_frame()
 *
 * start frame or group frame can be used as a hint frame
 *
 * @Return:
 *  1/is start or group frame, otherwise return 0.
 */
static inline int is_hint_frame(u8 encry, int len, u8 *bssid, u8 *src, u8 channel, u8 tods, u16 sn)
{
    int i;
    struct ap_info *ap_info;

    if (encry > ZC_ENC_TYPE_MAX) {
        return 0;
    }

    len -= zconfig_fixed_offset[encry][0];  /* dont't care about tkip-aes */

    for (i = 0; zconfig_hint_frame[i]; i++) {
        if (zconfig_hint_frame[i] == len) {
            goto found_match;
        }
    }

    return 0;

found_match:
    /* tods/fromds already locked? */
    if (!memcmp(zc_bssid, zero_mac, ETH_ALEN)) {
        //zero mac means not locked
        memcpy(zc_bssid, bssid, ETH_ALEN);
        memcpy(zc_src_mac, src, ETH_ALEN);
    } else {
        /*
         * 1) src not equal, bssid equal, interference
         * 2) src not equal, bssid not equal, interference
         * 3) src equal, bssid equal, good, go on
         * 4) src equal, bssid not equal
         *  if tods is true, replace old ssid in case of WDS
         *  if fromds is true, APP change the AP?? or WDS??
         *      in this situation, zc_bssid is set by tods,
         *      in WDS case, zc_bssid should be unchanged
         */

        if (memcmp(zc_src_mac, src, ETH_ALEN)) {//case 1,2
            //someone must be working in aws at the same time
            warn_on(1, "%c interference src:"MAC_FORMAT", bssid:"MAC_FORMAT"\r\n",
                    flag_tods(tods), MAC_VALUE(src), MAC_VALUE(bssid));
            return 0;
        } else {
            if (memcmp(zc_bssid, bssid, ETH_ALEN)) {//case 4
                if (tods) {
                    memcpy(zc_bssid, bssid, ETH_ALEN);
                    memcpy(zc_src_mac, src, ETH_ALEN);
                    //TODO: clear previous buffer, channel lock state?
                    if (zconfig_data->data[0].state_machine == STATE_CHN_LOCKED_BY_BR) { //zc_state
                        zconfig_data->data[0].state_machine = STATE_CHN_SCANNING;
                    }
                    warn("%c WDS! bssid:"MAC_FORMAT" -> bssid:"MAC_FORMAT"\r\n",
                         flag_tods(tods), MAC_VALUE(zc_bssid),
                         MAC_VALUE(bssid));
                } else {
                    log("%c WDS? src:"MAC_FORMAT" -> bssid:"MAC_FORMAT"\r\n",
                        flag_tods(tods), MAC_VALUE(src),
                        MAC_VALUE(bssid));
                    return 0;
                }
            } //else case 3
        }
    }

    zc_frame_offset = zconfig_fixed_offset[encry][0];/* delta, len(80211) - len(8023) */
    zc_group_pos = i * GROUP_NUMBER;
    zc_cur_pos = zc_group_pos;
    zc_group_sn = sn;
    zc_prev_sn = sn;
    zc_score_uplimit = score_max;

    //fix channel with apinfo if exist, otherwise return anyway.
    ap_info = zconfig_get_apinfo(bssid);
    warn_on(ap_info && ap_info->encry[tods] > ZC_ENC_TYPE_MAX,
            "invalid apinfo ssid:%s\r\n", ap_info->ssid);

    if (ap_info && ap_info->encry[tods] == encry && ap_info->channel) {
        if (channel != ap_info->channel) {
            info("fix channel from %d to %d\r\n", channel, ap_info->channel);
            zc_channel = ap_info->channel;//fix by ap_info channel
        }
    } else {
        /* warning: channel may eq 0! */
    };

    if (ap_info) { /* save ssid */
        strcpy((char *)zc_ssid, ap_info->ssid);
    } else { /* in case of zc_ssid is dirty by v2 */
        memset(zc_ssid, 0, ZC_MAX_SSID_LEN);
    }

    return 1;
}

/*
 * get_data_score()
 *
 * calc package score
 *
 * @Return:
 *  score between [0, 100]
 */
static inline u8 get_data_score(u16 group_sn, u16 sn_now, u16 sn_last, u8 index_now, u8 index_last, u8 tods)
{
    /*
    example: 1
    8+3 250 0  d0e cc:fa:00:c8:cf:d2 > ff:ff:ff:ff:ff:ff
    8+4 2bf 0  d15 cc:fa:00:c8:cf:d2 > ff:ff:ff:ff:ff:ff    //两个包index_delta=1, sn_delta=7

    example: 2
    8+0, 3e1,  9a5
     8+1, 13f,  9a7
             group_sn=9a7, sn=9ab-9a7, pos=e-9, len=3ce     //here, index_delta=5, sn_delta=4
             group_sn=9a7, sn=9ac-9ab, pos=f-9, len=454
             group_sn=9a7, sn=9ad-9ac, pos=10-9, len=4d2
    example: 3
     8+3, 225,  a32
     8+6, 3c7,  a39     //此处应该是16+6, index_delta=3, sn_delta=7
    example: 4
    0+0, 4e0,  da5
     0+7, 441,  dab     //此处应该是8+7， index_delta=7, sn_delta=6
    0+0, 4e0,  d89
     0+8, 4c2,  d8f     //此处应该是8+8， index_delta=8, sn_delta=6

    //example: 4
    0+0 [100] 294 0 4e0
     0+1 [60] 2a2 0 11a
     0+2 [40] 2aa 0 181
             group_sn:2aa, sn:2b8-2aa=14, pos:3-2, len:20a
             group_sn:2aa, sn:2bc-2b8=18, pos:4-2, len:28a
             group_sn:2aa, sn:2c0-2bc=22, pos:5-2, len:310
             group_sn:2aa, sn:2c4-2c0=26, pos:6-2, len:391
             group_sn:2aa, sn:2c8-2c4=30, pos:7-2, len:412
             group_sn:2aa, sn:2cc-2c8=34, pos:8-2, len:493
    */
    static const u16 score_level[][2] = {
        {0,         0},
        {1,         2}, //include, example 1, 3
        {4,         8},
        {8,         16},//example 1
        {15,        30},
        {40,        40},
        {0xFFFF,    score_max}  //the end missing seq, example 2
    };

    u16 sn_delta = sn_minus(sn_now, group_sn) - 1;
    u16 index_delta = (index_now - index_last) - 1;
    u16 delta = sn_delta + index_delta;

    u8 i = 0;
    u8 res;

    /* suppose: sn > zc_prev_sn, pos > zc_cur_pos */
    if (sn_compare(sn_now, group_sn) <= 0 || sn_compare(sn_now, zc_prev_sn) <= 0) {
        return score_min;
    } else if (index_now <= index_last) {
        return score_min;
    }

    while (delta > score_level[i][0]) { /* include */
        i++;
    }

    res = score_level[i][1];

    if (zc_score_uplimit > res) {
        return zc_score_uplimit - res;
    } else {
        return score_low;
    }
}

/*
    遍历所有分组，找到最多匹配的分组: 分组号，匹配的起止位置，匹配的最小score
    遍历分两步：遍历相等的数量 和 遍历空位置的数量
    guess_pos: 根据前后位置确定的pos
    match_pos: 根据匹配计算的pos

    1) 如果guess_pos && match_pos存在，且相等，则score += 5, pos = match_pos
    2)                                  不相等，则score -= 5, pos = match_pos
    3) 只有guess_pos存在，则score = 2
    4) 只有match_pos存在，则score不变
*/
static inline int try_to_sync_pos(u8 tods, u16 last_sn, u8 sn, int last_group_pos, int group_pos)
{
    int ret = -1, empty_match = 0, reason = 0;
    int guess_pos = -1, final_pos = -1;

    int max_match = 0, match_group = -1, match_end = GROUP_NUMBER, match_score = 0;
    int match, i, j, score; //loop variable

retry:
    for (i = 0; i <= zconfig_get_data_len(); i += GROUP_NUMBER) {
        for (match = 0, score = score_max, j = 1;
             j <= GROUP_NUMBER; j++) {
            if (!tmp_score(j)) {
                continue;
            }

            if (empty_match) {
                if (pkg_score(i + j) <= 1) {
                    match++;
                    score = 1;
                }
            } else {
                if (!pkg_len(i + j)) {
                    continue;
                }
                if (pkg_len(i + j) == tmp_len(j)) {
                    match++;
                    score = (score > pkg_score(i + j)) ? pkg_score(i + j) : score;
                } else {/* encounter first unmatch */
                    log("[%d]=%x, [%d]=%x\r\n", i + j, pkg_len(i + j), j, tmp_len(j));
                    break;
                }
            }
        }
        if (match > max_match) {
            max_match = match;
            match_group = i;
            match_end = j - 1;
            match_score = score;
            log("match=%d, match_group=%d, match_end=%d\r\n",
                match, match_group, match_end);
        }
    }

    if (!max_match && !empty_match) {//retry empty place match
        empty_match = 1;
        goto retry;
    }

    if (group_pos != -1) {//根据后位置确定
        guess_pos = group_pos - GROUP_NUMBER;//前一组
        if (guess_pos < 0) {
            guess_pos = (zconfig_get_data_len() / GROUP_NUMBER) * GROUP_NUMBER;
        }

        if (!max_match || empty_match) {//case 3
            match_score = 2;
            final_pos = guess_pos;
            reason = 3;
            goto replace;
            //can not del goto, cause guess_pos has higher priority than empty match
        }
    }
    //前位置 有效性难以判断，忽略

    if (max_match > 0) {
        if (max_match == 1) {
            match_score = match_score > 10 ? 10 : match_score;
        } else if (max_match == 2) {
            match_score = match_score > 20 ? 20 : match_score;
        } else if (max_match <= GROUP_NUMBER) {
            match_score = match_score > 30 ? 30 : match_score;
        } else {
            goto clear;
        }

        if (guess_pos != -1) {
            if (guess_pos == match_group) {//case 1
                match_end = GROUP_NUMBER;
                match_score += 2;//bonus
                final_pos = match_group;
                reason = 1;
            } else {//case 2
                match_score -= 0;//bonus
                if (max_match >= 2 && !empty_match) {
                    final_pos = match_group;
                } else {
                    final_pos = guess_pos;
                }
                reason = 2;
            }
        } else {//case 4: 只有match_pos存在
            final_pos = match_group;

            reason = 4;
        }
    } else {
        goto clear;
    }

replace:
    if (final_pos != -1) {
        reason = reason;
        info("\tX = %d, score=%d, match=%d, reason=%d\r\n", final_pos, match_score, max_match, reason);
        if (match_end != GROUP_NUMBER) {
            info("\t match from [1-%d]\r\n", match_end);
        }
        for (i = final_pos + 1, j = 1; i <= final_pos + match_end; i++, j++) {
            if (pkg_score(i) < match_score && tmp_score(j)) {
                pkg_len(i) = tmp_len(j);
                pkg_score(i) = (match_score > tmp_score(j) - 1) ?
                               (match_score - (tmp_score(j) - 1)) : match_score;//TODO
                info("\t%d+%d [%d] %c %-3x\r\n", final_pos, j, pkg_score(i), flag_tods(tods), tmp_len(j));

                zc_replace = 1;
                if (zc_max_pos < i) {
                    zc_max_pos = i;
                }

                ret = 0;
            }
        }
    }

clear:
    zc_pos_unsync = 0;
    memset((u8 *)tmp(0), 0, sizeof(zconfig_data->tmp_pkg[0]));
    return ret;
}

/*
    判断同一个位置是否应发生替换
    调用该函数的前提: 同一个位置pos, 相同的得分score, 不同的数据
    替换条件: 在各分组相同位置下，旧的数据有重复，新的数据无重复
*/
static inline int try_to_replace_same_pos(int tods, int pos, int new_len)
{
    int replace = 0, i, old_match = 0, new_match = 0;

    for (i = pos % GROUP_NUMBER; i <= zconfig_get_data_len();
         i += GROUP_NUMBER) {
        if (i != pos && pkg_len(i) == pkg_len(pos)) {
            old_match = 1;
        }

        if (pkg_len(i) == new_len) {
            new_match = 1;
        }
    }

    if (old_match && !new_match) {
        replace = 1;
        pkg_len(pos) = new_len;
    }

    return replace;
}

/*
 * 1/locked, 0/not locked
 */
u8 is_channel_locked(void)
{
    return zconfig_channel_locked != INVALID_CHANNEL;
}

/*
 * Note: this notification will be kept called, in case of
 *  user clear the channel locked state to re-scanning
 *  channel because of waiting timeout.
 */
u8 zconfig_callback_channel_locked(u8 channel)
{
    if (channel != zconfig_channel_locked) {
        info("channel lock @ %d\r\n", channel);
        zconfig_channel_locked = channel;
    }

    /*
     * if recv timeout, vendor may re-scanning channel,
     * so keep calling channel locked notification here.
     */
    zconfig_channel_locked_callback(channel, 0, zc_bssid);

    return 0;
}

/*
 * [IN] ssid or bssid
 * [OUT] auth, encry, channel
 */
u8 zconfig_get_auth_info(u8 *ssid, u8 *bssid, u8 *auth, u8 *encry, u8 *channel)
{
    u8 *valid_bssid = NULL;
    struct ap_info *ap_info = NULL;

    /* sanity check */
    if (!bssid || !memcmp(bssid, zero_mac, ETH_ALEN)) {
        valid_bssid = NULL;
    } else {
        valid_bssid = bssid;
    }

    /* use mac or ssid to search apinfo */
    if (valid_bssid) {
        ap_info = zconfig_get_apinfo(valid_bssid);
    } else {
        ap_info = zconfig_get_apinfo_by_ssid(ssid);
    }

    if (ap_info) {
        if (auth) {
            *auth = ap_info->auth;
        }
        if (encry) {
            *encry = ap_info->encry[1];    /* tods side */
        }
        if (!valid_bssid && bssid) {
            memcpy(bssid, ap_info->mac, ETH_ALEN);
        }
        if (channel) {
            *channel = ap_info->channel;
        }

        return 1;
    }

    return 0;
}

u8 zconfig_callback_over(u8 *ssid, u8 *passwd, u8 *bssid)
{
    u8 auth = ZC_AUTH_TYPE_INVALID, encry = ZC_ENC_TYPE_INVALID, channel = 0;

    info("zconfig done. ssid:%s, passwd:%s, mac:%02x%02x%02x%02x%02x%02x\r\n", ssid, passwd,
         bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

    if (zconfig_finished) {
        return 0;
    }

    zconfig_get_auth_info(ssid, bssid, &auth, &encry, &channel);

    zconfig_got_ssid_passwd_callback(ssid, passwd, bssid, auth, encry, channel);

    zconfig_finished = 1;

    return 0;
}

//p2p channel should be 1, 6, 11
static inline int __fix_channel(u8 channel)
{
    if (channel <= 3) {
        return 1;
    } else if (channel <= 8) {
        return 6;
    } else if (channel <= 13) {
        return 11;
    } else {
        return INVALID_CHANNEL;
    }
}

static inline void zconfig_set_state(u8 state, u8 tods, u8 channel)
{
    /* state change callback */
    switch (state) {
        case STATE_CHN_SCANNING:
            break;
        case STATE_CHN_LOCKED_BY_P2P:
            //locked state used by action/wps frame
            zconfig_callback_channel_locked(__fix_channel(channel));
            break;
        case STATE_CHN_LOCKED_BY_BR:
            //locked state used by br frame
            zconfig_callback_channel_locked(zc_channel ? zc_channel : channel);
            break;
        case STATE_RCV_DONE:
            /*
             * in case of p2p/router, direct into RCV_DONE state,
             * skiped the chn lock state, so better to call channel lock here
             */
            if (!is_channel_locked()) {
                zconfig_callback_channel_locked(__fix_channel(channel));
            }
            zconfig_callback_over(zc_ssid, zc_passwd, zc_bssid);
            break;
        default:
            break;
    }

    /*
     * state machine loop:
     * scanning -> p2p lock -> rcv_done
     * scanning -> (p2p) rcv_done
     * scanning -> br lock -> (br) rcv_done
     * scanning -> br lock -> (p2p) recv_done
     * scanning -> p2p lock -> br lock -> (br) recv_done
     *
     * watch out zc_state rolling back.
     * zconfig_set_state(CHN_LOCKED) will be called more than once,
     */
    if (zc_state < state) {
        zc_state = state;
    }
}

/*
    pkt_data & pkt_length:
        radio_hdr + 80211 hdr + payload, without fcs(4B)
    return:
        PKG_INVALID -- invalid pkt,
        PKG_START_FRAME -- start frame,
        PKG_DATA_FRAME -- data frame,
        PKG_ALINK_ROUTER -- alink router,
        PKG_GROUP_FRAME -- group frame,
        PKG_BC_FRAME -- broadcast frame
*/
int is_invalid_pkg(void *pkt_data, u32 pkt_length)
{
#define MIN_PKG         (10)
#define MAX_PKG         (1480 + 56 + 200)
    if (pkt_length < MIN_PKG || pkt_length > MAX_PKG) {
        return 1;
    } else {
        return 0;
    }
}

int zconfig_recv_callback_router(struct parser_res *res)
{
    static char guest_beacon_counter;
    struct ap_info *ap_info;

    u8 tods = res->tods;
    u8 *bssid = res->bssid;
    u8 channel = res->channel;

    u8 *ie = res->u.ie.alink_ie;

    info("found ALINK router! IE:%02hhx-%02hhx-%02hhx-%02hhx-%02hhx-%02hhx\r\n",
         ie[0], ie[1], ie[2], ie[3], ie[4], ie[5]);

    ap_info = zconfig_get_apinfo(bssid);
    bug_on(ap_info == NULL, "no router info found\r\n");

    /*
     * workaround: fix router guest ssid bug
     * router may send beacon with alibaba ie and guest ssid,
     * but device can not communicate with App in guest network
     * workaround here to ignore the first 5 guest ssid beacon
     */
    if (strstr(ap_info->ssid, "Guest")) {
        if (guest_beacon_counter < 5) {
            guest_beacon_counter++;
            return PKG_START_FRAME;
        }
    }

    strcpy((char *)zc_ssid, ap_info->ssid);
    strcpy((char *)zc_passwd, (char *)zc_tpsk);
    memcpy(zc_passwd + ZC_TPSK_LEN, ie + ZC_ALINK_IE_HDR, 16);
    memcpy((char *)zc_bssid, bssid, ETH_ALEN);

    info("callback for router\r\n");

    zconfig_set_state(STATE_RCV_DONE, tods, channel);

    return PKG_END;
}


ATTR int zconfig_recv_callback_a(struct parser_res *res)
{
    u8 *data = res->u.action.data;
    u16 len = res->u.action.data_len;

    u8 tods = res->tods;
    u8 channel = res->channel;

    int ret = get_ssid_passwd_from_a(data, len);

    if (ret == GOT_CHN_LOCK) {
        info("callback for v2:%02x%02x%02x\r\n",
             res->src[0], res->src[1], res->src[2]);
        goto chn_locked;
    } else if (ret == GOT_SSID_PASSWD) {
        goto rcv_done;
    } else if (ret == GOT_NOTHING)
        //FIXME: ios discover req goes here
    {
        return PKG_INVALID;
    } else {
        return PKG_INVALID;
    }

chn_locked:
    zconfig_set_state(STATE_CHN_LOCKED_BY_P2P, tods, channel);
    return PKG_START_FRAME;
rcv_done:
    zconfig_set_state(STATE_RCV_DONE, tods, channel);
    return PKG_END;
}

ATTR int zconfig_recv_callback_w(struct parser_res *res)
{
    u8 *data = res->u.wps.data;
    u16 len = res->u.wps.data_len;

    u8 tods = res->tods;
    u8 channel = res->channel;

    int ret = get_ssid_passwd_from_w(data, len, res->src);
    if (ret == GOT_CHN_LOCK) {
        info("callback for v2:%02x%02x%02x\r\n",
             res->src[0], res->src[1], res->src[2]);
        goto chn_locked;
    } else if (ret == GOT_SSID_PASSWD) {
        goto rcv_done;
    } else if (ret == GOT_NOTHING) {
        return PKG_INVALID;
    } else {
        return PKG_INVALID;
    }

chn_locked:
    zconfig_set_state(STATE_CHN_LOCKED_BY_P2P, tods, channel);
    return PKG_START_FRAME;
rcv_done:
    zconfig_set_state(STATE_RCV_DONE, tods, channel);
    return PKG_END;
}

ATTR int zconfig_recv_callback_broadcast(struct parser_res *res)
{
    u32 timestamp = zconfig_get_time();

    u8 *src = res->src;
    u8 *dst = res->dst;
    u8 *bssid = res->bssid;
    u8 tods = res->tods;
    u8 channel = res->channel;

    u16 sn = res->u.br.sn;
    u16 len = res->u.br.data_len;
    u8 encry_type = res->u.br.encry_type;

    int ret, pkg_type = PKG_INVALID;
    u8 score = 0, timeout = 0, equal = 0;

    u16 pos = 0, index = 0;


#ifdef PRINT_FRAME_INFO //debug for new-platform
    info("len=%d, %c, sn=%d, enc=%d, chn=%d, src=%02x%02x%02x%02x%02x%02x\r\n",
         len, flag_tods(tods), sn, encry_type, channel,
         src[0], src[1], src[2], src[3], src[4], src[5]);
#endif

#if 0   //test code, only recv fromDs (from AP)
    if (tods == 1) {
        goto drop;
    }
#endif

    /*
     * STATE_CHN_LOCKED_BY_P2P is set by v2 wps/action frame, which means
     * APP is sending v2, but if v2 is fail, APP will rollback to v1,
     * so still need to parse br frame here
     * even zc_state == STATE_CHN_LOCKED_BY_P2P.
     */
    if (zc_state == STATE_CHN_LOCKED_BY_P2P
        || zc_state == STATE_CHN_SCANNING) {
        if (is_hint_frame(encry_type, len, bssid, src, channel, tods, sn)) {
            info("hint frame: offset:%d, %c, sn:%x\r\n",
                 zc_frame_offset, flag_tods(tods), sn);

            info("src:%02x%02x%02x%02x%02x%02x, bssid:%02x%02x%02x%02x%02x%02x\r\n",
                 src[0], src[1], src[2], src[3], src[4], src[5],
                 bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);

            pkg_type = PKG_START_FRAME;
            zconfig_set_state(STATE_CHN_LOCKED_BY_BR, tods, channel);

            goto update_sn;
        } else if (!memcmp(zc_android_src, src, ETH_ALEN)) {
            struct ap_info *ap_info = zconfig_get_apinfo(bssid);
            if (ap_info) {
                strncpy((char *)zc_android_ssid, ap_info->ssid, ZC_MAX_SSID_LEN);
                memcpy(zc_android_bssid, bssid, ETH_ALEN);
                info("src %02x%02x%02x match %02x%02x%02x\r\n",
                     zc_android_src[0], zc_android_src[1],
                     zc_android_src[2], zc_android_bssid[0],
                     zc_android_bssid[1], zc_android_bssid[2]);
            }
        }
    } else if (zc_state == STATE_CHN_LOCKED_BY_BR) {
        /* same src mac & br & bssid */
        if (memcmp(&src[0], zc_src_mac, ETH_ALEN) ||
            memcmp(&dst[0], br_mac, sizeof(br_mac)) ||
            memcmp(bssid, zc_bssid, ETH_ALEN)) { //in case of WDS
            goto drop;
        }

        if (timestamp - zc_timestamp > time_interval) {
            info("\t\t\t\t\ttimestamp = %d\r\n", timestamp - zc_timestamp);
            timeout = 1;
        }

        ret = sn_compare(sn, zc_prev_sn);
        if (ret == 0) {
            log("drop: %3x == %3x\r\n", sn, zc_prev_sn);//log level, too many retry pkg
            goto drop;
        } else if ((sn_compare(sn, zc_prev_sn) < 0) && (!timeout)) {//if timeout, goto pos_unsync
            info("drop: %3x < %3x\r\n", sn, zc_prev_sn);//TODO: better not drop
            goto update_sn;//FIXME: update sn???
        }

        //assert(sn > zc_prev_sn && !timeout);
        if (len <= zc_frame_offset) { /* length invalid */
            goto drop;
        }

        len -= zc_frame_offset;

        if (is_data_frame(len)) {
            pkg_type = PKG_DATA_FRAME;
            index = get_data_index(len);
            pos = zc_group_pos + index;
            /*
             * pos_unsync: 进入条件,任一条
             *  case1: index rollback
             *  case2: index equal but len not equal
             *  case3: data frame & timeout
             * 退出条件：
             *  case1: 进入条件同时也是退出条件
             *  case2: 收到同步帧
             */
            if (index < zc_last_index || (index == zc_last_index && len != zc_last_len)
                || timeout) {
                if (zc_pos_unsync) {/* already in pos_unsync state */
                    log("\texit try_to_sync_pos: re-enter!\r\n");
                    try_to_sync_pos(tods, zc_prev_sn, sn, zc_group_pos, -1);
                }
                zc_pos_unsync = 1;/* also a new start */
                if (index < zc_last_index) {
                    log("\tenter try_to_sync_pos: rollback \r\n");
                } else if (timeout) {
                    log("\tenter try_to_sync_pos: timeout \r\n");
                } else {
                    log("\tenter try_to_sync_pos: != \r\n");
                }
            }
pos_unsync:
            if (zc_pos_unsync) {/* tmp save */
                package_save((u8 *)tmp(index), src, dst, tods, len);
                if (zc_pos_unsync == 1) {
                    tmp_score(index) = 1;
                } else {
                    tmp_score(index) = (sn - zc_prev_sn);    //TODO: index? last_tmp_score
                }
                zc_pos_unsync++; /* unsync pkg counter */
                info("\tX+%d [%d] %-3x %c %-3x\r\n", index, tmp_score(index), sn, flag_tods(tods), len);
                goto update_sn;//FIXME: update prev_sn or not?
            }

            // assert(sn > zc_prev_sn && pos > zc_cur_pos)
            score = get_data_score(zc_group_sn, sn, zc_prev_sn, pos, zc_cur_pos, tods);
            if (score == score_min) {//better not drop any pkg here
                info("\t drop: group_sn:%x, sn:%x-%x=%x, pos:%d-%d, len:%x\r\n",
                     zc_group_sn, sn, zc_prev_sn, sn_minus(sn, zc_group_sn), pos, zc_cur_pos, len);
                goto update_sn;
            } else {
                if (zc_score_uplimit > score) {
                    zc_score_uplimit = score;    /* inherit last limit */
                }

                zc_group_sn = sn;//TODO
                info("%d+%d [%d] %-3x %c %-3x\r\n", zc_group_pos, index, score, sn, flag_tods(tods), len);
            }
        } else {
            if (is_start_frame(len) || is_group_frame(len)) {
                u8 group = get_group_index(len);

                if (zc_pos_unsync) {
                    log("\texit try_to_sync_pos: group frame\r\n");
                    try_to_sync_pos(tods, zc_prev_sn, sn, zc_group_pos, group);
                }

                zc_cur_pos = group;
                zc_group_pos = group;
                zc_group_sn = sn;
                zc_score_uplimit = score_max;

                info("%d+%d [%d] %-3x %c %-3x\r\n", group, 0, zc_score_uplimit, sn, flag_tods(tods), len);

                //ignore PKG_GROUP_FRAME here
                pkg_type = PKG_START_FRAME;

                /*
                 * keep calling zconfig_set_state(), see Note about
                 * zconfig_callback_channel_locked()
                 */
                zconfig_set_state(STATE_CHN_LOCKED_BY_BR, tods, channel);

                //zc_replace may happen in try_to_sync_pos(), so goto is_recv_completed
                goto is_recv_completed;
            } else {
                info("\t invalid len = %d\r\n", len + zc_frame_offset);
                goto drop;
            }
        }

        /* start from pkg(1), leave pkg(0) for start frame */
        if (pos >= MAX_PKG_NUMS || pos <= 0) {
            log("msg index(%d) out of range!\r\n", pos);
            goto drop;
        }

        zc_cur_pos = pos;

        /*
           score now > last:
           1) data equal:   pkg_score = now
           2) not equal:    pkg_score = now, data replace
           score now == last:
           1) data equal:   pkg_score++ and ???
           2) not equal:    pkg_score cut down & give warning & try_to_replace
           score now < last:
           1) data equal:   score_uplimit up???
           2) not equal:    goto pos_unsync
         */
        equal = !package_cmp((u8 *)pkg(pos), src, dst, tods, len);

        if (score > pkg_score(pos)) {
            pkg_score(pos) = score; //update score first
            if (!equal) {
                zc_replace = 1;
                package_save((u8 *)pkg(pos), src, dst, tods, len);
                //dump_mac(src, bssid);
            }
        } else if (score == pkg_score(pos)) {/* range check ? */
            if (equal) {
                pkg_score(pos)++;
            } else { //not equal
                int replace = try_to_replace_same_pos(tods, pos, len);
                if (!replace) {
                    pkg_score(pos) /= 2;
                    if (score >= score_mid)//better not happen
                        warn("xxxxxxxx warn: pos=%d, score=[%d], %x != %x\r\n",
                             pos, score, pkg_len(pos), len);
                } else {
                    info("\t replace @ %d, len=%x\r\n", pos, len);
                }
            }
        } else {//pkg_score(pos) > score
            if (!equal) {/* data not equal */
                if (!zc_pos_unsync) {
                    zc_pos_unsync = 1;
                    log("\tenter try_to_sync_pos: data mismatch\r\n");
                    goto pos_unsync;
                } //else {} should not reach here
            } else {/* data equal */
                if (zc_score_uplimit >= score_mid && pkg_score(pos) - score < 10) {
                    u8 uplimit = (zc_score_uplimit + pkg_score(pos)) / 2;
                    if (zc_score_uplimit != uplimit) {
                        info("\t\t\t uplimit [%d] -> [%d]\r\n", zc_score_uplimit, uplimit);
                    }
                    zc_score_uplimit = uplimit;
                }
            }
        }

is_recv_completed:
        zc_max_pos = (zc_max_pos < zc_cur_pos) ? zc_cur_pos : zc_max_pos;
        if (zc_replace && zconfig_recv_completed(tods)) {
            zc_replace = 0;
            if (!zconfig_get_ssid_passwd(tods)) {
                /* we got it! */
                zconfig_set_state(STATE_RCV_DONE, tods, channel);
                return PKG_END;
            }
        }
    }

update_sn:
    zc_prev_sn = sn;

    zc_timestamp = timestamp;
    zc_last_index = index;
    zc_last_len = len;

    return pkg_type;

drop:
    return PKG_INVALID;
}

const char *zc_default_ssid = "aha";
const char *zc_default_passwd = "12345678";

int zconfig_recv_callback_default_ssid(struct parser_res *res)
{
    u8 tods = res->tods;
    u8 channel = res->channel;

    info("found default ssid: %s %s\r\n", zc_default_ssid, zc_default_passwd);

    strcpy((char *)zc_ssid, zc_default_ssid);
    strcpy((char *)zc_passwd, zc_default_passwd);

    zconfig_set_state(STATE_RCV_DONE, tods, channel);
    return PKG_END;
}

/* return 0 for success, -1 devid not match, otherwise return -2 */
int (*vendor_decrypt_ssid_passwd)(u8 *ie, u8 ie_len,
                                  u8 out_ssid[ZC_MAX_SSID_LEN],
                                  u8 out_passwd[ZC_MAX_PASSWD_LEN],
                                  u8 out_bssid[6]);

int zconfig_recv_callback_zero_config(struct parser_res *res)
{
    u8 tods = res->tods;
    u8 channel = res->channel;

    u8 *ie = res->u.ie.alink_ie;
    u8 ie_len = ie[1];
    int ret;

    if (!vendor_decrypt_ssid_passwd) {
        return PKG_INVALID;
    }

    if (res->u.ie.alink_ie_len < ie_len) {
        return PKG_INVALID;
    }

    ret = vendor_decrypt_ssid_passwd(ie, ie_len, zc_ssid, zc_passwd, zc_bssid);
    if (ret) {
        return PKG_INVALID;
    }

    info("Registrar decrypt %s.\r\n", !ret ? "success" : "failed");

    zconfig_set_state(STATE_RCV_DONE, tods, channel);

    return PKG_END;
}

/*  Variable
 *  Qos: 2Byte
 *  auth offset: 36 44 52 56
 *  group frame: 4e0 3e1~3e7
 */

#ifdef  AWSS_SUPPORT_HT40
u8 ht40_hint_frame_cnt[64];
u8 ht40_filter;
char ht40_rssi_low, ht40_rssi_high;
#define ht40_rssi_range         (15)    /* suggested by Fuzhibo */
/* for ios: start frame interval is 20/100 ms */
#define HIT_FRAME_PER_CHANNEL   (2)

#define IEEE80211_MIN_HDRLEN    (24)

u32 ht40_timestamp;
u8 ht40_channel[ZC_MAX_CHANNEL + 1];
u8 ht40_channel_filter[ZC_MAX_CHANNEL + 1];
u8 ht40_state;

void ht40_init(void)
{
    ht40_state = 0;
    ht40_filter = 0;
    ht40_rssi_low = 0;
    ht40_rssi_high = 0;
    ht40_timestamp = 0;
    memset(ht40_channel, 0, sizeof(ht40_channel));
    memset(ht40_channel_filter, 0, sizeof(ht40_channel_filter));
    memset(ht40_hint_frame_cnt, 0, sizeof(ht40_hint_frame_cnt));
}

int ht40_lock_channel(u8 channel, u8 filter)
{
    if (1 <= channel && channel <= 13) {
        if (!ht40_channel[channel]) {//replace when 0
            ht40_channel[channel]++;
            ht40_channel_filter[channel] = filter;
        } else {
            if (filter == ht40_channel_filter[channel]) {
                ht40_channel[channel]++;    /* increase */
            } else {
                ht40_channel[channel]--;    /* decrease */
            }
        }

        if (ht40_channel[channel] >= HIT_FRAME_PER_CHANNEL) {
            return 1;
        }
    } else {
        warn("bad channel %d\r\n", channel);
    }

    return 0;
}

int ht40_scanning_hint_frame(u8 filter, char rssi, u32 length, u8 channel)
{
    u8 channel_locked = 0, next_loop = 0;
    int hint_pos = -1;
    int tods = 1;
    int i, j, k;

    if (ht40_state != STATE_CHN_SCANNING) {
        return -1;
    }

    /* range check, max: 0x4e0 + tkip + qos, min: 0x3e0 + open */
    if (length > START_FRAME + zconfig_fixed_offset[2][0] + 2
        || length <= GROUP_FRAME + zconfig_fixed_offset[0][0]) {
        return -1;
    }

    for (i = 1; i >= 0; i--) //Qos or not
        for (j = 3; j >= 0; j--) //auth type, without open
            for (k = 0; k < 8; k++) {//group frame
                if (zconfig_hint_frame[k] + zconfig_fixed_offset[j][0] + i * 2 == length) {
                    hint_pos = i * 32 + j * 8 + k;
#if 1
                    info("\r\nfilter:%x, rssi:%d, len:%d, Qos:%d, auth:%d, group:%d, %s\r\n",
                         filter, rssi, length, i, j, k,
                         next_loop ? "DUP" : "");
#endif
                    if (!next_loop) {
                        channel_locked = ht40_lock_channel(channel, filter);
                        if (channel_locked) {
                            zconfig_set_state(STATE_CHN_LOCKED_BY_BR, tods, channel);

                        }
                        next_loop = 1;/* don't enter this loop again */
                    }

                    ht40_hint_frame_cnt[hint_pos]++;
                }
            }//end of for

    if (channel_locked) {
        ht40_rssi_high = rssi + ht40_rssi_range;
        if (ht40_rssi_high > -1) {
            ht40_rssi_high = -1;
        }
        ht40_rssi_low = rssi - ht40_rssi_range;
        if (ht40_rssi_low < -128) {
            ht40_rssi_low = -128;
        }

        ht40_filter = filter;

        info("filter:%x, rssi range:[%d, %d]\r\n",
             filter, ht40_rssi_low, ht40_rssi_high);
    }

    return hint_pos;
}

int ht40_get_qos_auth_group_info(u32 length)
{
    int count = 0, max_count = 0, max_count_pos = 0;
    int continues = 0, max_continues = 0, max_continues_pos = 0, second_continues = 0;
    int tods = 1;
    int i, j, k;

    if (zc_state != STATE_CHN_LOCKED_BY_BR
        || ht40_state != STATE_CHN_SCANNING) {
        return 0;
    }

    for (i = 1; i >= 0; i--) //Qos or not
        for (j = 3; j >= 0; j--) //auth type
            for (count = 0, continues = 0, k = 0; k < 8; k++) {//group frame
                int pos = i * 32 + j * 8 + k;

                if (ht40_hint_frame_cnt[pos]) {
                    count += ht40_hint_frame_cnt[pos];
                    if (count > max_count) {//NOTE: not >=, see continues
                        max_count = count;
                        max_count_pos = pos;
                    }

                    continues += 1;
                    if (continues >= max_continues) {
                        second_continues = max_continues;
                        max_continues = continues;
                        max_continues_pos = pos;
                    }
                }
            }

    info("max_cont:%d, sec_cont:%d, max_count:%d, max_cont_pos:%d, max_count_pos:%d\r\n",
         max_continues, second_continues, max_count, max_count_pos, max_continues_pos);

    if (max_continues > second_continues // not >=
        && max_count_pos == max_continues_pos) {
        u8 qos = max_count_pos / 32;
        u8 auth = (max_count_pos % 32) / 8;

        zc_frame_offset = zconfig_fixed_offset[auth][0] + qos * 2;
        length -= zc_frame_offset;
        if (is_start_frame(length) || is_group_frame(length)) {
            u8 group = get_group_index(length);

            zc_group_pos = group;
            zc_cur_pos = group;
            zc_score_uplimit = score_mid;
            ht40_timestamp = os_get_time_ms();

            ht40_state = STATE_RCV_IN_PROGRESS;
            info("len:%d, qos:%d, auth:%d, group:%d, offset:%d\r\n",
                 length, qos, auth, group, zc_frame_offset);
        } else {
            warn("invalid length:%d\r\n", length);
        }
    }

    return 0;
}

int zconfig_recv_callback_ht40_ctrl(u8 filter, char rssi, u32 length, u8 channel)
{
    u32 now = os_get_time_ms();
    int pkg_type = PKG_INVALID;
    u8 tods = 1, timeout = 0, equal = 0, score = 0;
    u16 pos = 0, index = 0, len = 0;

    int hint_pos = -1;

    if (length > IEEE80211_MIN_HDRLEN) {
        length -= IEEE80211_MIN_HDRLEN;
        len = length;
    } else {
        goto drop;
    }

    hint_pos = ht40_scanning_hint_frame(filter, rssi, length, channel);

    if (hint_pos >= 0) {
        ht40_get_qos_auth_group_info(length);
        return PKG_GROUP_FRAME;
    }

    if (ht40_state == STATE_RCV_IN_PROGRESS) {
        if (rssi <= ht40_rssi_low && rssi >= ht40_rssi_high) {
            goto drop;
        }
        if (filter != ht40_filter) {
            goto drop;
        }
        if (len <= zc_frame_offset) { /* length invalid */
            goto drop;
        }

        len -= zc_frame_offset;

        if (is_data_frame(len)) {
            pkg_type = PKG_DATA_FRAME;
            index = get_data_index(len);
            pos = zc_group_pos + index;

            if (now - ht40_timestamp > time_interval) {
                info("\t\t\t\t\ttimestamp = %d, pos:%d, len:%x\r\n",
                     now - ht40_timestamp, pos, len);
                timeout = 1;
                goto drop;
            }

            /*
             * pos_unsync: 进入条件,任一条
             *  case1: index rollback
             *  case2: index equal but len not equal
             *  case3: data frame & timeout
             * 退出条件：
             *  case1: 进入条件同时也是退出条件
             *  case2: 收到同步帧
             */
            if (pos < zc_cur_pos) {
                info("drop: index rollback. prev:%d, cur:%d\n", zc_cur_pos, pos);
                goto drop;
            }

            if (pos == zc_cur_pos && len != pkg_len(zc_cur_pos)) {
                info("drop: index equal, but len not. prev:%x, cur:%x\n",
                     pkg_len(pos), len);
                pkg_score(pos)--;
                goto drop;
            }

            if (pos > zc_cur_pos + 4) {
                info("drop: over range too much, prev:%d, cur:%d\n",
                     zc_cur_pos, pos);
                goto drop;
            }

            score = zc_score_uplimit - (pos - zc_cur_pos - 1);
            zc_score_uplimit = score;

            info("%d+%d [%d] -- T %-3x\r\n", zc_group_pos, index, score, len);
            /*
               score now > last:
               1) data equal:   pkg_score = now
               2) not equal:    pkg_score = now, data replace
               score now == last:
               1) data equal:   pkg_score++ and ???
               2) not equal:    pkg_score cut down & give warning & try_to_replace
               score now < last:
               1) data equal:   score_uplimit up???
               2) not equal:    goto pos_unsync
               */
            equal = !package_cmp((u8 *)pkg(pos), NULL, NULL, tods, len);

            if (score > pkg_score(pos)) {
                pkg_score(pos) = score; //update score first
                if (!equal) {
                    zc_replace = 1;
                    package_save((u8 *)pkg(pos), NULL, NULL, tods, len);
                }
            } else if (score == pkg_score(pos)) {/* range check ? */
                if (equal) {
                    pkg_score(pos)++;
                } else {
                    pkg_score(pos)--;
                }
            } else {//pkg_score(pos) > score
                /* do nothing */
            }

            zc_cur_pos = pos;
            zc_max_pos = (zc_max_pos < zc_cur_pos) ? zc_cur_pos : zc_max_pos;
            if (zc_replace && zconfig_recv_completed(tods)) {
                zc_replace = 0;
                if (!zconfig_get_ssid_passwd(tods)) {
                    /* we got it! */
                    zconfig_set_state(STATE_RCV_DONE, tods, channel);
                    return PKG_END;
                }
            }

            ht40_timestamp = now;
            return pkg_type;

        } else {
            if (is_start_frame(len) || is_group_frame(len)) {
                u8 group = get_group_index(len);

                if (!group || group == zc_group_pos + 8) {
                    zc_group_pos = group;
                    zc_score_uplimit = score_mid;

                    if (zc_cur_pos + 1 == group) {
                        pkg_score(zc_cur_pos) += 1;
                    }

                    zc_cur_pos = group;

                    info("%d+%d [%d] -- T %-3x\r\n", group, 0, zc_score_uplimit, len);

                    //ignore PKG_GROUP_FRAME here
                    pkg_type = PKG_START_FRAME;
                    ht40_timestamp = now;
                    return pkg_type;
                }
            }
        }
    }

drop:
    return PKG_INVALID;
}
#endif

/*
 * zconfig_recv_callback()
 *
 * ieee80211 package parser
 *
 * @Return:
 *  zconfig state
 */
ATTR int zconfig_recv_callback(void *pkt_data, u32 pkt_length, u8 channel,
                               int link_type, int with_fcs)
{
    int data_type = ALINK_INVALID;
    struct parser_res res;

    /* remove FCS filed */
    if (with_fcs) {
        pkt_length -= 4;
    }

    /* useless, will be removed */
    if (is_invalid_pkg(pkt_data, pkt_length)) {
        return PKG_INVALID;
    }

#ifdef AWSS_SUPPORT_HT40
    if (link_type == AWSS_LINK_TYPE_HT40_CTRL) {
        struct ht40_ctrl *ctrl = pkt_data;
        return zconfig_recv_callback_ht40_ctrl(ctrl->filter, ctrl->rssi, pkt_length, channel);
    }
#endif


    memset(&res, 0, sizeof(res));
    res.channel = channel;

    data_type = ieee80211_data_extract(pkt_data, pkt_length, link_type, &res);

    switch (data_type) {
        case ALINK_ROUTER:
            return zconfig_recv_callback_router(&res);
        case ALINK_ACTION:
            return zconfig_recv_callback_a(&res);
        case ALINK_WPS:
            return zconfig_recv_callback_w(&res);
        case ALINK_BROADCAST:
            return zconfig_recv_callback_broadcast(&res);
        case ALINK_DEFAULT_SSID:
            return zconfig_recv_callback_default_ssid(&res);
        case ALINK_ZERO_CONFIG:
            return zconfig_recv_callback_zero_config(&res);
        default:
            return PKG_INVALID;
    }
}

#include "git_version.h"

/* hexdump -C libzconfig.so |grep -C 3 '2015' */
static const char *zconfig_version = ALINK_AGENT_GIT_VERSION;

const char *zconfig_lib_version(void)
{
    return zconfig_version;
}

/* init mem & timer */
void zconfig_init(char *model, char *secret)
{
    info("awss lib %s\r\n", zconfig_version);

    zconfig_channel_locked = INVALID_CHANNEL;
    zconfig_finished = 0;

    zconfig_data = (struct zconfig_data *)zconfig_malloc(sizeof(struct zconfig_data));
    bug_on(!zconfig_data, "os_malloc failed!\r\n");
    memset(zconfig_data, 0, sizeof(struct zconfig_data));

    zconfig_aplist = (struct ap_info *)zconfig_malloc(sizeof(struct ap_info) * MAX_APLIST_NUM);
    bug_on(!zconfig_aplist, "os_malloc failed!\r\n");
    memset(zconfig_aplist, 0, sizeof(sizeof(struct ap_info) * MAX_APLIST_NUM));

    zconfig_calc_tpsk(model, secret, (char *)zc_tpsk, ZC_TPSK_LEN + 1);

    info("tpsk: %s\r\n", zc_tpsk);
#if 0
    ut_test();
#endif

#ifdef AWSS_SUPPORT_HT40
    ht40_init();
#endif
}

void zconfig_destroy(void)
{
    if (zconfig_data) {
        zconfig_free((void *)zconfig_data);
        zconfig_data = NULL;
    }
    if (zconfig_aplist) {
        zconfig_free((void *)zconfig_aplist);
        zconfig_aplist = NULL;
        zconfig_aplist_num = 0;
    }
}
