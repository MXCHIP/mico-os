/*
 *  Copyright (C) 2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include <stdint.h>
#include <mdns.h>
#include <mdns_port.h>

#include "mico.h"
#ifdef CONFIG_IPV6
#include <lwip/mld6.h>
#endif	/*	CONFIG_IPV6	*/

#include "mdns_opt.h"
#include "mdns_private.h"
#include "queue.h"

#if MDNS_QUERY_API

/* OVERVIEW
 *
 * The querier monitors any service instances on the network with the service
 * types specified by calls to mdns_query_monitor.  Each service instance has
 * at least 3 records of interest: a PTR which points to a SRV which points to
 * an A which conains an ipaddr.  The SRV may have an associated TXT record
 * that also needs to be tracked.  Information about the PTR/SRV/TXT records
 * are stored as a service_instance, and the service_instance contains a
 * reference to an A record.  We have this indirection because many services
 * may have the same A record.  Accordingly, each A record maintains a
 * cross-reference list of service_instances that refer to it.  This enables
 * updates to the A record to be passed on to the user without endless amounts
 * of searching.
 *
 * When we receive a message, we sort the answer records by type (see
 * mdns_parse_message).  This allows us to create and populate any new service
 * instances, update existing service instances, create new a records, update
 * existing A records, and notify the user without having to traverse the
 * entire list of services that we are monitoring over and over.  See
 * update_service_cache for details on the algorithm.
 */

/* global mdns state */
static void *query_thread;
static int ctrl_sock;
static int query_enabled;
static int mc_sock;
#ifdef CONFIG_IPV6
static int mc6_sock;
#else /* !CONFIG_IPV6 */
/* This will make sure that if CONFIG_IPV6 is not enabled,
 * mdns_send_msg won't send IPv6 packets. */
static int mc6_sock = -1;
#endif /* CONFIG_IPV6 */
#ifdef CONFIG_DNSSD_QUERY
static int uc_sock;
#endif
static struct mdns_message tx_msg;
static struct mdns_message rx_msg;

static struct mdns_question rx_questions[MDNS_MAX_QUESTIONS];
static struct mdns_resource rx_answers[MDNS_MAX_ANSWERS];
static struct mdns_resource rx_authorities[MDNS_MAX_AUTHORITIES];

/* Keeping the minimum size of Tx questions array as four. Array size would
 * grow with number of mDNS services to be queried.
 */
static struct mdns_question tx_questions[4 + CONFIG_MDNS_MAX_SERVICE_MONITORS];
static struct mdns_resource \
	tx_answers[3 * CONFIG_MDNS_SERVICE_CACHE_SIZE];
static struct mdns_resource \
	tx_authorities[3 * CONFIG_MDNS_SERVICE_CACHE_SIZE];

enum arec_state {
	AREC_STATE_INIT = 0,
	AREC_STATE_QUERYING,
	AREC_STATE_RESOLVED,
};

enum arec_event {
	AREC_EVENT_RX_REC = 0,
	AREC_EVENT_ADD_QUESTIONS,
};

struct service_instance;

static struct smons_list smons_active;
static struct smons_list smons_free;
static struct service_monitor smons[CONFIG_MDNS_MAX_SERVICE_MONITORS];

static struct mdns_service_config config_g[MDNS_MAX_SERVICE_CONFIG];
static struct sinst_list sinsts_free;
static struct service_instance sinsts[CONFIG_MDNS_SERVICE_CACHE_SIZE];

struct arec {
	SLIST_ENTRY(arec) list_item;
	struct sinst_list sinsts;
	uint32_t ipaddr;
#ifdef CONFIG_DNSSD_QUERY
	bool is_unicast;
	struct in_addr dns_addr;
#endif
	uint32_t ttl;
	uint8_t fqdn[MDNS_MAX_NAME_LEN + 1];
	enum arec_state state;
	uint32_t next_refresh;
	int ttl_percent;
};

SLIST_HEAD(arecs_list, arec);
static struct arecs_list arecs_active;
static struct arecs_list arecs_free;
static struct arec arecs[CONFIG_MDNS_SERVICE_CACHE_SIZE];

#ifdef CONFIG_IPV6
enum aaaa_rec_state {
	AAAA_REC_STATE_INIT = 0,
	AAAA_REC_STATE_QUERYING,
	AAAA_REC_STATE_RESOLVED,
};

enum aaaa_rec_event {
	AAAA_REC_EVENT_RX_REC = 0,
	AAAA_REC_EVENT_ADD_QUESTIONS,
};

struct aaaa_rec {
	SLIST_ENTRY(aaaa_rec) list_item;
	struct sinst_list sinsts;
	uint32_t ipaddr[4];
#ifdef CONFIG_DNSSD_QUERY
	bool is_unicast;
	struct in_addr dns_addr;
#endif
	uint32_t ttl;
	uint8_t fqdn[MDNS_MAX_NAME_LEN + 1];
	enum aaaa_rec_state state;
	uint32_t next_refresh;
	int ttl_percent;
};

SLIST_HEAD(aaaa_recs_list, aaaa_rec);
static struct aaaa_recs_list aaaa_recs_active;
static struct aaaa_recs_list aaaa_recs_free;
static struct aaaa_rec aaaa_recs[CONFIG_MDNS_SERVICE_CACHE_SIZE];
#endif /* CONFIG_IPV6 */

#define CTRL_MSG_MIN (sizeof(query_ctrl_msg) - \
					  sizeof(union query_ctrl_data))


#define SUBTRACT(a, b) ((a) > (b) ? (a) - (b) : 0)
#define ADD(a, b) ((a) > (UINT32_MAX - (b)) ? UINT32_MAX : (a) + (b))
#define CONVERT_TTL(ttl) ((ttl) > UINT32_MAX/1000 ? UINT32_MAX : (ttl) * 1000)
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

/* perform callback for service instance s and status code c */
static int do_callback(struct service_instance *s, int c)
{
	int ret;
	ret = (s->smon->cb)(s->smon->cbdata, &s->service, c);
	if (ret != kNoErr) {
		LOG("Warning: callback returned failure.\r\n");
	}
	return ret;
}


/* Send a control message msg to the server listening at localhost:port.
 * Expect an int response code from the server.
 */
int query_send_ctrl_msg(query_ctrl_msg *msg, uint16_t port)
{
    int ret;
    mico_queue_t *ctrl_queue;

    ret = mdns_socket_loopback(port, &ctrl_queue);
    if (ret == -1) {
        return ret;
    }
    ret = mico_rtos_push_to_queue(ctrl_queue, msg, 0);
    return ret;


#if 0
	int ret;
	struct sockaddr_in to;
	int s;
	fd_set fds;
	struct timeval t;
	int status;
	struct sockaddr_in from;
	socklen_t size = sizeof(struct sockaddr_in);

	s = mdns_socket_loopback(port), NULL);
	if (s < 0) {
		LOG("error: failed to create loopback socket\r\n");
		return -1;
	}

	memset((char *)&to, 0, sizeof(to));
	to.sin_family = PF_INET;
	to.sin_port = htons(port);
	to.sin_addr.s_addr = inet_addr("127.0.0.1");

	ret =
	    sendto(s, (char *)msg, msg->length, 0, (struct sockaddr *)&to,
		   sizeof(to));
	if (ret < 0) {
		//LOG("error: failed to send control message: %d\r\n", net_get_sock_error(s));
	    LOG("error: failed to send control message\r\n");
		ret = ERR_MDNS_NORESP;
		goto done;
	}

	FD_ZERO(&fds);
	FD_SET(s, &fds);
	SET_TIMEOUT(&t, 2000);
	ret = select(s + 1, &fds, NULL, NULL, &t);
	if (ret == -1) {
		LOG("error: select failed for control socket: %d\r\n", errno);
		ret = ERR_MDNS_NORESP;
		goto done;
	}

	if (!FD_ISSET(s, &fds)) {
		LOG("error: no control response received.\r\n");
		ret = ERR_MDNS_NORESP;
		goto done;
	}

	ret =
	    recvfrom(s, (char *)&status, sizeof(status), 0,
		     (struct sockaddr *)&from, &size);
	if (ret == -1) {
		LOG("error: failed to recv response to control message: %d\r\n",
		    errno);
		ret = ERR_MDNS_NORESP;
		goto done;
	}
	ret = status;
done:
	/* we do not check status of mdns_socket_close() */
	/*   assuming it will never fail */
	mdns_socket_close(&s);
	return ret;
#endif
}

/* find the service monitor for the specified fqst, or NULL if we're not
 * monitoring that service.  Note that the message m can be NULL if the fqst is
 * known to not contain pointers
 */
static struct service_monitor *find_service_monitor(struct mdns_message *m,
						    uint8_t * fqst)
{
	struct service_monitor *found = NULL;

	SLIST_FOREACH(found, &smons_active, list_item) {
		if (dname_cmp(NULL, found->fqst, m->data, fqst) == 0)
			return found;
	}
	return NULL;
}

/* find the service monitor and service instance for the specified fqst, or NULL
 * if we're not monitoring that service. Note that the message m can be NULL if
 * the fqst is known to not contain pointers.
 */

static struct service_monitor *find_smon_and_sinst(
	struct mdns_message *m, uint8_t *fqst, struct service_instance **sinst)
{
	struct service_monitor *smon_found = NULL;
	struct service_instance *sinst_found = NULL;

	SLIST_FOREACH(smon_found, &smons_active, list_item) {
		SLIST_FOREACH(sinst_found, &smon_found->sinsts, list_item) {
			if (dname_cmp(NULL, sinst_found->service.fqsn, m->data,
				      fqst) == 0) {
				*sinst = sinst_found;
				return smon_found;
			}
		}
	}
	*sinst = NULL;
	return NULL;
}

/* add a service monitor to our list of monitored services, or fail
 * appropriately
 */
