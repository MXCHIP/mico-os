#include "mico.h"
#include "platform.h"
#include "alink_platform.h"
#include "alink_export.h"
#include <CheckSumUtils.h>
#include "system_internal.h"
#include "mico_app_define.h"

#define alink_platform_log(M, ...) custom_log("", M, ##__VA_ARGS__)
// #define alink_platform_log(M, ...)

#define UART_LOG_FAST

#ifdef UART_LOG_FAST
#define UART_BUFFER_LENGTH 100
ring_buffer_t rx_buffer;
uint8_t rx_data[UART_BUFFER_LENGTH];
static int uart_inited = 0;
#endif

/*********************************** thread interface ***********************************/

/** @defgroup group_platform_thread platform_thread
 *  @{
 */

/**
 * @brief create a thread.
 *
 * @param[out] thread @n The new thread handle.
 * @param[in] name @n thread name.
 * @param[in] start_routine @n A pointer to the application-defined function to be executed by the thread.
 This pointer represents the starting address of the thread.
 * @param[in] arg @n A pointer to a variable to be passed to the start_routine.
 * @param[in] stack @n A pointer to stack buffer malloced by caller, if platform used this buffer, set stack_used to non-zero value,  otherwise set it to 0.
 * @param[in] stack_size @n The initial size of the stack, in bytes. see platform_get_thread_stack_size().
 * @param[out] stack_used @n if platform used stack buffer, set stack_used to 1, otherwise set it to 0.
 * @return
 @verbatim
 = 0: on success.
 = -1: error occur.
 @endverbatim
 * @see None.
 * @note None.
 */
int platform_thread_create(
                            _OUT_ void **thread,
                            _IN_ const char *name,
                            _IN_ void *(*start_routine)( void * ),
                            _IN_ void *arg,
                            _IN_ void *stack,
                            _IN_ uint32_t stack_size,
                            _OUT_ int *stack_used )
{
    if ( NULL != stack )
    {
        free( stack );
        *stack_used = 1;
    }
    return mico_rtos_create_thread( thread, MICO_APPLICATION_PRIORITY,
                                    name,
                                    (mico_thread_function_t) start_routine, stack_size,
                                    (uint32_t) arg );
}

/**
 * @brief exit the thread itself.
 *
 * @param[in] thread: itself thread handle.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_thread_exit( _IN_ void *thread )
{
    //alink_platform_log("!!thread exit %p!!", thread);

    if ( thread == NULL )
        mico_rtos_delete_thread( NULL );
    else
        mico_rtos_delete_thread( &thread );
}
/**
 * @brief sleep thread itself.
 *
 * @param[in] ms @n the time interval for which execution is to be suspended, in milliseconds.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_msleep( _IN_ uint32_t ms )
{
    mico_thread_msleep( ms );
}

/**
 * @brief Create a mutex.
 *
 * @return Mutex handle.
 * @see None.
 * @note None.
 */
void *platform_mutex_init( void )
{
    OSStatus err = kNoErr;
    mico_mutex_t mutex;

    err = mico_rtos_init_mutex( &mutex );
    //alink_platform_log("init mutex %p", mutex);
    require_noerr_action( err, exit, alink_platform_log("ERROR: Unable to init the mutex.") );
    return mutex;
    exit:
    return NULL;
}

/**
 * @brief Destroy the specified mutex object, it will free related resource.
 *
 * @param[in] mutex @n The specified mutex.
 * @return None.
 * @see None.
 * @note None.
 */

void platform_mutex_destroy( _IN_ void *mutex )
{
    OSStatus err = kNoErr;
    //alink_platform_log("deinit mutex %p", mutex);
    err = mico_rtos_deinit_mutex( &mutex );

    if ( err != 0 )
        alink_platform_log("ERROR: Unable to destroy the mutex.");
}

/**
 * @brief Waits until the specified mutex is in the signaled state.
 *
 * @param[in] mutex @n the specified mutex.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_mutex_lock( _IN_ void *mutex )
{
    OSStatus err = kNoErr;
    //alink_platform_log("lock mutex %p", mutex);
    err = mico_rtos_lock_mutex( &mutex );
    if ( err != 0 )
        alink_platform_log("ERROR: Unable to lock the mutex.");
}

/**
 * @brief Releases ownership of the specified mutex object..
 *
 * @param[in] mutex @n the specified mutex.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_mutex_unlock( _IN_ void *mutex )
{
    OSStatus err = kNoErr;
    //alink_platform_log("unlock mutex %p", mutex);
    err = mico_rtos_unlock_mutex( &mutex );
    if ( err != 0 )
        alink_platform_log("ERROR: Unable to unlock the mutex.");
}

/********************************* semaphore interface *********************************/

/** @defgroup group_platform_semaphore platform_semaphore
 *  @{
 */

/**
 * @brief Create a semaphore.
 *
 * @return semaphore handle.
 * @see None.
 * @note The recommended value of maximum count of the semaphore is 255.
 */
void *platform_semaphore_init( void )
{
    OSStatus err = kNoErr;
    mico_semaphore_t semaphore;

    err = mico_rtos_init_semaphore( &semaphore, 255 );
    //alink_platform_log("init sem %p", semaphore);
    require_noerr_action( err, exit, alink_platform_log("ERROR: Unable to init the semaphore.") );

    return semaphore;
    exit:
    return NULL;
}

/**
 * @brief Destroy the specified semaphore object, it will free related resource.
 *
 * @param[in] sem @n the specified sem.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_semaphore_destroy( _IN_ void *sem )
{
    OSStatus err = kNoErr;
    //alink_platform_log("deinit sem %p", sem);
    err = mico_rtos_deinit_semaphore( &sem );

    if ( err != 0 )
        alink_platform_log("ERROR: Unable to destroy the semaphore.");
}

/**
 * @brief Wait until the specified mutex is in the signaled state or the time-out interval elapses.
 *
 * @param[in] sem @n the specified semaphore.
 * @param[in] timeout_ms @n timeout interval in millisecond.
 If timeout_ms is PLATFORM_WAIT_INFINITE, the function will return only when the semaphore is signaled.
 * @return
 @verbatim
 =  0: The state of the specified object is signaled.
 =  -1: The time-out interval elapsed, and the object's state is nonsignaled.
 @endverbatim
 * @see None.
 * @note None.
 */
