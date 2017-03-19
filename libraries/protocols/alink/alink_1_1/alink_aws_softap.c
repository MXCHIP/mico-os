#include "alink_aws.h"
#include "SocketUtils.h"

#define aws_softap_log(format, ...)  custom_log("aws", format, ##__VA_ARGS__)

static mico_semaphore_t softap_sem;
static char softap_ssid[STR_SSID_LEN];
static char softap_passwd[STR_PASSWD_LEN];
static bool is_delete_tcp_server = false;

/* json info parser */
int aws_softap_get_ssid_and_passwd( char *msg, char ssid[STR_SSID_LEN],
                                    char passwd[STR_PASSWD_LEN] )
{
    char *ptr, *end, *name;
    int len;

    //ssid
    name = "\"ssid\":";
    ptr = strstr( msg, name );
    if ( !ptr )
    {
        aws_softap_log("%s not found!\n", name);
        goto exit;
    }
    ptr += strlen( name );
    while ( *ptr++ == ' ' )
        ;/* eating the beginning " */
    end = strchr( ptr, '"' );
    len = end - ptr;

    if ( len > STR_SSID_LEN )
    {
        goto exit;
    }
    strncpy( ssid, ptr, len );
    ssid[len] = '\0';

    //passwd
    name = "\"passwd\":";
    ptr = strstr( msg, name );
    if ( !ptr )
    {
        aws_softap_log( "%s not found!\n", name );
        goto exit;
    }

    ptr += strlen( name );
    while ( *ptr++ == ' ' )
        ;/* eating the beginning " */
    end = strchr( ptr, '"' );
    len = end - ptr;

    if ( len > STR_PASSWD_LEN )
    {
        goto exit;
    }
    strncpy( passwd, ptr, len );
    passwd[len] = '\0';

    //bssid-mac
    name = "\"bssid\":";
    ptr = strstr( msg, name );
    if ( !ptr )
    {
        aws_softap_log( "%s not found!\n", name );
        goto exit;
    }

    ptr += strlen( name );
    while ( *ptr++ == ' ' )
        ;/* eating the beginning " */
    end = strchr( ptr, '"' );
    len = end - ptr;

#if 0
    memset(aws_bssid, 0, sizeof(aws_bssid));

    sscanf(ptr, "%02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx",
        &aws_bssid[0], &aws_bssid[1], &aws_bssid[2],
        &aws_bssid[3], &aws_bssid[4], &aws_bssid[5]);
#endif

    return 0;
    exit:
    return -1;
}

void aws_softap_tcp_client_thread( mico_thread_arg_t arg )
{
    OSStatus err = kNoErr;
    int fd = (int) arg;
    int len = 0;
    fd_set readfds;
    struct timeval t;
    char *buf = NULL, *msg = NULL;
    int buf_size = 512, msg_size = 512;
    char ssid[32 + 1];
    char passwd[64 + 1];

    buf = (char*) malloc( buf_size );
    require_action( buf, exit, err = kNoMemoryErr );

    msg = (char*) malloc( msg_size );
    require_action( buf, exit, err = kNoMemoryErr );

    t.tv_sec = 5;
    t.tv_usec = 0;

    while ( 1 )
    {
        FD_ZERO( &readfds );
        FD_SET( fd, &readfds );

        require_action( select( fd+1, &readfds, NULL, NULL, &t) >= 0, exit, err = kConnectionErr );

        if ( FD_ISSET( fd, &readfds ) ) /*one client has data*/
        {
            len = recv( fd, buf, buf_size, 0 );

            buf[len] = 0;
            aws_softap_log("softap tcp server recv: %s", buf);

            err = aws_softap_get_ssid_and_passwd( buf, &ssid[0], &passwd[0] );
            if ( err == kNoErr )
            {
                strcpy( softap_ssid, ssid );
                strcpy( softap_passwd, passwd );
                snprintf(
                    msg, buf_size,
                    "{\"code\":1000, \"msg\":\"format ok\", \"model\":\"%s\", \"mac\":\"%s\"}",
                    vendor_get_model( ), vendor_get_mac( ) );
            } else
                snprintf(
                    msg, buf_size,
                    "{\"code\":2000, \"msg\":\"format error\", \"model\":\"%s\", \"mac\":\"%s\"}",
                    vendor_get_model( ), vendor_get_mac( ) );

            len = send( fd, msg, strlen( msg ), 0 );
            aws_softap_log( "ack %s\n", msg );

            if ( err == kNoErr )
            {
                mico_rtos_set_semaphore( &softap_sem );
                is_delete_tcp_server = true;
                goto exit;
            }
        }
    }
    exit:
    aws_softap_log( "TCP client thread exit with err: %d", err );
    if ( buf != NULL ) free( buf );
    if ( msg != NULL ) free( msg );
    SocketClose( &fd );
    mico_rtos_delete_thread( NULL );
}

