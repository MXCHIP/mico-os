/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>
#include <stdio.h>
#include "aws_lib.h"
//#include "aws_os.h"
#include "zconfig_lib.h"
#include "zconfig_utils.h"
#include "zconfig_config.h"
#include "zconfig_protocol.h"
#include "enrollee.h"
#include "os.h"

/* aws state machine */
enum {
    /* used by aws_state */
    AWS_SCANNING,
    AWS_CHN_LOCKED,
    AWS_SUCCESS,
    AWS_TIMEOUT,

    /* used by aws_stop */
    AWS_STOPPING,
    AWS_STOPPED
};

struct aws_info {
    u8 state;

    u8 cur_chn; /* current working channel */
    u8 chn_index;

    u8 locked_chn;
    u8 locked_bssid[6];

#define AWS_MAX_CHN_NUMS    (2 * 13 + 5)    /* +5 for safety gap */
    u8 chn_list[AWS_MAX_CHN_NUMS];

    u32 chn_timestamp;/* channel start time */
    u32 start_timestamp;/* aws start time */
    u8  stop;
} *aws_info;

struct aws_notify_info {
    char *model;
    char *mac;
    char *sn;

#define AWS_NOTIFY_MSG_LEN  (512)
    char *notify_msg;
} *aws_notify_info;

#define aws_state       (aws_info->state)
#define aws_locked_chn      (aws_info->locked_chn)
#define aws_locked_bssid    (&aws_info->locked_bssid[0])
#define aws_cur_chn     (aws_info->cur_chn)
#define aws_chn_index       (aws_info->chn_index)
#define aws_chn_list        (aws_info->chn_list)
#define aws_chn_timestamp   (aws_info->chn_timestamp)
#define aws_start_timestamp (aws_info->start_timestamp)
#define aws_stop        (aws_info->stop)
#define aws_model       (aws_notify_info->model)
#define aws_mac         (aws_notify_info->mac)
#define aws_sn          (aws_notify_info->sn)
#define aws_notify_msg      (aws_notify_info->notify_msg)

#define aws_channel_lock_timeout_ms (4 * 1000)

static const u8 aws_fixed_scanning_channels[] = {
    1, 6, 11, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13
};

/*
 * sniffer result/storage
 * use global variable/buffer to keep it usable after zconfig_destroy
 */
u8 aws_result_ssid[ZC_MAX_SSID_LEN];
u8 aws_result_passwd[ZC_MAX_PASSWD_LEN];
u8 aws_result_bssid[ETH_ALEN];/* mac addr */
u8 aws_result_auth = ZC_AUTH_TYPE_INVALID;
u8 aws_result_encry = ZC_ENC_TYPE_INVALID;
u8 aws_result_channel = 0;


void zconfig_channel_locked_callback(u8 primary_channel,
                                     u8 secondary_channel, u8 *bssid)
{
    aws_locked_chn = primary_channel;
    if (bssid) {
        memcpy(aws_locked_bssid, bssid, 6);
    }

    if (aws_state == AWS_SCANNING) {
        aws_state = AWS_CHN_LOCKED;
    }
}

void zconfig_got_ssid_passwd_callback(u8 *ssid, u8 *passwd,
                                      u8 *bssid, u8 auth, u8 encry, u8 channel)
{
    if (bssid) {
        info("ssid:%s, passwd:%s, bssid:%02x%02x%02x%02x%02x%02x, %s, %s, %d\r\n",
             ssid, passwd,
             bssid[0], bssid[1], bssid[2],
             bssid[3], bssid[4], bssid[5],
             zconfig_auth_str(auth), zconfig_encry_str(encry),
             channel);
    } else {
        info("ssid:%s, passwd:%s, bssid:--, %s, %s, %d\r\n",
             ssid, passwd,
             zconfig_auth_str(auth), zconfig_encry_str(encry),
             channel);
    }

    memcpy(aws_result_ssid, ssid, ZC_MAX_SSID_LEN);
    memcpy(aws_result_passwd, passwd, ZC_MAX_PASSWD_LEN);

    if (bssid) {
        memcpy(aws_result_bssid, bssid, ETH_ALEN);
    }
    aws_result_auth = auth;
    aws_result_encry = encry;
    aws_result_channel = channel;

    aws_state = AWS_SUCCESS;
}

u8 aws_next_channel(void)
{
    /* aws_chn_index start from -1 */
    while (1) {
        aws_chn_index++;
        if (aws_chn_index >= AWS_MAX_CHN_NUMS) {
            aws_chn_index = 0;    //rollback to start
        }
        if (aws_chn_list[aws_chn_index]) {  //valid channel
            break;
        }
    }

    aws_cur_chn = aws_chn_list[aws_chn_index];

    return aws_cur_chn;
}

