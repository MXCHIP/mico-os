/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef _AWS_PLATFORM_H_
#define _AWS_PLATFORM_H_
/*
 * platform porting API
 */

//一键配置超时时间, 建议超时时间1-3min, APP侧一键配置1min超时
int aws_timeout_period_ms(void);

//一键配置每个信道停留时间, 建议200ms-400ms
int aws_chn_scanning_period_ms(void);

//系统自boot起来后的时间, 用于判断收包之间的间隔时间
unsigned int platform_get_time_ms(void);

//aws库会调用该函数用于信道切换之间的sleep
void platform_msleep(int ms);

//系统malloc/free函数
void *platform_malloc(int size);
void platform_free(void *ptr);

//系统打印函数, 可不实现
void awss_platform_printf(int log_level, const char *log_tag, const char *file,
                          const char *fun, int line, const char *fmt, ...);

//product model/secret, i.e.
//model     "ALINKTEST_LIVING_LIGHT_SMARTLED"
//secret    "YJJZjytOCXDhtQqip4EjWbhR95zTgI92RVjzjyZF"
char *product_get_model(void);
char *product_get_secret(void);
//wifi mac string, format xx:xx:xx:xx:xx:xx
//Note: return NULL if product is registered by sn
char *os_wifi_get_mac(void);
//product sn string
//Note: return NULL if product is registered by mac
char *product_get_sn(void);
//return alink version
//return value:
//10 -- alink 1.0
//11 -- alink 1.1
//20 -- alink 2.0
int platform_alink_version(void);

//进入monitor模式, 并做好一些准备工作，如
//设置wifi工作在默认信道6
//若是linux平台，初始化socket句柄，绑定网卡，准备收包
//若是rtos的平台，注册收包回调函数aws_recv_80211_frame()到系统接口
void platform_monitor_open(void);

//退出monitor模式，回到station模式, 其他资源回收
void platform_monitor_close(void);

//wifi信道切换，信道1-13
void platform_channel_switch(char primary_channel, char secondary_channel,
                             char bssid[6]);

//通过以下函数发送配网成功通知给APP, 端口定义如下
#define UDP_TX_PORT         (65123)
#define UDP_RX_PORT         (65126)
int platform_broadcast_notification(char *msg, int msg_num);

#endif
