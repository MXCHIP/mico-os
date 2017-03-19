#include "alink_export.h"
#include "alink_aws.h"
#include "product.h"
#include "system_internal.h"

#define aws_log(format, ...)  custom_log("aws", format, ##__VA_ARGS__)

static mico_Context_t *mico_context;
static mico_semaphore_t net_semp = NULL;

char *vendor_get_model( void )
{
    return product_model;
}
char *vendor_get_secret( void )
{
    return product_secret;
}
char *vendor_get_mac( void )
{
    mico_system_status_wlan_t* wlan_status;
    mico_system_wlan_get_status( &wlan_status );
    return wlan_status->mac;
}

static void awsNotify_WifiStatusHandler( WiFiEvent event, mico_Context_t * const inContext )
{
    require( inContext, exit );

    switch ( event )
    {
        case NOTIFY_STATION_UP:
            inContext->micoSystemConfig.configured = allConfigured;
            mico_system_context_update( inContext ); //Update Flash content
            if ( net_semp )
                mico_rtos_set_semaphore( &net_semp );
            break;
        default:
            break;
    }
    exit:
    return;
}

void aws_softap_setup( void )
{
    /*
     * wilress params: 11BGN
     * channel: auto, or 1, 6, 11
     * authentication: OPEN
     * encryption: NONE
     * gatewayip: 172.31.254.250, netmask: 255.255.255.0
     * DNS server: 172.31.254.250.  IMPORTANT!!!  ios depend on it!
     * DHCP: enable
     * SSID: 32 ascii char at most
     * softap timeout: 5min
     */

    network_InitTypeDef_st wNetConfig;
    memset( &wNetConfig, 0x00, sizeof(network_InitTypeDef_st) );

    wNetConfig.wifi_mode = Soft_AP;
    snprintf( wNetConfig.wifi_ssid, STR_SSID_LEN, "alink_%s", vendor_get_model( ) );
    strcpy( wNetConfig.wifi_key, "" );
    strcpy( wNetConfig.local_ip_addr, "172.31.254.250" );
    strcpy( wNetConfig.net_mask, "255.255.255.0" );
    strcpy( wNetConfig.gateway_ip_addr, "172.31.254.250" );
    wNetConfig.dhcpMode = DHCP_Server;
    micoWlanStart( &wNetConfig );
    aws_log("soft ap : %s", wNetConfig.wifi_ssid);
}

static void aws_config_thread( uint32_t arg )
{
    OSStatus err = kNoErr;
    char ssid[32 + 1];
    char passwd[64 + 1];
    char bssid[6];
    char auth;
    char encry;
    char channel;
    int ret;

    mico_system_delegate_config_will_start( );
    aws_log("start alink_1_1 aws mode");

    err = mico_rtos_init_semaphore( &net_semp, 1 );
    require_noerr( err, exit );

    err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED,
                                       (void *) awsNotify_WifiStatusHandler,
                                       mico_context );
    require_noerr( err, exit );

    aws_start( NULL, NULL, NULL, NULL );

    ret = aws_get_ssid_passwd( &ssid[0], &passwd[0], &bssid[0], &auth, &encry, &channel );
    if ( !ret )
    {
        mico_system_delegate_config_will_stop( );
        MicoSysLed( false );
        aws_log("alink wireless setup timeout!");
        /* prepare and setup softap */
        aws_softap_setup( );
        /* tcp server to get ssid & passwd */
        aws_softap_tcp_server( &ssid[0], &passwd[0] );

        micoWlanSuspendSoftAP( );
    }
    mico_system_delegate_config_recv_ssid( ssid, passwd );
    aws_log("ssid:%s, passwd:%s", ssid, passwd);

    memcpy( mico_context->micoSystemConfig.ssid, ssid, maxSsidLen );
    memset( mico_context->micoSystemConfig.bssid, 0x0, 6 );
    memcpy( mico_context->micoSystemConfig.user_key, passwd, maxKeyLen );
    mico_context->micoSystemConfig.user_keyLength = strlen( passwd );
    mico_context->micoSystemConfig.dhcpEnable = true;

    system_connect_wifi_normal( system_context( ) );

    mico_rtos_get_semaphore( &net_semp, MICO_WAIT_FOREVER ); //wait until get semaphore
    mico_system_delegate_config_will_stop( );
    aws_notify_app( );

    exit:
    aws_destroy( );
    mico_system_notify_remove( mico_notify_WIFI_STATUS_CHANGED,
                               (void *) awsNotify_WifiStatusHandler );
    mico_rtos_deinit_semaphore( &net_semp );
    net_semp = NULL;
    mico_rtos_delete_thread( NULL );
}

OSStatus start_aws_config_mode( void )
{
    mico_context = mico_system_context_get( );

    return mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "aws", aws_config_thread,
                                    0x1000,
                                    0 );
}

