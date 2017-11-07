/**
 ******************************************************************************
 * @file    mdns.h
 * @author  William Xu
 * @version V1.0.0
 * @date    3-August-2017
 * @brief   mdns daemon APIs
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

#ifndef MDNS_H
#define MDNS_H

#include "mico.h"
#include "mdns_opt.h"

/** Maximum length of labels
 *
 * A label is one segment of a DNS name.  For example, "foo" is a label in the
 * name "foo.local.".  RFC 1035 requires that labels do not exceed 63 bytes.
 */
#define MDNS_MAX_LABEL_LEN	63	/* defined by the standard */

/** Maximum length of names
 *
 * A name is a list of labels such as "My Webserver.foo.local" or
 * mydevice.local.  RFC 1035 requires that names do not exceed 255 bytes.
 */
#define MDNS_MAX_NAME_LEN	255	/* defined by the standard : 255*/

/** Maximum length of key/value pair
 *
 * TXT records associated with a service are populated with key/value pairs.
 * These key/value pairs must not exceed this length.
 */
#define MDNS_MAX_KEYVAL_LEN	255	/* defined by the standard : 255*/

/** protocol values for the proto member of the mdns_service descriptor */
/** TCP Protocol */
#define MDNS_PROTO_TCP 0
/** UDP Protocol */
#define MDNS_PROTO_UDP 1

/** Maximum no. of services allowed to be announced on a single interface. */
#define MAX_MDNS_LST 5

/* Total number of interface config supported by mdns */
#if PLATFORM_ETH_ENABLE
#define MDNS_MAX_SERVICE_CONFIG 3
#else
#define MDNS_MAX_SERVICE_CONFIG 2
#endif

/** MDNS Error Codes */
#define ERR_MDNS_BASE              -36650   /** Starting error code for all mdns errors. */
#define	ERR_MDNS_INVAL             -36651   /** invalid argument */
#define ERR_MDNS_BADSRC            -36652   /** bad service descriptor */
#define ERR_MDNS_TOOBIG            -36653  /** not enough room for everything */
#define ERR_MDNS_NOIMPL            -36654  /** unimplemented feature */
#define ERR_MDNS_NOMEM             -36655  /** insufficient memory */
#define ERR_MDNS_INUSE             -36656  /** requested resource is in use */
#define ERR_MDNS_NORESP            -36657  /** requested resource is in use */
#define ERR_MDNS_FSOC              -36658  /** failed to create socket for mdns */
#define ERR_MDNS_FREUSE            -36659  /** failed to reuse multicast socket */
#define ERR_MDNS_FBINDTODEVICE     -36660  /** failed to bind mdns socket to device */
#define ERR_MDNS_FBIND             -36661  /** failed to bind mdns socket */
#define ERR_MDNS_FMCAST_JOIN       -36662  /** failed to join multicast socket */
#define ERR_MDNS_FMCAST_SET        -36663  /** failed to set multicast socket */
#define ERR_MDNS_FQUERY_SOC        -36664  /** failed to create query socket */
#define ERR_MDNS_FQUERY_THREAD     -36665  /** failed to create mdns thread */
#define ERR_MDNS_END               -36670  /** Last generic error code (inclusive) */

/** mDNS Interface State
 * mDNS interface state can be changed by using mdns_iface_state_change()
 * function. For details about when to use the enum please refer to
 * documentation for mdns_iface_state_change(). */
enum iface_state {
	/** UP the interface and announce services
	 * mDNS will probe and announce all services announced via
	 * mdns_announce_service() and/or mdns_announce_service_arr().
	 * mDNS will go through entire probing sequence explained in above
	 * functions. Interface state can be changed to UP, if its DOWN.
	 */
	UP = 0,
	/** DOWN the interface and de-announce services
	 * mDNS sends good bye packet with ttl=0 so that mDNS clients can remove
	 * the services from their mDNS cache table.
	 */
	DOWN,
	/** Forcefully re-announce services
	 * This state should be used after services are already
	 * announced and force announcement is needed due to some reason.
	 * mDNS will not perform probing sequence, as it does in case of UP, and
	 * will directly re-announce services.
	 */
	REANNOUNCE
};