void aws_switch_channel(void)
{
    int channel = aws_next_channel();
    os_awss_switch_channel(channel, 0, NULL);
    aws_chn_timestamp = os_get_time_ms();

    info("channel %d\r\n", channel);

}

enum {
    CHNSCAN_ONGOING, /* no timeout, continue */
    CHNSCAN_NEXT_CHN, /* should swith to next channel */
    CHNSCAN_TIMEOUT /* aws timeout */
};

ATTR int aws_is_chnscan_timeout(void)
{
    if (aws_stop == AWS_STOPPING) {
        info("aws will stop...\r\n");
        return CHNSCAN_TIMEOUT;
    }

    if (time_elapsed_ms_since(aws_chn_timestamp) > os_awss_get_channelscan_interval_ms()) {
        if ((0 != os_awss_get_timeout_interval_ms()) &&
            (time_elapsed_ms_since(aws_start_timestamp) > os_awss_get_timeout_interval_ms()))

        {
            return CHNSCAN_TIMEOUT;
        } else {
            return CHNSCAN_NEXT_CHN;
        }
    }

    return CHNSCAN_ONGOING;
}

int zconfig_add_active_channel(int channel)
{
    if (zconfig_is_valid_channel(channel)) {
        int fixed_channel_nums = sizeof(aws_fixed_scanning_channels);
        aws_chn_list[fixed_channel_nums + channel] = channel;
        return 0;
    }
    return -1;
}

/*
 * platform like mc300, 雄迈 depend on auth & encry type
 * so keep scanning if auth & encry is incomplete
 */
ATTR int aws_force_scanning(void)
{
#ifdef WITH_AUTH_ENCRY
    int timeout = sizeof(aws_fixed_scanning_channels) / sizeof(u8)
                  * os_awss_get_timeout_interval_ms() * 2; /* 2 round */

    /* force scanning useful only when aws is success */
    if (aws_state != AWS_SUCCESS) {
        return 0;
    }

    //channel scanning at most 2 round
    if (time_elapsed_ms_since(aws_start_timestamp) >= timeout) {
        return 0;//timeout
    } else {
        /*
         * for platform which need auth & encry, retry to get auth info
         */
        int found = zconfig_get_auth_info(aws_result_ssid, aws_result_bssid,
                                          &aws_result_auth, &aws_result_encry,
                                          &aws_result_channel);
        if (found) {
            return 0;
        } else {
            return 1;    /* keep scanning */
        }
    }
#else
    //no need scanning a round
    return 0;
#endif
}

/*
 * channel scanning/re-scanning control
 * Note: 修改该函数时，需考虑到各平台差异
 * 庆科平台：
 * --aws_switch_channel() 为空
 * --zconfig_destroy()会被调用两次，一次被aws_main_thread_fun()，一次被庆科驱动
 * linux/rtos平台差异
 * --vendor_recv_80211_frame()有实现，rtos平台该函数通常为空，通过注册callback方式收包
 */
void aws_main_thread_func(void)
{
    aws_start_timestamp = os_get_time_ms();

    /* channel switch init */
    aws_switch_channel();

rescanning:
    //start scaning channel
    while (aws_state == AWS_SCANNING || aws_force_scanning()) {
        switch (aws_is_chnscan_timeout()) {
            case CHNSCAN_ONGOING:
                break;
            case CHNSCAN_NEXT_CHN:
                aws_switch_channel();
                break;
            case CHNSCAN_TIMEOUT:
                goto timeout_scanning;
            default:
                break;
        }

        /* 80211 frame handled by callback */
        os_msleep(100);

        awss_broadcast_enrollee_info();
    }

    //channel lock
    info("[channel scanning] %d ms\r\n",
         time_elapsed_ms_since(aws_start_timestamp));

    /*
     * make sure switch to locked channel,
     * in case of inconsistent with aws_cur_chn
     */
    info("final channel %d\r\n", aws_locked_chn);
    os_awss_switch_channel(aws_locked_chn, 0, aws_locked_bssid);

    while (aws_state != AWS_SUCCESS) {
        /* 80211 frame handled by callback */
        os_msleep(100);

        if (aws_is_chnscan_timeout() == CHNSCAN_TIMEOUT) {
            goto timeout_recving;
        }

        if (aws_state == AWS_SCANNING) {
            info("channel rescanning...\n");
            goto rescanning;
        }
    }

    info("[channel recving] %d ms\r\n",
         time_elapsed_ms_since(aws_start_timestamp));

    goto success;

timeout_scanning:
    info("aws timeout scanning!\r\n");
timeout_recving:
    info("aws timeout recving!\r\n");
    aws_state = AWS_TIMEOUT;

    //os_softap_setup();

success:
    os_awss_close_monitor();
    /*
     * zconfig_destroy() after os_awss_monitor_close() beacause
     * zconfig_destroy will release mem/buffer that
     * zconfig_recv_callback will use
     *
     * Note: hiflying will reboot after calling this func, so
     *  aws_get_ssid_passwd() was called in os_awss_monitor_close()
     */
    zconfig_destroy();
}