static int add_service(struct service_monitor *smon)
{
	struct service_monitor *found = NULL;

	DBG("Adding service ");
	debug_print_name(NULL, smon->fqst);
	DBG(" to monitor list\r\n");

	/* ensure that we're not already monitoring this service */
	found = find_service_monitor(NULL, smon->fqst);
	if (found != NULL) {
		DBG("error: already monitoring this service\r\n");
		return ERR_MDNS_INUSE;
	}

	found = SLIST_FIRST(&smons_free);
	if (found == NULL) {
		DBG("error: no more service monitor slots available.\r\n");
		return ERR_MDNS_NOMEM;
	}
	SLIST_REMOVE_HEAD(&smons_free, list_item);

	memcpy(found, smon, sizeof(struct service_monitor));
	SLIST_INIT(&found->sinsts);
	found->refresh_period = 1000;	/* start the refresh period at 1s. */
	found->next_refresh = 0;
	SLIST_INSERT_HEAD(&smons_active, found, list_item);

	return kNoErr;
}

/* cleanup the sinst and put it back on the free list.  Caller is responsible
 * for notifying user if necessary.  Note that this function fixes up the arec
 * list too.
 */
static void cleanup_sinst(struct service_instance *sinst)
{
	SLIST_REMOVE(&sinst->smon->sinsts, sinst, service_instance, list_item);
	if (sinst->arec) {
		SLIST_REMOVE(&sinst->arec->sinsts, sinst, service_instance,
			     alist_item);
		if (SLIST_EMPTY(&sinst->arec->sinsts)) {
			SLIST_REMOVE(&arecs_active, sinst->arec, arec,
				     list_item);
			SLIST_INSERT_HEAD(&arecs_free, sinst->arec, list_item);
		}
	}
#ifdef CONFIG_IPV6
	if (sinst->aaaa_rec) {
		SLIST_REMOVE(&sinst->aaaa_rec->sinsts, sinst, service_instance,
			     aaaa_list_item);
		if (SLIST_EMPTY(&sinst->aaaa_rec->sinsts)) {
			SLIST_REMOVE(&aaaa_recs_active, sinst->aaaa_rec,
					aaaa_rec, list_item);
			SLIST_INSERT_HEAD(&aaaa_recs_free, sinst->aaaa_rec,
					list_item);
		}
	}
#endif /* CONFIG_IPV6 */
	SLIST_INSERT_HEAD(&sinsts_free, sinst, list_item);
	/* just in case any of our callers wants to know when to refresh this dead
	 * record, make the refresh time never.
	 */
	sinst->next_refresh = UINT32_MAX;
}

/* cleanup a service.  Caller is responsible for removing it from the active
 * list and moving it to the free list
 */
static void cleanup_service(struct service_monitor *smon)
{
	struct service_instance *sinst, *stmp;

	SLIST_FOREACH_SAFE(sinst, &smon->sinsts, list_item, stmp)
	    cleanup_sinst(sinst);
}

/* remove the service fqst from the monitor list.  If fqst is an empty string,
 * we unmonitor all services.
 */
static void remove_service(uint8_t * fqst)
{
	struct service_monitor *found;

	if (fqst[0] == 0) {
		DBG("Removing all services from monitor list:\r\n");
		while (!SLIST_EMPTY(&smons_active)) {
			found = SLIST_FIRST(&smons_active);
			SLIST_REMOVE_HEAD(&smons_active, list_item);
			DBG("\t");
			debug_print_name(NULL, found->fqst);
			DBG("\r\n");
			cleanup_service(found);
			SLIST_INSERT_HEAD(&smons_free, found, list_item);
		}
		return;
	}

	DBG("Removing service ");
	debug_print_name(NULL, fqst);
	DBG(" from monitor list\r\n");

	/* find the service of interest */
	found = find_service_monitor(NULL, fqst);
	if (found == NULL) {
		DBG("Warning: service was not being monitored\r\n");
		return;
	}
	SLIST_REMOVE(&smons_active, found, service_monitor, list_item);
	cleanup_service(found);
	SLIST_INSERT_HEAD(&smons_free, found, list_item);
}

/* find the service instance with the specified fqsn in the list of a
 * particular service monitor.  Return the service instance, or NULL if it was
 * not found.
 */
static struct service_instance *find_service_instance(struct service_monitor
						      *smon,
						      struct mdns_message *m,
						      uint8_t * fqsn)
{
	struct service_instance *found = NULL;

	SLIST_FOREACH(found, &smon->sinsts, list_item) {
		if (dname_cmp(NULL, found->service.fqsn, m->data, fqsn) == 0)
			return found;
	}
	return NULL;
}

static void reset_service_instance(struct service_instance *sinst)
{
	memset(sinst, 0, sizeof(struct service_instance));
	sinst->service.servname = sinst->sname;
	sinst->service.servtype = sinst->stype;
	sinst->service.domain = sinst->domain;

	/* start the ttl off at 1s.  If we actully have a suitable SRV in the
	 * current message, we'll update it.  Otherwise, we'll be all set up for
	 * an immediate refresh attempt.
	 */
	sinst->srv_ttl0 = 1000;
	sinst->srv_ttl = 1000;
	sinst->next_refresh = 0;
	sinst->ttl_percent = 20;
}

/* populate the service s with the bits and pieces in the fqsn from message
 * m.  Return 0 on success or -1 on failure.
 */
static int copy_servinfo(struct mdns_service *s, struct mdns_message *m,
			 uint8_t * fqsn)
{
	int ret;

	ret = dname_copy(s->fqsn, m->data, fqsn);
	if (ret == -1)
		return ret;

	fqsn = dname_label_to_c((char *)s->servname, m->data, fqsn, 1);
	if (fqsn == NULL)
		return -1;

	fqsn = dname_label_to_c((char *)s->servtype, m->data, fqsn, 0);
	if (fqsn == NULL)
		return -1;

	if (dname_label_cmp(NULL, (uint8_t *) "\4_tcp", m->data, fqsn) == 0)
		s->proto = MDNS_PROTO_TCP;
	else if (dname_label_cmp(NULL, (uint8_t *) "\4_udp", m->data, fqsn) == 0)
		s->proto = MDNS_PROTO_UDP;
	else {
		return -1;
	}

	fqsn = dname_label_next(m->data, fqsn);
	if (fqsn == NULL)
		return -1;

	/* NOTE: this means that our domain must be a single label like .local! */
	fqsn = dname_label_to_c((char *)s->domain, m->data, fqsn, 0);
	if (fqsn == NULL)
		return -1;

	return 0;
}

/* the TXT record r goes with the service instance sinst.  Update sinst if
 * necessary and set the dirty bit.
 */
static int update_txt(struct service_instance *sinst, struct mdns_resource *r)
{
	int len, ret = 0, rlen;

	len = r->rdlength < sinst->rawkvlen ? r->rdlength : sinst->rawkvlen;
	rlen = r->rdlength;

	/* is this an update? */
	if (sinst->service.keyvals == NULL ||
	    (rlen != sinst->rawkvlen && rlen < MDNS_MAX_KEYVAL_LEN + 1) ||
	    memcmp(r->rdata, sinst->rawkeyvals, len) != 0) {
		len = r->rdlength < MDNS_MAX_KEYVAL_LEN + 1 ?
		    r->rdlength : MDNS_MAX_KEYVAL_LEN + 1;
		memcpy(sinst->rawkeyvals, r->rdata, len);
		sinst->rawkvlen = len;
		sinst->service.keyvals = sinst->keyvals;
		txt_to_c_ncpy(sinst->keyvals, sizeof(sinst->keyvals),
			      r->rdata, r->rdlength);
		ret = 1;
	}

	return ret;
}

/* find the a record with the specified fqdn in the list of a active a records.
 * Return the arec, or NULL if it was not found.
 */
static struct arec *find_arec(struct mdns_message *m, uint8_t * fqdn)
{
	struct arec *found = NULL;

	SLIST_FOREACH(found, &arecs_active, list_item) {
		if (dname_cmp(NULL, found->fqdn, m->data, fqdn) == 0)
			return found;
	}
	return NULL;
}

#ifdef CONFIG_IPV6
/* find the aaaa record with the specified fqdn in the list of active aaaa
 * records. Return the aaaa_rec, or NULL if it was not found.
 */
static struct aaaa_rec *find_aaaa_rec(struct mdns_message *m, uint8_t * fqdn)
{
	struct aaaa_rec *found = NULL;

	SLIST_FOREACH(found, &aaaa_recs_active, list_item) {
		if (dname_cmp(NULL, found->fqdn, m->data, fqdn) == 0)
			return found;
	}
	return NULL;
}
#endif /* CONFIG_IPV6 */

/* set the sinst's arec to arec, and deal with any list coherency.  Return 0 if
 * we didn't have to update anything, or 1 otherwise.
 */
static int set_arec(struct service_instance *sinst, struct arec *arec)
{
	int ret = 0;

	if (sinst->arec != NULL) {
		/* We have an existing arec.  Take us off it's list. */
		SLIST_REMOVE(&sinst->arec->sinsts, sinst, service_instance,
			     alist_item);
		if (sinst->arec != arec)
			ret = 1;
	}

	if (arec->state == AREC_STATE_RESOLVED) {
		if (sinst->service.ipaddr != arec->ipaddr) {
			sinst->service.ipaddr = arec->ipaddr;
			ret = 1;
		}
		sinst->service.flags |= SERVICE_HAS_A_FLAG;
	} else {
		sinst->service.flags &= ~SERVICE_HAS_A_FLAG;
	}

	/* We have an arec, and we're not on any other arec's list */
	SLIST_INSERT_HEAD(&arec->sinsts, sinst, alist_item);
	sinst->arec = arec;
#ifdef CONFIG_DNSSD_QUERY
	arec->is_unicast = sinst->smon->is_unicast;
	arec->dns_addr = sinst->smon->dns_addr;
#endif
	return ret;
}