//setup softap server
void aws_softap_tcp_server_thread( uint32_t arg )
{
    UNUSED_PARAMETER( arg );
    OSStatus err = kNoErr;
    struct sockaddr_in server_addr, client_addr;
    socklen_t sockaddr_t_size = sizeof(client_addr);
    char client_ip_str[16];
    int tcp_listen_fd = -1, client_fd = -1;
    fd_set readfds;
    struct timeval t;

    t.tv_sec = 5;
    t.tv_usec = 0;

    tcp_listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    require_action( IsValidSocket( tcp_listen_fd ), exit, err = kNoResourcesErr );

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;/* Accept conenction request on all network interface */
    server_addr.sin_port = htons( SOFTAP_TCP_SERVER_PORT );/* Server listen on port: 20000 */

    err = bind( tcp_listen_fd, (struct sockaddr *) &server_addr, sizeof(server_addr) );
    require_noerr( err, exit );

    err = listen( tcp_listen_fd, 0 );
    require_noerr( err, exit );

    while ( 1 )
    {
        FD_ZERO( &readfds );
        FD_SET( tcp_listen_fd, &readfds );

        require( select( tcp_listen_fd + 1, &readfds, NULL, NULL, &t) >= 0, exit );

        if ( FD_ISSET( tcp_listen_fd, &readfds ) )
        {
            client_fd = accept( tcp_listen_fd, (struct sockaddr *) &client_addr, &sockaddr_t_size );
            if ( IsValidSocket( client_fd ) )
            {
                strcpy( client_ip_str, inet_ntoa( client_addr.sin_addr ) );
                aws_softap_log( "TCP Client %s:%d connected, fd: %d", client_ip_str, client_addr.sin_port, client_fd );
                if ( kNoErr
                     != mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "softap clients",
                                                 aws_softap_tcp_client_thread,
                                                 0x800, client_fd ) )
                    SocketClose( &client_fd );
            }
        }

        if ( is_delete_tcp_server == true )
        {
            goto exit;
        }
    }
    exit:
    aws_softap_log( "Server listerner thread exit with err: %d", err );
    SocketClose( &tcp_listen_fd );
    mico_rtos_delete_thread( NULL );
}

OSStatus aws_softap_tcp_server( char ssid[STR_SSID_LEN], char passwd[STR_PASSWD_LEN] )
{
    OSStatus err = kNoErr;

    err = mico_rtos_init_semaphore( &softap_sem, 1 );
    require_noerr( err, exit );

    err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "softap server",
                                   aws_softap_tcp_server_thread,
                                   0x1000,
                                   0 );
    require_noerr( err, exit );

    err = mico_rtos_get_semaphore( &softap_sem, MICO_NEVER_TIMEOUT );
    require_noerr( err, exit );

    strcpy( ssid, softap_ssid );
    strcpy( passwd, softap_passwd );

    exit:
    return err;
}