/** service descriptor
 *
 * Central to mdns is the notion of a service.  Hosts advertise service types
 * such as a website, a printer, some custom service, etc.  Network users can
 * use an mdns browser to discover services on the network.  Internally, this
 * mdns implementation uses the following struct to describe a service.  These
 * structs can be created by a user, populated, and passed to mdns announce
 * functions to specify services that are to be advertised. When a user starts a
 * query for services, the discovered services are passed back to the user in
 * this struct.
 *
 * The members include:
 *
 * servname: string that is the service instance name that will be advertised.
 * It could be something like "Brian's Website" or "Special Service on Device
 * #123".  This is the name that is typically presented to users browsing for
 * your service.  The servname must not exceed MDNS_MAX_LABEL_LEN bytes.  The
 * MDNS specification allows servname to be a UTF8 string.  However, only the
 * ascii subset of UTF-8 has been tested.
 *
 * servtype: string that represents the service type.  This should be a type
 * registered at http://dns-sd.org/ServiceTypes.html.  For example, "http" is
 * the service type for a web server and "ssh" is for an ssh server.  You may
 * use an unregisterd service type during development, but not in released
 * products.  Consider registering any new service types at the aforementioned
 * webpage.  servtype must be non-NULL.
 *
 * domain: string that represents the domain. If this value is NULL, domain
 * name ".local" will be used. The domain must not exceed
 * \ref MDNS_MAX_LABEL_LEN bytes.
 *
 * port: the tcp or udp port on which the service named servname is available
 * in network byte order.
 *
 * proto: Either MDNS_PROTO_TCP or MDNS_PROTO_UDP depending on what protocol
 * clients should use to connect to the service servtype.
 *
 * keyvals: NULL-terminated string of colon-separated key=value pairs.
 * Note: they can only be created by mdns_set_txt_rec() when initializing a service.
 *
 * keyvals are the key/value pairs for the TXT record associated with a service type.
 * For example, the servtype "http" defines the TXT keys "u", "p", and "path"
 * for the username, password, and path to a document.  If you supplied all of
 * these, the keyvals string would be:
 *
 * "u=myusername:p=mypassword:path=/index.html"
 *
 * If keyvals is NULL, no TXT record will be advertised.  If keyvals is ":", a
 * TXT record will appear, but it will not contain any key/value pairs.  The
 * key must be present (i.e., two contiguous ':' characters should never appear
 * in the keyvals string.)  A key may appear with no value.  The interpretation
 * of this depends on the nature of the service.  The length of a single
 * key/value pair cannot exceed MDNS_MAX_KEYVAL_LEN bytes.
 * It is the responsibility of the application to verify that the keyval string
 * is a valid string. The keyval string buffer is used by the mDNS module
 * internally and it can modify it. Hence, during subsequent calls to the mDNS
 * module, it is possible that the original string has been messed up and needs
 * to be recreated.
 *
 * ipaddr, fqsn, ptrname, kvlen and flags are for internal use only and should
 * not be dereferenced by the user.
 */
struct mdns_service
{
	/** Name of MDNS service  */
	const char *servname;
	/** Type of MDNS service */
	const char *servtype;
	/** Domain for MDNS service */
	const char *domain;
	/** Port number  */
	uint16_t port;
	/** Protocol used */
	int proto;
	/** Key value pairs for TXT records*/
	char *keyvals;
	/** IP Address of device */
	uint32_t ipaddr;
	/** IPv6 Address of device */
	uint32_t ip6addr[4];

	/** The following members are for internal use only and should not be
	 * dereferenced by the user.
	 */
	uint8_t fqsn[MDNS_MAX_NAME_LEN];
	/** PTR record name */
	uint8_t *ptrname;
	/** Length of keyvals string*/
	uint16_t kvlen;
	/**  MDNS flags */
	uint32_t flags;
};