char *platform_strdup(char *str)
{
    if (!str) {
        return NULL;
    }

    int size = strlen(str) + 1;
    char *dest = os_malloc(size);

    return strncpy(dest, str, size);
}

int aws_create_msg(char *model, char *mac, char *sn)
{
    aws_notify_msg = os_malloc(AWS_NOTIFY_MSG_LEN);
    warn_on(!aws_notify_msg, "os_malloc failed!\r\n");

    memset(aws_notify_msg, 0, AWS_NOTIFY_MSG_LEN);

    /*
     * FIXME: no able to backward compatible
     *
     * TODO: currently android APP only parse mac or sn,
     * when they fix this problem(directly pass mac & sn info to cloud),
     * we will send mac & sn info to APP.
     */
    snprintf(aws_notify_msg, AWS_NOTIFY_MSG_LEN,
             "{\"version\":\"1.6\",\"model\":\"%s\",\"sn\":\"%s\"}",
             model, sn);

    return 0;
}

void aws_destroy_msg(void)
{
    if (aws_notify_msg) {
        os_free(aws_notify_msg);
        aws_notify_msg = NULL;
    }
}

#define UDP_TX_PORT         (65123)
#define UDP_RX_PORT         (65126)

int aws_broadcast_notification(char *msg, int msg_num)
{
    int i, ret, timeout, result = 0;
    long fd;
    void *read_fds[OS_SOCKET_MAXNUMS];

    platform_netaddr_t netaddr = {"255.255.255.255", UDP_TX_PORT};

    int buf_len = 1024;
    char *buf = os_malloc(buf_len);

    for (i = 0; i < OS_SOCKET_MAXNUMS; i++) {
        read_fds[i] = OS_INVALID_FD;
    }

    fd = (long)os_udp_server_create(UDP_RX_PORT);
    OS_ASSERT(fd >= 0, "awss create socket failed!\r\n");

    //send notification
    for (i = 0; i < msg_num; i++) {
        ret = os_udp_sendto((void *)fd, msg, strlen(msg), &netaddr);
        if (ret < 0) {
            os_printf("awss send notify msg ERROR!\r\n");
        } else {
            os_printf("awss notify %d times, %s\r\n", i, msg);
        }

        read_fds[0] = (void *)fd;
        timeout = (200 + i * 100); //from 200ms to N * 100ms, N = 25

        ret = os_select(read_fds, NULL, timeout);
        if (ret > 0) {
            ret = os_udp_recvfrom((void *)fd, buf, buf_len, NULL);
            if (ret) {
                buf[ret] = '\0';
                os_printf("rx: %s\n", buf);
                result = 1;
                break;
            }
        }

        //vendor_msleep(200 + i * 100);
    }

    os_free(buf);
    os_udp_close((void *)fd);

    return result;
}

void aws_notify_app_prepare(void)
{
    aws_notify_info = os_malloc(sizeof(struct aws_notify_info));
    if (!aws_notify_info) {
        return;
    }
    memset(aws_notify_info, 0, sizeof(struct aws_notify_info));

    aws_model = os_malloc(OS_PRODUCT_MODEL_LEN);
    aws_mac = os_malloc(OS_MAC_LEN);
    aws_sn = os_malloc(OS_PRODUCT_SN_LEN);

    os_product_get_model(aws_model);
    os_wifi_get_mac_str(aws_mac);
    os_product_get_sn(aws_sn);

    if (!aws_model || (!aws_mac && !aws_sn)) {
        warn_on(1, "model, mac, sn is empty!\r\n");
    }

    info("model:%s, mac:%s, sn:%s\r\n", aws_model, aws_mac, aws_sn);

    aws_create_msg(aws_model, aws_mac, aws_sn);
    info("aws_notify_msg: %s\r\n", aws_notify_msg);
}

void aws_notify_app_unprepare(void)
{
    aws_destroy_msg();

    if (aws_model) {
        os_free(aws_model);
    }
    if (aws_mac) {
        os_free(aws_mac);
    }
    if (aws_sn) {
        os_free(aws_sn);
    }

    aws_model = NULL;
    aws_mac = NULL;
    aws_sn = NULL;

    os_free(aws_notify_info);
    aws_notify_info = NULL;
}