int platform_semaphore_wait( _IN_ void *sem, _IN_ uint32_t timeout_ms )
{
    OSStatus err = kNoErr;
    //alink_platform_log("wait sem %p", sem);
    err = mico_rtos_get_semaphore( &sem, timeout_ms );
    if ( err != 0 )
        err = -1;
    return err;
}
/**
 * @brief Increases the count of the specified semaphore object by 1.
 *
 * @param[in] sem @n the specified semaphore.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_semaphore_post( _IN_ void *sem )
{
    OSStatus err = kNoErr;

    //alink_platform_log("post sem %p", sem);
    err = mico_rtos_set_semaphore( &sem );
    if ( err != 0 )
        alink_platform_log("ERROR: Unable to post the semaphore.");
}

/** @} */ //end of platform_semaphore
/********************************** memory interface **********************************/

/** @defgroup group_platform_memory_manage platform_memory_manage
 *  @{
 */

/**
 * @brief Allocates a block of size bytes of memory, returning a pointer to the beginning of the block.
 *
 * @param[in] size @n specify block size in bytes.
 * @return A pointer to the beginning of the block.
 * @see None.
 * @note Block value is indeterminate.
 */
void *platform_malloc( _IN_ uint32_t size )
{
    return malloc( size );
}

/**
 * @brief Deallocate memory block
 *
 * @param[in] ptr @n Pointer to a memory block previously allocated with platform_malloc.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_free( _IN_ void *ptr )
{
    if ( ptr )
    {
        free( ptr );
    }
}

/**
 * @brief Create a udp server with the specified port.
 *
 * @param[in] port @n The specified udp sever listen port.
 * @return Server handle.
 @verbatim
 =  NULL: fail.
 != NULL: success.
 @endverbatim
 * @see None.
 * @note It is recommended to add handle value by 1, if 0(NULL) is a valid handle value in your platform.
 */
void *platform_udp_server_create( _IN_ uint16_t port )
{
    OSStatus err = kNoErr;
    int udp_fd = -1;

    struct sockaddr_in addr;

    udp_fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    if ( udp_fd == 0 )
    {
        udp_fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    }

    require_action( IsValidSocket( udp_fd ), exit,
                    alink_platform_log("ERROR: Unable to create the udp_server.") );

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons( port );
    err = bind( udp_fd, (struct sockaddr *) &addr, sizeof(addr) );
    alink_platform_log("err = %d ,addr.s_port = %d",err,addr.sin_port);
    require_noerr_action( err, exit, alink_platform_log("ERROR: Unable to bind the udp_server.") );
    return (void *) udp_fd;

    exit:
    if ( udp_fd < 0 )
        return NULL;
    close( udp_fd );
    return NULL;
}

/**
 * @brief Create a udp client.
 *
 * @param None
 * @return Client handle.
 @verbatim
 =  NULL: fail.
 != NULL: success.
 @endverbatim
 * @see None.
 * @note None.
 */
void *platform_udp_client_create( void )
{
    int fd = -1;

    fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    if ( fd == 0 )
    {
        fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    }

    require_action( IsValidSocket( fd ), exit,
                    alink_platform_log("ERROR: Unable to create the udp_client.") );
    alink_platform_log("Create UDP_Client fd:%d",fd);
    return (void *) fd;

    exit:
    return NULL;
}
/**
 * @brief Add this host to the specified udp multicast group.
 *
 * @param[in] netaddr @n Specify multicast address.
 * @return Multicast handle.
 @verbatim
 =  NULL: fail.
 != NULL: success.
 @endverbatim
 * @see None.
 * @note None.
 *
 */
void *platform_udp_multicast_server_create( pplatform_netaddr_t netaddr )
{
    OSStatus err = kNoErr;
    long fd = -1;
    struct sockaddr_in addr;
    ip_mreq opt;

    fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );

    if ( fd == 0 )
    {
        fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
    }

    require_action( IsValidSocket( fd ), exit, err = kNoResourcesErr );

    opt.imr_interface.s_addr = htonl( INADDR_ANY );
    opt.imr_multiaddr.s_addr = inet_addr( netaddr->host );
    err = setsockopt( fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &opt, sizeof(opt) );
    require_noerr_action( err, exit, alink_platform_log("ERROR: Unable to setsockopt.") );
    addr.sin_family = AF_INET;
    addr.sin_port = htons( netaddr->port );
    addr.sin_addr.s_addr = INADDR_ANY;
    err = bind( fd, (struct sockaddr *) &addr, sizeof(addr) );
    require_noerr_action( err, exit,
                          alink_platform_log("ERROR: Unable to create the multicast_udp_server.") );
    return (void *) fd;

    exit:
    if ( fd < 0 )
        return NULL;
    close( fd );
    return NULL;
}

/**
 * @brief Closes an existing udp connection.
 *
 * @param[in] handle @n the specified connection.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_udp_close( void *handle )
{
    close( (int) handle );
}

/**
 * @brief Sends data to a specific destination.
 *
 * @param[in] handle @n A descriptor identifying a connection.
 * @param[in] buffer @n A pointer to a buffer containing the data to be transmitted.
 * @param[in] length @n The length, in bytes, of the data pointed to by the buffer parameter.
 * @param[in] netaddr @n A pointer to a netaddr structure that contains the address of the target.
 *
 * @return
 @verbatim
 > 0: the total number of bytes sent, which can be less than the number indicated by length.
 = -1: error occur.
 @endverbatim
 * @see None.
 * @note blocking API.
 */
int platform_udp_sendto(
                         _IN_ void *handle,
                         _IN_ const char *buffer,
                         _IN_ uint32_t length,
                         _IN_ pplatform_netaddr_t netaddr )
{
    int len = -1;
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr( netaddr->host );
    addr.sin_port = htons( netaddr->port );
    alink_platform_log("udp send to  port :%d   IP: %s .",netaddr->port,netaddr->host);
    len = sendto( (int) handle, buffer, length, 0, (struct sockaddr *) &addr, sizeof(addr) );
    alink_platform_log("udp send return len: %d",len);

    return (len) > 0 ? len : -1;
}
/**
 * @brief Receives data from a udp connection.
 *
 * @param[in] handle @n A descriptor identifying a connection.
 * @param[out] buffer @n A pointer to a buffer to receive incoming data.
 * @param[in] length @n The length, in bytes, of the data pointed to by the buffer parameter.
 * @param[out] netaddr @n A pointer to a netaddr structure that contains the address of the source.
 * @return
 @verbatim
 >  0: The total number of bytes received, which can be less than the number indicated by length.
 <  0: Error occur.
 @endverbatim
 *
 * @see None.
 * @note blocking API.
 */
