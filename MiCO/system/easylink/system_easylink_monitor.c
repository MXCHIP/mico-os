/**
 ******************************************************************************
 * @file    system_easylink_monitor.c
 * @author  William Xu
 * @version V1.0.0
 * @date    20-May-2017
 * @brief   This file provide the easylink function for quick provisioning and
 *          first time configuration.
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

#include "mico.h"

#include "system_internal.h"
#include "easylink_internal.h"

#include "StringUtils.h"

#define MAX_CHANNEL_DELAY_TIME 3000
//#if (MICO_WLAN_CONFIG_MODE == CONFIG_MODE_EASYLINK) || (MICO_WLAN_CONFIG_MODE == CONFIG_MODE_EASYLINK_WITH_SOFTAP)

/******************************************************
 *               Function Declarations
 ******************************************************/
/* EasyLink event callback functions*/
static void easylink_wifi_status_cb( WiFiEvent event, system_context_t * const inContext );
static void easylink_complete_cb( network_InitTypeDef_st *nwkpara, system_context_t * const inContext );
static void easylink_extra_data_cb( int datalen, char* data, system_context_t * const inContext );

/* Thread perform easylink and connect to wlan */
static void easylink_monitor_thread( uint32_t inContext ); /* Perform easylink and connect to wlan */

extern void mico_wlan_monitor_no_easylink(void);

/******************************************************
 *               Variables Definitions
 ******************************************************/

static uint8_t lock_channel = 1;
static mico_bool_t wlan_channel_walker = MICO_TRUE;
static uint32_t wlan_channel_walker_interval = 100;

static mico_semaphore_t easylink_sem;         /**< Used to suspend thread while easylink. */
static mico_semaphore_t easylink_connect_sem; /**< Used to suspend thread while connection. */
static bool easylink_success = false;         /**< true: connect to wlan, false: start soft ap mode or roll back to previous settings */
static uint32_t easylink_id = 0;      /**< Unique for an easylink instance. */
static mico_thread_t easylink_monitor_thread_handler = NULL;
static mico_thread_t switch_channel_thread_handler = NULL;
static bool easylink_thread_force_exit = false;
static bool switch_channel_flag = true;

static mico_config_source_t source = CONFIG_BY_NONE;
static mico_connect_fail_config_t connect_fail_config = EXIT_EASYLINK;

/* AP usually works at channel 1, 6 or 11, increasing the probability of switching to these channels.*/
static const uint8_t ch_tbl[] = {1,6,11,2,3,4,5,1,6,11,7,8,9,10,1,6,11,12,13};

/******************************************************
 *               Function Definitions
 ******************************************************/

/* MiCO callback when WiFi status is changed */
static void easylink_wifi_status_cb( WiFiEvent event, system_context_t * const inContext )
{
    switch ( event )
    {
        case NOTIFY_STATION_UP:
            /* Connected to AP, means that the wlan configuration is right, update configuration in flash and update
             bongjour txt record with new "easylink_id" */
            easylink_bonjour_update( Station, easylink_id, inContext );
            inContext->flashContentInRam.micoSystemConfig.configured = allConfigured;
            mico_system_context_update( &inContext->flashContentInRam ); //Update Flash content
            mico_rtos_set_semaphore( &easylink_connect_sem ); //Notify Easylink thread
            break;
        default:
            break;
    }
    return;
}

/* MiCO callback when EasyLink is finished step 1, return SSID and KEY */
static void easylink_complete_cb( network_InitTypeDef_st *nwkpara, system_context_t * const inContext )
{
    OSStatus err = kNoErr;

    require_action_string( nwkpara, exit, err = kTimeoutErr, "EasyLink Timeout or terminated" );
#ifdef MICO_EXTRA_AP_NUM
    system_network_update(inContext, nwkpara->wifi_ssid);
#endif

    /* Store SSID and KEY*/
    mico_rtos_lock_mutex( &inContext->flashContentInRam_mutex );
    memcpy( inContext->flashContentInRam.micoSystemConfig.ssid, nwkpara->wifi_ssid, maxSsidLen );
    memset( inContext->flashContentInRam.micoSystemConfig.bssid, 0x0, 6 );
    memcpy( inContext->flashContentInRam.micoSystemConfig.user_key, nwkpara->wifi_key, maxKeyLen );
    inContext->flashContentInRam.micoSystemConfig.user_keyLength = strlen( nwkpara->wifi_key );
    memcpy( inContext->flashContentInRam.micoSystemConfig.key, nwkpara->wifi_key, maxKeyLen );
    inContext->flashContentInRam.micoSystemConfig.keyLength = strlen( nwkpara->wifi_key );
    inContext->flashContentInRam.micoSystemConfig.dhcpEnable = true;
    mico_rtos_unlock_mutex( &inContext->flashContentInRam_mutex );
    system_log("Get SSID: %s, Key: %s", inContext->flashContentInRam.micoSystemConfig.ssid, inContext->flashContentInRam.micoSystemConfig.user_key);

    source = (mico_config_source_t) nwkpara->wifi_retry_interval;
    exit:
    if ( err != kNoErr )
    {
        /*EasyLink timeout or error*/
        easylink_success = false;
        mico_rtos_set_semaphore( &easylink_sem );
    }
    return;
}