/** mdns_start
 *
 * Start the responder thread (and the querier thread if querying is enabled).
 *
 * Note that the mdns_start() function must be called only after the network
 * stack is initialized.
 *
 * The responder thread wait for application's request to announce services.
 * Using mdns_announce_service() and mdns_announce_service_arr() call, services
 * are added to mDNS service list and are announced on a given interface.
 *
 * The querier thread just opens a socket and then waits until a request for
 * monitoring any given service type or subtype is received from the
 * application. When received, the same is queried and the responses are
 * processed, cached and monitored.
 * 
 * \param domain domain name string.  If this value is NULL, the domain ".local"
 * will be used. The domain must not exceed \ref MDNS_MAX_LABEL_LEN bytes.
 *
 * \param hostname string that is the hostname to resolve. This would be the
 * "foo" in "foo.local", for example.  The hostname must not exceed
 * \ref MDNS_MAX_LABEL_LEN bytes.  If hostname is NULL, the responder capability
 * will not be launched, only query support will be enabled.  This is useful if
 * only the query functionality is desired.
 *
 * \return kNoErr for success or mdns error code
 *
 * NOTES:
 *
 * The domain and hostname must persist and remain unchanged between calls to
 * mdns_start and mdns_stop. Hence define these variables in global memory.
 *
 * While mdns_start returns immediately, the hostname and any servnames may
 * not be unique on the network.  In the event of a conflict, the names will
 * be appended with an integer.  For example, if the hostname "foo.local" is
 * taken, mdns will attempt to claim "foo-2.local", then foo-3.local, and so on
 * until the conflicts cease.  If mdns gets all the way to foo-9.local and
 * still fail, it waits for 5 seconds (per the mDNS specification) and then
 * starts back at foo.local.  If a developer anticipates a network to have many
 * of her devices, she should devise a sensible scheme outside of mdns to
 * ensure that the names are unique.
 */
int mdns_start(const char *domain, char *hostname);

/** 
 * mdns_stop
 *
 * Halt the mDNS responder thread (and querier thread if querying is enabled),
 * delete the threads and close the sockets
 *
 * Any services being monitored will be unmonitored.
 */
void mdns_stop(void);




/** mdns query callback
 *
 * A user initiates a query for services by calling the mdns_query_monitor
 * function with a fully-qualified service type, an mdns_query_cb, and an
 * opaque argument.  When a service instance is discovered, the query callback
 * will be invoked with following arguments:
 *
 * \param data a void * that was passed to mdns_query_monitor().  This can be
 * anything that the user wants, such as pointer to a custom internal data
 * structure.
 *
 * \param s A pointer to the struct mdns_service that was discovered.  The struct
 * mdns_service is only valid until the callback returns.  So if attributes of
 * the service (such as IP address and port) are required by the user for later
 * use, they must be copied and preserved elsewhere.
 *
 * \param status A code that reports the status of the query.  It takes one of the
 * following values:
 *
 * MDNS_DISCOVERED: The mdns_service s has just been discovered on the network
 * and will be monitored by the mdns stack.
 *
 * MDNS_UPDATED: The mdns_service s, which is being monitored, has been updated
 * in some way (e.g., it's IP address has changed, it's key/value pairs have
 * changed.)
 *
 * MDNS_DISAPPEARED: The mdns_service has left the network.  This usually
 * happens when a service has shut down, or when it has stopped responding
 * queries.  Applications may also detect this condition by some means outside
 * of mdns, such as a closed TCP connection.
 *
 * MDNS_CACHE_FULL: The mdns_service has been discovered.  However, the number
 * of monitored service instances has exceeded CONFIG_MDNS_SERVICE_CACHE_SIZE.
 * So the returned mdns_service may not be complete.  See NOTES below on other
 * implications of an MDNS_CACHE_FULL status.
 *
 * NOTES:
 *
 * The query callback should return kNoErr in the case where it has
 * discovered service of interest (MDNS_DISCOVERED, MDNS_UPDATED status). If
 * the callback return non-zero value for MDNS_DISCOVERED and MDNS_UPDATED
 * status codes, that particular service instance is not cached by the mDNS
 * querier. This is required as each cached service instance takes little above
 * 1KB memory and the device can't monitor large number of service instances.
 *
 * Callback implementers must take care to not make any blocking calls, nor to
 * call any mdns API functions from within callbacks.
 *
 */