#ifdef CONFIG_IPV6
/* set the sinst's aaaa_rec to aaaa_rec, and deal with any list coherency.
 * Return 0 if we didn't have to update anything, or 1 otherwise.
 */
static int set_aaaa_rec(struct service_instance *sinst, struct aaaa_rec
		*aaaa_rec)
{
	int ret = 0;

	if (sinst->aaaa_rec != NULL) {
		/* We have an existing aaaa_rec.  Take us off it's list. */
		SLIST_REMOVE(&sinst->aaaa_rec->sinsts, sinst, service_instance,
			     aaaa_list_item);
		if (sinst->aaaa_rec != aaaa_rec)
			ret = 1;
	}

	if (aaaa_rec->state == AAAA_REC_STATE_RESOLVED) {
		if (memcmp(sinst->service.ip6addr, aaaa_rec->ipaddr,
					sizeof(aaaa_rec->ipaddr))) {
			memcpy(sinst->service.ip6addr, aaaa_rec->ipaddr,
					sizeof(aaaa_rec->ipaddr));
			ret = 1;
		}

		sinst->service.flags |= SERVICE_HAS_AAAA_FLAG;
	} else {
		sinst->service.flags &= ~SERVICE_HAS_AAAA_FLAG;
	}

	/* We have an aaaa_rec, and we're not on any other aaaa_rec's list */
	SLIST_INSERT_HEAD(&aaaa_rec->sinsts, sinst, aaaa_list_item);
	sinst->aaaa_rec = aaaa_rec;
#ifdef CONFIG_DNSSD_QUERY
	aaaa_rec->is_unicast = sinst->smon->is_unicast;
	aaaa_rec->dns_addr = sinst->smon->dns_addr;
#endif
	return ret;
}
#endif /* CONFIG_IPV6 */

/* the SRV record r from the mdns_message m goes with the service instance
 * sinst.  Update sinst if necessary.  Also, start an A/AAAA record if there
 * isn't already one.  Return 1 if we updated the sinst.
 */
static int update_srv(struct service_instance *sinst, struct mdns_message *m,
		      struct mdns_resource *r, uint32_t elapsed)
{
	struct rr_srv *srv = (struct rr_srv *)r->rdata;
	struct mdns_service *s = &sinst->service;
	struct mdns_resource *a;
	struct arec *arec;
#ifdef CONFIG_IPV6
	struct aaaa_rec *aaaa_rec;
#endif /* CONFIG_IPV6 */
	int ret = 0;

	if (r->ttl == 0) {
		DBG("Got SRV goodbye for ");
		debug_print_name(NULL, sinst->service.fqsn);
		DBG(".\r\n");
		cleanup_sinst(sinst);
		return -1;
	}

	/* we found a service record for this sinst. */
	if (s->port != srv->port) {
		s->port = srv->port;
		ret = 1;
	}
	/* Now we have to make sure our service instance has an a record */
	SLIST_FOREACH(a, &m->as, list_item) {
		arec = find_arec(m, srv->target);
		if (arec == NULL) {
			/* there's no a record for this service.  Get one. */
			arec = SLIST_FIRST(&arecs_free);
			if (arec == NULL) {
				/* the arec cache is full.  Let the user deal
				 * with it */
				sinst->arec = NULL;
				goto done;
			}

			SLIST_REMOVE_HEAD(&arecs_free, list_item);
			memset(arec, 0, sizeof(struct arec));
			SLIST_INSERT_HEAD(&arecs_active, arec, list_item);
			dname_copy(arec->fqdn, m->data, srv->target);
		}
		ret += set_arec(sinst, arec);
	}
#ifdef CONFIG_IPV6
	/* Now we have to make sure our service instance has an aaaa record */
	SLIST_FOREACH(a, &m->aaaas, list_item) {
		aaaa_rec = find_aaaa_rec(m, srv->target);
		if (aaaa_rec == NULL) {
			/* there's no aaaa record for this service.  Get one. */
			aaaa_rec = SLIST_FIRST(&aaaa_recs_free);
			if (aaaa_rec == NULL) {
				/* the aaaa_rec cache is full.  Let the user
				 * deal with it */
				sinst->aaaa_rec = NULL;
				goto done;
			}

			SLIST_REMOVE_HEAD(&aaaa_recs_free, list_item);
			memset(aaaa_rec, 0, sizeof(struct aaaa_rec));
			SLIST_INSERT_HEAD(&aaaa_recs_active, aaaa_rec,
					list_item);
			dname_copy(aaaa_rec->fqdn, m->data, srv->target);
		}
		ret += set_aaaa_rec(sinst, aaaa_rec);
	}
#endif /* CONFIG_IPV6*/
done:
	/* finally, update the ttl and flags.  We augment the ttl by the elapsed
	 * time so we can do a wholesale inspection and update later.
	 */
	sinst->srv_ttl = ADD(CONVERT_TTL(r->ttl), elapsed);
	sinst->srv_ttl0 = CONVERT_TTL(r->ttl);
	sinst->ttl_percent = 20;
	sinst->next_refresh = ADD(sinst->srv_ttl0 * 80 / 100, elapsed);
	sinst->service.flags |= SERVICE_HAS_SRV_FLAG;
	return ret > 0 ? 1 : 0;
}

/* apply the elapsed time to the service instance.  If it's time to refresh,
 * add a suitable question to the message m and return 1.  If it's time to give
 * up on this record, return -1.  If there's nothing to do at this time, return
 * 0.
 */
static int apply_elapsed(struct service_instance *sinst, uint32_t elapsed,
			 struct mdns_message *m)
{
	/* start by updating all of the ttls */
	sinst->ptr_ttl = SUBTRACT(sinst->ptr_ttl, elapsed);
	sinst->srv_ttl = SUBTRACT(sinst->srv_ttl, elapsed);
	sinst->next_refresh = SUBTRACT(sinst->next_refresh, elapsed);

	if (sinst->next_refresh == 0) {
		/* time to refresh */
		if (sinst->ttl_percent < 5) {
			DBG("Timed out resolving SRV record for ");
			debug_print_name(NULL, sinst->service.fqsn);
			DBG(".  Evicting.\r\n");
			cleanup_sinst(sinst);
			return -1;
		}

		if (sinst->ttl_percent == 20) {
			/* this is the first refresh attempt.  Send it, and schedule the
			 * next one for 80% of the ttl.
			 */
			sinst->next_refresh = sinst->srv_ttl0 * 80 / 100;
		} else {
			sinst->next_refresh =
			    sinst->srv_ttl0 * sinst->ttl_percent / 100;
		}
		DBG("Refreshing SRV record for ");
		debug_print_name(NULL, sinst->service.fqsn);
		DBG(".\r\n");
		sinst->ttl_percent >>= 1;
		if (mdns_add_question(m, sinst->service.fqsn, T_ANY, C_IN) != 0) {
			LOG("Warning: failed to add query for SRV record.\r\n");
			return 0;
		} else {
			return 1;
		}
	}
	return 0;
}

/* march the sinst through its state machine subject to the event.  The sinst
 * must at least be initialized with a fqsn.  The arguments are variously
 * required depending on the event.
 */
#ifdef CONFIG_IPV6
static int update_sinst(struct service_instance *sinst, enum sinst_event e,
			struct mdns_message *m, struct arec *arec,
			struct aaaa_rec *aaaa_rec,
			struct mdns_resource *srvrr,
			struct mdns_resource *txt, uint32_t elapsed)
#else /* !CONFIG_IPV6 */
static int update_sinst(struct service_instance *sinst, enum sinst_event e,
			struct mdns_message *m, struct arec *arec,
			struct mdns_resource *srvrr,
			struct mdns_resource *txt, uint32_t elapsed)
#endif /* CONFIG_IPV6 */
{
	struct mdns_service *s = &sinst->service;
	int changes = 0, ret;

	/* apply state-independent updates */
	if (e == SINST_EVENT_GOT_SRV)
		changes = update_srv(sinst, m, srvrr, elapsed);

	else if (e == SINST_EVENT_GOT_TXT)
		changes = update_txt(sinst, txt);

	else if (e == SINST_EVENT_GOT_AREC) {
		if (arec != NULL)
			changes = set_arec(sinst, arec);
	}
#ifdef CONFIG_IPV6
	else if (e == SINST_EVENT_GOT_AAAA_REC) {
		if (aaaa_rec != NULL)
			changes = set_aaaa_rec(sinst, aaaa_rec);
	}
#endif /* CONFIG_IPV6 */

	/* now decide if we change state */
	switch (sinst->state) {
	case SINST_STATE_INIT:
		if ((e == SINST_EVENT_LOST_AREC)
#ifdef CONFIG_IPV6
				|| (e == SINST_EVENT_LOST_AAAA_REC)
#endif
		) {
			/* if we lose a/aaaa record, we just abandon ship.  We
			 * haven't alerted the user yet, so no need to send a
			 * DISAPPEAR message.
			 */
			cleanup_sinst(sinst);
			break;
		}

		if (e == SINST_EVENT_ADD_QUESTIONS) {
			ret = apply_elapsed(sinst, elapsed, m);
			changes = ret == -1 ? 0 : ret;
			break;
		}

		if (changes <= 0)
			break;

		if (SERVICE_IS_READY(s)) {
			ret = do_callback(sinst, MDNS_DISCOVERED);
			/* The application does not seem to be interested in
			   this service if ret != 0 */
			if (ret) {
				cleanup_sinst(sinst);
			} else {
				sinst->state = SINST_STATE_CLEAN;
			}
		}
		break;

	case SINST_STATE_CLEAN:
		if ((e == SINST_EVENT_LOST_AREC)
#ifdef CONFIG_IPV6
				|| (e == SINST_EVENT_LOST_AAAA_REC)
#endif
		) {
			cleanup_sinst(sinst);
			do_callback(sinst, MDNS_DISAPPEARED);
			break;
		}

		if (e == SINST_EVENT_ADD_QUESTIONS) {
			ret = apply_elapsed(sinst, elapsed, m);
			changes = 0;
			if (ret == -1)
				do_callback(sinst, MDNS_DISAPPEARED);
			else if (ret == 1) {
				s->flags &= ~SERVICE_HAS_SRV_FLAG;
				sinst->state = SINST_STATE_UPDATING;
				changes = 1;
			}
			break;
		}

		if (changes == 0)
			break;

		if (changes == -1) {
			do_callback(sinst, MDNS_DISAPPEARED);
			break;
		}

		if (SERVICE_IS_READY(s)) {
			ret = do_callback(sinst, MDNS_UPDATED);
			/* The application does not seem to be interested in
			   this service if ret != 0 */
			if (ret) {
				cleanup_sinst(sinst);
			}
		} else {
			sinst->state = SINST_STATE_UPDATING;
		}
		break;

	case SINST_STATE_UPDATING:
		if ((e == SINST_EVENT_LOST_AREC)
#ifdef CONFIG_IPV6
				|| (e == SINST_EVENT_LOST_AAAA_REC)
#endif
		) {
			cleanup_sinst(sinst);
			do_callback(sinst, MDNS_DISAPPEARED);
			break;
		}

		if (e == SINST_EVENT_ADD_QUESTIONS) {
			ret = apply_elapsed(sinst, elapsed, m);
			changes = ret == -1 ? 0 : ret;
			if (ret == -1)
				do_callback(sinst, MDNS_DISAPPEARED);
			break;
		}

		if (changes == 0)
			break;

		if (changes == -1) {
			do_callback(sinst, MDNS_DISAPPEARED);
			break;
		}

		if (SERVICE_IS_READY(s)) {
			ret = do_callback(sinst, MDNS_UPDATED);
			/* The application does not seem to be interested in
			   this service if ret != 0 */
			if (ret) {
				cleanup_sinst(sinst);
			} else {
				sinst->state = SINST_STATE_CLEAN;
			}
		}
		break;
	}
	return changes > 0 ? 1 : 0;
}

