
#include "include.h"
#include "rtos_pub.h"
#include "error.h"
#include "sockets.h"

#define tcp_server_log(M, ...) os_printf("TCP", M, ##__VA_ARGS__)

#define SERVER_PORT 20000 /*set up a tcp server,port at 20000*/

void tcp_client_thread( mico_thread_arg_t arg )
{
    OSStatus err = kNoErr;
    int fd = (int) arg;
    int len = 0;
    fd_set readfds; 
    char *buf = NULL;
    struct timeval t;

    buf = (char*) os_malloc( 1024 );
    
    t.tv_sec = 5;
    t.tv_usec = 0;
    
    while ( 1 )
    {		
        FD_ZERO( &readfds );
        FD_SET( fd, &readfds );

        select( fd+1, &readfds, NULL, NULL, &t);

        if ( FD_ISSET( fd, &readfds ) ) /*one client has data*/
        {
            len = recv( fd, buf, 1024, 0 );

            if ( len == 0 )
            {
                tcp_server_log( "TCP Client is disconnected, fd: %d", fd );
                goto exit;
            }

            //tcp_server_log("fd: %d, recv data %d from client", fd, len);
            len = send( fd, buf, len, 0 );
            //tcp_server_log("fd: %d, send data %d to client", fd, len);
        }
    }
    exit:
    if ( err != kNoErr ) tcp_server_log( "TCP client thread exit with err: %d", err );
    if ( buf != NULL ) free( buf );
    close( fd );
    mico_rtos_delete_thread( NULL );
}

/* TCP server listener thread */
void tcp_server_thread( mico_thread_arg_t arg )
{
    (void)( arg );
    OSStatus err = kNoErr;
    struct sockaddr_in server_addr, client_addr;
    socklen_t sockaddr_t_size = sizeof(client_addr);
    char client_ip_str[16];
    int tcp_listen_fd = -1, client_fd = -1;
    fd_set readfds;

    tcp_listen_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;/* Accept conenction request on all network interface */
    server_addr.sin_port = htons( SERVER_PORT );/* Server listen on port: 20000 */

    err = bind( tcp_listen_fd, (struct sockaddr *) &server_addr, sizeof(server_addr) );
    
    err = listen( tcp_listen_fd, 0 );
    
    while ( 1 )
    {
        FD_ZERO( &readfds );
        FD_SET( tcp_listen_fd, &readfds );

        select( tcp_listen_fd + 1, &readfds, NULL, NULL, NULL);

        if ( FD_ISSET( tcp_listen_fd, &readfds ) )
        {
            client_fd = accept( tcp_listen_fd, (struct sockaddr *) &client_addr, &sockaddr_t_size );
            if ( client_fd >= 0 )
            {
//                inet_ntoa( client_ip_str, client_addr.s_ip );
                strcpy( client_ip_str, inet_ntoa( client_addr.sin_addr ) );
                tcp_server_log( "TCP Client %s:%d connected, fd: %d", client_ip_str, client_addr.sin_port, client_fd );
                if ( kNoErr
                     != mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "TCP Clients",
                                                 tcp_client_thread,
                                                 0x800, client_fd ) ) {
                    close( client_fd );
					client_fd = -1;
                }
            }
        }
    }
    exit:
    if ( err != kNoErr ) tcp_server_log( "Server listerner thread exit with err: %d", err );
    close( tcp_listen_fd );
    mico_rtos_delete_thread( NULL );
}

int application_start( void )
{
    OSStatus err = kNoErr;
	return 0;

    /* Start TCP server listener thread*/
    err = mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "TCP_server", tcp_server_thread,
                                   0x800,
                                   0 );

    exit:
    mico_rtos_delete_thread( NULL );
    return err;
}