#define MDNS_DISCOVERED     1
#define MDNS_UPDATED        2
#define MDNS_DISAPPEARED    3
#define MDNS_CACHE_FULL     4

typedef int (* mdns_query_cb)(void *data, const struct mdns_service *s,
			int status);

/** mdns_query_monitor
 *
 * Query for and monitor instances of a service
 *
 * When instances of the specified service are discovered, the specified
 * query callback is called as described above.
 *
 * \param fqst Pointer to a null-terminated string specifying the fully-qualified
 * service type.  For example, "_http._tcp.local" would query for all http
 * servers in the ".local" domain.
 *
 * \param cb an mdns_query_cb to be called when services matching the specified fqst
 * are discovered, are updated, or disappear.  cb will be passed the opaque
 * data argument described below, a struct mdns_service that represents the
 * discovered service, and a status code.
 *
 * \param data a void * that will passed to cb when services are discovered, are
 * updated, or disappear.  This can be anything that the user wants, such as
 * pointer to a custom internal data structure.
 *
 * \param iface Interface handle on which services are to be queried. Interface
 * handle can be obtained from net_get_sta_handle() or net_get_uap_handle()
 * function calls
 *
 * \return kNoErr: the query was successfully launched.  The caller should expect
 * the mdns_query_cb to be invoked as instances of the specified service are
 * discovered.
 *
 * \return ERR_MDNS_INVAL: cb was NULL or fqst was not valid.
 *
 * \return ERR_MDNS_NOMEM: CONFIG_MDNS_MAX_SERVICE_MONITORS is already being
 * monitored.  Either this value must be increased, or a service must be
 * unmonitored by calling mdns_query_unmonitor.
 *
 * \return ERR_MDNS_INUSE: The specified service type is already being monitored by another
 * callback, and multiple callbacks per service are not supported.
 *
 * \return ERR_MDNS_NORESP: No response from the querier.  Perhaps it was not launched or
 * it has crashed.
 *
 * Note: multiple calls to mdns_query_service_start are allowed.  This enables
 * the caller to query for more than just one service type.
 */
int mdns_query_monitor(char *fqst, mdns_query_cb cb, void *data, netif_t iface);

/** mdns_query_unmonitor
 *
 * Stop monitoring a particular service
 *
 * \param fqst The service type to stop monitoring, or NULL to unmonitor all
 * services.
 *
 * \note Suppose a service has just been discovered and is being processed
 * while the call to mdns_query_monitor is underway.  A callback may be
 * generated before the service is unmonitored.
 */
void mdns_query_unmonitor(char *fqst);