#ifdef CONFIG_IPV6
/* This function is used to get reverse dot notation ipv6 address.
 * For SEP test suite, it is not required as of now.
 * TODO: May need to implment it for other querier applications. */
static void get_rev_ipv6(void *src, void *dst)
{
	memcpy(dst, src, sizeof(uint32_t) * 4);
}
#endif /* CONFIG_IPV6 */

/* set the ip address of the a/aaaa record and alert associated sinsts if
 * necessary.
 */
static void set_ip(struct arec *arec, struct mdns_resource *a)
{
	uint32_t ipaddr = get_uint32_t(a->rdata);
	struct service_instance *sinst, *sinst_tmp;

	if (arec->ipaddr == ipaddr)
		goto done;

	arec->ipaddr = ipaddr;
	SLIST_FOREACH_SAFE(sinst, &arec->sinsts, alist_item, sinst_tmp)
#ifdef CONFIG_IPV6
	    update_sinst(sinst, SINST_EVENT_GOT_AREC, NULL, arec, NULL, NULL,
			    NULL, 0);
#else /* !CONFIG_IPV6 */
	    update_sinst(sinst, SINST_EVENT_GOT_AREC, NULL, arec, NULL, NULL,
			 0);
#endif /* CONFIG_IPV6 */

done:
	arec->ttl = CONVERT_TTL(a->ttl);
	/* next refresh is when 20% of the ttl remains */
	arec->next_refresh = arec->ttl * 80 / 100;
	arec->ttl_percent = 10;
}

#ifdef CONFIG_IPV6
static void set_ip_v6(struct aaaa_rec *aaaa_rec, struct mdns_resource *a)
{
	uint32_t ipaddr[4];
	struct service_instance *sinst, *sinst_tmp;

	get_rev_ipv6(a->rdata, ipaddr);

	if (!memcmp(aaaa_rec->ipaddr, ipaddr, sizeof(ipaddr)))
		goto done;

	memcpy(aaaa_rec->ipaddr, ipaddr, sizeof(ipaddr));
	SLIST_FOREACH_SAFE(sinst, &aaaa_rec->sinsts, aaaa_list_item, sinst_tmp)
	    update_sinst(sinst, SINST_EVENT_GOT_AAAA_REC, NULL, NULL, aaaa_rec,
			    NULL, NULL, 0);
done:
	aaaa_rec->ttl = CONVERT_TTL(a->ttl);
	/* next refresh is when 20% of the ttl remains */
	aaaa_rec->next_refresh = aaaa_rec->ttl * 80 / 100;
	aaaa_rec->ttl_percent = 10;
}
#endif /* CONFIG_IPV6 */
/* we've tried and tried to get the A record but we can't.  Evict it from the
 * cache along with any sinsts associated with it.
 */
static void evict_arec(struct arec *arec)
{
	struct service_instance *sinst;

	SLIST_FOREACH(sinst, &arec->sinsts, alist_item)
#ifdef CONFIG_IPV6
	    update_sinst(sinst, SINST_EVENT_LOST_AREC, NULL, NULL, NULL, NULL,
			    NULL, 0);
#else
	    update_sinst(sinst, SINST_EVENT_LOST_AREC, NULL, NULL, NULL,
			    NULL, 0);
#endif

	/* update_slist should remove the sinst from the arec's list.  And, when
	 * the sinst list is empty, it should move the arec to the free list.
	 * Accordingly, the arec's sinsts list should be empty.
	 */
	ASSERT(SLIST_EMPTY(&arec->sinsts));

	/* just in case any of our callers wants to know when to refresh this dead
	 * record, make the refresh time never.
	 */
	arec->next_refresh = UINT32_MAX;
}

#ifdef CONFIG_IPV6
static void evict_aaaa_rec(struct aaaa_rec *aaaa_rec)
{
	struct service_instance *sinst;

	SLIST_FOREACH(sinst, &aaaa_rec->sinsts, aaaa_list_item)
	    update_sinst(sinst, SINST_EVENT_LOST_AAAA_REC, NULL, NULL, NULL,
			    NULL, NULL, 0);

	/* update_slist should remove the sinst from the aaaa_rec's list.  And,
	 * when the sinst list is empty, it should move the aaaa_rec to the free
	 * list. Accordingly, the aaaa_rec's sinsts list should be empty.
	 */
	ASSERT(SLIST_EMPTY(&aaaa_rec->sinsts));

	/* just in case any of our callers wants to know when to refresh this
	 * dead record, make the refresh time never.
	 */
	aaaa_rec->next_refresh = UINT32_MAX;
}
#endif /* CONFIG_IPV6 */

/* This is the arec state machine.  Update the A record considering the event
 * that just happened.  The other arguments may or may not be valid depending
 * on the event.  Alert any associated sinsts if necessary.  Return 1 if we
 * need to send a query, otherwise return 0.
 */
static int update_arec(struct arec *arec, enum arec_event e,
		       struct mdns_message *m, struct mdns_resource *a,
		       uint32_t elapsed)
{
	int ret = 0;

	if (e == AREC_EVENT_RX_REC && a->ttl == 0) {
		DBG("Got A record goodbye for ");
		debug_print_name(NULL, arec->fqdn);
		DBG(".\r\n");
		evict_arec(arec);
		return 0;
	}

	switch (arec->state) {
	case AREC_STATE_INIT:
		if (e == AREC_EVENT_RX_REC) {
			DBG("Immediately resolved A record for ");
			debug_print_name(NULL, arec->fqdn);
			DBG("\r\n");
			arec->state = AREC_STATE_RESOLVED;
			set_ip(arec, a);

		} else if (e == AREC_EVENT_ADD_QUESTIONS) {
			/* time to send the first query for this arec */
			DBG("Launching A record query for ");
			debug_print_name(NULL, arec->fqdn);
			DBG("\r\n");
			if (mdns_add_question(m, arec->fqdn, T_A, C_IN) != 0) {
				LOG("Warning: failed to add query for A record.\r\n");
			}
			ret = 1;
			arec->state = AREC_STATE_QUERYING;
			/* try to get the A record approximately once per second for 3
			 * seconds until we get it, or fail.  Use the same algorithm as the
			 * steady-state refresh period to simplify the code.  That is,
			 * refresh when 20%, 10%, and 5% of the ttl remains.
			 */
			arec->ttl = 1000;
			arec->next_refresh = arec->ttl * 80 / 100;
			arec->ttl_percent = 10;
		}
		break;

	case AREC_STATE_QUERYING:
		if (e == AREC_EVENT_RX_REC) {
			DBG("Resolved A record for ");
			debug_print_name(NULL, arec->fqdn);
			DBG("\r\n");
			arec->state = AREC_STATE_RESOLVED;
			set_ip(arec, a);
			DBG("Next update in %ld ms.\r\n", arec->next_refresh);

		} else if (e == AREC_EVENT_ADD_QUESTIONS) {
			/* still no response.  It's been "elapsed" ms since our last
			 * invocation.  Either try again or give up
			 */
			arec->next_refresh =
			    SUBTRACT(arec->next_refresh, elapsed);
			if (arec->next_refresh == 0 && arec->ttl_percent < 5) {
				/* we tried to refresh but failed.  So give up. */
				DBG("Failed to resolve A record for ");
				debug_print_name(NULL, arec->fqdn);
				DBG(".  Evicting.\r\n");
				evict_arec(arec);
				break;
			}

			if (arec->next_refresh == 0) {
				DBG("Still trying to refresh A record for ");
				debug_print_name(NULL, arec->fqdn);
				DBG("\r\n");
				if (mdns_add_question(m, arec->fqdn, T_A, C_IN)
				    != 0)
					LOG("Warning: failed to add query for A record.\r\n");
				ret = 1;
				arec->ttl_percent >>= 1;
				arec->next_refresh =
				    arec->ttl * arec->ttl_percent / 100;
			}
		}
		break;

	case AREC_STATE_RESOLVED:
		if (e == AREC_EVENT_RX_REC) {
			set_ip(arec, a);

		} else if (e == AREC_EVENT_ADD_QUESTIONS) {
			/* We're supposed to attempt to refresh when 20%, 10%, and 5% of
			 * the lifetime of the record remains.
			 */
			arec->next_refresh =
			    SUBTRACT(arec->next_refresh, elapsed);
			if (arec->next_refresh == 0) {
				DBG("Refreshing A record for ");
				debug_print_name(NULL, arec->fqdn);
				DBG("\r\n");
				if (mdns_add_question(m, arec->fqdn, T_A, C_IN)
				    != 0)
					LOG("Warning: failed to add query for A record.\r\n");
				ret = 1;
				arec->ttl_percent = 10;
				arec->next_refresh =
				    arec->ttl * arec->ttl_percent / 100;
				arec->state = AREC_STATE_QUERYING;
			}
		}
		break;
	}
	return ret;
}

