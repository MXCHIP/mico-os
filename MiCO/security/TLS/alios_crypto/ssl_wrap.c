/**
 ******************************************************************************
 * @file    ssl_wrap.c
 * @author  William Xu
 * @version V1.0.0
 * @date    24-Noc-2016
 * @brief   This file provide the SSL api conversion for MiCO.
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


#include "mico_socket.h"

/******************************************************
*                      Macros
******************************************************/


/******************************************************
*                      Macros
******************************************************/

/******************************************************
*                    Constants
******************************************************/

/******************************************************
*                   Enumerations
******************************************************/

/******************************************************
*                 Type Definitions
******************************************************/

typedef void* mico_ssl_t;

/******************************************************
*                    Structures
******************************************************/

/******************************************************
*               Static Function Declarations
******************************************************/

/******************************************************
*               Variable Definitions
******************************************************/

//static    CYASSL_CTX*     g_ssl_server_ctx     = 0;
static char *cert_buf=NULL, *prvkey_buf=NULL;
static char *client_cert_buf=NULL, *client_prvkey_buf=NULL;

static int ssl_version = TLS_V1_2_MODE;

//static VerifyCallback clientcb=NULL;
static char *no_ecc = "AES128-SHA:AES256-SHA:AES128-SHA256:AES256-SHA256:AES128-GCM-SHA256";
static char *defaultCipherList = NULL;

/******************************************************
*               Function Definitions
******************************************************/

void ssl_set_ecc(int enable)
{
    if (enable)
        defaultCipherList = NULL;
    else
        defaultCipherList = no_ecc;
}

void ssl_set_cert(const char *_cert_pem, const char *private_key_pem)
{
    cert_buf = (char*)_cert_pem;
    prvkey_buf = (char*)private_key_pem;
}

void ssl_set_client_cert(const char *_cert_pem, const char *private_key_pem)
{
    client_cert_buf = (char*)_cert_pem;
    client_prvkey_buf = (char*)private_key_pem;
}

//void ssl_set_client_verifycb(VerifyCallback cb)
//{
//	clientcb = cb;
//}

static void ssl_load_cert(CYASSL_CTX*ctx)
{
    CyaSSL_CTX_use_certificate_buffer(ctx, (const unsigned char*)cert_buf, strlen(cert_buf), SSL_FILETYPE_PEM);
    CyaSSL_CTX_use_PrivateKey_buffer(ctx, (const unsigned char*)prvkey_buf, strlen(prvkey_buf), SSL_FILETYPE_PEM);
}

static void ssl_client_load_cert(CYASSL_CTX*ctx)
{
	if (client_cert_buf == NULL)
		return;
	if (client_prvkey_buf == NULL)
		return;
    CyaSSL_CTX_use_certificate_buffer(ctx, (const unsigned char*)client_cert_buf, strlen(client_cert_buf), SSL_FILETYPE_PEM);
    CyaSSL_CTX_use_PrivateKey_buffer(ctx, (const unsigned char*)client_prvkey_buf, strlen(client_prvkey_buf), SSL_FILETYPE_PEM);
}

void ssl_set_client_version(ssl_version_type_t version)
{
	if (version < SSL_V3_MODE)
		return;
	if (version > TLS_V1_2_MODE)
		return;
	
	ssl_version = version;
}

//void ssl_version_set(int version)
//{
//	if (version < SSL_V3_MODE)
//		return;
//	if (version > TLS_V1_2_MODE)
//		return;
//
//	ssl_version = version;
//}

// mode == 1: client ssl
// mode == 2: server ssl
static CYASSL_CTX* ssl_wrap_init(int mode, int calen, char*ca)
{
    CYASSL_METHOD  *method_c, *method_s;
    CYASSL_CTX* ctx;
    if (mode == 1) {
#ifdef WOLFSSL_MAX_STRENGTH
		method_c = CyaTLSv1_2_client_method();
#else
		switch(ssl_version) {
		case SSL_V3_MODE:
		case TLS_V1_0_MODE:
			method_c = CyaTLSv1_client_method();
			break;
		case TLS_V1_1_MODE:
			method_c = CyaTLSv1_1_client_method();
			break;
		case TLS_V1_2_MODE:
			method_c = CyaTLSv1_2_client_method();
			break;
		default:
			method_c = CyaTLSv1_1_client_method();
			break;
		}
#endif		
		method_c->downgrade = 1;
        ctx = CyaSSL_CTX_new(method_c);
        if (ctx == NULL)
            return NULL;

		ssl_client_load_cert(ctx);
        if (defaultCipherList != NULL) {
            SSL_CTX_set_cipher_list(ctx, defaultCipherList);
        }
    } else {
#ifdef WOLFSSL_MAX_STRENGTH
		method_s = CyaTLSv1_2_client_method();
#else
		switch(ssl_version) {
		case SSL_V3_MODE:
		case TLS_V1_0_MODE:
			method_s = CyaTLSv1_server_method();
			break;
		case TLS_V1_1_MODE:
			method_s = CyaTLSv1_1_server_method();
			break;
		case TLS_V1_2_MODE:
			method_s = CyaTLSv1_2_server_method();
			break;
		default:
			method_s = CyaTLSv1_1_server_method();
			break;
		}
#endif		
		method_s->downgrade = 1;
        ctx = SSL_CTX_new(method_s);
        if (ctx == NULL)
            return NULL;

        ssl_load_cert(ctx);
        SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, 0); 
        if (defaultCipherList != NULL) {
            SSL_CTX_set_cipher_list(ctx, defaultCipherList);
        }
    }

    return ctx;
}

