/**
 ******************************************************************************
 * @file    mico_station_monitor.c
 * @author  Yang Haibo
 * @version V1.0.0
 * @date    01-11-2017
 * @brief   This file provide the function to monitor station status. 
            Start softap when station disconnected too long.
            Stop softap when station connected.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

/** @file
 *  Implementation of the MICOFS External-Use file system.
 */

#include "mico.h"

static mico_semaphore_t sem;
static int station_up = 0, softap_up = 0;
static char softap_ssid[33], softap_key[64];
static uint32_t softap_wait_seconds;

#define station_m_log(format, ...)  custom_log("mico", format, ##__VA_ARGS__)

static void micoNotify_WifiStatusHandler(WiFiEvent event,  void* inContext)
{
  switch (event) 
  {
  case NOTIFY_STATION_UP:
    station_m_log("Station up");
    station_up = 1;
    mico_rtos_set_semaphore(&sem);
    break;
  case NOTIFY_STATION_DOWN:
    station_m_log("Station down");
    station_up = 0;
    mico_rtos_set_semaphore(&sem);
    break;
  default:
    break;
  }
}

static void station_monitro_func( mico_thread_arg_t arg )
{
    while(1) {
        mico_rtos_get_semaphore(&sem, MICO_WAIT_FOREVER);
        if (station_up == 1) {
            if (softap_up) {
                station_m_log("Stop softap");
                micoWlanSuspendSoftAP();
                softap_up = 0;
            }
        } else if (softap_up == 0) {
            network_InitTypeDef_st wNetConfig;

            mico_rtos_get_semaphore(&sem, softap_wait_seconds*1000);
            if (station_up == 1)
                continue;
            
            /* Setup Soft AP*/
            memset( &wNetConfig, 0x0, sizeof(network_InitTypeDef_st) );
            strcpy( (char*) wNetConfig.wifi_ssid, softap_ssid );
            strcpy( (char*) wNetConfig.wifi_key, softap_key );
            wNetConfig.wifi_mode = Soft_AP;
            wNetConfig.dhcpMode = DHCP_Server;
            strcpy( (char*) wNetConfig.local_ip_addr, "10.10.0.1" );
            strcpy( (char*) wNetConfig.net_mask, "255.255.255.0" );
            strcpy( (char*) wNetConfig.dnsServer_ip_addr, "10.10.0.1" );

            station_m_log("Establish SofAP, SSID:%s and KEY:%s", wNetConfig.wifi_ssid, wNetConfig.wifi_key);

            micoWlanStart( &wNetConfig );
            softap_up = 1;
        }
    }

    mico_rtos_delete_thread(NULL);
}

/* Start a softap if station is disconnected more than trigger_seconds.
 * Stop softap if station is connected.
 * The softap's ssid and passphres is setted as <ssid> <key>
 */
int mico_station_status_monitor(char *ssid, char*key, int trigger_seconds)
{
    int err = kNoErr;
    
    mico_rtos_init_semaphore(&sem, 1);
    /* Register user function when wlan connection status is changed */
    err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED, (void *)micoNotify_WifiStatusHandler, NULL );
    require_noerr( err, exit );

    strncpy(softap_ssid, ssid, 33);
    strncpy(softap_key, key, 64);
    softap_wait_seconds = trigger_seconds;
    
    mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "station monitor",
        station_monitro_func, 1024, 0);
exit:
    return err;
}