/** mdns_announce_service
 *
 * Announce single mDNS service on an interface
 *
 * This function checks validity of service and if service is invalid
 * then it will return with appropriate error code.
 * Function sends command on mDNS control socket for announcing service on
 * "iface" interface. mDNS then announces the service by following prescribed
 * set of steps mentioned below:
 *  -# Send probing packets for a given service or services 3 times, spaced 250
 *  ms apart.
 *  -# If a conflict is received for any service record, resolve them by
 *  appending a number to that particular record to make it unique. Go to step
 *  1.
 *  -# Announce the service or services.
 * In order to announce multiple services simultaneously, use
 * mdns_announce_service_arr(). This is recommended to reduce network
 * congestion. Maximum no. of services that can be announced per interface
 * is \ref MAX_MDNS_LST
 *
 * \param service Pointer to \ref mdns_service structure corresponding to
 * service to be announced. Structure must persist and remain unchanged between
 * calls to mdns_start and mdns_stop. Hence define this variable in global
 * memory.
 *
 * \param iface Pointer to interface handle on which service is to be announced.
 * Interface handle can be obtained from net_get_sta_handle or
 * net_get_uap_handle function calls.
 *
 * \return kNoErr: success
 *
 * \return ERR_MDNS_INVAL: input was invalid.  Perhaps a label exceeded
 * MDNS_MAX_LABEL_LEN, or a name composed of the supplied label exceeded
 * MDNS_MAX_NAME_LEN. Perhaps hostname was NULL and the query capability is not
 * compiled. Perhaps service pointer or iface pointer is NULL
 *
 * \return ERR_MDNS_BADSRC: one of the service descriptors in the service
 * was invalid.  Perhaps key/val pair exceeded MDNS_MAX_KEYVAL_LEN
 *
 * \return ERR_MDNS_TOOBIG: The combination of name information and service
 * descriptors does not fit in a single packet, which is required by this
 * implementation
 *
 * \return kGeneralErr: No space to add new interface or a new service in a given
 * interface.
 *
 */
int mdns_announce_service(struct mdns_service *service, netif_t iface);

/** mdns_deannounce_service
 *
 * Deannounce single service from an interface
 *
 * This function sends command on mDNS control socket to deannounce a given
 * service on "iface" interface. mDNS then de-announces the service by sending
 * a good bye packet with ttl=0 so that mDNS clients can remove the services
 * from their mDNS cache table. Function removes service from a list of services
 * associated with that interface.
 * In order to de-announce multiple services simultaneously, use
 * mdns_deannounce_service_arr(). This is recommended to reduce network
 * congestion.
 *
 * \param service Pointer to \ref mdns_service structure corresponding to
 * service to be de-announced
 *
 * \param iface Pointer to interface handle on which service is to be
 * de-announced. Interface handle can be obtained from net_get_sta_handle or
 * net_get_uap_handle function calls
 *
 * \return kNoErr: success
 *
 * \return ERR_MDNS_INVAL: invalid parameters. Perhaps service pointer or
 * iface pointer is NULL
 *
 */
int mdns_deannounce_service(struct mdns_service *service, netif_t iface);

/* mdns_announce_service_arr
 *
 * Announce multiple services simultaneously
 *
 * This function checks validity of all services in array and if any service is
 * invalid then it will return with appropriate error code.
 * Function sends command on mDNS control socket for announcing services in a
 * single mDNS packet on "iface" interface. mDNS follows same set of steps
 * as described in mdns_announce_service(), to announce given services.
 * Maximum no. of services that can be announced per interface is
 * \ref MAX_MDNS_LST. If no. of services in an array exceeds \ref MAX_MDNS_LST,
 * no services from given array are announced.
 *
 * \param services array of pointers to \ref mdns_service structure
 * corresponding to services to be announced. Array must persist and remain
 * unchanged between calls to mdns_start and mdns_stop. Hence define this
 * variable in global memory. Array should be NULL terminated.
 *
 * \param iface Pointer to interface handle on which service is to be announced.
 * Interface handle can be obtained from net_get_sta_handle or
 * net_get_uap_handle function calls.
 *
 * \return kNoErr: success
 *
 * \return ERR_MDNS_INVAL: input was invalid.  Perhaps a label exceeded
 * MDNS_MAX_LABEL_LEN, or a name composed of the supplied labels exceeded
 * MDNS_MAX_NAME_LEN. Perhaps hostname was NULL and the query capability is not
 * compiled. Perhaps services pointer or iface pointer is NULL
 *
 * \return ERR_MDNS_BADSRC: one of the service descriptors in the services
 * list was invalid.  Perhaps one of the key/val pairs exceeded
 * MDNS_MAX_KEYVAL_LEN
 *
 * \return ERR_MDNS_TOOBIG: The combination of name information and service
 * descriptors does not fit in a single packet, which is required by this
 * implementation
 *
 * \return kGeneralErr: No space to add new interface or services in a given
 * interface. Maximum no. of services that can be announced per interface is
 * \ref MAX_MDNS_LST
 *
 */