int platform_udp_recvfrom(
                           _IN_ void *handle,
                           _OUT_ char *buffer,
                           _IN_ uint32_t length,
                           _OUT_OPT_ pplatform_netaddr_t netaddr )

{
    int len;
    socklen_t addrLen = sizeof(struct sockaddr_in);
    struct sockaddr_in addr;

    len = recvfrom( (int) handle, buffer, length, 0, (struct sockaddr *) &addr, &addrLen );

    if ( len > 0 )
    {
        if ( NULL != netaddr )
        {
            netaddr->port = ntohs( addr.sin_port );

            if ( NULL != netaddr->host )
            {
                strcpy( netaddr->host, inet_ntoa( addr.sin_addr ) );
                alink_platform_log("recv msg from %s:%d",netaddr->host,netaddr->port);
            }
        }

        return len;
    }
    
    return -1;
}

/**
 * @brief Create a tcp server with the specified port.
 *
 * @param[in] port @n The specified tcp sever listen port.
 * @return Server handle.
 @verbatim
 =  NULL: fail.
 != NULL: success.
 @endverbatim
 * @see None.
 * @note None.
 */
void *platform_tcp_server_create( _IN_ uint16_t port )
{
    OSStatus err = kNoErr;

    int localConfiglistener_fd = -1;
    struct sockaddr_in addr;

    localConfiglistener_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

    if ( localConfiglistener_fd == 0 )
    {
        localConfiglistener_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    }

    require_action( IsValidSocket( localConfiglistener_fd ), exit,
                    alink_platform_log("ERROR: Unable to create the tcp_server.") );
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons( port );
    err = bind( localConfiglistener_fd, (struct sockaddr *) &addr, sizeof(addr) );
    require_noerr_action( err, exit, alink_platform_log("ERROR: Unable to bind the tcp_server.") );

    err = listen( localConfiglistener_fd, 0 );
    require_noerr_action( err, exit,
                          alink_platform_log("ERROR: Unable to listen the tcp_server.") );
    alink_platform_log("TCP Server established at port: %d, fd: %d", addr.sin_port, localConfiglistener_fd);
    return (void *) localConfiglistener_fd;
    exit:
    if ( localConfiglistener_fd < 0 )
        return NULL;
    close( localConfiglistener_fd );
    return NULL;
}

/**
 * @brief Permits an incoming connection attempt on a tcp server.
 *
 * @param[in] server @n The specified tcp sever.
 * @return Connection handle.
 * @see None.
 * @note None.
 */
void *platform_tcp_server_accept( _IN_ void *server )
{
    int new_client;
    struct sockaddr_in addr;
    uint32_t sockaddr_t_size = sizeof(addr);
    sockaddr_t_size = sizeof(struct sockaddr);
    new_client = accept( (int) server, (struct sockaddr *) &addr, &sockaddr_t_size );
    require_action( IsValidSocket( new_client ), exit,
                    alink_platform_log("ERROR: tcp_server Unable to accept .") );
    return (void *) new_client;
    exit:
    return NULL;
}

/**
 * @brief Establish a connection.
 *
 * @param[in] netaddr @n The destination address.
 * @return Connection handle
 @verbatim
 =  NULL: fail.
 != NULL: success.
 @endverbatim
 * @see None.
 * @note None.
 */
void *platform_tcp_client_connect( _IN_ pplatform_netaddr_t netaddr )
{
    OSStatus err = kNoErr;
    int remoteTcpClient_fd = -1;
    struct sockaddr_in addr;
    int retryCount = 0;
    int opt = 0;
    struct hostent *host = NULL;

    alink_platform_log("host name: %s", netaddr->host);
    host = gethostbyname( netaddr->host );
    require_noerr_action( host == NULL, exit,
                          alink_platform_log("ERROR: Unable to resolute the host address.") );

    tcp_reconnect:

    remoteTcpClient_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );

    if ( remoteTcpClient_fd == 0 )
    {
        remoteTcpClient_fd = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    }

    require_action( IsValidSocket( remoteTcpClient_fd ), exit,
                    alink_platform_log("ERROR: Unable to create the tcp_client.") );
#if 1
    // set keepalive
    opt = 1;
    setsockopt( remoteTcpClient_fd, SOL_SOCKET, SO_KEEPALIVE, (void *) &opt, sizeof(opt) ); // ����socket��Keepalive����
    opt = 10;
    setsockopt( remoteTcpClient_fd, IPPROTO_TCP, TCP_KEEPIDLE, (void *) &opt, sizeof(opt) ); // TCP IDLE 10���Ժ�ʼ���͵�һ��Keepalive��
    opt = 5;
    setsockopt( remoteTcpClient_fd, IPPROTO_TCP, TCP_KEEPINTVL, (void *) &opt, sizeof(opt) ); // TCP�����Keepalive�ļ��ʱ����10�롣
    opt = 3;
    setsockopt( remoteTcpClient_fd, IPPROTO_TCP, TCP_KEEPCNT, (void *) &opt, sizeof(opt) ); // Keepalive ����Ϊ3��
#endif
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = *(uint32_t *) (*host->h_addr_list);
    addr.sin_port = htons( netaddr->port );
    alink_platform_log("ip %s port %d fd %d\n", inet_ntoa(addr.sin_addr), addr.sin_port, remoteTcpClient_fd);

    err = connect( remoteTcpClient_fd, (struct sockaddr *) &addr, sizeof(addr) );
    
    if ( 0 != err )
    {
        close( remoteTcpClient_fd );
        remoteTcpClient_fd = -1;

        mico_thread_msleep( 1000 );
        alink_platform_log("tcp reconnect.");
        if ( retryCount < 5 )
        {
            retryCount++;
            goto tcp_reconnect;
        } else
        {
            goto exit;
        }
    }

    require_noerr_action( err, exit,
                          alink_platform_log("ERROR: Unable to connect the host address.") );

    alink_platform_log("tcp fd = %d", remoteTcpClient_fd) ;

    return (void *) remoteTcpClient_fd;

    exit:
    if ( remoteTcpClient_fd < 0 )
        return PLATFORM_INVALID_FD;
    close( remoteTcpClient_fd );
    remoteTcpClient_fd = -1;
    return PLATFORM_INVALID_FD;
}

