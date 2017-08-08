/**
 ******************************************************************************
 * @file    mdns_opt.h
 * @author  William Xu
 * @version V1.0.0
 * @date    3-August-2017
 * @brief   This file provide mdns default configurations
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

#ifndef __MDNS_OPT_H
#define __MDNS_OPT_H

#include "mico_opt.h"

#ifdef __cplusplus
extern "C" {
#endif

/* If developer needs change these options, define them in mico_config.h */

/** Define this option to include debugging and logging.
 *  By default this option is disabled.
 */
#if !defined CONFIG_MDNS_DEBUG
#define CONFIG_MDNS_DEBUG                      MICO_DEBUG_OFF
#endif

/** Developers who require the ability to query for services.
  * This enables the mdns_query_* functions.  These functions
  * will return ERR_MDNS_NOIMPL if the mdns library was built without
  * CONFIG_MDNS_QUERY defined. By default this option is disabled.
  *
  * Enabling this option adds an overhead of about 16KB. This includes 
  * support for caching two mDNS-SD service entries.
  */
#if !defined CONFIG_MDNS_QUERY
#define CONFIG_MDNS_QUERY                      0
#endif

/** enables the support to query for and monitor instances of a 
  * service published by unicast DNS server.
  */
#if !defined CONFIG_DNSSD_QUERY
#define CONFIG_DNSSD_QUERY                      0
#endif

/** Maximum number of mDNS service instances that can be announced.
 */
#if !defined CONFIG_MDNS_MAX_SERVICE_ANNOUNCE
#define CONFIG_MDNS_MAX_SERVICE_ANNOUNCE       3
#endif

/**  Maximum number of service types that can be monitored
  * for example _http._tcp.local., _airplay._tcp.local. are service types
  * can be monitored in mdns query mode
  */
#if !defined CONFIG_MDNS_MAX_SERVICE_MONITORS
#define CONFIG_MDNS_MAX_SERVICE_MONITORS       1
#endif

/** Maximum number of service instances that can be monitored
 *
 * Suppose CONFIG_MDNS_SERVICE_CACHE_SIZE is 16 and that a user has invoked
 * mdns_query_monitor to monitor services of type _http._tcp.local. Assume
 * the query callback handler returns kNoErr for all the instances
 * discovered.
 *
 * Further,suppose that this particular domain has 17 instances of this type.
 * The first 16 instances to be discovered will result in 16 callbacks with the
 * status MDNS_DISCOVERED.  These instances will be cached and monitored for
 * updates, disappearance, etc.  When the 17th instance is discovered, the
 * callback will be called as usual, but the status will be MDNS_CACHE_FULL,
 * and the service will not be monitored.  While a best effort is made to
 * deliver all of the service information, the mdns_service may be incomplete.
 * Specifically, the ipaddr may be 0 and the service name may be "".  Further,
 * the callback may be called again if the 17th instance of the service
 * announces itself on the network again.  If one of the other services
 * disappears, the next announcement from the 17th instance will result in a
 * callback with status MDNS_DISCOVERED, and from that point forward it will be
 * monitored.
 *
 * So what's the "best" value for CONFIG_MDNS_SERVICE_CACHE_SIZE?  This depends
 * on the application and on the field in which the application is deployed.  If
 * a particular application knows that it will never see more than 6 instances
 * of a service, then 6 is a fine value for CONFIG_MDNS_SERVICE_CACHE_SIZE.  In
 * this case, callbacks with a status of MDNS_CACHE_FULL would represent a
 * warning or error condition.  Similarly, if an application cannot handle any
 * more than 10 instances of a service, then CONFIG_MDNS_SERVICE_CACHE_SIZE
 * should be 10 and callbacks with a status of MDNS_CACHE_FULL can be ignored.
 * If the maximum number of service instances is not known, and the application
 * retains its own state for each instance of a service, it may be able to use
 * that state to do the right thing when the callback status is MDNS_CACHE_FULL.
 *
 * For applications with constrained memory ,a point to note is that each
 * service instance requires little above 1K bytes. This should be considered
 * while deciding the CONFIG_MDNS_SERVICE_CACHE_SIZE.
 *
 * The default value of CONFIG_MDNS_SERVICE_CACHE_SIZE is set to 5.
 */
#if !defined CONFIG_MDNS_SERVICE_CACHE_SIZE
#define CONFIG_MDNS_SERVICE_CACHE_SIZE         5
#endif

/** This option enables the support for extended mDNS. Note that enabling
  * ex-mDNS would render querier and responder unresponsive to ".local" domain.
  *
  * Extended mDNS extends specification of mDNS to site-local scope in order to
  * support multi-hop LANs that forward multicast packets but do not provide
  * a unicast DNS service. Note that selecting Extended-mDNS requires top-level
  * domain name ".site", instead of mDNS' ".local" domain name. Both domains
  * are functionally disjoint, both from a name space and address space
  * perspective. Application could register to only one domain name i.e enabling
  * ex-mDNS would mean .local will not work for querier as well as responder.
  */
#if !defined CONFIG_XMDNS
#define CONFIG_XMDNS                           0
#endif




/* Convert global defines to mdns internal build options, Do Not Change! */
#if CONFIG_MDNS_QUERY
#define MDNS_QUERY_API
#endif

#if MICO_CONFIG_IPV6
#define CONFIG_IPV6
#endif

#ifdef __cplusplus
} /*extern "C" */
#endif

#endif //__MICO_OPT_H
