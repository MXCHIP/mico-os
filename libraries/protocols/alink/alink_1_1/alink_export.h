/*
 * Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
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

#ifndef _ALINK_EXPORT_H_
#define _ALINK_EXPORT_H_

typedef int (*alink_func) (void *, void *);
typedef int (*alink_func_rw)(unsigned char *buffer, unsigned int len);
typedef int (*alink_func_get_stats) (int type, char *stats);

#define MAX_SYS_CALLBACK 1
enum ALINK_SYSTEM_CALLBACK {
    ALINK_FUNC_SERVER_STATUS = 0
};

/*
  *  ALINK_FUNC_SERVER_STATUS. Example:
  *  int func(void* mac, void* status) {
  *      char* mac_str = (char)* mac;
  *      int i_status = *(int*)status;
  *      .... // Do your own logic.
  *  }
  */

/******************************************/
#define ALINK_TRUE 		(1)
#define ALINK_FALSE 		(0)

#define ALINK_OK 		(0)
#define ALINK_ERR 		(-1)
#define ALINK_SOCKET_ERR 	(-2)
#define ALINK_ERR_NO_MEMORY 	(-3)
#define ALINK_WAIT_FOREVER	(-1)

#define MAX_NAME_LEN	(80 + 1)
#define STR_TOKEN_LEN	(64 + 1)
#define STR_AUTHTOKEN_LEN	(64 + 1)
#define STR_ADMIN_LEN	(32 + 1)
#define STR_UUID_LEN	(32 + 1)
#define STR_SN_LEN	(64 + 1)
#define STR_MODEL_LEN	(80 + 1)
#define STR_MAC_LEN	(17 + 1)/* include : */
#define STR_KEY_LEN	(20 + 1)
#define STR_SEC_LEN	(40 + 1)
#define STR_UTC_LEN	(18)
#define STR_MSG_LEN	(128)
#define STR_NAME_LEN            (32 + 1)
#define STR_CID_LEN             (64 + 1)

#define STR_SHORT_LEN (32)
#define STR_LONG_LEN (128)

enum ALINK_CALLBACK {
    ACB_GET_DEVICE_STATUS = 0,
    ACB_SET_DEVICE_STATUS = 1,
    ACB_SET_DEVICE_STATUS_ARRAY = 2,
    ACB_REQUEST_REMOTE_SERVICE = 3,
    ACB_GET_DEVICE_STATUS_BY_RAWDATA = 4,
    ACB_SET_DEVICE_STATUS_BY_RAWDATA = 5,
    ACB_SET_DEVICE_DEBUGCONFIG = 6,
    ACB_GET_DEVICE_DEBUGCONFIG = 7,
    ACB_REQUEST_DEVICE_UPGRADE = 8,
    ACB_REQUEST_DEVICE_UPGRADE_CANCEL = 9,
    ACB_REQUEST_DEVICE_UNUPGRADE = ACB_REQUEST_DEVICE_UPGRADE_CANCEL ,
    MAX_CALLBACK_NUM
};

enum ALINK_SYSTEM_FUNC {
    ALINK_FUNC_AVAILABLE_MEMORY = 0,
    ALINK_FUNC_PREPARE_NETWORK,
    ALINK_FUNC_WAITING_NETWORK,
    ALINK_FUNC_SET_UTC_TIME,
    ALINK_FUNC_GET_STATISTICS,
    ALINK_FUNC_MAX
};

enum ALINK_STATUS {
	ALINK_STATUS_LINK_DOWN = 0,
	ALINK_STATUS_LINK_UP = 1,
	ALINK_STATUS_INITED = 1,
	ALINK_STATUS_AUTHTOKEN_RCVD,
	ALINK_STATUS_REGISTERED,
	ALINK_STATUS_LOGGED,
	ALINK_STATUS_LOGOUT
};

enum ALINK_NETWORK {
    ALINK_NW_UP = 0,
    ALINK_NW_DOWN
};

