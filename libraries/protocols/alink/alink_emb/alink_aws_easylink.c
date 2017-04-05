#include "mico.h"
#include "StringUtils.h"
#include "alink_export.h"
#include "alink_platform.h"
#include "HTTPUtils.h"
#include "SocketUtils.h"
#include "alink_aws.h"

#define aws_el_log(format, ...)  custom_log("", format, ##__VA_ARGS__)

#define kEasyLinkURLAuth          "/auth-setup"

static OSStatus onReceivedData( struct _HTTPHeader_t * httpHeader,
                                uint32_t pos,
                                uint8_t *data,
                                size_t len,
                                void * userContext );

static bool is_stop=false;
static char easylink_server_address[16];
static mico_semaphore_t easylink_sem = NULL;
static char ssid[32] = {0};
static char key[64] = {0};

static void awssEasyLinkNotify_EasyLinkCompleteHandler( network_InitTypeDef_st *nwkpara, mico_Context_t * const inContext )
{
    OSStatus err = kNoErr;

    require_action( inContext, exit, err = kParamErr );
    require_action( nwkpara, exit, err = kTimeoutErr );

    /* Store SSID and KEY*/
    memcpy( ssid, nwkpara->wifi_ssid, maxSsidLen );
    memcpy( key, nwkpara->wifi_key, maxKeyLen );

    aws_el_log("Get SSID: %s, Key: %s",ssid, key);

    exit:
    if ( err != kNoErr )
    {
        /*EasyLink timeout or error*/
        aws_el_log("EasyLink step 1 ERROR, err: %d", err);
        mico_rtos_set_semaphore( &easylink_sem );
    }
    return;
}

static void awssEasyLinkNotify_EasyLinkGetExtraDataHandler( int datalen, char* data,
                                                            mico_Context_t * const inContext )
    {
        OSStatus err = kNoErr;
        int index;
        uint32_t *ipInfo, ipInfoCount;
        char *debugString;
        struct in_addr ipv4_addr;
        mico_system_status_wlan_t *wlan_status;
        mico_system_wlan_get_status( &wlan_status );

        require_action( inContext, exit, err = kParamErr );

        debugString = DataToHexStringWithSpaces( (const uint8_t *) data, datalen );
        aws_el_log("Get user info: %s", debugString);
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

        /* Read identifier */
        ipInfo = (uint32_t *) &data[index];

        /* Identifier: 1 x uint32_t or Identifier/localIp/netMask/gateWay/dnsServer: 5 x uint32_t */
        ipInfoCount = (datalen - index) / sizeof(uint32_t);
        require_action( ipInfoCount >= 1, exit, err = kParamErr );

        if( ipInfoCount == 1 ){
            ipv4_addr.s_addr = ntohl(*(uint32_t *)(ipInfo));
            strcpy( (char *) easylink_server_address, inet_ntoa( ipv4_addr ) );
            aws_el_log("Get auth info: %s, EasyLink server ip address: %s", data, easylink_server_address);
        }else{
            aws_el_log("Use static ip address");
        }

        exit:
        if ( err != kNoErr )
        {
            /*EasyLink error*/
            aws_el_log("EasyLink step 2 ERROR, err: %d", err);
        }

        mico_rtos_set_semaphore( &easylink_sem );
        return;
}

