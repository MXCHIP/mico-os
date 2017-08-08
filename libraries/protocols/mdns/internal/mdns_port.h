/**
 ******************************************************************************
 * @file    mdns_port.h
 * @author  William Xu
 * @version V1.0.0
 * @date    3-August-2017
 * @brief   porting layer for mdns
 * The porting layer is comprised of functions that must be implemented and
 * linked with mdns.  Various compiler and system defines are also implemented
 * in this file.  Feel free to expand these to work with you system.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#ifndef __MDNS_PORT_H__
#define __MDNS_PORT_H__

#if 0
/* system-dependent definitions */
#if MDNS_SYSTEM_LINUX
/* bsd socket stuff */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h> /* close */

#include <stdio.h>  /* sprintf */
#include <stdint.h> /* for uint8_t and friends */
#include <string.h> /* memset, memcpy, memmove, strlen, strchr, strcpy */

#else
#error "mdns target system is not defined"
#endif

/* compiler-dependent definitions */
#ifdef __GNUC__
/* wrappers to define packed structures */
#define BEGIN_PACK
#define END_PACK __attribute__((__packed__))

#else
#error "mdns compiler is not defined"
#endif

#endif
#include <stdio.h>
#include "mico.h"
#include "mdns_opt.h"

/* For little endian systems */
#ifndef MDNS_ENDIAN_LITTLE
#define MDNS_ENDIAN_LITTLE
#endif

/* For Big endian systems */
/*
#ifndef MDNS_ENDIAN_BIG
#define MDNS_ENDIAN_BIG
#endif
*/

#if defined(MDNS_ENDIAN_BIG) && defined(MDNS_ENDIAN_LITTLE)
#error "Please define only one endianness"
#elif !defined(MDNS_ENDIAN_BIG) && !defined(MDNS_ENDIAN_LITTLE)
#error "Please define endianness of target"
#endif

#define MDNS_THREAD_RESPONDER 1
#define MDNS_THREAD_QUERIER   2

/* mdns control sockets id
 *
 * mdns uses two control sockets to communicate between the mdns threads and
 * any API calls. This control socket is actually a virtual socket bonded to
 * os message queue.
 */
#define MDNS_CTRL_RESPONDER 0
#define MDNS_CTRL_QUERIER   1

/*
 * mdsn_thread_entry: Thread entry point function
 */
typedef void (*mdns_thread_entry)(void);

/*
 * mdns_thread_create: Create a thread
 *
 * mdns_thread_create should create and launch the thread.  mdns launches one
 * thread for the responder and one for the querier.
 *
 * entry: thread entry point function
 * data: data to be passed to entry when thread is launched
 *
 * id: some targets may wish to know which thread is being created with a
 * specific call to mdns_thread_create.  To facilitate this, a thread id is
 * passed at thread creation time.  One of the MDNS_THREAD_* id values will be
 * passed at thread creation time.
 *
 * Returns: NULL on failure; a pointer to an opaque type that represents the
 * thread on success.  This return type is passed to the other thread
 * functions.
 *
 */
void *mdns_thread_create(mdns_thread_entry entry, int id);

/*
 * mdns_thread_delete: Delete a thread
 *
 * t: pointer to thread to be deleted.  If NULL, no action is taken.
 *
 */
void mdns_thread_delete(void *t);

/*
 * mdns_thread_yield: yield to other runnable threads
 *
 * Some mdns routines need to yield after sending commands to the mdns thread.
 * This allows the mdns thread to run and respond to the command.
 */
void mdns_thread_yield(void *t);

/*
 * mdns_log: printf-like function to write log messages
 *
 * The mdns daemon will write log messages depending on its build-time
 * log-level.  See mdns.h for details.
 */
#define mdns_log(...) MICO_LOG(CONFIG_MDNS_DEBUG, "MDNS LOG", M, ##__VA_ARGS__)

/*
 * mdns_time_ms: get current time in milliseconds
 *
 * The mdns daemon needs a millisecond up counter for calculating timeouts.
 * The base of the count is arbitrary.  For example, this function could return
 * the number of milliseconds since boot, or since the beginning of the epoch,
 * etc.  Wrap-around is handled internally.  The precision should be to the
 * nearest 10ms.
 */
uint32_t mdns_time_ms(void);

/*
 * mdns_rand_range: get random number between 0 and n
 *
 * The mdns daemon needs to generate random numbers for various timeout ranges.
 * This function is needed to return a random number between 0 and n. It should
 * provide a uniform random field.
 *
 * The program must initialize the random number generator before starting the
 * mdns thread.
 */
int mdns_rand_range(int n);

/* mdns_socket_mcast: create a multicast socket
 *
 * More specifically, this function must create a non-blocking socket bound to
 * the to the specified IPv4 multicast address and port on the interface on
 * which mdns is to be running.  The multicast TTL should be set to 255 per the
 * mdns specification.
 *
 * Returns the socket descriptor suitable for use with FD_SET, select,
 * recvfrom, etc.  Returns -1 on error.
 *
 * Note: if available, the SO_REUSEADDR sockopt should be enabled.  This allows
 * for the stopping and restarting of mdns without waiting for the socket
 * timeout.
 *
 * Note: when recvfrom is called on this socket, the MSG_DONTWAIT flag will be
 * passed.  This may be sufficient to ensure non-blocking behavior.
 */
int mdns_socket_mcast(void);


/* mdns_iface_group_state_change: Join or leave multicast group for a net interface
 * this function is called by
 *
 * iface: network interface
 */



/* mdns_socket_loopback: Return or create a socket binded to an OS message queue,
 * mdns use select the socket to wait for the event on the message queue.
 *
 * id: MDNS_CTRL_RESPONDER or MDNS_CTRL_QUERIER
 *
 * queue: Provide the address to store the message queue, generate a new one if not existed
 *
 * size: size of one queue message
 *
 * Returns the socket descriptor that bonded to message queue. the socket descriptor
 * suitable for use with FD_SET, select.
 */
int mdns_socket_queue(uint8_t id, mico_queue_t **queue, int msg_size);

/* mdns_socket_close: close a socket
 *
 * s: non-negative control socket returned from either mdns_socket_mcast or
 * mdns_socket_queue.
 */
int mdns_socket_close(int *s);


#endif /* __MDNS_PORT_H__ */
