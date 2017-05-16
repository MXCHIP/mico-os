/**
 ******************************************************************************
 * @file    system_easylink.c
 * @author  William Xu
 * @version V1.0.0
 * @date    20-July-2015
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
#include "StringUtils.h"
#include "HTTPUtils.h"
#include "SocketUtils.h"

#include "system.h"
#include "easylink_internal.h"

//#if (MICO_WLAN_CONFIG_MODE == CONFIG_MODE_EASYLINK) || (MICO_WLAN_CONFIG_MODE == CONFIG_MODE_EASYLINK_WITH_SOFTAP)

/* Internal vars and functions */
static mico_semaphore_t easylink_sem;         /**< Used to suspend thread while easylink. */
static mico_semaphore_t easylink_connect_sem; /**< Used to suspend thread while connection. */
static bool easylink_success = false;         /**< true: connect to wlan, false: start soft ap mode or roll back to previous settings */
static uint32_t easylinkIndentifier = 0;      /**< Unique for an easylink instance. */
static mico_thread_t easylink_thread_handler = NULL;
static bool easylink_thread_force_exit = false;

/* Perform easylink and connect to wlan */
static void easylink_thread( uint32_t inContext );

static mico_config_source_t source = CONFIG_BY_NONE;
static uint8_t airkiss_random = 0x0;
static void airkiss_broadcast_thread( uint32_t arg );

/* MiCO callback when WiFi status is changed */
static void EasyLinkNotify_WifiStatusHandler( WiFiEvent event, system_context_t * const inContext )
{
    system_log_trace();
    require( inContext, exit );

    switch ( event )
    {
        case NOTIFY_STATION_UP:
            /* Connected to AP, means that the wlan configuration is right, update configuration in flash and update
             bongjour txt record with new "easylinkIndentifier" */
            easylink_bonjour_update( Station, easylinkIndentifier, inContext );
            inContext->flashContentInRam.micoSystemConfig.configured = allConfigured;
            mico_system_context_update( &inContext->flashContentInRam ); //Update Flash content
            mico_rtos_set_semaphore( &easylink_connect_sem ); //Notify Easylink thread
            break;
        case NOTIFY_AP_DOWN:
            /* Remove bonjour service under soft ap interface */
            mdns_suspend_record( "_easylink_config._tcp.local.", Soft_AP, true );
            break;
        default:
            break;
    }
    exit:
    return;
}

/* MiCO callback when EasyLink is finished step 1, return SSID and KEY */
static void EasyLinkNotify_EasyLinkCompleteHandler( network_InitTypeDef_st *nwkpara, system_context_t * const inContext )
{
    system_log_trace();
    OSStatus err = kNoErr;

    require_action( inContext, exit, err = kParamErr );
    require_action( nwkpara, exit, err = kTimeoutErr );

    /* Store SSID and KEY*/
    mico_rtos_lock_mutex( &inContext->flashContentInRam_mutex );
    memcpy( inContext->flashContentInRam.micoSystemConfig.ssid, nwkpara->wifi_ssid, maxSsidLen );
    memset( inContext->flashContentInRam.micoSystemConfig.bssid, 0x0, 6 );
    memcpy( inContext->flashContentInRam.micoSystemConfig.user_key, nwkpara->wifi_key, maxKeyLen );
    inContext->flashContentInRam.micoSystemConfig.user_keyLength = strlen( nwkpara->wifi_key );
    inContext->flashContentInRam.micoSystemConfig.dhcpEnable = true;
    mico_rtos_unlock_mutex( &inContext->flashContentInRam_mutex );
    system_log("Get SSID: %s, Key: %s", inContext->flashContentInRam.micoSystemConfig.ssid, inContext->flashContentInRam.micoSystemConfig.user_key);

    source = (mico_config_source_t) nwkpara->wifi_retry_interval;
    exit:
    if ( err != kNoErr )
    {
        /*EasyLink timeout or error*/
        system_log("EasyLink step 1 ERROR, err: %d", err);
        easylink_success = false;
        mico_rtos_set_semaphore( &easylink_sem );
    }
    return;
}

/* MiCO callback when EasyLink is finished step 2, return extra data 
 data format: AuthData#Identifier(localIp/netMask/gateWay/dnsServer)
 Auth data: Provide to application, application will decide if this is a proter configuration for currnet device
 Identifier: Unique id for every easylink instance send by easylink mobile app
 localIp/netMask/gateWay/dnsServer: Device static ip address, use DHCP if not exist
 */