#ifdef CONFIG_IPV6
/* This is the aaaa_rec state machine.  Update the AAAA record considering the
 * event that just happened. The other arguments may or may not be valid
 * depending on the event.  Alert any associated sinsts if necessary.  Return 1
 * if we need to send a query, otherwise return 0.
 */
static int update_aaaa_rec(struct aaaa_rec *aaaa_rec, enum aaaa_rec_event e,
		       struct mdns_message *m, struct mdns_resource *a,
		       uint32_t elapsed)
{
	int ret = 0;

	if (e == AAAA_REC_EVENT_RX_REC && a->ttl == 0) {
		DBG("Got AAAA record goodbye for ");
		debug_print_name(NULL, aaaa_rec->fqdn);
		DBG(".\r\n");
		evict_aaaa_rec(aaaa_rec);
		return 0;
	}

	switch (aaaa_rec->state) {
	case AAAA_REC_STATE_INIT:
		if (e == AAAA_REC_EVENT_RX_REC) {
			DBG("Immediately resolved AAAA record for ");
			debug_print_name(NULL, aaaa_rec->fqdn);
			DBG("\r\n");
			aaaa_rec->state = AAAA_REC_STATE_RESOLVED;
			set_ip_v6(aaaa_rec, a);

		} else if (e == AAAA_REC_EVENT_ADD_QUESTIONS) {
			/* time to send the first query for this aaaa_rec */
			DBG("Launching AAAA record query for ");
			debug_print_name(NULL, aaaa_rec->fqdn);
			DBG("\r\n");
			if (mdns_add_question(m, aaaa_rec->fqdn, T_AAAA, C_IN)
					!= 0) {
				LOG("Warning: failed to add query for AAAA"
						"record.\r\n");
			}
			ret = 1;
			aaaa_rec->state = AAAA_REC_STATE_QUERYING;
			/* try to get the AAAA record approximately once per
			 * second for 3 seconds until we get it, or fail.  Use
			 * the same algorithm as the steady-state refresh period
			 * to simplify the code.  That is,
			 * refresh when 20%, 10%, and 5% of the ttl remains.
			 */
			aaaa_rec->ttl = 1000;
			aaaa_rec->next_refresh = aaaa_rec->ttl * 80 / 100;
			aaaa_rec->ttl_percent = 10;
		}
		break;

	case AAAA_REC_STATE_QUERYING:
		if (e == AAAA_REC_EVENT_RX_REC) {
			DBG("Resolved AAAA record for ");
			debug_print_name(NULL, aaaa_rec->fqdn);
			DBG("\r\n");
			aaaa_rec->state = AAAA_REC_STATE_RESOLVED;
			set_ip_v6(aaaa_rec, a);
			DBG("Next update in %ld ms.\r\n",
					aaaa_rec->next_refresh);

		} else if (e == AAAA_REC_EVENT_ADD_QUESTIONS) {
			/* still no response.  It's been "elapsed" ms since our
			 * last invocation.  Either try again or give up
			 */
			aaaa_rec->next_refresh =
			    SUBTRACT(aaaa_rec->next_refresh, elapsed);
			if (aaaa_rec->next_refresh == 0 && aaaa_rec->ttl_percent
					< 5) {
				/* we tried to refresh but failed.  So give up
				 */
				DBG("Failed to resolve AAAA record for ");
				debug_print_name(NULL, aaaa_rec->fqdn);
				DBG(".  Evicting.\r\n");
				evict_aaaa_rec(aaaa_rec);
				break;
			}

			if (aaaa_rec->next_refresh == 0) {
				DBG("Still trying to refresh AAAA record for ");
				debug_print_name(NULL, aaaa_rec->fqdn);
				DBG("\r\n");
				if (mdns_add_question(m, aaaa_rec->fqdn, T_AAAA,
						C_IN) != 0)
					LOG("Warning: failed to add query for"
							"AAAA record.\r\n");
				ret = 1;
				aaaa_rec->ttl_percent >>= 1;
				aaaa_rec->next_refresh =
				    aaaa_rec->ttl * aaaa_rec->ttl_percent / 100;
			}
		}
		break;

	case AAAA_REC_STATE_RESOLVED:
		if (e == AAAA_REC_EVENT_RX_REC) {
			set_ip_v6(aaaa_rec, a);
		} else if (e == AAAA_REC_EVENT_ADD_QUESTIONS) {
			/* We're supposed to attempt to refresh when 20%, 10%,
			 * and 5% of the lifetime of the record remains.
			 */
			aaaa_rec->next_refresh =
			    SUBTRACT(aaaa_rec->next_refresh, elapsed);
			if (aaaa_rec->next_refresh == 0) {
				DBG("Refreshing AAAA record for ");
				debug_print_name(NULL, aaaa_rec->fqdn);
				DBG("\r\n");
				if (mdns_add_question(m, aaaa_rec->fqdn, T_AAAA,
						C_IN) != 0)
					LOG("Warning: failed to add query for"
							"AAAA record.\r\n");
				ret = 1;
				aaaa_rec->ttl_percent = 10;
				aaaa_rec->next_refresh =
				    aaaa_rec->ttl * aaaa_rec->ttl_percent / 100;
				aaaa_rec->state = AAAA_REC_STATE_QUERYING;
			}
		}
		break;
	}
	return ret;
}
#endif /* CONFIG_IPV6 */
static struct service_instance overflow_sinst;
static struct arec overflow_arec;
#ifdef CONFIG_IPV6
static struct aaaa_rec overflow_aaaa_rec;
#endif /* CONFIG_IPV6 */

/* we received a packet.  If it matches a service that we're monitoring, update
 * the cache and (if necessary) alert the user.
 */