int mdns_announce_service_arr(struct mdns_service *services[], netif_t iface);

/* mdns_deannounce_service_arr
 *
 * Deannounce mulitple services simultaneously
 *
 * This function sends command on mDNS control socket to de-announce given set
 * of services in a single mDNS packet on "iface" interface.
 * mDNS then de-announce services as described in mdns_deannounce_service().
 * Function removes given set of services from a list of services associated
 * with that interface.
 *
 * \param services array of pointer to \ref mdns_service structure corresponding
 * to services to be de-announced
 *
 * \param iface Pointer to interface handle on which service is to be
 * de-announced. Interface handle can be obtained from net_get_sta_handle or
 * net_get_uap_handle function calls
 *
 * \return kNoErr: success
 *
 * \return ERR_MDNS_INVAL: invalid parameters. Perhaps services pointer or
 * iface pointer is NULL
 *
 */
int mdns_deannounce_service_arr(struct mdns_service *services[], netif_t iface);

/* mdns_deannounce_service_all
 *
 * Deannounce all registered services on a given interface
 *
 * This function sends command on mDNS control socket to de-announce all
 * services, in a single mDNS packet, announced on "iface" interface. mDNS then
 * de-announce services as described in mdns_deannounce_service(). Function
 * removes all services from a list of services associated with that interface.
 *
 * \param iface Pointer to interfae handle on which service is to be
 * de-announced. Interface handle can be obtained from net_get_sta_handle or
 * net_get_uap_handle function calls.
 */
int mdns_deannounce_service_all(netif_t iface);

/** mdns_iface_state_change
 *
 * Send interface state change event to mdns
 *
 * This will start or stop mdns state machine for given interface. Before
 * calling this function for first time, interface and services should be added
 * to mDNS using mdns_announce_service() or mdns_announce_service_arr().
 *
 * For \ref UP state, mdns responder will initially send init probes for all
 * announced services to check for name conflict in services. If there are any
 * conflicts it will resolve them by appending integer after service name.
 * After checking name conflicts, it will announce services to multicast group.
 *
 * \ref DOWN state will send good bye packet corresponding to all announced
 * services on a given interface.
 * Note that taking an interface DOWN will not remove services from a list of
 * services associated with that interface. In order to do so, use
 * mdns_deannounce_service() or mdns_deannounce_service_arr() or
 * mdns_deannounce_service_all().
 *
 * \ref REANNOUNCE will forcefully reannounce services. \ref REANNOUNCE do not
 * send init probes and directly re-announces the services.
 * Generally REANNOUNCE should be used during when TXT records are updated.
 *
 * In the events like link-lost, DHCP change event, or change in host name or
 * the service name, application should repeat the initial probing sequence.
 * This can be achieved by calling this function with \ref DOWN state, followed
 * by calling it again with \ref UP state. This would help to resolve service
 * name conflicts if introduced by other devices. Also the previous service
 * entry in mDNS clients will be removed and new updated entry will be added.
 *
 * \param iface interface handle
 *
 * \param state valid interface state from \ref iface_state
 *
 * \return kNoErr for success or mdns error code
 *
 */
int mdns_iface_state_change(netif_t iface, enum iface_state state);

/** mdns_set_hostname
 *
 *  Set new host name, use mdns_iface_state_change(iface, REANNOUNCE) to anounce
 *  the new host name.
 *
 */
void mdns_set_hostname(char *hostname);