static void EasyLinkNotify_EasyLinkGetExtraDataHandler( int datalen, char* data,
                                                        system_context_t * const inContext )
{
    system_log_trace();
    OSStatus err = kNoErr;
    int index;
    uint32_t *identifier, ipInfoCount;
    char *debugString;
    struct in_addr ipv4_addr;

    require_action( inContext, exit, err = kParamErr );

    if ( source == CONFIG_BY_AIRKISS )
    {
        airkiss_random = *data;
        goto exit;
    }

    debugString = DataToHexStringWithSpaces( (const uint8_t *) data, datalen );
    system_log("Get user info: %s", debugString);
    free( debugString );

    /* Find '#' that seperate anthdata and identifier*/
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
    easylinkIndentifier = *identifier;

    /* Identifier: 1 x uint32_t or Identifier/localIp/netMask/gateWay/dnsServer: 5 x uint32_t */
    ipInfoCount = (datalen - index) / sizeof(uint32_t);
    require_action( ipInfoCount >= 1, exit, err = kParamErr );

    mico_rtos_lock_mutex( &inContext->flashContentInRam_mutex );

    if ( ipInfoCount == 1 )
    { //Use DHCP to obtain local ip address
        inContext->flashContentInRam.micoSystemConfig.dhcpEnable = true;
        system_log("Get auth info: %s, EasyLink identifier: %lx", data, easylinkIndentifier);
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

        system_log("Get auth info: %s, EasyLink identifier: %lx, local IP info:%s %s %s %s ", data, easylinkIndentifier, inContext->flashContentInRam.micoSystemConfig.localIp,
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

OSStatus system_easylink_start( system_context_t * const inContext )
{
    system_log_trace();
    OSStatus err = kUnknownErr;
    mdns_record_state_t easylink_service_state = mdns_get_record_status( "_easylink_config._tcp.local.", Station);

    /* Remove previous _easylink_config service if existed */
    if ( easylink_service_state != RECORD_REMOVED )
    {
        UnSetTimer( easylink_remove_bonjour );
        easylink_remove_bonjour( );
        mico_rtos_delay_milliseconds( 1500 );
    }

#if ( MICO_WLAN_CONFIG_MODE & _CONFIG_SOFTAP )
    /* Start config server */
    err = config_server_start( );
    require_noerr( err, exit );
#endif

    /* Start easylink thread, existed? stop and re-start! */
    if( easylink_thread_handler )
    {
        system_log("EasyLink processing, force stop..");
        easylink_thread_force_exit = true;
        mico_rtos_thread_force_awake( &easylink_thread_handler );
        mico_rtos_thread_join( &easylink_thread_handler );
    }

    err = mico_rtos_create_thread( &easylink_thread_handler, MICO_APPLICATION_PRIORITY, "EASYLINK", easylink_thread,
                                   0x1000, (mico_thread_arg_t) inContext );
    require_noerr_string( err, exit, "ERROR: Unable to start the EasyLink thread." );

    /* Make sure easylink thread is running */
    mico_rtos_delay_milliseconds(20);

exit:
    return err;
}

void easylink_thread( uint32_t inContext )
{
    system_log_trace();
    OSStatus err = kNoErr;
    system_context_t *Context = (system_context_t *) inContext;

    easylinkIndentifier = 0x0;
    easylink_success = false;

    easylink_thread_force_exit = false;

    source = CONFIG_BY_NONE;
    mico_system_notify_register( mico_notify_EASYLINK_WPS_COMPLETED,
                                 (void *) EasyLinkNotify_EasyLinkCompleteHandler,
                                 (void *) inContext );
    mico_system_notify_register( mico_notify_EASYLINK_GET_EXTRA_DATA,
                                 (void *) EasyLinkNotify_EasyLinkGetExtraDataHandler,
                                 (void *) inContext );
    mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED,
                                 (void *) EasyLinkNotify_WifiStatusHandler, (void *) inContext );

    mico_rtos_init_semaphore( &easylink_sem, 1 );
    mico_rtos_init_semaphore( &easylink_connect_sem, 1 );

    /* If use CONFIG_MODE_SOFT_AP only, skip easylink mode, establish soft ap directly */
restart:
    mico_system_delegate_config_will_start( );
    system_log("Start easylink combo mode");
    micoWlanStartEasyLinkPlus( EasyLink_TimeOut / 1000 );
    while( mico_rtos_get_semaphore( &easylink_sem, 0 ) == kNoErr );
    err = mico_rtos_get_semaphore( &easylink_sem, MICO_WAIT_FOREVER );

    /* Easylink force exit by user, clean and exit */
    if( err != kNoErr && easylink_thread_force_exit )
    {
        system_log("EasyLink waiting for terminate");
        micoWlanStopEasyLinkPlus( );
        mico_rtos_get_semaphore( &easylink_sem, 3000 );
        system_log("EasyLink canceled by user");
        goto exit;
    }

    /* EasyLink Success */
    if ( easylink_success == true )
    {
        mico_system_delegate_config_recv_ssid( Context->flashContentInRam.micoSystemConfig.ssid,
                                               Context->flashContentInRam.micoSystemConfig.user_key );
        system_connect_wifi_normal( Context );

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

        /*SSID or Password is not correct, module cannot connect to wlan, so restart EasyLink again*/
        require_noerr_action_string( err, restart, micoWlanSuspend(), "Re-start easylink commbo mode" );
        mico_system_delegate_config_success( source );

        /* Start bonjour service for new device discovery */
        err = easylink_bonjour_start( Station, easylinkIndentifier, Context );
        require_noerr( err, exit );
        SetTimer( 60 * 1000, easylink_remove_bonjour );

        goto exit;
    }
    else /* EasyLink failed or disabled, start soft ap mode if CONFIG_MODE_EASYLINK_WITH_SOFTAP is set*/
    {
#if ( MICO_WLAN_CONFIG_MODE & _CONFIG_SOFTAP ) //start soft ap mode
        micoWlanSuspend();
        mico_thread_msleep( 20 );

restart_sofap:
        err = easylink_softap_start( Context );
        require_noerr( err, exit );

        /* Wait for station connection */
        while( mico_rtos_get_semaphore( &easylink_connect_sem, 0 ) == kNoErr );
        err = mico_rtos_get_semaphore( &easylink_connect_sem, 60000 + EasyLink_ConnectWlan_Timeout );
        /* Easylink force exit by user, clean and exit */
        if( err != kNoErr && easylink_thread_force_exit )
        {
            micoWlanSuspend();
            system_log("EasyLink connection canceled by user");
            easylink_thread_force_exit = false;
            goto exit;
        }

        /*SSID or Password is not correct, module cannot connect to wlan, so restart EasyLink again*/
        require_noerr_action_string( err, restart_sofap, micoWlanSuspend(), "Re-start easylink soft ap mode" );
        mico_system_delegate_config_success( source );

        /* Start bonjour service for easylink mode */
        easylinkIndentifier = easylink_softap_get_identifier( );
        err = easylink_bonjour_start( Station, easylinkIndentifier, Context );
        require_noerr( err, exit );
        SetTimer( 60 * 1000, easylink_remove_bonjour );

        mico_system_delegate_config_success( CONFIG_BY_SOFT_AP );
#endif

#ifdef EasyLink_Needs_Reboot
        /*so roll back to unconfig mode if easylink is triggered by external */
        if( Context->flashContentInRam.micoSystemConfig.configured == unConfigured2 )
        {
            Context->flashContentInRam.micoSystemConfig.configured = unConfigured;
            mico_system_context_update( &Context->flashContentInRam );
        }

#endif
        /*so roll back to previous settings  (if it has) and connect*/
        if(Context->flashContentInRam.micoSystemConfig.configured != unConfigured)
        {
            MICOReadConfiguration( Context );
#ifdef EasyLink_Needs_Reboot
            Context->flashContentInRam.micoSystemConfig.configured = allConfigured;
            mico_system_context_update( &Context->flashContentInRam );
#endif
            system_connect_wifi_normal( Context );
        } else
        {
            /*module should power down in default setting*/
            system_log("Wi-Fi power off");
            micoWlanPowerOff();
        }
    }

exit:
    easylink_thread_force_exit = false;

    mico_system_delegate_config_will_stop( );

    mico_system_notify_remove( mico_notify_WIFI_STATUS_CHANGED, (void *) EasyLinkNotify_WifiStatusHandler );
    mico_system_notify_remove( mico_notify_EASYLINK_WPS_COMPLETED, (void *) EasyLinkNotify_EasyLinkCompleteHandler );
    mico_system_notify_remove( mico_notify_EASYLINK_GET_EXTRA_DATA, (void *) EasyLinkNotify_EasyLinkGetExtraDataHandler );

#ifndef MICO_CONFIG_SERVER_ENABLE
    err = config_server_stop( );
    require_noerr( err, exit );
#endif 

    mico_rtos_deinit_semaphore( &easylink_sem );
    mico_rtos_deinit_semaphore( &easylink_connect_sem );
    easylink_thread_handler = NULL;
    mico_rtos_delete_thread( NULL );
}

//#endif

