#include "mico.h"
#include "system_internal.h"
#include "StringUtils.h"
#include "HTTPUtils.h"
#include "SocketUtils.h"

#include "system.h"
#include "airkiss_discovery/airkiss.h"

#define AIRKISS_CONF_TIMEOUT (60*1000) // 60 seconds.

static airkiss_context_t akcontex;
static mico_config_source_t source = CONFIG_BY_NONE;
const airkiss_config_t akconf_fn = {
    (airkiss_memset_fn) &memset,
    (airkiss_memcpy_fn) &memcpy,
    (airkiss_memcmp_fn) &memcmp,
    (airkiss_printf_fn) &printf };

static mico_thread_t airkiss_thread_handler = NULL;
static int airkiss_thread_force_exit = 0;
//static uint8_t       airkiss_random = 0x0;
static mico_semaphore_t airkiss_sem = NULL, airkiss_connect_sem = NULL;
static int airkiss_state;
static int airkiss_complete = 0;

static void airkiss_broadcast( uint8_t random );
void airkiss_conf_thread( uint32_t inContext );

void airkiss_EasyLinkNotify_CompleteHandler( network_InitTypeDef_st *nwkpara, system_context_t * const inContext )
{

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
}

/**
 * @brief  hi_EasyLinkNotify_GetExtraDataHandler
 * @note
 * @param
 * @retval
 **/
void airkiss_EasyLinkNotify_GetExtraDataHandler( int datalen, char* data, void * inContext )
{
    system_log("[%s]", __FUNCTION__);
    system_log("Get user info: %s", data);
}

/* MiCO callback when WiFi status is changed */
static void airkiss_WifiStatusHandler( WiFiEvent event, system_context_t * const inContext )
{
    require( inContext, exit );

    switch ( event )
    {
        case NOTIFY_STATION_UP:
            inContext->flashContentInRam.micoSystemConfig.configured = allConfigured;
            mico_system_context_update( &inContext->flashContentInRam ); //Update Flash content
            mico_rtos_set_semaphore( &airkiss_connect_sem ); //Notify Easylink thread
            break;
        default:
            break;
    }
    exit:
    return;
}

OSStatus system_airkiss_start( system_context_t * const inContext )
{
    OSStatus err = kUnknownErr;

    /* Start easylink thread, existed? stop and re-start! */
    if ( airkiss_thread_handler )
    {
        system_log("Airkiss processing, force stop..");
        airkiss_thread_force_exit = true;
        mico_rtos_thread_force_awake( &airkiss_thread_handler );
        mico_rtos_thread_join( &airkiss_thread_handler );
    }

    airkiss_thread_force_exit = false;

    err = mico_rtos_create_thread( &airkiss_thread_handler, MICO_APPLICATION_PRIORITY, "airkiss", airkiss_conf_thread,
                                   0x1000,
                                   (mico_thread_arg_t) inContext );
    require_noerr_string( err, exit, "ERROR: Unable to start the Airkiss thread." );

    /* Make sure easylink thread is running */
    mico_rtos_delay_milliseconds( 20 );

    exit:
    return err;
}

void system_airkiss_stop( )
{
    
    if ( airkiss_thread_handler )
    {
        system_log("Airkiss processing, force stop..");
        airkiss_thread_force_exit = true;
        mico_rtos_thread_force_awake( &airkiss_thread_handler );
        mico_rtos_thread_join( &airkiss_thread_handler );
    }
    
    mico_wlan_stop_monitor( );
}

static void monitor_cb( uint8_t * frame, int len )
{
    int ret;

    ret = airkiss_recv( &akcontex, frame, len );
    if ( airkiss_state != ret ) {
        if ( ret == AIRKISS_STATUS_CHANNEL_LOCKED ) {
            airkiss_state = ret;
            system_log("Airkiss channel LOCKED");
        }
        else if ( ret == AIRKISS_STATUS_COMPLETE ) {
            airkiss_state = ret;
        }
        mico_rtos_set_semaphore( &airkiss_sem );
    }
}

