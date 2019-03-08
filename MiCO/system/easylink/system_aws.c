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

#ifndef MICO_AWS_AP_SSID_HEADER
#define MICO_AWS_AP_SSID_HEADER     "AWS_"
#endif

#define AWS_NOTIFY_INTERVAL (20*1000)
#define AWS_NOTIFY_TIMES    500
//#if (MICO_WLAN_CONFIG_MODE == CONFIG_MODE_EASYLINK) || (MICO_WLAN_CONFIG_MODE == CONFIG_MODE_EASYLINK_WITH_SOFTAP)

/******************************************************
 *               Function Declarations
 ******************************************************/
/* EasyLink event callback functions*/
static void aws_wifi_status_cb( WiFiEvent event, system_context_t * const inContext );
static void aws_complete_cb( network_InitTypeDef_st *nwkpara, system_context_t * const inContext );

/* Thread perform easylink and connect to wlan */
static void aws_thread( uint32_t inContext ); /* Perform easylink and connect to wlan */
char* aws_notify_msg_create(system_context_t *context);

/******************************************************
 *               Variables Definitions
 ******************************************************/
static mico_semaphore_t aws_sem;         /**< Used to suspend thread while easylink. */
static mico_semaphore_t aws_connect_sem; /**< Used to suspend thread while connection. */
static bool aws_success = false;         /**< true: connect to wlan, false: start soft ap mode or roll back to previous settings */
static mico_thread_t aws_thread_handler = NULL;
static bool aws_thread_force_exit = false;
static uint32_t awsIndentifier = 0;      /**< Unique for an aws instance. */
static bool is_config_by_softap = false;
static bool is_config_by_aws = false;
static bool is_ap_up = false;


/******************************************************
 *               Function Definitions
 ******************************************************/

/* MiCO callback when WiFi status is changed */
static void aws_wifi_status_cb( WiFiEvent event, system_context_t * const inContext )
{
    switch ( event )
    {
        case NOTIFY_STATION_UP:
            inContext->flashContentInRam.micoSystemConfig.configured = allConfigured;
            mico_system_context_update( &inContext->flashContentInRam ); //Update Flash content
            mico_rtos_set_semaphore( &aws_connect_sem ); //Notify Easylink thread
            break;
        default:
            break;
    }
    return;
}

/* MiCO callback when EasyLink is finished step 1, return SSID and KEY */
static void aws_complete_cb( network_InitTypeDef_st *nwkpara, system_context_t * const inContext )
{
    OSStatus err = kNoErr;

    require_action_string( nwkpara, exit, err = kTimeoutErr, "AWS Timeout or terminated" );

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
    aws_success = true;
    is_config_by_softap = false;
    is_config_by_aws = true;
    exit:
    if ( err != kNoErr )
    {
        /*EasyLink timeout or error*/
        aws_success = false;
    }

    if ( is_ap_up == false )
    {
        mico_rtos_set_semaphore( &aws_sem );
    }

    return;
}

#define UDP_TX_PORT         (65123)
#define UDP_RX_PORT         (65126)

MAY_BE_UNUSED static int aws_broadcast_notification(char *msg, int msg_num)
{
    int i, ret, result = 0;
    int fd;
    socklen_t addrlen;
    fd_set readfds;
    struct timeval t;
    struct sockaddr_in s_addr;
    int buf_len = 1024;
    char *buf = malloc(buf_len);
    uint8_t stop = 0xEE;
    
    memset(buf, 0, buf_len);
    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
    {
        system_log("CREATE UDP SOCKET ERROR!!!\n");
        return -1;
    }
    memset(&s_addr, 0, sizeof(struct sockaddr_in));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = INADDR_ANY;
    s_addr.sin_port = htons(UDP_RX_PORT);
    if (bind(fd, (struct sockaddr*)&s_addr, sizeof(s_addr)) < 0)
    {
        system_log("BIND UDP SOCKET ERROR!!!\n");
        return -2;
    }
    system_log("UDP SOCKET initialized!\n");

    memset(&s_addr, 0, sizeof(struct sockaddr_in));
    s_addr.sin_family = AF_INET;
    s_addr.sin_addr.s_addr = INADDR_BROADCAST;
    s_addr.sin_port = htons(UDP_TX_PORT);
    //stop APP broadcast aws packets.
    for(i=0; i<10; i++) {
        ret = sendto(fd, &stop, 1, 0, (struct sockaddr *)&s_addr, sizeof(s_addr));
        if (ret > 0) {
            i++; // sendto fail don't i++, max sending times is 10, min sending times is 5.
        }
        mico_rtos_thread_msleep(20);
    }
    
    //send notification
    for (i = 0; i < msg_num; i++) {
        if (aws_thread_force_exit == true) {
            break;
        }
        ret = sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&s_addr, sizeof(s_addr));
        if (ret < 0) {
            system_log("awss send notify msg ERROR!\r\n");
        } 

        FD_ZERO(&readfds);
        t.tv_sec = 0;
        t.tv_usec = AWS_NOTIFY_INTERVAL;
        FD_SET(fd, &readfds);
        ret = select(fd+1, &readfds, NULL, NULL, &t);
        if (ret > 0) {
            addrlen = sizeof(s_addr);
            ret = recvfrom(fd, buf, buf_len, 0, (struct sockaddr *)&s_addr, &addrlen);
            system_log("rx len %d\n", ret);
            if (ret > 0) {
                //buf[ret] = '\0';
                system_log("rx: %s\n", buf);
                buf[strlen(buf)-1] = '\0';
                sprintf(buf, "%s,\"IP\":\"%s\",\"PORT\":%d}", buf, (char *)inet_ntoa(s_addr.sin_addr), hton16(s_addr.sin_port));
                mico_easylink_aws_delegate_recv_notify_msg(buf);
                result = 1;
                break;
            }
        }
    }

    free(buf);
    close(fd);
    if (result == 0) {
        system_log("awss notify %d times, no response\r\n", msg_num);
    }
    return result;
}

