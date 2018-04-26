#ifndef _HOSTAPD_CONFIG_H_
#define _HOSTAPD_CONFIG_H_

#include "include.h"
#include "defs.h"

#if CFG_WIFI_WEP
#define CFG_AUTH_ALG                   (WPA_AUTH_ALG_OPEN | WPA_AUTH_ALG_SHARED)
#else
#define CFG_AUTH_ALG                   WPA_AUTH_ALG_OPEN
#endif // CFG_WIFI_WEP

#define CFG_SUPPORTED_MAX_STA_NUM      2
#define CFG_SUPPORT_80211G             1

#endif // _HOSTAPD_CONFIG_H_