/**
 * @brief Sends data on a connection.
 *
 * @param[in] handle @n A descriptor identifying a connection.
 * @param[in] buffer @n A pointer to a buffer containing the data to be transmitted.
 * @param[in] length @n The length, in bytes, of the data pointed to by the buffer parameter.
 * @return
 @verbatim
 >  0: The total number of bytes sent, which can be less than the number indicated by length.
 <  0: Error occur.
 @endverbatim
 * @see None.
 * @note Blocking API.
 */
int platform_tcp_send( _IN_ void *handle, _IN_ const char *buffer, _IN_ uint32_t length )
{
    int len;
    len = send( (int) handle, (void *) buffer, length, 0 );
    return (len) > 0 ? len : -1;
}

/**
 * @brief Receives data from a tcp connection.
 *
 * @param[in] handle @n A descriptor identifying a connection.
 * @param[out] buffer @n A pointer to a buffer to receive incoming data.
 * @param[in] length @n The length, in bytes, of the data pointed to by the buffer parameter.
 * @return
 @verbatim
 >  0: The total number of bytes received, which can be less than the number indicated by length.
 <  0: Error occur.
 @endverbatim
 *
 * @see None.
 * @note Blocking API.
 */
int platform_tcp_recv( _IN_ void *handle, _OUT_ char *buffer, _IN_ uint32_t length )
{
    int len;
    len = recv( (int) handle, (void *) buffer, length, 0 );
    return len > 0 ? len : -1;
}

/**
 * @brief Closes an existing tcp connection.
 *
 * @param[in] handle @n the specified connection.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_tcp_close( _IN_ void *handle )
{
    close( (int) handle );
}

/**
 * @brief Determines the status of one or more connection, waiting if necessary, to perform synchronous I/O.
 *
 * @param[in,out] handle_read @n
 @verbatim
 [in]: An optional pointer to a set of connection to be checked for readability.
 handle_read[n] > 0, care the connection, and the value is handle of the careful connection.
 handle_read[n] = NULL, uncare.
 [out]: handle_read[n] = NULL, the connection unreadable; != NULL, the connection readable.
 @endverbatim
 * @param[in,out] handle_write: @n
 @verbatim
 [in]: An optional pointer to a set of connection to be checked for writability.
 handle_write[n] > 0, care the connection, and the value is handle of the careful connection.
 handle_write[n] = NULL, uncare.
 [out]: handle_write[n] = NULL, the connection unwritable; != NULL, the connection wirteable.
 @endverbatim
 * @param[in] timeout_ms: @n Timeout interval in millisecond.
 * @return
 @verbatim
 =  0: The timeout interval elapsed.
 >  0: The total number of connection handles that are ready.
 <  0: A connection error occur.
 @endverbatim
 * @see None.
 * @note None.
 */
int platform_select(
                     _INOUT_OPT_ void *read_fds[PLATFORM_SOCKET_MAXNUMS],
                     _INOUT_OPT_ void *write_fds[PLATFORM_SOCKET_MAXNUMS],
                     _IN_ int timeout_ms )
{
    OSStatus err = kNoErr;
    int i;
    struct timeval t, *ptime = NULL;
    fd_set pfd_read_set, pfd_write_set;

    if ( PLATFORM_WAIT_INFINITE != timeout_ms )
    {
        t.tv_sec = timeout_ms / 1000;
        t.tv_usec = (timeout_ms % 1000) * 1000;
        ptime = &t;
    }

    FD_ZERO( &pfd_read_set );
    if ( NULL != read_fds )
    {
        for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
        {
            if ( PLATFORM_INVALID_FD != read_fds[i] )
            {
                FD_SET( (int )read_fds[i], &pfd_read_set );
            }
        }
    }

    FD_ZERO( &pfd_write_set );
    if ( NULL != write_fds )
    {
        for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
        {
            if ( PLATFORM_INVALID_FD != write_fds[i] )
            {
                FD_SET( (int )write_fds[i], &pfd_write_set );

            }
        }
    }
    err = select( PLATFORM_SOCKET_MAXNUMS, &pfd_read_set, &pfd_write_set, NULL, ptime );
    if ( err > 0 )
    {
        if ( NULL != read_fds )
        {
            for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
            {
                if ( PLATFORM_INVALID_FD != read_fds[i] )
                {
                    if ( !FD_ISSET( (int )read_fds[i], &pfd_read_set ) )
                    {
                        read_fds[i] = PLATFORM_INVALID_FD;
                    }
                }
            }
        }

        if ( NULL != write_fds )
        {
            for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
            {
                if ( PLATFORM_INVALID_FD != write_fds[i] )
                {
                    if ( !FD_ISSET( (int )write_fds[i], &pfd_write_set ) )
                    {
                        write_fds[i] = PLATFORM_INVALID_FD;
                    }
                }
            }
        }
    } else
    {
        for ( i = 0; i < PLATFORM_SOCKET_MAXNUMS; ++i )
        {
            if ( NULL != read_fds )
            {
                read_fds[i] = PLATFORM_INVALID_FD;
            }

            if ( NULL != write_fds )
            {
                write_fds[i] = PLATFORM_INVALID_FD;
            }
        }
    }
    return (err >= 0) ? err : -1;
}

/** @} */ //end of platform_network
/************************************ SSL interface ************************************/
static void *mutex_ssl = NULL;
static void *ali_ssl = NULL;
/** @defgroup group_platform_ssl platform_ssl
 *  @{
 */

/**
 * @brief Establish a ssl connection.
 *
 * @param[in] tcp_fd @n The network connection handle.
 * @param[in] server_cert @n Specify the sever certificate.
 * @param[in] server_cert_len @n Length of sever certificate, in bytes.
 * @return SSL handle.
 * @see None.
 * @note None.
 */
