#ifndef _LWIP_API_DEFINE_
#define _LWIP_API_DEFINE_

#include "stdint.h"
#include "mico_socket.h"

typedef struct ip_addr {
  uint32_t addr;
} ip_addr_t;

/** Callback which is invoked when a hostname is found.
 * A function of this type must be implemented by the application using the DNS resolver.
 * @param name pointer to the name that was looked up.
 * @param ipaddr pointer to an ip_addr_t containing the IP address of the hostname,
 *        or NULL if the name could not be found (or on any other error).
 * @param callback_arg a user-specified callback argument passed to dns_gethostbyname
*/
typedef void (*dns_found_callback)(const char *name, ip_addr_t *ipaddr, void *callback_arg);

typedef struct _lwip_api_ {
	int (*lwip_accept)(int s, struct sockaddr *addr, socklen_t *addrlen);
	int (*lwip_bind)(int s, const struct sockaddr *name, socklen_t namelen);
	int (*lwip_shutdown)(int s, int how);
	int (*lwip_getpeername) (int s, struct sockaddr *name, socklen_t *namelen);
	int (*lwip_getsockname) (int s, struct sockaddr *name, socklen_t *namelen);
	int (*lwip_getsockopt) (int s, int level, int optname, void *optval, socklen_t *optlen);
	int (*lwip_setsockopt) (int s, int level, int optname, const void *optval, socklen_t optlen);
	int (*lwip_close)(int s);
	int (*lwip_connect)(int s, const struct sockaddr *name, socklen_t namelen);
	int (*lwip_listen)(int s, int backlog);
	int (*lwip_recv)(int s, void *mem, size_t len, int flags);
	int (*lwip_read)(int s, void *mem, size_t len);
	int (*lwip_recvfrom)(int s, void *mem, size_t len, int flags,
	      struct sockaddr *from, socklen_t *fromlen);
	int (*lwip_send)(int s, const void *dataptr, size_t size, int flags);
	int (*lwip_sendto)(int s, const void *dataptr, size_t size, int flags,
	    const struct sockaddr *to, socklen_t tolen);
	int (*lwip_socket)(int domain, int type, int protocol);
	int (*lwip_write)(int s, const void *dataptr, size_t size);
	int (*lwip_select)(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset,
	                struct timeval *timeout);
	int (*lwip_ioctl)(int s, long cmd, void *argp);
	int (*lwip_fcntl)(int s, int cmd, int val);
	
	void (*lwip_freeaddrinfo)(struct addrinfo *ai);
	int (*lwip_getaddrinfo)(const char *nodename,
		   const char *servname,
		   const struct addrinfo *hints,
		   struct addrinfo **res);

	char * (*ipaddr_ntoa)(const ip_addr_t *addr);
	uint32_t (*ipaddr_addr)(const char *cp);
    struct hostent * (*lwip_gethostbyname) (const char *name);
    char *(*sethostname)( char *name );
	char* (*get_dhcp_classid)( void );
	char* (*set_dhcp_classid)( char *classid );

    /* For inet_ntop and inet_pton */
    const char * (*inet_ntop) (int af, const void *cp, char *buf, socklen_t len);
    int (*inet_pton) (int af, const char *cp, void *buf);
	int (*dns_gethostbyname) (const char *hostname, ip_addr_t *addr, dns_found_callback found,
                  void *callback_arg);
	int (*lwip_gethostbyname_r)(const char *name, struct hostent *ret, char *buf,
                size_t buflen, struct hostent **result, int *h_errnop);
} lwip_api_t;

#endif