#if PLATFORM_CONFIG_AWS_SOFTAP_COEXISTENCE
void mico_notify_ap_up( void )
{
	system_log("ap is up");
	is_ap_up = true;
}

void aws_uap_configured_cd( uint32_t id )
{
    aws_success = true;
    awsIndentifier = id;
    is_config_by_softap = true;
    is_config_by_aws = false;
    micoWlanSuspendSoftAP( );
    mico_rtos_set_semaphore( &aws_sem );
}
#endif

static void aws_thread( uint32_t arg )
{
    OSStatus err = kNoErr;
    system_context_t *context = (system_context_t *) arg;
    char *aws_msg = aws_notify_msg_create(context);

    if (aws_msg == NULL) {
        system_log("Not enough memory!!");
        goto exit;
    }

    aws_success = false;
    aws_thread_force_exit = false;

    mico_system_notify_register( mico_notify_EASYLINK_WPS_COMPLETED,    (void *) aws_complete_cb,      context );
    mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED,       (void *) aws_wifi_status_cb,   context );

    mico_rtos_init_semaphore( &aws_sem,            1 );
    mico_rtos_init_semaphore( &aws_connect_sem,    1 );

restart:
    mico_system_delegate_config_will_start( );
    system_log("Start AWS mode");
#if PLATFORM_CONFIG_AWS_SOFTAP_COEXISTENCE
    char wifi_ssid[32];
    sprintf(wifi_ssid, "%s%c%c%c%c%c%c", MICO_AWS_AP_SSID_HEADER,
            context->micoStatus.mac[9], context->micoStatus.mac[10], context->micoStatus.mac[12],
            context->micoStatus.mac[13], context->micoStatus.mac[15], context->micoStatus.mac[16]);

    system_log("Enable softap %s in aws", wifi_ssid);
    config_server_set_uap_cb( aws_uap_configured_cd );
    mico_wlan_aws_uap_start(EasyLink_TimeOut / 1000, wifi_ssid, NULL, 6,mico_notify_ap_up);
#else
    micoWlanStartAws( EasyLink_TimeOut / 1000 );
#endif
    while( mico_rtos_get_semaphore( &aws_sem, 0 ) == kNoErr );
    err = mico_rtos_get_semaphore( &aws_sem, MICO_WAIT_FOREVER );

    /* Easylink force exit by user, clean and exit */
    if( err != kNoErr && aws_thread_force_exit )
    {
        system_log("AWS waiting for terminate");
        micoWlanStopAws( );
        mico_rtos_get_semaphore( &aws_sem, 3000 );
        system_log("AWS canceled by user");
        goto exit;
    }

    /* AWS Success */
    if ( aws_success == true )
    {
        mico_system_delegate_config_recv_ssid( context->flashContentInRam.micoSystemConfig.ssid,
                                               context->flashContentInRam.micoSystemConfig.user_key );
        micoWlanSuspend();
        system_connect_wifi_normal( context );

        /* Wait for station connection */
        while( mico_rtos_get_semaphore( &aws_connect_sem, 0 ) == kNoErr );
        err = mico_rtos_get_semaphore( &aws_connect_sem, EasyLink_ConnectWlan_Timeout );
        /* AWS force exit by user, clean and exit */
        if( err != kNoErr && aws_thread_force_exit )
        {
            micoWlanSuspend();
            system_log("AWS connection canceled by user");
            goto exit;
        }

        /*SSID or Password is not correct, module cannot connect to wlan, so restart AWS again*/
        require_noerr_action_string( err, restart, micoWlanSuspend(), "Re-start AWS mode" );
        if ( is_config_by_aws == true )
        {
            mico_system_delegate_config_success( CONFIG_BY_AWS );
        }

        if ( is_config_by_softap == true )
        {
            mico_system_delegate_config_success( CONFIG_BY_SOFT_AP );
        }

        if ( is_config_by_aws == true )
        {
            /* mico_config.h can define MICO_AWS_NOTIFY_DISABLE to disable send aws notification */
#ifndef MICO_AWS_NOTIFY_DISABLE
            /* Start AWS udp notify */
            aws_broadcast_notification(aws_msg, AWS_NOTIFY_TIMES);
#endif
        }
        goto exit;
    }
    else /* aws failed */
    {
#ifndef PLATFORM_CONFIG_AWS_SOFTAP_COEXISTENCE
        mico_system_delegate_easylink_timeout(context);
#endif
    }