/*
 * send broadcast msg to APP to stop the aws process.
 * this func will block 10~20s.
 * IMPORTANT:
 *      Calling this func as soon as wifi connected(dhcp ready), but
 *      if alink need to do unbind/factory_reset, calling this func after
 *      alink_wait_connect(NULL, 10), 10 means timeout 10s.
 */
#define AWS_NOTIFY_APP_TIMES    (50)
void aws_notify_app(void)
{
    aws_notify_app_prepare();
    aws_broadcast_notification(aws_notify_msg, AWS_NOTIFY_APP_TIMES);
    aws_notify_app_unprepare();
}

/* return 1 means got APP ack or notify timeout */
int aws_notify_app_nonblock(void)
{
    static int times;
    int result = 0;
    if (!times) {
        times++;
        aws_notify_app_prepare();
        result = aws_broadcast_notification(aws_notify_msg, 1);
    } else if (times < AWS_NOTIFY_APP_TIMES) {
        times++;
        result = aws_broadcast_notification(aws_notify_msg, 1);
    } else {
        result = 1;
    }

    if (result) {
        aws_notify_app_unprepare();
        times = 0;
    }

    return result;
}

int aws_80211_frame_handler(char *buf, int length, enum AWSS_LINK_TYPE link_type, int with_fcs)
{
    static unsigned int lock_start;

    int ret = zconfig_recv_callback(buf, length, aws_cur_chn, link_type, with_fcs);

    if (aws_state == AWS_CHN_LOCKED) {
        switch (ret) {
            case PKG_START_FRAME:
            case PKG_DATA_FRAME:
            case PKG_GROUP_FRAME:
                lock_start = os_get_time_ms();
                break;
            default:
                if (time_elapsed_ms_since(lock_start) > aws_channel_lock_timeout_ms) {              /* set to rescanning */
                    aws_state = AWS_SCANNING;
                }
                break;
        }
    }

    return ret;
}

#ifdef CONFIG_AWSS_BLE
extern int alink_ble_init();
extern void alink_ble_deinit();
#endif
void aws_start(char *model, char *secret, char *mac, char *sn)
{
    aws_info = os_malloc(sizeof(struct aws_info));
    if (!aws_info) {
        return;
    }
    memset(aws_info, 0, sizeof(struct aws_info));

    aws_state = AWS_SCANNING;

    /* start from -1 */
    aws_chn_index = 0xff;
    memcpy(aws_chn_list, aws_fixed_scanning_channels,
           sizeof(aws_fixed_scanning_channels));

    memset(aws_result_ssid, 0, sizeof(aws_result_ssid));
    memset(aws_result_passwd, 0, sizeof(aws_result_passwd));
    memset(aws_result_bssid, 0, sizeof(aws_result_bssid));
    aws_result_auth = ZC_AUTH_TYPE_INVALID;
    aws_result_encry = ZC_ENC_TYPE_INVALID;
    aws_result_channel = 0;

    char *model_str = os_malloc(OS_PRODUCT_MODEL_LEN);
    char *secret_str = os_malloc(OS_PRODUCT_SECRET_LEN);

    zconfig_init(os_product_get_model(model_str), os_product_get_secret(secret_str));

    os_free(model_str);
    os_free(secret_str);

#ifdef CONFIG_AWSS_BLE
    alink_ble_init();
#endif

    os_awss_open_monitor(aws_80211_frame_handler);

    awss_init_enrollee_info();

    aws_main_thread_func();
}

void aws_destroy(void)
{
    if (aws_info) {
        aws_stop = AWS_STOPPING;
        while (aws_state != AWS_SUCCESS && aws_state != AWS_TIMEOUT) {
            os_msleep(100);
        }

        os_free(aws_info);


        aws_info = NULL;

        awss_destroy_enrollee_info();

#ifdef CONFIG_AWSS_BLE
        alink_ble_deinit();
#endif
    }
}

int aws_get_ssid_passwd(char *ssid, char *passwd, unsigned char *bssid,
                        char *auth, char *encry, unsigned char *channel)
{
    if (aws_state == AWS_SUCCESS) {
        if (ssid) {
            strncpy(ssid, (char *)aws_result_ssid, ZC_MAX_SSID_LEN);
        }
        if (passwd) {
            strncpy(passwd, (char *)aws_result_passwd, ZC_MAX_PASSWD_LEN);
        }
        if (bssid) {
            memcpy(bssid, aws_result_bssid, ETH_ALEN);
        }
        if (auth) {
            *auth = aws_result_auth;
        }
        if (encry) {
            *encry = aws_result_encry;
        }
        if (channel) {
            *channel = aws_result_channel;
        }
        return 1;
    } else {
        return 0;
    }
}

const char *aws_version(void)
{
    return zconfig_lib_version();
}