static int update_service_cache(struct mdns_message *m, uint32_t elapsed)
{
	struct service_monitor *smon = NULL;
	struct mdns_resource *ptr, *r, *rtmp, *a;
	struct service_instance *sinst = NULL;
	struct rr_srv *srv;
	struct arec *arec;
#ifdef CONFIG_IPV6
	struct aaaa_rec *aaaa_rec;
#endif /* CONFIG_IPV6 */

	if (m->header->flags.fields.qr != RESPONSE)
		return 0;

	SLIST_FOREACH(ptr, &m->ptrs, list_item) {
		smon = find_service_monitor(m, ptr->name);
		if (smon == NULL)
			continue;
		/* This PTR record is interesting.  Analyze it. */
		sinst = find_service_instance(smon, m, ptr->rdata);
		if (sinst != NULL) {
			/* sneak a bit of state machine logic in here */
			DBG("Got PTR with ttl=%ld for ", ptr->ttl);
			debug_print_name(NULL, sinst->service.fqsn);
			DBG(".\r\n");
			if (ptr->ttl == 0) {
				if (sinst->state == SINST_STATE_CLEAN ||
				    sinst->state == SINST_STATE_UPDATING)
					do_callback(sinst, MDNS_DISAPPEARED);
				cleanup_sinst(sinst);
			} else {
				sinst->ptr_ttl =
				    ADD(CONVERT_TTL(ptr->ttl), elapsed);
			}
			continue;
		}
		/* This is a new service instance. */
		sinst = SLIST_FIRST(&sinsts_free);
		if (sinst != NULL) {
			SLIST_REMOVE_HEAD(&sinsts_free, list_item);
			reset_service_instance(sinst);
			SLIST_INSERT_HEAD(&smon->sinsts, sinst, list_item);
			sinst->smon = smon;
			if (copy_servinfo(&sinst->service, m, ptr->rdata) == -1) {
				LOG("Warning: failed to copy service info.\r\n");
				SLIST_REMOVE_HEAD(&smon->sinsts, list_item);
				SLIST_INSERT_HEAD(&sinsts_free, sinst,
						  list_item);
				continue;
			}
			sinst->ptr_ttl = ADD(CONVERT_TTL(ptr->ttl), elapsed);
			continue;
		}

		/* If we get here, it means that we don't have any room in our cache.
		 * So make a best effort here using the overflow data structures by
		 * collecting all of the relevant records, and alerting the user if
		 * this is an overflow item.
		 */
		if (ptr->ttl == 0)
			continue;

		sinst = &overflow_sinst;
		reset_service_instance(sinst);
		sinst->smon = smon;
		if (copy_servinfo(&sinst->service, m, ptr->rdata) == -1) {
			LOG("Warning: failed to copy service info.\r\n");
			continue;
		}

		SLIST_FOREACH_SAFE(r, &m->txts, list_item, rtmp) {
			if (dname_cmp
			    (m->data, r->name, NULL, sinst->service.fqsn) == 0
			    && r->ttl != 0) {
				update_txt(sinst, r);
				SLIST_REMOVE(&m->txts, r, mdns_resource,
					     list_item);
			}
		}

		SLIST_FOREACH_SAFE(r, &m->srvs, list_item, rtmp) {
			if (dname_cmp
			    (m->data, r->name, NULL, sinst->service.fqsn) != 0
			    || r->ttl == 0)
				continue;
			srv = (struct rr_srv *)r->rdata;
			sinst->service.port = srv->port;
			memset(&overflow_arec, 0, sizeof(struct arec));
			sinst->arec = &overflow_arec;
			dname_copy(overflow_arec.fqdn, m->data, srv->target);
#ifdef CONFIG_IPV6
			memset(&overflow_aaaa_rec, 0, sizeof(struct aaaa_rec));
			sinst->aaaa_rec = &overflow_aaaa_rec;
			dname_copy(overflow_aaaa_rec.fqdn, m->data,
					srv->target);
#endif /* CONFIG_IPV6 */
			SLIST_REMOVE(&m->srvs, r, mdns_resource, list_item);
		}

		if (sinst->arec) {
			SLIST_FOREACH(a, &m->as, list_item) {
				if (dname_cmp
				    (m->data, a->name, NULL,
				     sinst->arec->fqdn) != 0 || a->ttl == 0)
					continue;
				sinst->service.ipaddr = get_uint32_t(a->rdata);
				break;
			}
		}
#ifdef CONFIG_IPV6
		if (sinst->aaaa_rec) {
			SLIST_FOREACH(a, &m->aaaas, list_item) {
				if (dname_cmp
				    (m->data, a->name, NULL,
				     sinst->aaaa_rec->fqdn) != 0 || a->ttl == 0)
					continue;
				memcpy(a->rdata, sinst->service.ip6addr,
						sizeof(sinst->service.ip6addr));
				break;
			}
		}
#endif /* CONFIG_IPV6 */

		do_callback(sinst, MDNS_CACHE_FULL);
	}

	/* Any SRV, TXT, A or AAAA records that remain in the message are either
	 * not of interest, or are related to existing service instances.  These
	 * appear for a few reasons.  Like if we previously got a message with a
	 * PTR of interest but no SRV/TXT, or we got an SRV of interest but no A
	 * Do the TXT and SRV records first.  This will ensure that if there is
	 * an A record in the list that goes with the SRV, it will be resolved
	 * right away.
	 */
	SLIST_FOREACH(r, &m->txts, list_item) {
		smon = find_smon_and_sinst(m, r->name, &sinst);
		if (smon == NULL || sinst == NULL)
			continue;

		DBG("Got TXT with ttl=%ld for ", r->ttl);
		debug_print_name(NULL, sinst->service.fqsn);
		DBG(".\r\n");
#ifdef CONFIG_IPV6
		update_sinst(sinst, SINST_EVENT_GOT_TXT, m, NULL, NULL, NULL, r,
			     elapsed);
#else /* !CONFIG_IPV6 */
		update_sinst(sinst, SINST_EVENT_GOT_TXT, m, NULL, NULL, r,
			     elapsed);
#endif /* CONFIG_IPV6 */
	}

	SLIST_FOREACH(r, &m->srvs, list_item) {
		smon = find_smon_and_sinst(m, r->name, &sinst);
		if (smon == NULL || sinst == NULL)
			continue;

		DBG("Got SRV with ttl=%ld for ", r->ttl);
		debug_print_name(NULL, sinst->service.fqsn);
		DBG(".\r\n");
#ifdef CONFIG_IPV6
		update_sinst(sinst, SINST_EVENT_GOT_SRV, m, NULL, NULL, r, NULL,
			     elapsed);
#else /* !CONFIG_IPV6 */
		update_sinst(sinst, SINST_EVENT_GOT_SRV, m, NULL, r, NULL,
			     elapsed);
#endif /* CONFIG_IPV6 */
	}

	SLIST_FOREACH(a, &m->as, list_item) {
		arec = find_arec(m, a->name);
		if (arec == NULL)
			continue;
		DBG("Got A with ttl=%ld for ", a->ttl);
		debug_print_name(NULL, arec->fqdn);
		DBG(".\r\n");
		update_arec(arec, AREC_EVENT_RX_REC, m, a, elapsed);
	}

#ifdef CONFIG_IPV6
	SLIST_FOREACH(a, &m->aaaas, list_item) {
		aaaa_rec = find_aaaa_rec(m, a->name);
		if (aaaa_rec == NULL)
			continue;
		DBG("Got AAAA with ttl=%ld for ", a->ttl);
		debug_print_name(NULL, aaaa_rec->fqdn);
		DBG(".\r\n");
		update_aaaa_rec(aaaa_rec, AAAA_REC_EVENT_RX_REC, m, a, elapsed);
	}
#endif
	return 0;
}

/* calculate the size of the answer that contains the ptr record of a service
 * instance.
 */
static int ptr_size(struct service_instance *sinst)
{
	/* our ptr contains an answer, an offset to the fqst, a service label, the
	 * length of the service label, and another offset to the fqst, and the len
	 * of the ptr data.
	 */
	uint16_t len = 2 * sizeof(uint16_t) + sizeof(uint32_t);	/* answer */
	len += 2 * sizeof(uint16_t);	/* offsets to fqst */
	len += strlen(sinst->service.servname) + 1;	/* service label, its len */
	len += sizeof(uint16_t);	/* len of ptr data */
	return len;
}

/* add the known answers from smon to the message m.  Return 0 if everything
 * went fine, or -1 if the answers didn't all fit.  Also take this opportunity
 * to update the ttls of the records.
 */
static int add_known_answers(struct mdns_message *m,
			     struct service_monitor *smon, uint32_t elapsed)
{

	struct service_instance *sinst;
	int len;

	SLIST_FOREACH(sinst, &smon->sinsts, list_item) {

		/* first add the PTR record for the service. */
		if (sinst->ptr_ttl == 0)
			continue;

		len = ptr_size(sinst);
		if (len > TAILROOM(m))
			goto fail;

		mdns_add_answer_o(m, smon->fqst_offset, T_PTR, C_IN,
				  sinst->ptr_ttl / 1000);
		smon->fqsn_offset = m->cur - m->data;
		mdns_add_name_lo(m, (uint8_t *) sinst->service.servname,
				 smon->fqst_offset);
	}
	return 0;

fail:
	LOG("Warning: all known answers didn't fit in packet\r\n");
	return -1;
}

/* given the elapsed time in ms since the last activity, prepare a query in the
 * message m.  Return -1 for error, 0 for no packet to send, and 1 to send the
 * packet.  Also update the next_event pointer with milliseconds until the next
 * event, or to UINT32_MAX if no scheduling is necessary.
 */
static int prepare_query(struct mdns_message *m, uint32_t elapsed,
	 uint32_t *next_event, bool is_unicast, struct in_addr *out_addr)
{
	struct service_monitor *smon;
	struct service_instance *sinst, *stmp;

	int ret = 0;
	struct arec *arec, *atmp;
#ifdef CONFIG_IPV6
	struct aaaa_rec *aaaa_rec, *aaaa_tmp;
#endif /* CONFIG_IPV6 */

	*next_event = UINT32_MAX;

	if (mdns_query_init(m) != 0)
		return -1;

	/* add questions for any unresolved A records. */
	SLIST_FOREACH_SAFE(arec, &arecs_active, list_item, atmp) {
#ifdef CONFIG_DNSSD_QUERY
		if (arec->is_unicast != is_unicast)
			continue;
		if (arec->is_unicast == true)
			*out_addr = arec->dns_addr;
#endif
		ret +=
		    update_arec(arec, AREC_EVENT_ADD_QUESTIONS, m, NULL,
				elapsed);
		*next_event = MIN(*next_event, arec->next_refresh);
	}

#ifdef CONFIG_IPV6
	SLIST_FOREACH_SAFE(aaaa_rec, &aaaa_recs_active, list_item, aaaa_tmp) {
		ret +=
		    update_aaaa_rec(aaaa_rec, AAAA_REC_EVENT_ADD_QUESTIONS, m,
				    NULL, elapsed);
		*next_event = MIN(*next_event, aaaa_rec->next_refresh);
	}
#endif /* CONFIG_IPV6 */

	/* We make two passes.  One to update all of the refresh times and to
	 * populate the questions section, and another to populated the known
	 * answers section
	 */

	SLIST_FOREACH(smon, &smons_active, list_item) {

#ifdef CONFIG_DNSSD_QUERY
		if (smon->is_unicast != is_unicast)
			continue;
#endif
		if (smon->next_refresh <= elapsed) {

			/* double the refresh period, topping out at 60s, and add a
			 * suitable question
			 */
			smon->next_refresh = 0;
			smon->refresh_period = smon->refresh_period > 30000 ?
			    60000 : smon->refresh_period * 2;
			smon->fqst_offset = m->cur - m->data;
			if (mdns_add_question(m, smon->fqst, T_ANY, C_IN) != 0) {
				LOG("ERROR: failed to populate questions!\r\n");
				return -1;
			}
			DBG("Added query for service ");
			debug_print_name(NULL, smon->fqst);
			DBG("\r\n");
#ifdef CONFIG_DNSSD_QUERY
			/* Since we have only one unicast socket, there will be
			   only one smon for unicast */
			if (smon->is_unicast == true)
				*out_addr = smon->dns_addr;
#endif
			ret += 1;
		} else {
			smon->next_refresh =
			    SUBTRACT(smon->next_refresh, elapsed);
			*next_event = MIN(*next_event, smon->next_refresh);
#ifdef CONFIG_DNSSD_QUERY
			if (smon->is_unicast == true)
				*out_addr = smon->dns_addr;
#endif
		}

		SLIST_FOREACH_SAFE(sinst, &smon->sinsts, list_item, stmp) {
			ret +=
#ifdef CONFIG_IPV6
			    update_sinst(sinst, SINST_EVENT_ADD_QUESTIONS, m,
					 NULL, NULL, NULL, NULL, elapsed);
#else /* !CONFIG_IPV6 */
			    update_sinst(sinst, SINST_EVENT_ADD_QUESTIONS, m,
					 NULL, NULL, NULL, elapsed);
#endif /* CONFIG_IPV6 */
			*next_event = MIN(*next_event, sinst->next_refresh);
#ifdef CONFIG_DNSSD_QUERY
			if (smon->is_unicast == true)
				*out_addr = smon->dns_addr;
#endif
		}
	}

	if (ret == 0)
		return ret;

	/* Okay.  We've added all of the questions that we want to ask.  Now we
	 * populate the known-answers section.  We know which service instances to
	 * add to the known answers because their service monitors have a
	 * next_refresh time of 0.
	 *
	 * NOTE: we don't support the TC bit.  So we only put as many answers as
	 * will fit into a single packet.  This may generate unnecessary network
	 * traffic by inducing responses with answers that actually are known.
	 */
	SLIST_FOREACH(smon, &smons_active, list_item) {
#ifdef CONFIG_DNSSD_QUERY
		if (smon->is_unicast != is_unicast)
			continue;
#endif
		if (smon->next_refresh == 0) {
			add_known_answers(m, smon, elapsed);
			smon->next_refresh = smon->refresh_period;
			*next_event = MIN(*next_event, smon->next_refresh);
		}
#ifdef CONFIG_DNSSD_QUERY
		if (smon->is_unicast == true)
			*out_addr = smon->dns_addr;
#endif
	}
	return ret > 0 ? 1 : 0;
}