void *platform_ssl_connect( _IN_ void *tcp_fd, _IN_ const char *server_cert,
                            _IN_ int server_cert_len )
{
    void *remoteTcpSSL = NULL;
    int errno;

    if ( tcp_fd < 0 )
    {
        return NULL;
    }
    if ( mutex_ssl == NULL )
        mutex_ssl = platform_mutex_init( );
    remoteTcpSSL = ssl_connect( (int) tcp_fd, server_cert_len, (char*) server_cert, &errno );

    ali_ssl = remoteTcpSSL;
    alink_platform_log("fd : %d  ,  SSL : %p ,  err :  %d", (int)tcp_fd,remoteTcpSSL,errno);
    return remoteTcpSSL;
}

/**
 * @brief Sends data on a ssl connection.
 *
 * @param[in] ssl @n A descriptor identifying a ssl connection.
 * @param[in] buffer @n A pointer to a buffer containing the data to be transmitted.
 * @param[in] length @n The length, in bytes, of the data pointed to by the buffer parameter.
 * @return
 @verbatim
 >  0: The total number of bytes sent, which can be less than the number indicated by length.
 <  0: Error occur.
 @endverbatim
 * @see None.
 * @note Blocking API.
 */

int platform_ssl_send( _IN_ void *ssl, _IN_ const char *buffer, _IN_ int length )
{
    int len = -1;

    if ( ssl != ali_ssl )
    {
        alink_platform_log("wrong ssl: input %p, need %p\r\n", ssl, ali_ssl);
        return -1;
    }

    alink_platform_log("ssl(%p) send %p(%d)...", ssl, buffer, length);
    platform_mutex_lock( mutex_ssl );
    len = ssl_send( ssl, (char *) buffer, length );
    platform_mutex_unlock( mutex_ssl );
    alink_platform_log("ssl send %p(%d) done", buffer, len);
    
    return (len > 0) ? len : -1;
}

/**
 * @brief Receives data from a ssl connection.
 *
 * @param[in] ssl @n A descriptor identifying a ssl connection.
 * @param[out] buffer @n A pointer to a buffer to receive incoming data.
 * @param[in] length @n The length, in bytes, of the data pointed to by the buffer parameter.
 * @return
 @verbatim
 >  0: The total number of bytes received, which can be less than the number indicated by length.
 <  0: Error occur.
 @endverbatim
 *
 * @see None.
 * @note blocking API.
 */

int platform_ssl_recv( _IN_ void *ssl, _IN_ char *buffer, _IN_ int length )
{
    int len = -1;

    if ( ssl != ali_ssl )
    {
        alink_platform_log("wrong ssl: input %p, need %p\r\n", ssl, ali_ssl);
        return -1;
    }

    alink_platform_log("ssl(%p) recv %d...", ssl, length);
    platform_mutex_lock( mutex_ssl );
    len = ssl_recv( ssl, buffer, length );
    platform_mutex_unlock( mutex_ssl );
    alink_platform_log("ssl recv %d done", len);
    
    return (len > 0) ? len : -1;
}

/**
 * @brief Closes an existing ssl connection.
 *
 * @param[in] ssl: @n the specified connection.
 * @return None.
 * @see None.
 * @note None.
 */
int platform_ssl_close( _IN_ void *ssl )
{
    int ret;
    alink_platform_log("ssl close %p", ssl);
    ali_ssl = NULL;
    platform_mutex_lock( mutex_ssl );
    ret = ssl_close( ssl );
    platform_mutex_unlock( mutex_ssl );
    return ret;
}

/********************************** system interface **********************************/

/** @defgroup group_platform_system platform_system
 *  @{
 */

/**
 * @brief check system network is ready(get ip address) or not.
 *
 * @param None.
 * @return 0, net is not ready; 1, net is ready.
 * @see None.
 * @note None.
 */
int platform_sys_net_is_ready( void )
{
    OSStatus err = kNoErr;
    LinkStatusTypeDef wifi_link;
    err = micoWlanGetLinkStatus( &wifi_link );
    require_noerr( err, exit );
    if ( wifi_link.is_connected == true )
        return 1;
    else
        return 0;
    exit:
    return 0;
}

/**
 * @brief reboot system immediately.
 *
 * @param None.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_sys_reboot( void )
{
    mico_system_power_perform( mico_system_context_get( ), eState_Software_Reset );
}

/**
 * @brief Retrieves the number of milliseconds that have elapsed since the system was boot.
 *
 * @param None.
 * @return the number of milliseconds.
 * @see None.
 * @note None.
 */
uint32_t platform_get_time_ms( void )
{
    return mico_rtos_get_time( );
}

uint64_t platform_get_utc_time( uint64_t * p_utc )
{

    return 0;
}

int platform_thread_get_stack_size( _IN_ const char *thread_name )
{
    if ( 0 == strcmp( thread_name, "wsf_receive_worker" ) )
    {
        alink_platform_log("get wsf receive worker\n");
        return 0x2500;
    }
    else if ( 0 == strcmp( thread_name, "wsf_send_worker" ) )
    {
        alink_platform_log("get wsf send worker\n");
        return 0x800;
    }
    else if ( 0 == strcmp( thread_name, "wsf_callback_worker" ) )
    {
        alink_platform_log("get wsf callback worker\n");
        return 0x800;
    }
    else if ( 0 == strcmp( thread_name, "fota_thread" ) )
    {
        alink_platform_log("get fota thread\n");
        return 0x800;
    }
    else if ( 0 == strcmp( thread_name, "cota_thread" ) )
    {
        alink_platform_log("get cota thread\n");
        return 0x400;
    }
    else if ( 0 == strcmp( thread_name, "alcs_thread" ) )
    {
        alink_platform_log("get alcs thread\n");
        return 0x2000;
    } else if ( 0 == strcmp( thread_name, "work queue" ) )
    {
        alink_platform_log("get work queue thread\n");
        return 0x800;
    } else if ( 0 == strcmp( thread_name, "asr_websocket_thread" ) )
    {
        return 0x2000; //0x4000
    } else if ( 0 == strcmp( thread_name, "alink_main_thread" ) )
    {
        alink_platform_log( "get alink_main_thread\n" );
        return 0xc00;
    }
    else if ( 0 == strcmp( thread_name, "wsf_worker_thread" ) )
    {
        alink_platform_log( "get wsf_worker_thread\n" );
        return 0x2100;
    }
    else if ( 0 == strcmp( thread_name, "firmware_upgrade_pthread" ) )
    {
        alink_platform_log( "get firmware_upgrade_pthread\n" );
        return 0xc00;
    }
    else if ( 0 == strcmp( thread_name, "send_worker" ) )
    {
        alink_platform_log( "get send_worker\n" );
        return 0x800;
    }
    else if ( 0 == strcmp( thread_name, "callback_thread" ) )
    {
        alink_platform_log( "get callback_thread\n" );
        return 0x800;
    }
    else
    {
        alink_platform_log("thread name=%s\n", thread_name);
        while ( 1 )
        {
        };
    }

}

