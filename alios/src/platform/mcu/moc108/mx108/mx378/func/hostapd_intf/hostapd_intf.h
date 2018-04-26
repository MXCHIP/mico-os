#ifndef _HOSTAPD_INTF_H_
#define _HOSTAPD_INTF_H_

#define WPAS_DEBUG

#ifdef WPAS_DEBUG
#define WPAS_PRT       os_printf
#define WPAS_WPRT      warning_prf
#else
#define WPAS_PRT       os_null_printf
#define WPAS_WPRT      warning_prf
#endif

#if CFG_WIFI_STATION_MODE
void wpa_enable_traffic_port_at_NonOpensystem(void);
void wpa_enable_traffic_port_at_ApNonOpensystem(void);
#endif

#endif
// eof