/** Set mDNS TXT Records
 *
 * This function sets the TXT record field for a given mDNS service.
 * mDNS TXT record string is in-place converted and stored in a format defined
 * by TXT resource record format.
 *
 * \note This function MUST be called before adding or announcing
 * the mDNS service. Otherwise the resultant behavior can be erroneous.
 *
 * \param s Pointer to mDNS service
 *
 * \param keyvals TXT record string consisting of one or more key value pairs,
 * which can be separated by a separator like '|', '.'. For example,
 * \code
 * char keyvals[] = "key1=value1|key2=value2|key3=value3"
 * \endcode
 * The separator value is also passed to this function as mentioned below.
 * keyvals string must persist between the calls to mdns_start and mdns_stop.
 * Hence define these variables in global memory
 *
 *
 * \param separator The separator used to separate individual key value pairs in
 * the above mentioned TXT record string. If TXT record contains separator, replace
 * with '/[separator]'. For example, if separator use '|', replace '|' with '/|' in
 * key value pair.
 *
 * \return kNoErr on success
 *
 * \return ERR_MDNS_TOOBIG if length of TXT record exceeds the permissible
 * limit as specified by \ref MDNS_MAX_KEYVAL_LEN
 *
 * \return ERR_MDNS_INVAL if value of TXT record string is invalid
 *
 */
int mdns_set_txt_rec(struct mdns_service *s, char *keyvals, char separator);

#if CONFIG_DNSSD_QUERY
/** dnssd_query_monitor
 *
 * Query for and monitor instances of a service published by unicast DNS server.
 *
 * When instances of the specified service are discovered, the specified
 * query callback is called as described above.
 *
 * \param fqst Pointer to a null-terminated string specifying the fully-
 * qualified service type.  For example, "_http._tcp.mxchiptest" would query
 * for all http servers in the ".mxchiptest" domain.
 *
 * \param cb an mdns_query_cb to be called when services matching the specified
 * fqst are discovered, are updated, or disappear.  Callback will be passed the
 * opaque data argument described below, a struct mdns_service that represents
 * the discovered service, and a status code.
 *
 * \param dns_addr DNS IP address in form of struct in_addr. One can get the IP
 * from DNS string using gethostbyname API.
 *
 * \param data a void * that will passed to cb when services are discovered,
 * are updated, or disappear.  This can be anything that the user wants, such
 * as pointer to a custom internal data structure.
 *
 * \return kNoErr: the query was successfully launched.  The caller should
 * expect the mdns_query_cb to be invoked as instances of the specified service
 * are discovered.
 *
 * \return ERR_MDNS_INVAL: cb was NULL or fqst was not valid.
 *
 * \return ERR_MDNS_NOMEM: CONFIG_MDNS_MAX_SERVICE_MONITORS is already being
 * monitored.  Either this value must be increased, or a service must be
 * unmonitored by calling mdns_query_unmonitor.
 *
 * \return ERR_MDNS_INUSE: The specified service type is already being
 * monitored by another callback, and multiple callbacks per service are not
 * supported.
 *
 * \return ERR_MDNS_NORESP: No response from the querier.  Perhaps it was not
 * launched or it has crashed.
 *
 * Note #1: Only single dnssd query monitor is allowed.
 * Note #2: Internally, state machines for DNS and mDNS queriers are same, hence
 * to save the footprint DNS and mDNS queriers are merged together.
 */
int dnssd_query_monitor(char *fqst, mdns_query_cb cb,
			struct in_addr dns_addr, void *data, netif_t iface);

/** dnssd_query_unmonitor
 *
 * Stop monitoring a particular DNS-SD service on unicast socket
 *
 * \param fqst The service type to stop monitoring, or NULL to unmonitor all
 * services.
 *
 * \note Suppose a service has just been discovered and is being processed
 * while the call to mdns_query_monitor is underway.  A callback may be
 * generated before the service is unmonitored.
 */
void dnssd_query_unmonitor(char *fqst);
#endif

/** mdns_cli_init
 *
 * Add mdns commands to mico CLI, shuld be excuted after cli_init or mico_system_init
 *
 * return: none
 */
void mdns_cli_init(void);

#endif