int platform_get_free_memory_size( void )
{
    return MicoGetMemoryInfo( )->free_memory;
}

/** @} */ //end of platform_system
/***************************** firmware upgrade interface *****************************/

/** @defgroup group_platform_firmware_upgrade platform_firmware_upgrade
 *  @{
 */
static uint32_t offset = 0;
/**
 * @brief initialize a firmware upgrade.
 *
 * @param None
 * @return None.
 * @see None.
 * @note None.
 */
void platform_flash_program_start( void )
{
    alink_platform_log("platform flash program start");
    offset = 0;
}

void platform_firmware_upgrade_start( void )
{
    platform_flash_program_start( );
}

/**
 * @brief save firmware upgrade data to flash.
 *
 * @param[in] buffer: @n A pointer to a buffer to save data.
 * @param[in] length: @n The length, in bytes, of the data pointed to by the buffer parameter.
 * @return 0, Save success; -1, Save failure.
 * @see None.
 * @note None.
 */

int platform_flash_program_write_block( _IN_ char *buffer, _IN_ uint32_t length )
{
    OSStatus err = kNoErr;

    alink_platform_log("firmware write len: %ld, offset: %ld", length, offset);

    if ( offset == 0 )
    {
        err = MicoFlashErase( MICO_PARTITION_OTA_TEMP, 0,
                              MicoFlashGetInfo( MICO_PARTITION_OTA_TEMP )->partition_length );
        require_noerr( err, exit );
    }
    err = MicoFlashWrite( MICO_PARTITION_OTA_TEMP, &offset, (uint8_t *) buffer, length );
    require_noerr( err, exit );

    return kNoErr;
    exit:
    return kGeneralErr;
}

int platform_firmware_upgrade_write( _IN_ char *buffer, _IN_ uint32_t length )
{
    return platform_flash_program_write_block( buffer, length );
}

/**
 * @brief indicate firmware upgrade data complete, and trigger data integrity checking,
 and then reboot the system.
 *
 * @param None.
 * @return 0: Success; -1: Failure.
 * @see None.
 * @note None.
 */
int platform_flash_program_stop( void )
{
    uint32_t filelen, flashaddr, len = 0, left;
    uint8_t md5_recv[16];
    uint8_t md5_calc[16];
    uint16_t crc = 0;
    CRC16_Context ctx_crc;
    md5_context ctx;
    uint8_t *tmpbuf;

#define TMP_BUF_LEN 1024

    tmpbuf = (uint8_t*) malloc( TMP_BUF_LEN );
    require_noerr_action( tmpbuf == NULL, exit,
                          alink_platform_log("ERROR: Not enough memory") );

    filelen = offset - 16;
    flashaddr = filelen;
    MicoFlashRead( MICO_PARTITION_OTA_TEMP, &flashaddr, (uint8_t *) md5_recv, 16 );
    CRC16_Init( &ctx_crc );
    InitMd5( &ctx );
    flashaddr = 0;
    left = filelen;

    while ( left > 0 )
    {
        if ( left > TMP_BUF_LEN )
        {
            len = TMP_BUF_LEN;
        } else
        {
            len = left;
        }
        left -= len;
        MicoFlashRead( MICO_PARTITION_OTA_TEMP, &flashaddr, (uint8_t *) tmpbuf, len );
        Md5Update( &ctx, (uint8_t *) tmpbuf, len );
        CRC16_Update( &ctx_crc, tmpbuf, len );
    }

    Md5Final( &ctx, md5_calc );
    CRC16_Final( &ctx_crc, &crc );

    if ( memcmp( md5_calc, md5_recv, 16 ) != 0 )
    {
        alink_platform_log("RX:   %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            md5_recv[0], md5_recv[1], md5_recv[2], md5_recv[3],
            md5_recv[4], md5_recv[5], md5_recv[6], md5_recv[7],
            md5_recv[8], md5_recv[9], md5_recv[10], md5_recv[11],
            md5_recv[12], md5_recv[13], md5_recv[14], md5_recv[15]);
        alink_platform_log("Need: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x",
            md5_calc[0], md5_calc[1], md5_calc[2], md5_calc[3],
            md5_calc[4], md5_calc[5], md5_calc[6], md5_calc[7],
            md5_calc[8], md5_calc[9], md5_calc[10], md5_calc[11],
            md5_calc[12], md5_calc[13], md5_calc[14], md5_calc[15]);
        alink_platform_log("user crc check fail\r\n");
        goto exit;
    }

    alink_platform_log("OTA bin md5 check success");

    mico_ota_switch_to_new_fw( filelen, crc );

    alink_platform_log("Rebooting...");

    return kNoErr;
    exit:
    return kGeneralErr;
}

int platform_firmware_upgrade_finish( void )
{
    return platform_flash_program_stop( );
}

const char *platform_get_storage_directory( void )
{
    //storage(flash) path
    return "/tmp/";
}

/************************************ io interface ************************************/

/** @defgroup group_platform_io platform_io
 *  @{
 */