int ssl_close(void* ssl);

void* ssl_connect(int fd, int calen, char*ca, int *errno)
{
    struct CYASSL *    fd_ssl;
	CYASSL_CTX* ctx;
    *errno = -1;

    ctx = ssl_wrap_init(1, calen, ca);

    if (ctx == 0)
        return 0;

    fd_ssl = CyaSSL_new(ctx);
    if (fd_ssl == NULL) {
        *errno = -203;//MEMORY_ERROR
        return 0;
    }
	if (calen > 0) {
        if (SSL_SUCCESS != CyaSSL_CTX_load_verify_buffer(ctx, (const unsigned char*)ca,
                                      calen, SSL_FILETYPE_PEM))
           return 0;
        SSL_set_verify(fd_ssl, SSL_VERIFY_PEER, clientcb); 
    } else {
        SSL_set_verify(fd_ssl, SSL_VERIFY_NONE, 0); 
    }
    CyaSSL_set_fd(fd_ssl, fd);
    if (CyaSSL_connect(fd_ssl) != SSL_SUCCESS) {
        *errno = fd_ssl->error;
        ssl_close((void*)fd_ssl);
        return 0;
    }
	*errno = -0;
    return (void*)fd_ssl;
}

void* ssl_connect_sni(int fd, int calen, char*ca, char *sni_servername, int *errno)
{
    struct CYASSL *    fd_ssl;
	CYASSL_CTX* ctx;
    *errno = -1;

    ctx = ssl_wrap_init(1, calen, ca);

    if (ctx == 0)
        return 0;

    if (sni_servername) {
        if (CyaSSL_CTX_UseSNI(ctx, 0, sni_servername, XSTRLEN(sni_servername)) != SSL_SUCCESS) {
			*errno = -125;
            return 0;
        } 
	}
    
    fd_ssl = CyaSSL_new(ctx);
    if (fd_ssl == NULL) {
        *errno = -203;//MEMORY_ERROR
        return 0;
    }
	if (calen > 0) {
        if (SSL_SUCCESS != CyaSSL_CTX_load_verify_buffer(ctx, (const unsigned char*)ca,
                                      calen, SSL_FILETYPE_PEM))
           return 0;
        SSL_set_verify(fd_ssl, SSL_VERIFY_PEER, clientcb); 
    } else {
        SSL_set_verify(fd_ssl, SSL_VERIFY_NONE, 0); 
    }
    CyaSSL_set_fd(fd_ssl, fd);
    if (CyaSSL_connect(fd_ssl) != SSL_SUCCESS) {
        *errno = fd_ssl->error;
        ssl_close((void*)fd_ssl);
        return 0;
    }
	*errno = -0;
    return (void*)fd_ssl;
}


#ifndef F_GETFL
#define F_GETFL 3
#endif
#ifndef F_SETFL
#define F_SETFL 4
#endif
#ifndef O_NONBLOCK
#define O_NONBLOCK  1 /* nonblocking I/O */
#endif
enum {
	SELECT_FAIL,
	TIMEOUT,
	RECV_READY,
	ERROR_READY
};

static void tcp_set_nonblocking(int *sockfd)
{
	int flags = lwip_fcntl(*sockfd, F_GETFL, 0);
	lwip_fcntl(*sockfd, F_SETFL, flags | O_NONBLOCK);
}
static int tcp_select(int socketfd, int to_sec)
{
	fd_set recvfds, errfds;
	int nfds = socketfd + 1;
	struct timeval timeout = { (to_sec > 0) ? to_sec : 0, 0};
	int result;

	FD_ZERO(&recvfds);
	FD_SET(socketfd, &recvfds);
	FD_ZERO(&errfds);
	FD_SET(socketfd, &errfds);

	result = lwip_select(nfds, &recvfds, NULL, &errfds, &timeout);

	if (result == 0)
		return TIMEOUT;
	else if (result > 0) {
		if (FD_ISSET(socketfd, &recvfds))
			return RECV_READY;
		else if (FD_ISSET(socketfd, &errfds))
			return ERROR_READY;
	}

	return SELECT_FAIL;
}