void airkiss_conf_thread( uint32_t inContext )
{
    int ret;
    uint32_t end_time, channel = 1;
    airkiss_result_t akresult;
    MAY_BE_UNUSED system_context_t
    *Context = (system_context_t *) inContext;

    mico_system_delegate_config_will_start( );
    system_log("Start airkiss mode");
    end_time = mico_rtos_get_time( ) + AIRKISS_CONF_TIMEOUT;

    ret = mico_rtos_init_semaphore( &airkiss_sem, 1 );
    require_noerr_string( ret, exit, "ERROR: Unable to init airkiss sem." );

    ret = mico_rtos_init_semaphore( &airkiss_connect_sem, 1 );
    require_noerr_string( ret, exit, "ERROR: Unable to init airkiss connect sem." );

    airkiss_state = AIRKISS_STATUS_CONTINUE;
    ret = airkiss_init( &akcontex, &akconf_fn );
    require_noerr_string( ret, exit, "ERROR: airkiss init return." );

    mico_wlan_register_monitor_cb( monitor_cb );

    mico_system_notify_register( mico_notify_EASYLINK_WPS_COMPLETED,
                                 (void *) airkiss_EasyLinkNotify_CompleteHandler,
                                 (void *) inContext );
    mico_system_notify_register( mico_notify_EASYLINK_GET_EXTRA_DATA,
                                 (void *) airkiss_EasyLinkNotify_GetExtraDataHandler,
                                 (void *) inContext );

    mico_wlan_start_monitor( );

    while ( 1 ) {
        if ( airkiss_complete )
            break;
        if ( airkiss_thread_force_exit == true )
            break;
        if ( mico_rtos_get_time( ) > end_time ) {
            system_log("Airkiss configure timeout!!");
            break;
        }
        /*connect by easylink-v2*/
        if ( source == CONFIG_BY_EASYLINK_V2 ) {
            mico_wlan_stop_monitor( );

            mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED,
                                         (void *) airkiss_WifiStatusHandler,
                                         (void *) inContext );
            system_connect_wifi_normal( Context );

            /* Wait for station connection */
            ret = mico_rtos_get_semaphore( &airkiss_connect_sem, EasyLink_ConnectWlan_Timeout );
            require_noerr_string( ret, exit, "ERROR: can't connect to AP." );
            break;

        }

        ret = mico_rtos_get_semaphore( &airkiss_sem, 100 );
        switch ( airkiss_state ) {
            case AIRKISS_STATUS_CONTINUE:
                mico_wlan_set_channel( channel++ );
                airkiss_change_channel( &akcontex );
                if ( channel == 14 )
                    channel = 1;
                break;

            case AIRKISS_STATUS_COMPLETE:
                system_log("Airkiss completed");
                ret = airkiss_get_result( &akcontex, &akresult );
                require_noerr_string( ret, exit, "ERROR: airkiss get result" );

                system_log("Airkiss ok: ssid=\"%s\", pwd=\"%s\"",
                    akresult.ssid, akresult.pwd);

                mico_wlan_stop_monitor( );

                mico_system_delegate_config_will_stop( );

                /* Store SSID and KEY*/
                mico_rtos_lock_mutex( &Context->flashContentInRam_mutex );
                memset( Context->flashContentInRam.micoSystemConfig.ssid, 0x0, maxSsidLen );
                memcpy( Context->flashContentInRam.micoSystemConfig.ssid, akresult.ssid, akresult.ssid_length );
                memset( Context->flashContentInRam.micoSystemConfig.bssid, 0x0, 6 );
                memcpy( Context->flashContentInRam.micoSystemConfig.user_key, akresult.pwd, maxKeyLen );
                Context->flashContentInRam.micoSystemConfig.user_keyLength = akresult.pwd_length;
                Context->flashContentInRam.micoSystemConfig.dhcpEnable = true;
                mico_rtos_unlock_mutex( &Context->flashContentInRam_mutex );
                system_log("Get SSID: %s, Key: %s", Context->flashContentInRam.micoSystemConfig.ssid, Context->flashContentInRam.micoSystemConfig.user_key);
                mico_system_delegate_config_recv_ssid( Context->flashContentInRam.micoSystemConfig.ssid,
                                                       Context->flashContentInRam.micoSystemConfig.user_key );

                mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED,
                                             (void *) airkiss_WifiStatusHandler,
                                             (void *) inContext );
                system_connect_wifi_normal( Context );


                /* Wait for station connection */
                ret = mico_rtos_get_semaphore( &airkiss_connect_sem, EasyLink_ConnectWlan_Timeout );
                require_noerr_string( ret, exit, "ERROR: can't connect to AP." );

                mico_system_delegate_config_will_stop( );

                system_log("Connect Success, send wechat");
                airkiss_broadcast( akresult.random );
                airkiss_complete = 1;

                break;

            case AIRKISS_STATUS_CHANNEL_LOCKED:
                default:
                break;
        }
    }
    airkiss_complete = 0;
    source = CONFIG_BY_NONE;

    exit:
    mico_system_delegate_config_will_stop( );

    if ( airkiss_sem )
        mico_rtos_deinit_semaphore( &airkiss_sem );

    if ( airkiss_connect_sem )
        mico_rtos_deinit_semaphore( &airkiss_connect_sem );

    airkiss_sem = NULL;
    airkiss_connect_sem = NULL;
    airkiss_thread_handler = NULL;
    mico_rtos_delete_thread( NULL );
}

static void airkiss_broadcast( uint8_t random )
{
    OSStatus err = kNoErr;
    int fd;
    struct sockaddr_in addr;
    int i = 0;

    fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    require_action( IsValidSocket( fd ), exit, err = kNoResourcesErr );

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl( INADDR_BROADCAST );
    addr.sin_port = htons( 10000 );

    system_log( "Send AirKiss ack to WECHAT" );

    while ( 1 )
    {
        sendto( fd, &random, 1, 0, (struct sockaddr *) &addr, sizeof(addr) );
        mico_thread_msleep( 100 );
        i++;
        if ( i > 20 )
            break;
    }
    exit:
    if ( err != kNoErr )
        system_log( "thread exit with err: %d", err );
    SocketClose( &fd );
}