/**
 * @brief Writes formatted data to stream.
 *
 * @param[in] fmt: @n String that contains the text to be written, it can optionally contain embedded format specifiers
 that specifies how subsequent arguments are converted for output.
 * @param[in] ...: @n the variable argument list, for formatted and inserted in the resulting string replacing their respective specifiers.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_printf( _IN_ const char *fmt, ... )
{
#if 1
    extern mico_mutex_t stdio_tx_mutex;
    static char zc_log_buf[512];
    va_list args;
    int len;
#ifdef UART_LOG_FAST
    if ( uart_inited == 0 )
    {
        mico_uart_config_t uart_config;

        uart_inited = 1;
        uart_config.baud_rate = 921600;
        uart_config.data_width = DATA_WIDTH_8BIT;
        uart_config.parity = NO_PARITY;
        uart_config.stop_bits = STOP_BITS_1;
        uart_config.flow_control = FLOW_CONTROL_DISABLED;
        uart_config.flags = UART_WAKEUP_DISABLE;

        ring_buffer_init( (ring_buffer_t *) &rx_buffer, (uint8_t *) rx_data, UART_BUFFER_LENGTH );
        MicoUartInitialize( UART_FOR_APP, &uart_config, (ring_buffer_t *) &rx_buffer );
    }
#endif

    mico_rtos_lock_mutex( &stdio_tx_mutex );
    va_start( args, fmt );
    len = vsnprintf( zc_log_buf, sizeof(zc_log_buf), fmt, args );
    va_end( args );

    if ( len > 511 )
    {
        len = 511;
    }
#ifdef UART_LOG_FAST
    MicoUartSend( UART_FOR_APP, zc_log_buf, len );
#else
    printf("%s", zc_log_buf);
#endif
    mico_rtos_unlock_mutex( &stdio_tx_mutex );
#endif
}

/** @} */ //end of group_io
/********************************** config interface **********************************/

/** @defgroup group_platform_config platform_config
 *  @{
 */

/**
 * @brief Read configure data from the start of configure zone.
 *
 * @param[in] buffer @n A pointer to a buffer to receive incoming data.
 * @param[in] length @n Specify read length, in bytes.
 * @return
 @verbatim
 =  0, read success.
 =  -1, read failure.
 @endverbatim
 * @see None.
 * @note None.
 */
int platform_config_read( char *buffer, int length )
{
    application_config_t *application_config = mico_system_context_get_user_data(
        mico_system_context_get( ) );

    memcpy( buffer, application_config->alink_config.alink_config_data, length );
    return 0;
}

/**
 * @brief Write configure data from the start of configure zone.
 *
 * @param[in] buffer @n A pointer to a buffer to receive incoming data.
 * @param[in] length @n Specify write length, in bytes.
 * @return
 @verbatim
 =  0, write success.
 =  -1, write failure.
 @endverbatim
 * @see None.
 * @note None.
 */
int platform_config_write( const char *buffer, int length )
{
    application_config_t *application_config = mico_system_context_get_user_data(
        mico_system_context_get( ) );

    memcpy( application_config->alink_config.alink_config_data, buffer, length );
    mico_system_context_update( mico_system_context_get( ) );
    return 0;
}

/** @} */ //end of platform_config
/******************************** wifi module interface ********************************/

/** @defgroup group_platform_wifi_module platform_wifi_module
 *  @{
 */

/**
 * @brief Get model of the wifi module.
 *
 * @param[in] model_str @n Buffer for using to store model string.
 * @return  A pointer to the start address of model_str.
 * @see None.
 * @note None.
 */

char *platform_get_module_name( char name_str[PLATFORM_MODULE_NAME_LEN] )
{
    memcpy( name_str, MODEL, PLATFORM_MODULE_NAME_LEN );
    return name_str;
}

/**
 * @brief Get WIFI received signal strength indication(rssi).
 *
 * @param None.
 * @return The level number, in dBm.
 * @see None.
 * @note None.
 */
int platform_wifi_get_rssi_dbm( void )
{
    LinkStatusTypeDef ap_state;
    micoWlanGetLinkStatus( &ap_state );
    return ap_state.wifi_strength;
}

/**
 * @brief Get WIFI MAC string with format like: xx:xx:xx:xx:xx:xx.
 *
 * @param[out] mac_str @n Buffer for using to store wifi MAC string.
 * @return A pointer to the start address of mac_str.
 * @see None.
 * @note None.
 */
char *platform_wifi_get_mac( char mac_str[PLATFORM_MAC_LEN] )
{
    unsigned char mac[6];

    mico_wlan_get_mac_address( mac );
    sprintf( mac_str, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0],
             mac[1], mac[2], mac[3], mac[4], mac[5] );

    alink_platform_log("MAC: mac_str=%s.", mac_str);

    return mac_str;
}

/**
 * @brief Get WIFI IP string with format like: xx:xx:xx:xx:xx:xx,
 and return IP with binary form, in network byte order.
 *
 * @param[out] ip_str @n Buffer for using to store IP string, in numbers-and-dots notation form.
 * @return IP with binary form, in network byte order.
 * @see None.
 * @note None.
 */
uint32_t platform_wifi_get_ip( char ip_str[PLATFORM_IP_LEN] )
{
    IPStatusTypedef para;
    micoWlanGetIPStatus( &para, Station );
    memcpy( ip_str, para.ip, PLATFORM_IP_LEN );
    return inet_addr( ip_str );
}

/**
 * @brief Get unique chip id string.
 *
 * @param[out] cid_str @n Buffer for using to store chip id string.
 * @return A pointer to the start address of cid_str.
 * @see None.
 * @note None.
 */
char *platform_get_chipid( char cid_str[PLATFORM_CID_LEN] )
{
    return strncpy( cid_str, "2D0044000F47333139373038", PLATFORM_CID_LEN );
}

/**
 * @brief Get the os version of wifi module firmware.
 *
 * @param[in] version_str @n Buffer for using to store version string.
 * @return  A pointer to the start address of version_str.
 * @see None.
 * @note None.
 */
char *platform_get_os_version( char version_str[PLATFORM_OS_VERSION_LEN] )
{
    strcpy( version_str, MicoGetVer( ) );
    return version_str;
}

/** @} */ //end of platform_wifi_module
/************************* awss(alink wireless setup service) interface ***************************/

/** @defgroup group_platform_awss platform_awss(alink wireless setup service)
 *  @{
 */

/**
 * @brief Get timeout interval, in millisecond, of per awss.
 *
 * @param None.
 * @return The timeout interval.
 * @see None.
 * @note The recommended value is 60,000ms.
 */