static int NonBlockingSSL_Connect(CYASSL *ssl, uint32_t connect_time)
{
	int ret = SSL_connect(ssl);
	int error = SSL_get_error(ssl, 0);
	int sockfd = (int) CyaSSL_get_fd(ssl);
	int select_ret;
	uint32_t timeout = mico_rtos_get_time() + connect_time;
	int i = 0;
	while (ret != SSL_SUCCESS && (error == SSL_ERROR_WANT_READ ||
				error == SSL_ERROR_WANT_WRITE)) {
		if (timeout < mico_rtos_get_time())
			break;
		select_ret = tcp_select(sockfd, 1);

		if ((select_ret == RECV_READY) ||
				(select_ret == ERROR_READY)) {
			ret = SSL_connect(ssl);
			error = SSL_get_error(ssl, 0);
		} else if (select_ret == TIMEOUT) {
			error = SSL_ERROR_WANT_READ;
		}
		else {
			error = SSL_FATAL_ERROR;
		}
	}

	return ret;
}

void* ssl_nonblock_connect(int fd, int calen, char*ca, int *errno, int seconds)
{
    struct CYASSL *    fd_ssl;
	CYASSL_CTX* ctx;

    *errno = -1;
    ctx = ssl_wrap_init(1, calen, ca);
    if (ctx == 0)
        return 0;
    fd_ssl = CyaSSL_new(ctx);
    if (fd_ssl == NULL) {
        *errno = -203;//MEMORY_ERROR
        SSL_CTX_free(ctx);
        return 0;
    }
	if (calen > 0) {
        if (SSL_SUCCESS != CyaSSL_CTX_load_verify_buffer(ctx, (const unsigned char*)ca,
                                      calen, SSL_FILETYPE_PEM)) {
		   SSL_CTX_free(ctx);
		   return 0;
        }
        SSL_set_verify(fd_ssl, SSL_VERIFY_PEER, clientcb); 
    } else {
        SSL_set_verify(fd_ssl, SSL_VERIFY_NONE, 0); 
    }
    CyaSSL_set_fd(fd_ssl, fd);
	wolfSSL_set_using_nonblock(fd_ssl, 1);
    tcp_set_nonblocking(&fd);
	if (seconds <= 0)
		seconds = 5;
    if (NonBlockingSSL_Connect(fd_ssl, seconds*1000) != SSL_SUCCESS) {
        *errno = fd_ssl->error;
        ssl_close((void*)fd_ssl);
        return 0;
    }
	*errno = -0;
    return (void*)fd_ssl;
}

void* ssl_accept(int fd)
{
    SSL*    fd_ssl;
	CYASSL_CTX* ctx;
	
    ctx = ssl_wrap_init(0, 0, NULL);

    if (ctx == 0) 
        return 0;
    
    fd_ssl = SSL_new(ctx);
    if (fd_ssl == NULL)
        return 0;
    
    SSL_set_fd(fd_ssl, fd);
    
    if (SSL_accept(fd_ssl) != SSL_SUCCESS) {
        ssl_close((void*)fd_ssl);
        return 0;
    }

    return (void*)fd_ssl;
}

int ssl_send(void* ssl, void *data, size_t len)
{
    return SSL_write((SSL*)ssl, data, len);
}

/* return the read data length */
int ssl_recv(void* ssl, void *data, size_t len)
{
    return SSL_read((SSL*)ssl, data, len);
}

int ssl_close(void* ssl)
{
	CYASSL_CTX* ctx;
	int fd;
	
	ctx = ((SSL*)ssl)->ctx;
	fd = wolfSSL_get_fd(ssl);
    SSL_shutdown((SSL*)ssl);
    SSL_free((SSL*)ssl);
	close(fd);
	wolfSSL_CTX_free(ctx);
	
    return 0;
}

int ValidateDate(const unsigned char* date, unsigned char format, int dateType)
{ 
	return 1; 
}

int ssl_socket( mico_ssl_t ssl )
{
    return wolfSSL_get_fd( (const WOLFSSL*)ssl );
}

int ssl_pending(void*ssl)
{
    return CyaSSL_pending(ssl);
}


void ssl_set_using_nonblock(void* ssl, int nonblock)
{
    wolfSSL_set_using_nonblock(ssl, nonblock);
}

int ssl_get_error(void* ssl, int ret)
{
    wolfSSL_get_error(ssl, ret);
}