exit:
    aws_thread_force_exit = false;
    if (aws_msg) {
        free(aws_msg);
    }
    mico_system_delegate_config_will_stop( );

    mico_system_notify_remove( mico_notify_WIFI_STATUS_CHANGED, (void *)aws_wifi_status_cb );
    mico_system_notify_remove( mico_notify_EASYLINK_WPS_COMPLETED, (void *)aws_complete_cb );

    mico_rtos_deinit_semaphore( &aws_sem );
    mico_rtos_deinit_semaphore( &aws_connect_sem );
    aws_thread_handler = NULL;
    mico_rtos_delete_thread( NULL );
}

OSStatus mico_easylink_aws( mico_Context_t * const in_context, mico_bool_t enable )
{
    OSStatus err = kUnknownErr;

    require_action( in_context, exit, err = kNotPreparedErr );

    /* easylink thread existed? stop! */
    if ( aws_thread_handler ) {
        system_log("EasyLink processing, force stop..");
        aws_thread_force_exit = true;
        mico_rtos_thread_force_awake( &aws_thread_handler );
        mico_rtos_thread_join( &aws_thread_handler );
    }

    if ( enable == MICO_TRUE ) {
        err = mico_rtos_create_thread( &aws_thread_handler, MICO_APPLICATION_PRIORITY, "aws", aws_thread,
                                       0x1000, (mico_thread_arg_t) in_context );
        require_noerr_string( err, exit, "ERROR: Unable to start the EasyLink thread." );

        /* Make sure easylink is already running, and waiting for sem trigger */
        mico_rtos_delay_milliseconds( 1000 );
    }

    exit:
    return err;
}

#define AWS_NOTIFY_MSG_LEN 512
char* aws_notify_msg_create(system_context_t *context)
{
    char *aws_notify_msg = (char*)malloc(AWS_NOTIFY_MSG_LEN);
    char sn[64];
    uint8_t mac[6];

    mico_wlan_get_mac_address(mac);
    sprintf(sn, "%02X0%02X0%02X0%02X0%02X0%02X", 
        mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
    
    if (aws_notify_msg == NULL) {
        
        goto exit;
    }
    memset(aws_notify_msg, 0, AWS_NOTIFY_MSG_LEN);
#if 1
    snprintf(aws_notify_msg, AWS_NOTIFY_MSG_LEN,
             "{\"FW\":\"%s\",\"HD\":\"%s\",\"PO\":\"%s\",\"RF\":\"%s\",\"MAC\":\"%s\",\"OS\":\"%s\",\"MD\":\"%s\",\"MF\":\"%s\"",
             FIRMWARE_REVISION, HARDWARE_REVISION, PROTOCOL,
             context->micoStatus.rf_version, context->micoStatus.mac,
             MicoGetVer( ), MODEL, MANUFACTURER );

    sprintf(aws_notify_msg, "%s,\"wlan unconfigured\":\"F\"", aws_notify_msg);

#ifdef MICO_CONFIG_SERVER_ENABLE
    sprintf(aws_notify_msg, "%s,\"FTC\":\"T\",\"PORT\":%d", aws_notify_msg,MICO_CONFIG_SERVER_PORT);
#else
    sprintf(aws_notify_msg, "%s,\"FTC\":\"F\"", aws_notify_msg);
#endif
#else
    sprintf(aws_notify_msg, "{\"version\":\"1.6\",\"model\":\"%s\",\"sn\":\"%s\"}",
        "ALINKTEST_LIVING_LIGHT_ALINK_TEST", sn);
#endif
    sprintf(aws_notify_msg, "%s,\"ExtraData\":\"", aws_notify_msg);

    mico_easylink_aws_delegate_send_notify_msg(aws_notify_msg);

     sprintf(aws_notify_msg, "%s\"}", aws_notify_msg);
exit:
    return aws_notify_msg;
}

//#endif