/* MiCO callback when EasyLink is finished step 2, return extra data 
 data format: [AuthData#Identifier]<localIp/netMask/gateWay/dnsServer>
 Auth data: Provide to application, application will decide if this is a proter configuration for currnet device
 Identifier: Unique id for every easylink instance send by easylink mobile app
 localIp/netMask/gateWay/dnsServer: Device static ip address, use DHCP if not exist
 */
static void easylink_extra_data_cb( int datalen, char* data, system_context_t * const inContext )
{
    OSStatus err = kNoErr;
    int index;
    uint32_t *identifier, ipInfoCount;
    char *debugString;
    struct in_addr ipv4_addr;

    debugString = DataToHexStringWithSpaces( (const uint8_t *) data, datalen );
    system_log("Get user info: %s", debugString);
    free( debugString );

    /* Find '#' that separate authdata and identifier*/
    for ( index = datalen - 1; index >= 0; index-- )
    {
        if ( data[index] == '#' && ((datalen - index) == 5 || (datalen - index) == 25) )
            break;
    }
    require_action( index >= 0, exit, err = kParamErr );

    /* Check auth data by device */
    data[index++] = 0x0;
    err = mico_system_delegate_config_recv_auth_data( data );
    require_noerr( err, exit );

    /* Read identifier */
    identifier = (uint32_t *) &data[index];
    easylink_id = *identifier;

    /* Identifier: 1 x uint32_t or Identifier/localIp/netMask/gateWay/dnsServer: 5 x uint32_t */
    ipInfoCount = (datalen - index) / sizeof(uint32_t);
    require_action( ipInfoCount >= 1, exit, err = kParamErr );

    mico_rtos_lock_mutex( &inContext->flashContentInRam_mutex );

    if ( ipInfoCount == 1 )
    { //Use DHCP to obtain local ip address
        inContext->flashContentInRam.micoSystemConfig.dhcpEnable = true;
        system_log("Get auth info: %s, EasyLink identifier: %lx", data, easylink_id);
    } else
    { //Use static ip address
        inContext->flashContentInRam.micoSystemConfig.dhcpEnable = false;
        ipv4_addr.s_addr = *(identifier+1);
        strcpy( (char *) inContext->micoStatus.localIp, inet_ntoa( ipv4_addr ) );
        ipv4_addr.s_addr = *(identifier+2);
        strcpy( (char *) inContext->micoStatus.netMask, inet_ntoa( ipv4_addr ) );
        ipv4_addr.s_addr = *(identifier+3);
        strcpy( (char *) inContext->micoStatus.gateWay, inet_ntoa( ipv4_addr ) );
        ipv4_addr.s_addr = *(identifier+4);
        strcpy( (char *) inContext->micoStatus.dnsServer, inet_ntoa( ipv4_addr ) );

        system_log("Get auth info: %s, EasyLink identifier: %lx, local IP info:%s %s %s %s ", data, easylink_id, inContext->flashContentInRam.micoSystemConfig.localIp,
            inContext->flashContentInRam.micoSystemConfig.netMask, inContext->flashContentInRam.micoSystemConfig.gateWay,inContext->flashContentInRam.micoSystemConfig.dnsServer);
    }
    mico_rtos_unlock_mutex( &inContext->flashContentInRam_mutex );
    source = CONFIG_BY_EASYLINK_V2;

    exit:
    if ( err != kNoErr )
    {
        /*EasyLink error*/
        system_log("EasyLink step 2 ERROR, err: %d", err);
        easylink_success = false;
    } else
        /* Easylink success after step 1 and step 2 */
        easylink_success = true;

    mico_rtos_set_semaphore( &easylink_sem );
    return;
}

