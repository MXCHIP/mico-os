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
#include "easylink_internal.h"

#include "StringUtils.h"


/******************************************************
 *               Function Declarations
 ******************************************************/
/* EasyLink event callback functions*/
static void easylink_wifi_status_cb( WiFiEvent event, system_context_t * const inContext );
static void easylink_complete_cb( network_InitTypeDef_st *nwkpara, system_context_t * const inContext );

/* Thread perform wps and connect to wlan */
static void easylink_wps_thread( uint32_t inContext ); /* Perform easylink and connect to wlan */

/******************************************************
 *               Variables Definitions
 ******************************************************/
static mico_semaphore_t easylink_sem;         /**< Used to suspend thread while easylink. */
static mico_semaphore_t easylink_connect_sem; /**< Used to suspend thread while connection. */
static bool easylink_success = false;         /**< true: connect to wlan, false: start soft ap mode or roll back to previous settings */
static uint32_t easylinkIndentifier = 0;      /**< Unique for an easylink instance. */
static mico_thread_t easylink_wps_thread_handler = NULL;
static bool easylink_thread_force_exit = false;

static mico_config_source_t source = CONFIG_BY_NONE;

static mico_wps_device_detail_t wps_config =
{
    .device_name     = DEFAULT_NAME,
    .manufacturer    = MANUFACTURER,
    .model_name      = MODEL,
    .model_number    = HARDWARE_REVISION,
    .serial_number   = SERIAL_NUMBER,
    .device_category = MICO_WPS_DEVICE_COMPUTER,
    .sub_category    = 07,//7,
    .config_methods  = WPS_CONFIG_PHYSICAL_PUSH_BUTTON,
};

/******************************************************
 *               Function Definitions
 ******************************************************/

/* MiCO callback when WiFi status is changed */
static void easylink_wifi_status_cb( WiFiEvent event, system_context_t * const inContext )
{
    switch ( event )
    {
        case NOTIFY_STATION_UP:
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

    /* Store SSID and KEY*/
    mico_rtos_lock_mutex( &inContext->flashContentInRam_mutex );
    memset( inContext->flashContentInRam.micoSystemConfig.bssid, 0x0, 6 );
    memset( inContext->flashContentInRam.micoSystemConfig.ssid, 0x0, maxSsidLen );
    memset( inContext->flashContentInRam.micoSystemConfig.user_key, 0x0, maxKeyLen );

    memcpy( inContext->flashContentInRam.micoSystemConfig.ssid, nwkpara->wifi_ssid, nwkpara->local_ip_addr[0] );
    memcpy( inContext->flashContentInRam.micoSystemConfig.user_key, nwkpara->wifi_key, nwkpara->local_ip_addr[1] );
    inContext->flashContentInRam.micoSystemConfig.user_keyLength = nwkpara->local_ip_addr[1];
    inContext->flashContentInRam.micoSystemConfig.dhcpEnable = true;
    mico_rtos_unlock_mutex( &inContext->flashContentInRam_mutex );
    system_log("Get SSID: %s, Key: %s", inContext->flashContentInRam.micoSystemConfig.ssid,
                                                    inContext->flashContentInRam.micoSystemConfig.user_key);

    source = CONFIG_BY_WPS;

exit:
    if ( err != kNoErr )
    {
        /*EasyLink timeout or error*/
        easylink_success = false;
        mico_rtos_set_semaphore( &easylink_sem );
    } else {
        easylink_success = true;
        mico_rtos_set_semaphore( &easylink_sem );
    }

    return;
}


static void easylink_remove_bonjour_from_sta(void)
{
    easylink_remove_bonjour(INTERFACE_STA);
}

static void easylink_wps_thread( uint32_t arg )
{
    OSStatus err = kNoErr;
    system_context_t *context = (system_context_t *) arg;

    easylinkIndentifier = 0x0;
    easylink_success = false;
    easylink_thread_force_exit = false;

    source = CONFIG_BY_NONE;
    mico_system_notify_register( mico_notify_EASYLINK_WPS_COMPLETED,    (void *) easylink_complete_cb,      context );
    mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED,       (void *) easylink_wifi_status_cb,   context );

    mico_rtos_init_semaphore( &easylink_sem,            1 );
    mico_rtos_init_semaphore( &easylink_connect_sem,    1 );

restart:
    mico_system_delegate_config_will_start( );
    system_log("Start easylink Wi-Fi protected setup mode(WPS) mode");
    mico_wlan_start_wps(&wps_config, EasyLink_TimeOut / 1000);
    while( mico_rtos_get_semaphore( &easylink_sem, 0 ) == kNoErr );
    err = mico_rtos_get_semaphore( &easylink_sem, MICO_WAIT_FOREVER );

    /* Easylink force exit by user, clean and exit */
    if( err != kNoErr && easylink_thread_force_exit )
    {
        system_log("EasyLink waiting for terminate");
        mico_wlan_stop_wps( );
        mico_rtos_get_semaphore( &easylink_sem, 3000 );
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

        /*SSID or Password is not correct, module cannot connect to wlan, so restart EasyLink again*/
        require_noerr_action_string( err, restart, micoWlanSuspend(), "Re-start easylink combo mode" );
        mico_system_delegate_config_success( source );

        /* Start bonjour service for new device discovery */
        err = easylink_bonjour_start( Station, easylinkIndentifier, context );
        require_noerr( err, exit );
        SetTimer( 60 * 1000, easylink_remove_bonjour_from_sta );

        goto exit;
    }
    else /* EasyLink failed */
    {
        /* so roll back to previous settings  (if it has) and connect */
        if ( context->flashContentInRam.micoSystemConfig.configured != unConfigured ) {
            system_log("Roll back to previous settings");
            MICOReadConfiguration( context );
            system_connect_wifi_normal( context );
        }
        else {
            /*module should power down in default setting*/
            system_log("Wi-Fi power off");
            micoWlanPowerOff();
        }
    }

exit:
    easylink_thread_force_exit = false;

    mico_system_delegate_config_will_stop( );

    mico_system_notify_remove( mico_notify_WIFI_STATUS_CHANGED, (void *)easylink_wifi_status_cb );
    mico_system_notify_remove( mico_notify_EASYLINK_WPS_COMPLETED, (void *)easylink_complete_cb );

    mico_rtos_deinit_semaphore( &easylink_sem );
    mico_rtos_deinit_semaphore( &easylink_connect_sem );
    easylink_wps_thread_handler = NULL;
    mico_rtos_delete_thread( NULL );
}

OSStatus mico_easylink_wps( mico_Context_t * const in_context, mico_bool_t enable )
{
    OSStatus err = kUnknownErr;

    require_action( in_context, exit, err = kNotPreparedErr );

    easylink_remove_bonjour( INTERFACE_STA );

    /* easylink thread existed? stop! */
    if ( easylink_wps_thread_handler ) {
        system_log("WPS processing, force stop..");
        easylink_thread_force_exit = true;
        mico_rtos_thread_force_awake( &easylink_wps_thread_handler );
        mico_rtos_thread_join( &easylink_wps_thread_handler );
    }

    if ( enable == MICO_TRUE ) {
        err = mico_rtos_create_thread( &easylink_wps_thread_handler, MICO_APPLICATION_PRIORITY, "WPS", easylink_wps_thread,
                                       0x1000, (mico_thread_arg_t) in_context );
        require_noerr_string( err, exit, "ERROR: Unable to start the WPS thread." );

        /* Make sure easylink is already running, and waiting for sem trigger */
        mico_rtos_delay_milliseconds( 100 );
    }

    exit:
    return err;
}