enum ALINK_LOGLEVEL_BIT {
    ALINK_LL_NONE_BIT = -1,
    ALINK_LL_FATAL_BIT,
    ALINK_LL_ERROR_BIT,
    ALINK_LL_INFO_BIT,
    ALINK_LL_DEBUG_BIT,
    ALINK_LL_TRACE_BIT,
    ALINK_LL_MAX_BIT
};

enum JSONTYPE {
    JNONE = -1,
    JSTRING = 0,
    JOBJECT,
    JARRAY,
    JNUMBER,
    JTYPEMAX
};

#define ALINK_LL_NONE  0
#define ALINK_LL_FATAL (1 << ALINK_LL_FATAL_BIT)
#define ALINK_LL_ERROR (1 << ALINK_LL_ERROR_BIT)
#define ALINK_LL_INFO  (1 << ALINK_LL_INFO_BIT)
#define ALINK_LL_DEBUG (1 << ALINK_LL_DEBUG_BIT)
#define ALINK_LL_TRACE (1 << ALINK_LL_TRACE_BIT)
typedef enum {
    MEMUSED = 0,
    DISCONCOUNTER,
    DISAPCOUNTER,
    MALFORMEDPACKETS,
    LATESTRTT,
    AVERAGERTT,
    WIFISTRENGTH,
    MAX_STATS
} STATSTYPE;

/******************************************/
typedef struct alink_down_cmd {
    int id;
    int time;
    char *account;
    char *param;
    int method;
    char uuid[STR_UUID_LEN];
    char *retData;
} alink_down_cmd, *alink_down_cmd_ptr;

typedef struct alink_up_cmd {

    /*If the command is not a response of any down-cmd, resp_id MUST be set to -1 */
    int resp_id;
    int emergency;
    const char *target;
    const char *param;

} alink_up_cmd, *alink_up_cmd_ptr;

/******************************************/
typedef int (*alink_callback) (alink_down_cmd_ptr);

typedef int (alink_JsonParse_CB)(char* p_cName, int iNameLen, char* p_cValue, int iValueLen, int iValueType, void* p_Result);
/******************************************/
struct device_info {
    /* optional */
    char sn[STR_SN_LEN];
    char name[STR_NAME_LEN];
    char brand[STR_NAME_LEN];
    char type[STR_NAME_LEN];
    char category[STR_NAME_LEN];
    char manufacturer[STR_NAME_LEN];
    char version[STR_NAME_LEN];

    /* mandatory */
    /* must follow the format xx:xx:xx:xx:xx:xx */
    char mac[STR_MAC_LEN];

    /* manufacturer_category_type_name */
    char model[STR_MODEL_LEN];

    /* mandatory for gateway, optional for child dev */
    char cid[STR_CID_LEN];
    char key[STR_KEY_LEN];
    char secret[STR_SEC_LEN];

    alink_callback dev_callback[MAX_CALLBACK_NUM];
    alink_func sys_callback[MAX_SYS_CALLBACK];
    char ip[16];
    char key_sandbox[STR_KEY_LEN];
    char secret_sandbox[STR_SEC_LEN];
};

/******************************************/


/******************************************/