void mico_easylink_monitor_set_lock_channel(uint8_t ch)
{
    if (ch < 1 || ch > 13)
        return;
    lock_channel = ch;
}

static void switch_channel_thread(mico_thread_arg_t arg)
{
    mico_time_t current;
    uint8_t wlan_channel = 0, i=0;
    uint16_t delay_time, total_delay;
    
    while(switch_channel_flag)
    {
        mico_time_get_time( &current );
        if ( current > (mico_time_t) arg ) {
            easylink_success = false;
            mico_rtos_set_semaphore( &easylink_sem );
            break;
        }

        if( wlan_channel_walker == MICO_TRUE){
            wlan_channel = ch_tbl[i++];
            if ( i >= sizeof(ch_tbl) ) i = 0;
            mico_wlan_monitor_set_channel( wlan_channel );
            lock_channel = wlan_channel;
            mico_easylink_monitor_delegate_channel_changed( wlan_channel );
        } else {
            if (lock_channel != wlan_channel) {
                system_log("Lock channel from %d to %d", wlan_channel, lock_channel);
                wlan_channel = lock_channel;
                mico_wlan_monitor_set_channel( wlan_channel );
                lock_channel = wlan_channel;
                mico_easylink_monitor_delegate_channel_changed( wlan_channel );
            }
        }
        mico_rtos_delay_milliseconds(wlan_channel_walker_interval);
        total_delay = 0;
        do {
            delay_time = mico_easylinK_monitor_delay_switch();
            if (delay_time > 0) {
                total_delay += delay_time;
                if (total_delay > MAX_CHANNEL_DELAY_TIME) {
                    break;
                }
                mico_rtos_delay_milliseconds(delay_time);
            }
        } while(delay_time > 0);
    }
    switch_channel_thread_handler = NULL;
    mico_rtos_delete_thread(NULL);
}

static void monitor_cb( uint8_t * frame, int len )
{
    mico_easylink_monitor_delegate_package_recved( frame, len );
}

static void easylink_monitor_thread( uint32_t arg )
{
    OSStatus err = kNoErr;
    system_context_t *context = (system_context_t *) arg;

    mico_time_t current;
    easylink_id = 0x0;
    easylink_success = false;
    easylink_thread_force_exit = false;

    source = CONFIG_BY_NONE;
    mico_system_notify_register( mico_notify_EASYLINK_WPS_COMPLETED,    (void *) easylink_complete_cb,      context );
    mico_system_notify_register( mico_notify_EASYLINK_GET_EXTRA_DATA,   (void *) easylink_extra_data_cb,    context );
    mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED,       (void *) easylink_wifi_status_cb,   context );

    mico_rtos_init_semaphore( &easylink_sem,            1 );
    mico_rtos_init_semaphore( &easylink_connect_sem,    1 );

    mico_wlan_register_monitor_cb( monitor_cb );

restart:
    mico_system_delegate_config_will_start( );
    system_log("Start easylink monitor mode");
    mico_easylink_monitor_delegate_will_start( );
    micoWlanSuspend();
    mico_wlan_start_monitor( );

    wlan_channel_walker = MICO_TRUE;
    mico_time_get_time( &current );
    switch_channel_flag = true;
    mico_rtos_create_thread(&switch_channel_thread_handler, MICO_DEFAULT_WORKER_PRIORITY, "sw_channel",
                            switch_channel_thread, 0x1000, (mico_thread_arg_t)(current + EasyLink_TimeOut));

    while( mico_rtos_get_semaphore( &easylink_sem, 0 ) == kNoErr );
    err = mico_rtos_get_semaphore( &easylink_sem, MICO_WAIT_FOREVER );

    switch_channel_flag = false;
    mico_wlan_stop_monitor();
    mico_easylink_monitor_delegate_stoped();

    /* Easylink force exit by user, clean and exit */
    if( err != kNoErr && easylink_thread_force_exit )
    {
        system_log("EasyLink canceled by user");
        goto exit;
    }

    /* EasyLink Success */
    if ( easylink_success == true )
    {
        mico_system_delegate_config_recv_ssid( context->flashContentInRam.micoSystemConfig.ssid,
                                               context->flashContentInRam.micoSystemConfig.user_key );
        system_connect_wifi_normal( context );

        /* Wait for station connection */
        while( mico_rtos_get_semaphore( &easylink_connect_sem, 0 ) == kNoErr );
        err = mico_rtos_get_semaphore( &easylink_connect_sem, EasyLink_ConnectWlan_Timeout );
        /* Easylink force exit by user, clean and exit */
        if( err != kNoErr && easylink_thread_force_exit )
        {
            micoWlanSuspend();
            system_log("EasyLink connection canceled by user");
            goto exit;
        }

        source = (source == CONFIG_BY_NONE) ? CONFIG_BY_MONITOR : source;

        /* Easylink connect result */
        if ( err != kNoErr )
        {
            connect_fail_config = mico_system_delegate_config_result( source, MICO_FALSE );
            if ( RESTART_EASYLINK == connect_fail_config )
                 {
                system_log("Re-start easylink combo mode");
                micoWlanSuspend( );
                goto restart;
            } else {
                system_log("exit easylink combo mode");
                micoWlanSuspendStation( );
                goto exit;
            }
        }
        else
        {
            mico_system_delegate_config_result( source, MICO_TRUE );
            mico_easylink_monitor_delegate_connect_success( source );
        }
    }
    else /* EasyLink failed */
    {
        mico_system_delegate_easylink_timeout( context );
    }