#ifdef CONFIG_DNSSD_QUERY
extern bool dns_socket_used;
#endif
/* Main query thread */
static void do_querier(void)
{
	int max_sock;
	struct sockaddr_in from;
	int active_fds;
	fd_set fds;
	int ret = 0, i, in_sock = -1;
	struct timeval *timeout = NULL, tv;
	socklen_t in_size;
	uint32_t start_wait, stop_wait, sleep_time, next_event;
	int status, len = 0;
	query_enabled = 1;
	mico_queue_t *ctrl_query_queue;
	query_ctrl_msg ctrl_msg;

	LOG("do_querier() launched\r\n");
	while (query_enabled) {
		FD_ZERO(&fds);
		FD_SET(ctrl_sock, &fds);

#ifdef CONFIG_DNSSD_QUERY
		/* The select call on a uc_sock can only be made if that
		   socket is not closed by \ref dnssd_query_unmonitor api.
		   So adding the below check */
		if (dns_socket_used)
			FD_SET(uc_sock, &fds);
#endif

		FD_SET(mc_sock, &fds);
#ifdef CONFIG_IPV6
		FD_SET(mc6_sock, &fds);
		max_sock = ctrl_sock > mc6_sock ? ctrl_sock : mc6_sock;
#else
		max_sock = ctrl_sock > mc_sock ? ctrl_sock : mc_sock;
#endif /* CONFIG_IPV6 */

#ifdef CONFIG_DNSSD_QUERY
		if (dns_socket_used)
			max_sock = max_sock > uc_sock ? max_sock : uc_sock;
#endif

		start_wait = mdns_time_ms();
		active_fds = select(max_sock + 1, &fds, NULL, NULL,
				timeout);
		stop_wait = mdns_time_ms();
		/* round up to prevent ttls of 1ms from never expiring */
		sleep_time = ADD(interval(start_wait, stop_wait), 1);

		if (active_fds < 0)
			LOG("error: net_select() failed: %d\r\n", active_fds);

		/* handle control events
		 */
		if (FD_ISSET(ctrl_sock, &fds)) {
            if( mdns_socket_loopback(MDNS_CTRL_QUERIER, &ctrl_query_queue) == -1 ) {
                LOG("error: loopback socket err");
                continue;
            }

            ret = mico_rtos_pop_from_queue(ctrl_query_queue, &ctrl_msg, 0);
			/* we at least need a command and length */
			if (ret == -1) {
				LOG("Warning: querier failed to get control message\r\n");
				status = ERR_MDNS_INVAL;
			} else {
			    DBG("Querier got control message: %d.\r\n", ctrl_msg.command);
				if (ctrl_msg.command == MDNS_CTRL_HALT) {
					LOG("Querier done.\r\n");
					query_enabled = 0;
					status = kNoErr;
				} else if (ctrl_msg.command ==
					   MDNS_CTRL_MONITOR) {
					status =
					    add_service(&ctrl_msg.data.smon);
				} else if (ctrl_msg.command ==
					   MDNS_CTRL_UNMONITOR) {
					remove_service(ctrl_msg.data.fqst);
					status = kNoErr;
				} else {
					LOG("Unkown control message %d\r\n",
					    ctrl_msg.command);
					status = ERR_MDNS_NOIMPL;
				}
			}
#if 0
			/* send status back */
			ret =
			    sendto(ctrl_sock, (char *)&status, sizeof(status),
				   0, (struct sockaddr *)&from, sizeof(from));

			if (ret == -1)
				//LOG("error: failed to send control status: %d\r\n", net_get_sock_error(ctrl_sock));
			    LOG("error: failed to send control status\r\n");
#endif
		}


		if (FD_ISSET(mc_sock, &fds)
#ifdef CONFIG_IPV6
	 || (FD_ISSET(mc6_sock, &fds))
#endif
#ifdef CONFIG_DNSSD_QUERY
		/*
		 * We don't add uc_sock if we are not monitoring dnssd service.
		 * Hence before using FD_ISSET(), check for dns_socket_used
		 * flag.
		 */
	 || (dns_socket_used == true && FD_ISSET(uc_sock, &fds))
#endif
		) {
			DBG("querier got message\r\n");
			in_size = sizeof(struct sockaddr_in);
			if (FD_ISSET(mc_sock, &fds)) {
				in_sock = mc_sock;
			}
#ifdef CONFIG_IPV6
			else if (FD_ISSET(mc6_sock, &fds)) {
				in_sock = mc6_sock;
			}
#endif
#ifdef CONFIG_DNSSD_QUERY
			else if (dns_socket_used &&
				   FD_ISSET(uc_sock, &fds)) {
				in_sock = uc_sock;
			}
#endif

			len = recvfrom(in_sock, (char *)rx_msg.data,
				 sizeof(rx_msg.data), MSG_DONTWAIT,
				 (struct sockaddr *)&from, &in_size);
			if (len < 0) {
				//LOG("querier failed to recv packet: %d\r\n", net_get_sock_error(in_sock));
			    LOG("querier failed to recv packet\r\n");
				continue;
			}

			ret = mdns_parse_message(&rx_msg, len);
			if (ret == 0)
				update_service_cache(&rx_msg, sleep_time);
		}
		DBG("Preparing next query\r\n");
		struct in_addr out_addr;
		uint32_t mcast_next_event = UINT32_MAX;
#ifdef CONFIG_DNSSD_QUERY
		uint32_t ucast_next_event = UINT32_MAX;
#endif
		ret = prepare_query(&tx_msg, sleep_time, &mcast_next_event,
				    false, &out_addr);
		if (ret == 1) {
			for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++)
				if (config_g[i].iface_idx != INTERFACE_NONE) {
					DBG("Sending on multicast port\n\r");
					mdns_send_msg(&tx_msg, mc_sock,
						      mc6_sock, htons(5353),
					      config_g[i].iface_idx, 0);
				}
		}
#ifdef CONFIG_DNSSD_QUERY
		if (dns_socket_used) {
			ret = prepare_query(&tx_msg, sleep_time,
				&ucast_next_event, true, &out_addr);
			if (ret == 1) {
				for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++)
					if (config_g[i].iface_idx != INTERFACE_NONE) {
						DBG("Sending on unicast"
						    " port\n\r");
						dns_send_msg(&tx_msg, uc_sock,
						     53,
						     config_g[i].iface_idx,
						     out_addr);
					}
			}
		}

		next_event = MIN(mcast_next_event, ucast_next_event);
#else
		next_event = mcast_next_event;
#endif
		if (next_event == UINT32_MAX) {
			DBG("No event scheduled\r\n");
			timeout = NULL;
		} else {
			DBG("Next event in %ld ms\r\n", next_event);
			SET_TIMEOUT(&tv, next_event);
			timeout = &tv;
		}
	}

	if (!query_enabled) {
		LOG("Signalled to stop mdns_querier\r\n");
		query_enabled = 1;
		mico_rtos_delete_thread(NULL);
	}

}

/* Launch the query thread */
int query_launch()
{
	int i;

	/* Assign buffers to rx and tx messages */
	rx_msg.questions = rx_questions;
	rx_msg.answers = rx_answers;
	rx_msg.authorities = rx_authorities;

	tx_msg.questions = tx_questions;
	tx_msg.answers = tx_answers;
	tx_msg.authorities = tx_authorities;

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
	    config_g[i].iface_idx = INTERFACE_NONE;
	}

	/* Initially, all service monitors are on the free list */
#ifdef CONFIG_IPV6
	ip6_addr_t mdns_ipv6_addr;
#endif	/*	CONFIG_IPV6	*/
	SLIST_INIT(&smons_active);
	SLIST_INIT(&smons_free);

	for (i = 0; i < CONFIG_MDNS_MAX_SERVICE_MONITORS; i++) {
		SLIST_INIT(&smons[i].sinsts);
		SLIST_INSERT_HEAD(&smons_free, &smons[i], list_item);
	}

	/* Initially, all service instances are in the free list */
	SLIST_INIT(&sinsts_free);
	for (i = 0; i < CONFIG_MDNS_SERVICE_CACHE_SIZE; i++) {
		reset_service_instance(&sinsts[i]);
		SLIST_INSERT_HEAD(&sinsts_free, &sinsts[i], list_item);
	}

	/* Initially, all arec instances are in the free list */
	SLIST_INIT(&arecs_active);
	SLIST_INIT(&arecs_free);
	for (i = 0; i < CONFIG_MDNS_SERVICE_CACHE_SIZE; i++) {
		memset(&arecs[i], 0, sizeof(struct arec));
		SLIST_INSERT_HEAD(&arecs_free, &arecs[i], list_item);
	}