#ifdef __cplusplus
extern "C" {
#endif

/***
 * @desc:    Configure sdk features, such as default thread number of thread pool
 * @para:    config: configure that need to be set
 * @retc:    None/Always Success.
 */

/***
 * @desc:    Start ALINK-SDK. Note alink_config should be called first if there's any special settings.
 * @para:    dev: Device info. If ALINK-SDK is running on gateway, the 'dev' MUST implies the gateway device.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_start(struct device_info *dev);

/***
 * @desc:    End ALINK-SDK. Note this function returns when all thread has already been shutdown.
 * @para:    None
 * @retc:    Always return ALINK_OK in normal mode. Returns ALINK_ERR if DEBUG_MEM is set and MEMORY_CHECK is failed.
 */
    int alink_end();

/***
 * @desc:    Attach a new sub-device to ALINK SDK and server. This API is supposed to return immediatly.
 * @para:    dev: Device info of sub-device.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_attach_sub_device(struct device_info *dev);

/***
 * @desc:    Detach an existed sub-device to ALINK SDK and server. This API is supposed to return immediatly.
 * @para:    dev: Device info of sub-device.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_detach_sub_device(struct device_info *dev);

#ifdef ALINK_SUB_DEVICE
/***
 * @desc:    Remove device information by id from both server and local.
 * @para:    id: Mac address. ("xx:xx:xx:xx:xx:xx") or SN (max len refs STR_SN_LEN)
 *           if(id == NULL): reset the device itself
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_factory_reset(const char *id);
#else
/***
 * @desc:    Remove all device information (including sub-device) from both server and local.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_factory_reset(void);
#endif

/***
 * @desc:    Set special callbacks to ALINK-SDK.
 * @para:    method:
 *               func:
 * @retc:    None/Always Success.
 */
    int alink_set_callback(int method, alink_func func);

/***
 * @desc:    Get alink time. The SDK sync with server every 30 minutes
 *           or anytime if SDK is notified by the server if the timestamp is out order.
 * @para:    None
 * @retc:    utc time, the number of seconds since Epoth.
 */
    unsigned int alink_get_time(void);

/***
 * @desc:    Post device data. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    Refer to ALINK protocol API returns
 */
    int alink_post_device_data(alink_up_cmd_ptr cmd);
/***
 * @desc:    Post device data array. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_post_device_data_array(alink_up_cmd_ptr cmd);

#ifdef ALINK_BONJOUR_SERVICE
/***
 * @desc:    Post relate device. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_post_relate_devices(alink_up_cmd_ptr cmd);
#endif

/***
 * @desc:    Register remote service. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_register_remote_service(alink_up_cmd_ptr cmd);
/***
 * @desc:    Unregister remote service. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_unregister_remote_service(alink_up_cmd_ptr cmd);
/***
 * @desc:    Post remote service response. Refer to ALINK protocol.
 * @para:    cmd: Command for the API. Memory referred by cmd is ready to release right after the function returns.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_post_remote_service_rsp(alink_up_cmd_ptr cmd);

/***
 * @desc:    Normally, the ALINK-SDK is responsible to decide when to send the message to server
             if alink_up_cmd_ptr.emergency is set to ALINK_FALSE. However, one could demand ALINK-SDK to send
             all reserved messages (if any) immediatly by calling this method.
 * @para:    None
 * @retc:    ALINK_OK/ALINK_ERR (Only if ALINK is not started)
 */
    int alink_flush();

/***
 * @desc:    Get uuid by device mac or sn. Set mac or sn to NULL if one wish to get the main device's uuid (also by main device's mac or sn).
 * @para:    id: Mac address. ("xx:xx:xx:xx:xx:xx") or SN (max len refs STR_SN_LEN)
 * @retc:    NULL if mac or sn does NOT match any device. Blank string ("") if the uuid is not set yet.
 */
    const char *alink_get_uuid(const char *id);

/***
 * @desc:    Get device mac by uuid. Set uuid to NULL if one wish to get the main device's mac (also by main device's uuid).
 * @para:    uuid(max len refs STR_UUID_LEN)
 * @retc:    NULL if uuid does NOT match any device. Mac address. ("xx:xx:xx:xx:xx:xx").
 */
    const char *get_mac_by_uuid(const char *uuid);


/***
 * @desc:    Wait until alink is connected to server and successfully logged in, or simple timeout.
 *               The function can be interrupted in other thread by calling alink_wakeup_connect_waiting.
 * @para:    id: Device mac or sn of which the function is waiting. Set to NULL implies the main device.
                  timeout: seconds before the function timeout, set to -1 if wait forever.
 * @retc:    ALINK_OK if alink is connected to server and successfully logged in. ALINK_ERR if timeout or being wakeup.
 */
    int alink_wait_connect(const char *id, int timeout /*second */ );

/***
 * @desc:    Wake up the device waiting function.
 * @para:    id: Device mac or sn of which the function was waiting. Set to NULL implies the main device.
 * @retc:    None. Return immediately.
 */
    void alink_wakeup_connect_waiting(const char *id);

/***
 * @desc:    Set alink loglevel, default is ALINK_LL_ERROR.
 * @retc:    None.
 */
    void alink_set_loglevel(int loglevel);

/***
 * @desc:    backup device data to alink server
 * @para:    id: Device mac or sn. Set to NULL implies the main device.
 *           key/val: key pair that will be backup.
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_backup_data(char *id, char *key, char *val);

/***
 * @desc:    retrieve device data from alink server
 * @para:    id: Device mac or sn. Set to NULL implies the main device.
 *           key: name of key to be retrieved
 *           val: buffer to hold the return string
 *           val_len: input/output params.
 *                    input: size of val buffer;
 *                    output: len of data retrieved from server,
 *                    if ouput_size > input_size, copy at most input_size to val buffer
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_retrieve_data(char *id, char *key, char *val, int *val_len);

/***
 * @desc:    retrieve device status from alink server
 * @para:    id: Device mac or sn. Set to NULL implies the main device.
 *           buf: buffer to hold the return string
 *           buf_len: input/output params.
 *                    input: size of buffer;
 *                    output: len of data retrieved from server,
 *                    if ouput_size > input_size, copy at most input_size to buffer
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_sync_device_status(char *id, char *buf, int *buf_len);

#ifdef ALINK_SUB_DEVICE
/***
 * @desc:    retrieve model info from alink server
 * @para:    mid: model id, correspond with model info
 *           model: return model from alink server
 *           input: mid; output: mode
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_get_mode_info(int mid, char *mode);

/***
 * @desc:    retrieve subdevice dkey from alink server
 * @para:    id: sub device mac or sn
 *           mode: sub device mode info
 *           rkey: random key
 *           ckey: challenge key
 *           dkey: device key
 *           input: id, mode, rkey, ckey; output: dkey
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_get_sub_device_key(char *id, char *mode, char *rkey, char *ckey, char *dkey);
#endif

/***
 * @desc:    generate password for device to connect alink router
 * @para:    ie: alink router IE
 *           model: product model
 *           secret: product secret
 *           passwd: output: buffer to hold the generated password
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_get_passwd(unsigned char *ie, char *model, char *secret, char passwd[64]);

/***
 * @desc:    enable/disable sandbox mode
 * @para:    bEnabled: sandbox switch
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_enable_sandbox_mode();

/***
 * @desc:    set networking status
 * @para:    state: The current network connection status
 * @retc:    None. Return immediately.
 */
    //void alink_update_network_status(int status);
    int alink_register_cb(int method , void  *func);

/***
 * @desc:    parse json string
 * @para:    p_cJsonStr: json string pointer
 *           iStrLen:  json string length
 *           pfnCB: callback function to handle each <name>:<value> pair which is parsed from json string
 *           p_Result: params which used in callback function
 * @retc:    ALINK_OK/ALINK_ERR
 */
    int alink_JsonParseNameAndValue(char* p_cJsonStr, int iStrLen, alink_JsonParse_CB pfnCB, void* p_CBData);

/***
 * @desc:    parse json string, return value by name
 * @para:    p_cJsonStr: json string pointer
 *           iStrLen:  json string length
 *           p_cName: name string pointer
 *           p_iValueLen(output): lenth of the value
 *           p_iValueType(output): type of the value
 * @retc:    NULL or json string pointer
 */
    char* alink_JsonGetValueByName(char* p_cJsonStr, int iStrLen, char* p_cName, int* p_iValueLen, int* p_iValueType);

/***
 * @desc:    get json object from array, return object by position
 * @para:    p_cJsonStr: json string pointer
 *           iStrLen:  json string length
 *           iPos: position in array, start from "0"
 *           p_ObjLen(output): lenth of the object
 * @retc:    NULL or json object pointer
 */
    char* alink_JsonArrayGetObject(char* p_cJsonStr, int iStrLen, int iPos, int* p_ObjLen);



/* auth type */
enum AWS_AUTH_TYPE {
	AWS_AUTH_TYPE_OPEN,
	AWS_AUTH_TYPE_SHARED,
	AWS_AUTH_TYPE_WPAPSK,
	AWS_AUTH_TYPE_WPA8021X,
	AWS_AUTH_TYPE_WPA2PSK,
	AWS_AUTH_TYPE_WPA28021X,
	AWS_AUTH_TYPE_WPAPSKWPA2PSK,
	AWS_AUTH_TYPE_MAX = AWS_AUTH_TYPE_WPAPSKWPA2PSK,
	AWS_AUTH_TYPE_INVALID = 0xff,
};

/* encry type */
enum AWS_ENC_TYPE {
	AWS_ENC_TYPE_NONE,
	AWS_ENC_TYPE_WEP,
	AWS_ENC_TYPE_TKIP,
	AWS_ENC_TYPE_AES,
	AWS_ENC_TYPE_TKIPAES,
	AWS_ENC_TYPE_MAX = AWS_ENC_TYPE_TKIPAES,
	AWS_ENC_TYPE_INVALID = 0xff,
};

/* link type */
enum AWS_LINK_TYPE {
	AWS_LINK_TYPE_NONE,
	AWS_LINK_TYPE_PRISM,
	AWS_LINK_TYPE_80211_RADIO,
	AWS_LINK_TYPE_80211_RADIO_AVS
};

//用于打印aws配网库版本
const char *aws_version(void);

//将monitor模式下抓到的包传入该函数进行处理
//参数：
//	buf: frame buffer
//	length: frame len
//	link_type: see enum AWS_LINK_TYPE
//	with_fcs: frame include 80211 fcs field, the tailing 4bytes
//
//说明：
//	适配前执行以下命令, 检查link_type和with_fcs参数
//	a) iwconfig wlan0 mode monitor	#进入monitor模式
//	b) iwconfig wlan0 channel 6	#切换到信道6(以路由器信道为准)
//	c) tcpdump -i wlan0 -s0 -w file.pacp	#抓包保存文件
//	d) 用wireshark或者omnipeek打开，检查包头格式，及包尾是否包含FCS 4字节
//
//	常见的包头类型为：
//	无额外的包头：AWS_LINK_TYPE_NONE
//	radio header: hdr_len = *(unsigned short *)(buf + 2)
//	avs header: hdr_len = *(unsigned long *)(buf + 4)
//	prism header: hdr_len = 144
//
int aws_80211_frame_handler(char *buf, int length,
		int link_type, int with_fcs);

//启动一键配网服务, 该函数会block，直到配网成功或者超时退出,
//	超时时间由aws_timeout_period_ms设置
//参数：
//	model: 产品model, 如
//	secret: 产品secret, 如
//	mac: 产品mac地址，如11:22:33:44:55:66
//	sn: 产品sn条码，通常填NULL
void aws_start(char *model, char *secret, char *mac, char *sn);
//{该函数大致流程如下:
//	init();
//	platform_monitor_open();
//	aws_main_thread_func();
//	platform_monitor_close();
//	destroy();
//}

//aws_start返回后，调用该函数，获取ssid和passwd等信息
//aws成功时，ssid & passwd一定会返回非NULL字符串, 但bssid和auth, encry, channel
//	有可能会返回NULL或者INVALID值(取决于是否能在wifi列表里搜索命中)
//aws失败超时后，该函数会返回0, 且所有参数为NULL或INVALID VALUE
//
//auth defined by enum AWS_AUTH_TYPE
//encry defined by enum AWS_ENC_TYPE
//
//返回值：1--成功，0--失败
int aws_get_ssid_passwd(char ssid[32 + 1], char passwd[64 + 1], char bssid[6],
	char *auth, char *encry, char *channel);

//发送广播通知APP，配网成功。
//默认会广播2min, 广播过程中收到APP应答后，提前终止
void aws_notify_app(void);

//配网结束（成功或失败）后，调用该函数，释放配网库占用的资源
void aws_destroy(void);

#ifdef __cplusplus
}
#endif
/******************************************/
#endif