exit:
    easylink_thread_force_exit = false;

    mico_system_delegate_config_will_stop( );

    mico_system_notify_remove( mico_notify_WIFI_STATUS_CHANGED, (void *)easylink_wifi_status_cb );
    mico_system_notify_remove( mico_notify_EASYLINK_WPS_COMPLETED, (void *)easylink_complete_cb );
    mico_system_notify_remove( mico_notify_EASYLINK_GET_EXTRA_DATA, (void *)easylink_extra_data_cb );

    mico_rtos_deinit_semaphore( &easylink_sem );
    mico_rtos_deinit_semaphore( &easylink_connect_sem );
    easylink_monitor_thread_handler = NULL;
    mico_rtos_delete_thread( NULL );
}

OSStatus mico_easylink_monitor_channel_walker( mico_bool_t enable, uint32_t interval )
{
    wlan_channel_walker = enable;

    if( enable == MICO_TRUE ) wlan_channel_walker_interval = interval;

    return kNoErr;
}

OSStatus mico_easylink_monitor_save_result( network_InitTypeDef_st *nwkpara )
{
    system_context_t * context = system_context( );

    if( context == NULL ) return kNotPreparedErr;
#ifdef MICO_EXTRA_AP_NUM
    system_network_update(context, nwkpara->wifi_ssid);
#endif

    memcpy( context->flashContentInRam.micoSystemConfig.ssid, nwkpara->wifi_ssid, maxSsidLen );
    memset( context->flashContentInRam.micoSystemConfig.bssid, 0x0, 6 );
    memcpy( context->flashContentInRam.micoSystemConfig.user_key, nwkpara->wifi_key, maxKeyLen );
    context->flashContentInRam.micoSystemConfig.user_keyLength = strlen( nwkpara->wifi_key );
    context->flashContentInRam.micoSystemConfig.dhcpEnable = true;

    system_log("Get SSID: %s, Key: %s", context->flashContentInRam.micoSystemConfig.ssid, context->flashContentInRam.micoSystemConfig.user_key);

    easylink_success = true;
    mico_rtos_set_semaphore( &easylink_sem );
    return kNoErr;
}

OSStatus mico_easylink_monitor_with_easylink( mico_Context_t * const in_context, mico_bool_t enable )
{
    OSStatus err = kNoErr;

    require_action( in_context, exit, err = kNotPreparedErr );

    easylink_remove_bonjour( );

    /* easylink thread existed? stop! */
    if ( easylink_monitor_thread_handler ) {
        system_log("EasyLink monitor processing, force stop..");
        easylink_thread_force_exit = true;
        mico_rtos_thread_force_awake( &easylink_monitor_thread_handler );
        mico_rtos_thread_join( &easylink_monitor_thread_handler );
    }

    if ( enable == MICO_TRUE ) {
        err = mico_rtos_create_thread(&easylink_monitor_thread_handler, MICO_DEFAULT_LIBRARY_PRIORITY, "EASYLINK",
                                      easylink_monitor_thread, 0x1000, (mico_thread_arg_t)in_context);
        require_noerr_string( err, exit, "ERROR: Unable to start the EasyLink monitor thread." );

        /* Make sure easylink is already running, and waiting for sem trigger */
        mico_rtos_delay_milliseconds( 1000 );
    }

    exit:
    return err;
}


OSStatus mico_easylink_monitor( mico_Context_t * const in_context, mico_bool_t enable )
{
    mico_wlan_monitor_no_easylink();
    return mico_easylink_monitor_with_easylink( in_context, enable );
}



//#endif