static void alink_connect_to_ftc_server( void )
{
    OSStatus err;
    int client_fd = -1;
    fd_set readfds;
    char ipstr[16];
    struct sockaddr_in addr;
    HTTPHeader_t *httpHeader = NULL;
    struct in_addr in_addr;
    char  *json_str = NULL;
    uint8_t  *httpResponse = NULL;
    size_t      httpResponseLen = 0;
    char dev_name[30];
    char json_message_fmt[] = "{ \"T\": \"Current Configuration\", \"N\": \"%s\", \"C\": [ { \"N\": \"MICO SYSTEM\", \"C\": [ { \"N\": \"model\", \"C\": \"%s\", \"P\": \"RO\" }, { \"N\": \"mac\",\"C\": \"%s\", \"P\": \"RO\" }, ] } ], \"PO\": \"com.alink.ha\", \"HD\": \"%s\", \"FW\": \"%s\" }";
    mico_system_status_wlan_t *wlan_status;
    mico_system_wlan_get_status( &wlan_status );

    in_addr.s_addr = inet_addr( easylink_server_address );
    strcpy( ipstr, inet_ntoa(in_addr));
    aws_el_log("host ip: %s", ipstr);

    /*HTTPHeaderCreateWithCallback set some callback functions */
    httpHeader = HTTPHeaderCreateWithCallback( 1024, onReceivedData, NULL, NULL );
    require_action( httpHeader, exit, err = kNoMemoryErr );

    client_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    addr.sin_family = AF_INET;
    addr.sin_addr = in_addr;
    addr.sin_port = htons(FTC_PORT);
    err = connect( client_fd, (struct sockaddr *)&addr, sizeof(addr) );
    require_noerr_string( err, exit, "connect http server failed" );

    snprintf(dev_name, 30, "%s(%c%c%c%c)",MODEL,
             wlan_status->mac[12], wlan_status->mac[13],
             wlan_status->mac[15], wlan_status->mac[16]);
    json_str = malloc(400);
    memset(json_str, 0x00, 400);
    sprintf(json_str, json_message_fmt, dev_name, product_model, wlan_status->mac, HARDWARE_REVISION, product_dev_version);
    err =  CreateHTTPMessage( "POST", kEasyLinkURLAuth, kMIMEType_JSON, (uint8_t *)json_str, strlen(json_str), &httpResponse, &httpResponseLen );
    aws_el_log("create message %d\r\n%.*s", httpResponseLen, httpResponseLen, httpResponse);
    require_noerr( err, exit );
    require( httpResponse, exit );
    /* Send HTTP Request */
    SocketSend( client_fd, httpResponse, httpResponseLen );
    free(json_str);
    free(httpResponse);

    FD_ZERO( &readfds );
    FD_SET( client_fd, &readfds );

    select( client_fd + 1, &readfds, NULL, NULL, NULL );
    if ( FD_ISSET( client_fd, &readfds ) )
    {
        /*parse header*/
        err = SocketReadHTTPHeader( client_fd, httpHeader );
        switch ( err )
        {
            case kNoErr:
                PrintHTTPHeader( httpHeader );
                err = SocketReadHTTPBody( client_fd, httpHeader );/*get body data*/
                require_noerr( err, exit );
                /*get data and print*/
                break;
            case EWOULDBLOCK:
                case kNoSpaceErr:
                case kConnectionErr:
                default:
                    aws_el_log("ERROR: HTTP Header parse error: %d", err);
                break;
        }
    }

    exit:
    aws_el_log( "Exit: Client exit with err = %d, fd: %d", err, client_fd );
    SocketClose( &client_fd );
    HTTPHeaderDestory( &httpHeader );
}

/*one request may receive multi reply*/
static OSStatus onReceivedData( struct _HTTPHeader_t * inHeader, uint32_t inPos, uint8_t * inData,
                                size_t inLen, void * inUserContext )
{
    OSStatus err = kNoErr;
    aws_el_log("recv data: %s", inData);
    return err;
}

static void aws_easylink_config_thread( uint32_t arg )
{
    OSStatus err;
    mico_Context_t *mico_context =  mico_system_context_get( );

    mico_rtos_init_semaphore( &easylink_sem, 1);

    err = mico_system_notify_register( mico_notify_EASYLINK_WPS_COMPLETED,
                                 (void *) awssEasyLinkNotify_EasyLinkCompleteHandler,
                                 (void *) mico_context );
    require_noerr( err, exit );

    err = mico_system_notify_register( mico_notify_EASYLINK_GET_EXTRA_DATA,
                                 (void *) awssEasyLinkNotify_EasyLinkGetExtraDataHandler,
                                 (void *) mico_context );
    require_noerr( err, exit );

    mico_rtos_get_semaphore( &easylink_sem, MICO_WAIT_FOREVER );
    if( is_stop == true ){
        is_stop = false;
        goto exit;
    }
    awss_stop();

    err = platform_awss_connect_ap(60*1000, ssid, key, 0, 0, NULL, 0 );
    require_noerr( err, exit );

    alink_connect_to_ftc_server();

    exit:
    mico_system_notify_remove(mico_notify_EASYLINK_WPS_COMPLETED,
                              (void *) awssEasyLinkNotify_EasyLinkCompleteHandler);
    mico_system_notify_remove(mico_notify_EASYLINK_GET_EXTRA_DATA,
                              (void *) awssEasyLinkNotify_EasyLinkGetExtraDataHandler);
    mico_rtos_deinit_semaphore( &easylink_sem );
    mico_rtos_delete_thread(NULL);
}

OSStatus awss_easylink_start( void )
{
    return mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "ae", aws_easylink_config_thread,
                                               0x1500, 0 );
}

OSStatus awss_easylink_stop( void )
{
    OSStatus err = kNoErr ;
    is_stop = true;
    if( easylink_sem != NULL ){
        err =  mico_rtos_set_semaphore( &easylink_sem );
    }
    return err;
}