int platform_awss_get_timeout_interval_ms( void )
{
    return 60000;
}

/**
 * @brief Get time length, in millisecond, of per channel scan.
 *
 * @param None.
 * @return The timeout interval.
 * @see None.
 * @note None. The recommended value is between 200ms and 400ms.
 */
int platform_awss_get_channelscan_interval_ms( void )
{
    return 300;
}

static platform_awss_recv_80211_frame_cb_t awss_cb;
static void monitor_cb( uint8_t *data, int len )
{
    (*awss_cb)( (char *) data, len, 0, 0 ); //AWS_LINK_TYPE_NONE
}

/**
 * @brief Set wifi running at monitor mode,
 and register a callback function which will be called when wifi receive a frame.
 *
 * @param[in] cb @n A function pointer, called back when wifi receive a frame.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_awss_open_monitor( platform_awss_recv_80211_frame_cb_t cb )
{
    awss_cb = cb;
    mico_wlan_register_monitor_cb( (monitor_cb_t) monitor_cb );
    mico_wlan_start_monitor( );
    mico_wlan_set_channel( 6 );
}

/**
 * @brief Close wifi monitor mode, and set running at station mode.
 *
 * @param None.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_awss_close_monitor( void )
{
    mico_wlan_stop_monitor( );
}

/**
 * @brief Switch to specific wifi channel.
 *
 * @param[in] primary_channel @n Primary channel.
 * @param[in] secondary_channel @n Auxiliary channel.
 * @param[in] bssid @n A pointer to wifi BSSID.
 * @return None.
 * @see None.
 * @note None.
 */
void platform_awss_switch_channel(
                                   char primary_channel,
                                   char secondary_channel,
                                   char bssid[ETH_ALEN] )
{
    mico_wlan_set_channel( primary_channel );
}

static mico_semaphore_t net_semp = NULL;
static void awsNotify_WifiStatusHandler( WiFiEvent event, mico_Context_t * const inContext )
{
    require( inContext, exit );

    switch ( event )
    {
        case NOTIFY_STATION_UP:
            inContext->micoSystemConfig.configured = allConfigured;
            mico_system_context_update( inContext ); //Update Flash content
            if ( net_semp != NULL )
            {
                mico_rtos_set_semaphore( &net_semp );
            }
            break;
        default:
            break;
    }
    exit:
    return;
}

static void awsNotify_DHCPCompletedlHandler( IPStatusTypedef *pnet,
                                             mico_Context_t * const inContext )
{
    require( inContext, exit );
    if ( net_semp != NULL )
    {
        mico_rtos_set_semaphore( &net_semp );
    }
    alink_platform_log("ip %s", pnet->ip);
    alink_platform_log("mask %s", pnet->mask);
    alink_platform_log("gate %s", pnet->gate);
    alink_platform_log("dns %s", pnet->gate);
    exit:
    return;
}
/**
 * @brief Wifi AP connect function
 *
 * @param[in] connection_timeout_ms @n AP connection timeout in ms or PLATFORM_WAIT_INFINITE
 * @param[in] ssid @n AP ssid
 * @param[in] passwd @n AP passwd
 * @param[in] auth @n optional(AWSS_AUTH_TYPE_INVALID), AP auth info
 * @param[in] encry @n optional(AWSS_ENC_TYPE_INVALID), AP encry info
 * @param[in] bssid @n optional(NULL or zero mac address), AP bssid info
 * @param[in] channel @n optional, AP channel info
 * @return
 @verbatim
 = 0: connect AP & DHCP success
 = -1: connect AP or DHCP fail/timeout
 @endverbatim
 * @see None.
 * @note None.
 */
int platform_awss_connect_ap(
                              _IN_ uint32_t connection_timeout_ms,
                              _IN_ char ssid[PLATFORM_MAX_SSID_LEN],
                              _IN_ char passwd[PLATFORM_MAX_PASSWD_LEN],
                              _IN_OPT_ enum AWSS_AUTH_TYPE auth,
                              _IN_OPT_ enum AWSS_ENC_TYPE encry,
                              _IN_OPT_ uint8_t bssid[ETH_ALEN],
                              _IN_OPT_ uint8_t channel )
{
    OSStatus err;
    mico_Context_t *mico_context = mico_system_context_get( );

    mico_system_delegate_config_recv_ssid( ssid, passwd );
    alink_platform_log("ssid:%s, passwd:%s", ssid, passwd);

    err = mico_rtos_init_semaphore( &net_semp, 2 );
    require_noerr( err, exit );

    err = mico_system_notify_register( mico_notify_WIFI_STATUS_CHANGED,
                                       (void *) awsNotify_WifiStatusHandler,
                                       mico_context );
    require_noerr( err, exit );

    err = mico_system_notify_register( mico_notify_DHCP_COMPLETED,
                                       (void *) awsNotify_DHCPCompletedlHandler,
                                       mico_context );
    require_noerr( err, exit );

    memcpy( mico_context->micoSystemConfig.ssid, ssid, maxSsidLen );
    memset( mico_context->micoSystemConfig.bssid, 0x0, 6 );
    memcpy( mico_context->micoSystemConfig.user_key, passwd, maxKeyLen );
    mico_context->micoSystemConfig.user_keyLength = strlen( passwd );
    mico_context->micoSystemConfig.dhcpEnable = true;

    system_connect_wifi_normal( system_context( ) );

    err = mico_rtos_get_semaphore( &net_semp, connection_timeout_ms ); //wait until get semaphore
    require_noerr_action( err, exit, alink_platform_log("DHCP ERR") );

    err = mico_rtos_get_semaphore( &net_semp, connection_timeout_ms ); //wait until get semaphore
    require_noerr_action( err, exit, alink_platform_log("CONNECTED ERR") );

    mico_system_delegate_config_will_stop( );

    exit:
    mico_system_notify_remove( mico_notify_WIFI_STATUS_CHANGED,
                               (void *) awsNotify_WifiStatusHandler );
    mico_system_notify_remove( mico_notify_DHCP_COMPLETED,
                               (void *) awsNotify_DHCPCompletedlHandler );
    mico_rtos_deinit_semaphore( &net_semp );
    net_semp = NULL;
    return (err == kNoErr) ? 0 : -1;

}
/** @} */ //end of platform__awss