#ifdef CONFIG_IPV6
	SLIST_INIT(&aaaa_recs_active);
	SLIST_INIT(&aaaa_recs_free);
	for (i = 0; i < CONFIG_MDNS_SERVICE_CACHE_SIZE; i++) {
		memset(&aaaa_recs[i], 0, sizeof(struct aaaa_rec));
		SLIST_INSERT_HEAD(&aaaa_recs_free, &aaaa_recs[i], list_item);
	}
#endif /* CONFIG_IPV6 */

	/* create both ends of the control socket */
	ctrl_sock = mdns_socket_loopback(MDNS_CTRL_QUERIER, NULL);
	if (ctrl_sock < 0) {
		LOG("Failed to create query control socket: %d\r\n", ctrl_sock);
		return ctrl_sock;
	}
#ifdef CONFIG_XMDNS
	mc_sock = mdns_socket_mcast(inet_addr("239.255.255.251"), htons(5353));
#else
	mc_sock = mdns_socket_mcast(inet_addr("224.0.0.251"), htons(5353));
#endif

	if (mc_sock < 0) {
		LOG("error: unable to open multicast socket in querier\r\n");
		return mc_sock;
	}

#ifdef CONFIG_IPV6
	int ret;
#ifdef CONFIG_XMDNS
	ip6addr_aton("FF05::FB", &mdns_ipv6_addr);
#else
	ip6addr_aton("FF02::FB", &mdns_ipv6_addr);
#endif /*	CONFIG_XMDNS	*/

	mc6_sock = mdns6_socket_mcast(mdns_ipv6_addr, htons(5353));
	if (mc6_sock < 0) {
		LOG("error: unable to open multicast socket in responder\r\n");
		return mc6_sock;
	}
	ret = mld6_joingroup(IP6_ADDR_ANY, &mdns_ipv6_addr);
	if (ret < 0) {
		LOG("error: unable to join IPv6 mDNS multicast group\r\n");
		return ret;
	}
#endif	/*	CONFIG_IPV6	*/

	query_thread = mdns_thread_create(do_querier, MDNS_THREAD_QUERIER);
	if (query_thread == NULL)
		return ERR_MDNS_FQUERY_THREAD;
	return kNoErr;
}

static int signal_and_wait_for_query_halt()
{
	int total_wait_time = 100 * 100;    /* 10 seconds */
	int check_interval = 100;   /* 100 ms */
	int num_iterations = total_wait_time / check_interval;

	if (!query_enabled) {
		LOG("Warning: mdns responder not running\r\n");
		return kNoErr;
	}

	while (query_enabled && num_iterations--)
	    mico_rtos_delay_milliseconds(check_interval);

	if (!num_iterations)
		LOG("Error: timed out waiting for mdns querier to stop\r\n");

	return query_enabled ? kGeneralErr : kNoErr;
}


/* Send the halt command to the query thread */
int query_halt(void)
{
	int ret;

	ret = mdns_send_ctrl_msg(MDNS_CTRL_HALT, MDNS_CTRL_QUERIER);
	if (ret != 0) {
		LOG("Warning: failed to send HALT message to mdns querier\r\n");
		return kGeneralErr;
	}
	ret = signal_and_wait_for_query_halt();

	if (ret != kNoErr)
		LOG("Warning: failed to HALT mdns querier\r\n");

	/* force a halt */
	if (query_enabled) {
		LOG("Warning: failed to halt mdns querier, forcing.\r\n");
		query_enabled = 0;
	}

	ret = mico_rtos_delete_thread(query_thread);
	if (ret != kNoErr)
		LOG("Warning: failed to delete thread.\r\n");

	ret = mdns_socket_close(&ctrl_sock);
#ifdef CONFIG_DNSSD_QUERY
	ret = dns_socket_close(&uc_sock);
#endif
	ret = mdns_socket_close(&mc_sock);
#ifdef CONFIG_IPV6
	ret = mdns_socket_close(&mc6_sock);
#endif

	return ret;
}

static int save_query_monitor(struct service_monitor *m, char *fqst,
			      mdns_query_cb cb, void *data)
{
	int ret;

	m->cb = cb;
	m->cbdata = data;

	ret = strlen(fqst);
	if (ret > MDNS_MAX_NAME_LEN)
		return ERR_MDNS_INVAL;

	ret = dnameify(fqst, strlen(fqst) + 1, '.', m->fqst);
	if (ret == -1) {
		LOG("Failed to parse fully-qualified service type\r\n");
		return ERR_MDNS_INVAL;
	}
	/* dnameify doesn't null-terminate by design.  But we want to use
	 * dname_cmp, etc. on the fqst.  So we null terminate here.
	 */
	m->fqst[ret] = 0;

	/* Why don't we just populate a free smon instead of copying one all over
	 * the place and sending a control message?  The reason is that the mdns
	 * thread owns the smons, and we don't want to have to use mutexes or other
	 * locks at runtime
	 */
	return kNoErr;
}

/* Function adds interface to config_g and returns configuration index*/
int mdns_add_iface(netif_t iface)
{
	int i, config_idx = -1;

	for (i = MDNS_MAX_SERVICE_CONFIG - 1; i >= 0; i--) {
		/* If interface is already registered, break the loop */
		if (config_g[i].iface_idx == iface) {
			return i;
		} else if (config_g[i].iface_idx == INTERFACE_NONE)
			config_idx = i;
	}

	/* No space for new interface */
	if (config_idx == -1)
		return kGeneralErr;

	config_g[config_idx].iface_idx = iface;

	return config_idx;
}

#ifdef CONFIG_DNSSD_QUERY
int dnssd_query_monitor(char *fqst, mdns_query_cb cb,
			struct in_addr dns_addr, void *data, void *iface)
{
	struct query_ctrl_msg ctrl_msg;
	struct service_monitor *m = &ctrl_msg.data.smon;
	int ret, config_idx;

	if (dns_addr.s_addr == 0) {
		LOG("Invalid DNS address\r\n");
		return ERR_MDNS_INVAL;
	}
	if (cb == NULL) {
		LOG("mdns query callback must not be NULL\r\n");
		return ERR_MDNS_INVAL;
	}

	config_idx = mdns_add_iface(iface);

	if (config_idx == kGeneralErr)
		return kGeneralErr;

	m->dns_addr = dns_addr;
	m->is_unicast = true;
	ret = save_query_monitor(m, fqst, cb, data);
	if (ret != kNoErr)
		return ret;

	ctrl_msg.command = MDNS_CTRL_MONITOR;
	ctrl_msg.length = sizeof(struct query_ctrl_msg);

	uc_sock = dns_socket_ucast(53);
	if (uc_sock < 0) {
		LOG("error: unable to open multicast socket in querier\r\n");
		return uc_sock;
	}
	DBG("Created unicast socket for DNS Client\n\r");
	ret = query_send_ctrl_msg(&ctrl_msg, MDNS_CTRL_QUERIER);
	if (ret != kNoErr)
		dns_socket_close(uc_sock);
	return ret;
}

void dnssd_query_unmonitor(char *fqst)
{
	mdns_query_unmonitor(fqst);
	dns_socket_close(uc_sock);
	return;
}
#endif

int mdns_query_monitor(char *fqst, mdns_query_cb cb, void *data, netif_t iface)
{
	query_ctrl_msg ctrl_msg;
	struct service_monitor *m = &ctrl_msg.data.smon;
	int config_idx;

	if (cb == NULL) {
		LOG("mdns query callback must not be NULL\r\n");
		return ERR_MDNS_INVAL;
	}

	config_idx = mdns_add_iface(iface);

	if (config_idx == kGeneralErr)
		return kGeneralErr;

#ifdef CONFIG_DNSSD_QUERY
	m->is_unicast = false;
#endif
	int ret = save_query_monitor(m, fqst, cb, data);
	if (ret != kNoErr)
		return ret;

	ctrl_msg.command = MDNS_CTRL_MONITOR;
	ctrl_msg.length = sizeof(query_ctrl_msg);
	LOG("Start monitor: %d\r\n", ctrl_msg.command);
	return query_send_ctrl_msg(&ctrl_msg, MDNS_CTRL_QUERIER);
}

void mdns_query_unmonitor(char *fqst)
{
	query_ctrl_msg ctrl_msg;
	int ret;

	if (fqst == NULL)
		ret = 0;

	else {
		ret = strlen(fqst);
		if (ret > MDNS_MAX_NAME_LEN) {
			LOG("error: invalid service type\r\n");
			return;
		}
		ret = dnameify(fqst, strlen(fqst) + 1, '.', ctrl_msg.data.fqst);
		if (ret == -1) {
			LOG("Failed to parse fully-qualified service type\r\n");
			return;
		}
	}
	ctrl_msg.data.fqst[ret] = 0;

	ctrl_msg.command = MDNS_CTRL_UNMONITOR;
	ctrl_msg.length = sizeof(query_ctrl_msg);
	query_send_ctrl_msg(&ctrl_msg, MDNS_CTRL_QUERIER);
	return;
}

#else

int query_launch()
{
	/* not implmented, but we transparently drop through here so the
	 * querier can be "started" by mdns_start
	 */
	return MICO_SUCCESS;
}

int query_halt(void)
{
	/* not implmented, but we transparently drop through here so the
	 * querier can be halted by mdns_stop
	 */
	return MICO_SUCCESS;
}

int mdns_query_monitor(char *fqst, mdns_query_cb cb, void *data, netif_t iface)
{
	return ERR_MDNS_NOIMPL;
}

void mdns_query_unmonitor(char *fqst)
{
	return;
}
#ifdef CONFIG_DNSSD_QUERY
int dnssd_query_monitor(char *fqst, mdns_query_cb cb,
		      struct in_addr dns_addr, void *data, void *iface)
{
	return ERR_MDNS_NOIMPL;
}

void dnssd_query_unmonitor(char *fqst)
{
	return;
}
#endif

#endif
