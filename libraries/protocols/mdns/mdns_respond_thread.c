/**
 ******************************************************************************
 * @file    mdns_responder_thread.c
 * @author  William Xu
 * @version V1.0.0
 * @date    8-Aug-2017
 * @brief   This file provide the mdns responder daemon thread
 *
 * This code implements the responder portion of mdns.
 * Responder can
 * 1) ANNOUNCE new service(s)
 *    - do_responder() receives control data to announce service(s) with list of
 *      service(s) to be announced and interface handle on which service(s) are
 *      to be announced, as a part of control request
 *    - mdns_add_service_iface() adds service(s) in a global list of service
 *      pointers maintained per interface
 *    - send_init_probes() announces mdns service(s) over the network
 *
 * 2) DEANNOUNCE service(s)
 *    - do_responder() receives control data to deannounce service(s) with list
 *      of service(s) to be deannounced and interface handle on which service(s)
 *      are to be deannounced, as a part of control request
 *    - send_close_probe() deannounces mdns service(s)
 *    - mdns_remove_service_iface() removes service(s) from a global list of
 *      service pointers maintained per interface
 *
 * 3) REANNOUNCE service(s)
 *    - do_responder() receives control data to reannounce service(s) with
 *      interface handle on which services are to be reannounced, as a part of
 *      control request
 *    - prepare_announcement() than reannounces all previously announced
 *      services on a given interface
 *    - Note that reannouncement of service doesn't go through probing state
 *      again. Instead it announces service(s) directly
 *
 * 4) DOWN the interface
 *    - do_responder() receives control data to take interface down with the
 *      required interface handle, as a part of cotrol request
 *    - send_close_probe() deannounces all announced services on that interface
 *    - interface_state is set to STOPPED
 *    - Note that taking interface DOWN doesn't remove services from global list
 *      of service pointers maintained per interface
 *
 * 5) UP the interface
 *    - do_responder() receives control data to up the interface after it has
 *      been taken down for some purpose. Also interface handle of the required
 *      interface is provided as a part of control request
 *    - send_init_probes() announces all services from global service pointer
 *      list associated with the given interface handle
 *    - interface_state is set to RUNNING
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

#include <stdio.h>
#include "mdns.h"
#include "mdns_private.h"
#include "mdns_port.h"
#include "mico_errno.h"

/* Time interval (in msecs) between consecutive probes.
 * RFC 6762 mentions that minimum time interval between consecutive probes
 * should be 250ms.
 * Due to various factors in the network - congestion, network stack
 * processing, etc. the exact time of packet arrival on destination can be
 * variable. Hence the interval between consecutive probe packet transmission
 * should accomodate this variance.
 * Setting it to higher number (575) to ensure that any two consecutive probe
 * packets are received on the client devices are always separated by at least
 * 250ms.
 */
#define MDNS_INTER_PROBE_INTERVAL	575
#define MDNS_TTL_THRESHOLD		159

/* hname and domname are cstring, not DNS string */
static char *hname;
static const char *domname;

/* Fully qualified domain name. It is something like 'node.local.' */
static uint8_t fqdn[MDNS_MAX_NAME_LEN + 1];
static struct mdns_service_config config_g[MDNS_MAX_SERVICE_CONFIG];
int num_config_g;

/* global mdns state */
static void *responder_thread;
static int mc_sock = -1;

#ifdef CONFIG_BONJ_CONFORMANCE
/* Variable to check if response is unicast or not */
static bool unicast_resp;
#endif /* CONFIG_BONJ_CONFORMANCE */

static int ctrl_sock;
static int responder_enabled;

/* sn_conflict will be set when service name conflict is encountered. */
static bool sn_conflict;
extern mdns_responder_stats mr_stats;
static struct mdns_message tx_msg;
static struct mdns_message rx_msg;

static struct mdns_question rx_questions[MDNS_MAX_QUESTIONS];
static struct mdns_resource rx_answers[MDNS_MAX_ANSWERS];
static struct mdns_resource rx_authorities[MDNS_MAX_AUTHORITIES];

/* Keeping the minimum size of Tx questions array as four. Array size would
 * grow with number of mDNS services to be announced.
 * Answers and authorities sections in Tx message typically contains following
 * entries:
 * A record, AAAA record, ARPA PTR record,
 * Additionally 3 records (SRV, TXT, PTR) are present per mDNS service
 * */
static struct mdns_question tx_questions[4 + CONFIG_MDNS_MAX_SERVICE_ANNOUNCE];
static struct mdns_resource \
	tx_answers[4 + (3 * CONFIG_MDNS_MAX_SERVICE_ANNOUNCE)];
static struct mdns_resource \
	tx_authorities[4 + (3 * CONFIG_MDNS_MAX_SERVICE_ANNOUNCE)];

/* This is the common service for which mDNS-SD based browsers send the request
 * as the first step. Responder must respond with all the domains of the
 * services that device hosts to a dnssd query.
 */
struct mdns_service services_dnssd = {
	.servname = "_services",
	.servtype = "dns-sd",
	.domain   = "local",
	.proto    = MDNS_PROTO_UDP,
	.port     = 8888,
	.keyvals  = NULL,
};

/* Resource records of type below are stored in an array. Below are the array
 * indices for each RR. This simplifies sorting process.
 */
enum type_indicies {
	A_INDEX = 0,
	CNAME_INDEX,
	PTR_INDEX,
	TXT_INDEX,
#ifdef CONFIG_IPV6
	AAAA_INDEX,
#endif	/*	CONFIG_IPV6	*/
	SRV_INDEX,
	MAX_INDEX,
};

extern struct sockaddr_in mdns_mquery_v4group;
extern struct sockaddr_in6 mdns_mquery_v6group;

/* Function finds all the authority records in the message m that have
 * specified name. Authorities are stored in a sorted manner in order of
 * increasing type. This is required in order to compare the authorities in
 * lexicographically increasing order as required by the spec.
 * "name" must be a complete dname without any pointers.
 * Returns number of valid records in the list.
 */
static int find_authorities(struct mdns_message *m, uint8_t * name,
			    struct mdns_resource *results[MAX_INDEX])
{
	int i, n = 0;

	for (i = 0; i < MAX_INDEX; i++)
		results[i] = 0;

	for (i = 0; i < m->num_authorities; i++) {

		if (dname_cmp(m->data, m->authorities[i].name, NULL, name) != 0)
			continue;

		/* The authority matches. Add it to the results */
		n++;
		switch (m->authorities[i].type) {
		case T_A:
			results[A_INDEX] = &m->authorities[i];
			break;
		case T_CNAME:
			results[CNAME_INDEX] = &m->authorities[i];
			break;
		case T_PTR:
			results[PTR_INDEX] = &m->authorities[i];
			break;
		case T_TXT:
			results[TXT_INDEX] = &m->authorities[i];
			break;
#ifdef CONFIG_IPV6
		case T_AAAA:
			results[AAAA_INDEX] = &m->authorities[i];
			break;
#endif	/*	CONFIG_IPV6	*/
		case T_SRV:
			results[SRV_INDEX] = &m->authorities[i];
			break;
		default:
			MDNS_LOG("Warning: unexpected record of type %d",
			    m->authorities[i].type);
			n--;
		}
	}
	return n;
}

/* Function compares resource a from message ma with resource b from message mb.
 * Returns 1 if a is lexicographically later than b
 *	   0 if a is lexicographically earlier than b
 *	   -1 if both are equal
 */
static int rr_cmp(struct mdns_message *ma, struct mdns_resource *a,
		  struct mdns_message *mb, struct mdns_resource *b)
{
	int min, i, ret = 0;
	struct rr_srv *sa, *sb;

	/* Check the fields in the order specified (class, type, then rdata).
	 * Normally, name of the resources are compared first, but by the
	 * time this function is called, their equality has already been
	 * verified.
	 */
	if ((a->class & ~0x8000) > (b->class & ~0x8000))
		return 1;

	if ((a->class & ~0x8000) < (b->class & ~0x8000))
		return -1;

	if (a->type > b->type)
		return 1;

	if (a->type < b->type)
		return -1;

	/* Since class and type are same, check the rdata.
	 * This part depends on the record type because any names might have to
	 * be decompressed.
	 */
	switch (a->type) {
	case T_A:
		/* A record always contains a 4-byte IP address */
		ret = memcmp(a->rdata, b->rdata, 4);
		break;

#ifdef CONFIG_IPV6
	case T_AAAA:
		/* AAAA record always contains a 16-byte IPv6 address */
		ret = memcmp(a->rdata, b->rdata, MDNS_AAAA_RR_LEN);
		break;
#endif	/*	CONFIG_IPV6	*/

	case T_CNAME:
	case T_PTR:
		/* some records just have a dname */
		ret = dname_cmp(ma->data, a->rdata, mb->data, b->rdata);
		break;

	case T_SRV:
		/* first check the fixed part of the record */
		ret = memcmp(a->rdata, b->rdata, sizeof(struct rr_srv));
		if (ret != 0)
			break;
		sa = (struct rr_srv *)a->rdata;
		sb = (struct rr_srv *)b->rdata;
		ret = dname_cmp(ma->data, sa->target, mb->data, sb->target);
		break;

	case T_TXT:
	default:
		min = a->rdlength > b->rdlength ? b->rdlength : a->rdlength;
		for (i = 0; i < min; i++) {
			if (((unsigned char *)a->rdata)[i] >
			    ((unsigned char *)b->rdata)[i])
				return 1;
			if (((unsigned char *)a->rdata)[i] >
			    ((unsigned char *)b->rdata)[i])
				return -1;
		}
		/* if we get all the way here, one rdata is a substring of the
		 * other.  The longer one will be considered as
		 * lexicographically later.
		 */
		if (a->rdlength > b->rdlength)
			return 1;
		if (a->rdlength < b->rdlength)
			return -1;
	}
	ret = ret > 0 ? 1 : ret;
	ret = ret < 0 ? -1 : ret;
	return ret;
}

/* Function compares authority record set in message p1 with authority recored
 * set in p2.
 * Returns 1 if p1 is greater than p2
 *	   0 if both are equal
 *	   -1 if p1 is lesser than p2
 */
static int authority_set_cmp(struct mdns_message *p1, struct mdns_message *p2,
			     uint8_t *name)
{
	struct mdns_resource *r1[MAX_INDEX], *r2[MAX_INDEX];
	int ret = 0, i1 = 0, i2 = 0, n1, n2;

	/* As per spec, records must be checked in lexicographically increasing
	 * order. find_authorities gives records in this fashion */
	n1 = find_authorities(p1, name, r1);
	n2 = find_authorities(p2, name, r2);
	if (n1 == 0 && n2 == 0)
		return 0;

	/* now check them in order of increasing type */
	while (1) {
		/* advance to the next valid record */
		while (i1 < MAX_INDEX && r1[i1] == NULL)
			i1++;
		/* Start navigating from the record set having same
		 * index in the second mdns_message */
		i2 = i1;

		while (i2 < MAX_INDEX && r2[i2] == NULL)
			i2++;

		if (i1 == MAX_INDEX && i2 == MAX_INDEX) {
			/* The record sets are absolutely identical. */
			ret = 0;
			break;
		}

		if (i1 == MAX_INDEX) {
			/* p2 has more records than p1 this means p2 is greater
			 */
			ret = -1;
			break;
		}

		if (i2 == MAX_INDEX) {
			/* p1 has more records than p2 this means p2 is greater
			 */
			ret = 1;
			break;
		}

		ret = rr_cmp(p1, r1[i1], p2, r2[i2]);
		if (ret != 0)
			break;
		i1++;
		i2++;
	}
	return ret;
}

/* During probing sequence, if a conflict is encountered, host and service names
 * must be appended with -2 or -3 and likewise. Function returns maximum number
 * of extra bytes that might be required to be appended to the original name
 * when sending a probe.
 */
static int max_probe_growth(int num_services)
{
	/* 2 bytes are required for each instance of the host name that appears
	 * in the probe (A record q, A record a, inaddrarpa PTR, one for each
	 * SRV PTR), and 2 bytes for each service name.
	 */
	return 2 * 3 + 2 * 2 * num_services;
}

/* During probing sequence, if a conflict is encountered, host and service names
 * must be appended with -2 or -3 and likewise. Function returns maximum number
 * of extra bytes that might be required to be appended to the original name
 * when sending a response.
 */
static int max_response_growth(int num_services)
{
	/* 2 bytes are required for each instance of the host name that appears
	 * in the:
	 * Query part:
	 * +2 A record q
	 * +2 * num services
	 *
	 * Answer part:
	 * +2 A record a
	 * +2 inaddrarpa PTR
	 * +2 * num services SRV
	 * +2 * num services SRV PTR
	 * +2 * num services TX
	 */

	return 2 * 3 + 4 * 2 * num_services;
}

static mico_mutex_t mdns_mutex;

enum mdns_iface_state {
	STOPPED = 0,
	STARTED,
	RUNNING,
};
static enum mdns_iface_state interface_state[MDNS_MAX_SERVICE_CONFIG];

static uint32_t get_msec_time(struct timeval t)
{
	return (t.tv_sec * 1000) + (t.tv_usec / 1000);
}
static int get_min_timeout(struct timeval *t, int num_config)
{
	int i, min_idx = 0;

	for (i = 0; i < num_config; i++) {
		if (interface_state[i] == STOPPED || get_msec_time(t[i]) == 0)
			continue;
		else {
			if (get_msec_time(t[min_idx]) == 0)
				min_idx = i;
			else if (get_msec_time(t[i]) <
					get_msec_time(t[min_idx]))
				min_idx = i;
		}
	}
	return min_idx;
}

/* Re-calculate timeout for all interface */
static void update_all_timeout(struct timeval *probe_wait_time,
	uint32_t start_wait, uint32_t stop_wait)
{
	int i;

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		if (interface_state[i] != STOPPED &&
			config_g[i].iface_idx != INTERFACE_NONE) {
			recalc_timeout(&probe_wait_time[i], start_wait,
			stop_wait, get_msec_time(probe_wait_time[i]));
		}
	}
}

/* Re-calculate timeout for all interfaces except one */
static void update_others_timeout(int idx, struct timeval *probe_wait_time,
		uint32_t start_wait, uint32_t stop_wait)
{
	int i;

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		if (interface_state[i] != STOPPED)
			if (i != idx && config_g[i].iface_idx != INTERFACE_NONE) {
				recalc_timeout(&probe_wait_time[i], start_wait,
				stop_wait,
				get_msec_time(probe_wait_time[i]));
			}
	}
}

/* Reset the fqdn to hostname.domain.
 */
static void reset_fqdn(void)
{
	uint8_t *p;

	p = dname_put_label(fqdn, hname);
	dname_put_label(p, domname);
}

/* Reset the internal fully qualified service name of s to the values supplied
 * by the user at launch time.
 */
static void reset_fqsn(struct mdns_service *s)
{
	uint8_t *p;

	s->ptrname = dname_put_label((uint8_t *) s->fqsn, s->servname);
	/* when adding the service type, append a leading '_' and adjust the
	 * length.
	 */
	p = dname_put_label(s->ptrname + 1, s->servtype);
	*s->ptrname = *(s->ptrname + 1) + 1;
	*(s->ptrname + 1) = '_';

	p = dname_put_label(p, s->proto == MDNS_PROTO_TCP ? "_tcp" : "_udp");

	dname_put_label(p, domname);
}

/* Set mdns hostname */
void mdns_set_hostname(char *hostname)
{
	hname = hostname;
	reset_fqdn();
}

/* Check if label is valid or not
 * Returns 0 if label is invalid
 *	   1 if label is valid
 */
#ifdef MDNS_CHECK_ARGS
static int valid_label(const char *name)
{
	if (strlen(name) > MDNS_MAX_LABEL_LEN || strchr(name, '.') != NULL)
		return 0;
	return 1;
}
#else
#define valid_label(name) (1)
#endif


/* Function figure out incoming packet is came from which interface and returns
 * corresponding index of global mdns service configuration array */
static int get_config_idx_from_ip(uint32_t incoming_ip)
{
	int i, ret = -1;
	uint32_t interface_ip, interface_mask;
	IPStatusTypedef interface_ip_info;
	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
	    micoWlanGetIPStatus(&interface_ip_info, config_g[i].iface_idx);
	    interface_ip = inet_addr(interface_ip_info.ip);
	    interface_mask = inet_addr(interface_ip_info.mask);

        if (interface_ip == incoming_ip) // from myself.
            return -1;
        if ( interface_ip != 0 && (interface_ip & interface_mask) == (incoming_ip & interface_mask))
			ret = i;
#ifdef CONFIG_BONJ_CONFORMANCE
		/* If current interface has link local address or incoming
		 * packet is from link-local address then
		 * the above comparsion will give false result.
		 * Hence, temporarily return the interface index if either
		 * ip is link-local.
		 * TODO: Instead of matching the interface IP with
		 * incoming_ip, come up with better method to figure
		 * out the interface from which the packet has come.
		 */
		else if ((interface_ip & 0xff) == 0xa9) {
			ret = i;
		} else if ((incoming_ip & 0xff) == 0xa9) {
			ret = i;
		}
#endif /* CONFIG_BONJ_CONFORMANCE */

	}
	return ret;
}

/* Function returns index of global mdns service configuration array,
 * corresponding to provided interface */
static int get_config_idx_from_iface(netif_t iface_idx)
{
	int i;

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		if (iface_idx == config_g[i].iface_idx)
			return i;
	}
	return kGeneralErr;
}

/* Get IP address of interface from its interface index*/
static uint32_t get_interface_ip(int config_idx)
{
	uint32_t ip;
	IPStatusTypedef interface_ip_info;

	micoWlanGetIPStatus(&interface_ip_info, config_g[config_idx].iface_idx);
	ip = inet_addr(interface_ip_info.ip);
	return ip;
}

/* Function creates the inaddrarpa domain name DNS string for ipaddr.
 * Output buffer must be at least MDNS_INADDRARPA_LEN and
 * ipaddr must be in host order.
 */
static void ipaddr_to_inaddrarpa(uint32_t ipaddr, uint8_t *out)
{
	uint8_t *ptr = out;
	uint8_t byte;
	int i, len;

	/* This is a DNS name of the form 45.1.168.192.in-addr.arpa */
	for (i = 0; i < 4; i++) {
		byte = ipaddr & 0xff;
		ipaddr >>= 8;
		len = sprintf((char *)ptr + 1, "%d", byte);
		*ptr = len;
		ptr += len + 1;
	}
	*ptr++ = 7;
	sprintf((char *)ptr, "in-addr");
	ptr += 7;
	*ptr++ = 4;
	sprintf((char *)ptr, "arpa");
}

#ifdef CONFIG_IPV6
int mdns_add_aaaa_authority(struct mdns_message *m, int config_idx)
{
	int ret, i;
	ipv6_addr_t ipv6_addrs[MICO_IPV6_NUM_ADDRESSES];
	micoWlanGetIP6Status(ipv6_addrs, MICO_IPV6_NUM_ADDRESSES, config_g[config_idx].iface_idx);

	for (i = 0; i < MICO_IPV6_NUM_ADDRESSES; i++) {
	    if( IP6_ADDR_IS_VALID(ipv6_addrs[i].addr_state) ) {
	        ret = mdns_add_authority(m, fqdn, T_AAAA, C_IN, 255);
	        if (ret)
	            return ret;
	        ret = mdns_add_aaaa(m, &ipv6_addrs[i].address);
	        if (ret)
	            return ret;
	    }
	}
	return kNoErr;

}

int mdns_add_aaaa_answer(struct mdns_message *m, int config_idx, uint32_t ttl)
{
	int ret, i;
	ipv6_addr_t ipv6_addrs[MICO_IPV6_NUM_ADDRESSES];
    micoWlanGetIP6Status(ipv6_addrs, MICO_IPV6_NUM_ADDRESSES, config_g[config_idx].iface_idx);

    for (i = 0; i < MICO_IPV6_NUM_ADDRESSES; i++) {
        if( IP6_ADDR_IS_VALID(ipv6_addrs[i].addr_state) ) {
            ret = mdns_add_answer(m, fqdn, T_AAAA, C_FLUSH, ttl);
            if (ret)
                return ret;
            ret = mdns_add_aaaa(m, &ipv6_addrs[i].address);
            if (ret)
                return ret;
        }
    }
	return kNoErr;

}
#endif	/*	CONFIG_IPV6	*/

/* Function either adds all the services or subset of services (depending on
 * services argument) that device publishes, to the specified section
 * of the message m.
 * Returns 0 on success
 *	   -1 on error.
 */
int responder_add_all_services(struct mdns_message *m, int section,
			       uint32_t ttl, int config_idx, struct mdns_service
			       *services[])
{
	int i;
	struct mdns_service *s;

	/* If services pointer is NULL, add all services from global array
	 * else add only given services */
	if (services != NULL) {
		for (; *services != NULL; services++) {
			if (mdns_add_srv_ptr_txt(m, *services, fqdn, section,
						ttl) != 0) {
				return -1;
			}
		}
	} else {
		for (i = 0; i < MAX_MDNS_LST; i++) {
			s = config_g[config_idx].services[i];
			if (s != NULL) {
				if (mdns_add_srv_ptr_txt(m, s, fqdn, section,
							ttl) != 0) {
					return -1;
				}
			}
		}
	}
	return 0;
}

static int prepare_announcement(struct mdns_message *m, uint32_t ttl,
		int config_idx, struct mdns_service *services[])
{
	uint32_t my_ipaddr;
	static uint8_t in_addr_arpa[MDNS_INADDRARPA_LEN];

	mdns_response_init(m);
	my_ipaddr = get_interface_ip(config_idx);
	ipaddr_to_inaddrarpa(ntohl(my_ipaddr), in_addr_arpa);
	if ( my_ipaddr != 0 ) {
	    if ( mdns_add_answer(m, fqdn, T_A, C_FLUSH, ttl) != 0 ||
	         mdns_add_uint32_t(m, htonl(my_ipaddr)) != 0 ||
	         mdns_add_answer(m, in_addr_arpa, T_PTR, C_IN, ttl) != 0 ||
	         mdns_add_name(m, fqdn) != 0 ) {
	        return -1;
	    }
	}

#ifdef CONFIG_IPV6
    if (  mdns_add_aaaa_answer(m, config_idx, ttl) != 0 ) {
        return -1;
    }
#endif  /*  CONFIG_IPV6 */

    if ( responder_add_all_services(m, MDNS_SECTION_ANSWERS, ttl, config_idx, services) != 0) {
        return -1;
    }

	return 0;
}

/* Function prepares probe packet, for initial probing sequence, in message m.
 * Probe must contain a question for each resource record that we intend on
 * advertising and an authority with proposed answers.
 */
static int prepare_probe(struct mdns_message *m, int config_idx,
		struct mdns_service *services[])
{
	uint32_t my_ipaddr;
	static uint8_t in_addr_arpa[MDNS_INADDRARPA_LEN];

	mdns_query_init(m);
	my_ipaddr = get_interface_ip(config_idx);
	ipaddr_to_inaddrarpa(ntohl(my_ipaddr), in_addr_arpa);
	if (!sn_conflict) {
		if (mdns_add_question(m, fqdn, T_ANY, C_IN) != 0 ||
				mdns_add_question(m, in_addr_arpa, T_ANY,
					C_FLUSH) != 0 ||
				responder_add_all_services(m,
					MDNS_SECTION_QUESTIONS, 0,
					config_idx, services) != 0 ||
				mdns_add_authority(m, fqdn, T_A, C_IN,
					255) != 0 ||
				mdns_add_uint32_t(m, htonl(my_ipaddr)) != 0 ||
#ifdef CONFIG_IPV6
				mdns_add_aaaa_authority(m, config_idx) ||
#endif	/*	CONFIG_IPV6	*/
				mdns_add_authority(m, in_addr_arpa, T_PTR, C_IN,
					255) != 0 ||
				mdns_add_name(m, fqdn) != 0 ||
				responder_add_all_services(m,
					MDNS_SECTION_AUTHORITIES, 255,
					config_idx, services) != 0) {
			MDNS_LOG("Resource records don't fit into probe packet."
					);
			return -1;
		}
	/* If in the probing phase, device's service name conflicts with other
	 * device's service name, send probe for service name only.
	 * This is required to pass Bonjour Conformance Test Suite.
	 */
	} else if (sn_conflict) {
		if (responder_add_all_services(m, MDNS_SECTION_QUESTIONS, 0,
				config_idx, services) != 0 ||
			responder_add_all_services(m, MDNS_SECTION_AUTHORITIES,
				255, config_idx, services) != 0) {
			MDNS_LOG("Resource records don't fit into probe packet."
					);
			return -1;
		}
		/* Reset service name conflict flag */
		sn_conflict = false;
	}

	/* Populate the internal data structures of m to facilitate easy
	 * comparision of probe to a probe response later. Update the end
	 * pointer at the end of buffer so that packet can be grown if
	 * necessary.
	 */
	if (mdns_parse_message(m, VALID_LENGTH(m)) == -1) {
		while (1) ;
	}
	m->end = m->data + sizeof(m->data) - 1;
	return 0;
}
static int send_close_probe(int config_idx, struct mdns_service *services[])
{
	int ret;

	ret = prepare_announcement(&tx_msg, 0, config_idx, services);
	if (ret != 0)
		return -1;

	mr_stats.tx_bye++;
	return mdns_send_msg(&tx_msg, mc_sock, htons(5353), config_g[config_idx].iface_idx, 0);
}

int mdns_iface_group_state_change(netif_t iface, enum iface_mc_group_state state)
{
    int ret;
    mdns_ctrl_data msg;

    if (state == JOIN) {
        msg.cmd = MDNS_CTRL_IFACE_JOIN_GROUP;
    } else if (state == LEAVE) {
        msg.cmd = MDNS_CTRL_IFACE_LEAVE_GROUP;
    } else
        return ERR_MDNS_INVAL;

    msg.iface = iface;
    msg.service = NULL;
    ret = mdns_send_ctrl_msg(MDNS_CTRL_RESPONDER, &msg);
    return ret;
}

int mdns_iface_state_change(netif_t iface, enum iface_state state)
{
	int ret;
	mdns_ctrl_data msg;

	if (state == UP) {
		mr_stats.iface_up++;
		msg.cmd = MDNS_CTRL_IFACE_UP;
	} else if (state == REANNOUNCE) {
		mr_stats.iface_reannounce++;
		msg.cmd = MDNS_CTRL_REANNOUNCE;
	} else if (state == DOWN) {
		mr_stats.iface_down++;
		msg.cmd = MDNS_CTRL_IFACE_DOWN;
	} else
		return ERR_MDNS_INVAL;
	msg.iface = iface;
	msg.service = NULL;
	ret = mdns_send_ctrl_msg(MDNS_CTRL_RESPONDER, &msg);
	return ret;
}


/* Flags to ensure that A and AAAA type of address records are sent only once
 * per response. */
bool a_added;
#ifdef CONFIG_IPV6
bool aaaa_added;
#endif
/* Cache flush bit will be set to C_FLUSH, if response is to be sent on
 * Multicast address and to C_IN response is required to be sent on Unicast
 * address */
int c_flush;

#ifdef CONFIG_BONJ_CONFORMANCE
static int known_answer_suppression(struct mdns_message *rx,
		struct mdns_message *tx, int config_idx)
{
	int i;
	int ret = RS_ANS_NOT_FOUND;
	bool cur_tc_bit = false;
	struct mdns_resource *a;
	struct mdns_service **s;

	/* As per RFC 6762, section 7.2 - Multipacket
	 * Known-Answer Suppression, if TC(truncated) bit is
	 * set, device should prepare a response but must not
	 * send it untill all known-answers are received with
	 * TC bit resetted. */
	if (rx->header->flags.fields.tc) {
		ret = RS_NO_SEND;
		cur_tc_bit = true;
	}

	/* As per RFC 6762, section 7.1- Known-Answer
	 * suppression, if our PTR record is in known-answer
	 * list, and TTL is greater than half of the original
	 * TTL, do not send response.*/
	for (i = 0; i < rx->num_answers; i++) {
		a = &rx->answers[i];
		for (s = config_g[config_idx].services;
				s != NULL && *s != NULL; s++) {
			if (a->type == T_SRV) {
				if (dname_cmp(rx->data, a->name, NULL,
							(*s)->fqsn) == 0) {
					if (a->ttl > MDNS_TTL_THRESHOLD) {
						ret |= RS_NO_SEND;
						break;
					}
					if ((mdns_add_answer(tx, (*s)->fqsn,
							T_SRV, C_FLUSH,
							255) != 0) ||
							(mdns_add_srv(tx, 0, 0,
								(*s)->port,
								fqdn) != 0))
						return RS_ERROR;
					if (!cur_tc_bit)
						ret |= RS_SEND;
					else
						ret |= RS_NO_SEND;
				}
			}

			if (a->type == T_PTR) {
				/* Since PTR records are shared, one ought to
				 * see if the answer in the query is really
				 * device's, by comparing its service name.
				 */
				if ((dname_cmp(rx->data, a->name, NULL,
							(*s)->ptrname) == 0) &&
					(dname_cmp(rx->data, a->rdata, NULL,
							(*s)->fqsn) == 0)) {
					if (a->ttl > MDNS_TTL_THRESHOLD) {
						ret |= RS_NO_SEND;
						break;
					}
					if ((mdns_add_answer(tx, (*s)->ptrname,
							T_PTR, C_IN,
							255) != 0) ||
							(mdns_add_name(tx,
								fqdn) != 0))
						return RS_ERROR;
					if (!cur_tc_bit)
						ret |= RS_SEND;
					else
						ret |= RS_NO_SEND;
				}
			}
		}
	}
	return ret;
}
#endif

/* Function adds IPv4 address record of the device in response buffer(tx).
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 0 (RS_NO_SEND) set if query is not for this device
 *        or bit 1 (RS_SEND) set if a response needs to be sent
 */
static int mdns_add_a_response(struct mdns_message *tx, int config_idx)
{
	if (a_added)
		return RS_NO_SEND;

	uint32_t my_ipaddr = get_interface_ip(config_idx);

	if( my_ipaddr == 0 )
	    return RS_NO_SEND;

	if (mdns_add_answer(tx, fqdn, T_A, c_flush, 255) != 0 ||
			mdns_add_uint32_t(tx, htonl(my_ipaddr)) != 0)
		return RS_ERROR;

	a_added = true;
	return RS_SEND;
}

#ifdef CONFIG_IPV6
/* Function adds IPv6 address record of the device in response buffer(tx).
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 0 (RS_NO_SEND) set if query is not for this device
 *        or bit 1 (RS_SEND) set if a response needs to be sent
 */
static int mdns_add_aaaa_response(struct mdns_message *tx, int config_idx)
{
	if (aaaa_added)
		return RS_NO_SEND;

	if (mdns_add_aaaa_answer(tx, config_idx, 255) != 0)
		return RS_ERROR;

	aaaa_added = true;
	return RS_SEND;
}
#endif /* CONFIG_IPV6 */

/* Function adds PTR record of the device in response buffer(tx).
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 1 (RS_SEND) set if a response needs to be sent
 */
static int mdns_add_ptr_response(struct mdns_message *tx, uint8_t *name,
		uint8_t *value)
{
	if ((mdns_add_answer(tx, name, T_PTR, C_IN, 255) != 0) ||
			(mdns_add_name(tx, value) != 0))
		return RS_ERROR;
	return RS_SEND;
}

/* Function adds SRV record of the device in response buffer(tx).
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 1 (RS_SEND) set if a response needs to be sent
 */
static int mdns_add_srv_response(struct mdns_message *tx, uint8_t *name,
		int port, uint8_t *target)
{
	if ((mdns_add_answer(tx, name, T_SRV, c_flush, 255) != 0) ||
			(mdns_add_srv(tx, 0, 0, port, target) != 0))
		return RS_ERROR;
	return RS_SEND;
}

/* Function adds TXT record of the device in response buffer(tx).
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 1 (RS_SEND) set if a response needs to be sent
 */
static int mdns_add_txt_response(struct mdns_message *tx, uint8_t *name,
		char *keyvals, uint16_t kvlen)
{
	if ((mdns_add_answer(tx, name, T_TXT, c_flush, 255) != 0) ||
		(mdns_add_txt(tx, keyvals, kvlen) != 0))
		return RS_ERROR;
	return RS_SEND;
}

/* Function checks if the received A query is for this device.
 * If the query matches with device's RR, device responds with its AAAA record.
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 0 (RS_NO_SEND) set if query is not for this device
 *        and/or bit 1 (RS_SEND) set if a response needs to be sent
 */
static inline int mdns_handle_a_query(struct mdns_message *rx,
		struct mdns_message *tx, struct mdns_question *q,
		int config_idx)
{
	if (dname_cmp(rx->data, q->qname, NULL, fqdn) == 0)
		return mdns_add_a_response(tx, config_idx);
	else
		return RS_NO_SEND;
}

#ifdef CONFIG_IPV6
/* Function checks if the received AAAA query is for this device.
 * If the query matches with device's RR, device responds with its AAAA record.
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 0 (RS_NO_SEND) set if query is not for this device
 *        and/or bit 1 (RS_SEND) set if a response needs to be sent
 */
static inline int mdns_handle_aaaa_query(struct mdns_message *rx,
		struct mdns_message *tx, struct mdns_question *q,
		int config_idx)
{
	if (dname_cmp(rx->data, q->qname, NULL, fqdn) == 0)
		return mdns_add_aaaa_response(tx, config_idx);
	else
		return RS_NO_SEND;
}
#endif /* CONFIG_IPV6 */

/* Function checks if the received PTR query is for this device.
 * If the query matches with device's RR, device responds with set of records
 * depending on the type of the query.
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 0 (RS_NO_SEND) set if query is not for this device
 *        and/or bit 1 (RS_SEND) set if a response needs to be sent
 *        and/or bit 2 (RS_SEND_DELAY) set if the response needs to be sent
 *        with a delay
 */
static inline int mdns_handle_ptr_query(struct mdns_message *rx,
		struct mdns_message *tx, struct mdns_question *q,
		int config_idx)
{
	int ret = RS_NO_SEND;
	uint8_t in_addr_arpa[MDNS_INADDRARPA_LEN];
	struct mdns_service **s;

	ipaddr_to_inaddrarpa(ntohl(get_interface_ip(config_idx)), in_addr_arpa);

	/* If the request is for arpa address of the device, add only its PTR
	 * record in the response */
	if (dname_cmp(rx->data, q->qname, NULL, in_addr_arpa) == 0)
		return mdns_add_ptr_response(tx, in_addr_arpa, fqdn);

	/* If the request is for _services._dns-sd._udp.local, respond with
	 * PTR names of all services that we host.
	 */
	if (dname_cmp(rx->data, q->qname, NULL, services_dnssd.fqsn) == 0) {
		services_dnssd.flags |= SRV_ADDED;
		for (s = config_g[config_idx].services; s != NULL && *s != NULL;
				s++) {
			/* Add SRV only once */
			if (((*s)->flags & SRV_ADDED))
				continue;

			/* Add our PTR entry to the listing. This should have
			 * the fqsn of the services_mdns-sd but have the ptrname
			 * of this particular entry. */
			if (mdns_add_ptr_response(tx, services_dnssd.fqsn,
					(*s)->ptrname) == RS_ERROR)
				return RS_ERROR;
			(*s)->flags |= SRV_ADDED;

		}
		return RS_SEND_DELAY;
	}

	/* if the querier wants PTRs to services that device hosts, add
	 * following records
	 * 1) A
	 * 2) AAAA
	 * 3) SRV
	 * 4) PTR
	 * 5) TXT
	 */
	for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++) {
		if (dname_cmp(rx->data, q->qname, NULL, (*s)->ptrname) == 0) {
			/* As per RFC 6762, section 6 - Responding, In any case
			 * where there may be multiple responses, such as
			 * queries where the answer is a member of a shared
			 * resource record set, each responder SHOULD delay its
			 * response
			 */
			ret |= RS_SEND_DELAY;
			goto ptr_add;
		} else if (dname_cmp(rx->data, q->qname, NULL, (*s)->fqsn)
				== 0) {
ptr_add:
			if (mdns_add_a_response(tx, config_idx) == RS_ERROR)
				return RS_ERROR;

#ifdef CONFIG_IPV6
			if (mdns_add_aaaa_response(tx, config_idx) == RS_ERROR)
				return RS_ERROR;
#endif	/*	CONFIG_IPV6	*/

			if (!((*s)->flags & SRV_ADDED)) {
				if (mdns_add_srv_response(tx, (*s)->fqsn,
						(*s)->port, fqdn) == RS_ERROR)
					return RS_ERROR;
				(*s)->flags |= SRV_ADDED;
			}

			if (mdns_add_ptr_response(tx, (*s)->ptrname, (*s)->fqsn)
					== RS_ERROR)
				return RS_ERROR;

			if ((*s)->keyvals && !((*s)->flags & TXT_ADDED)) {
				if (mdns_add_txt_response(tx, (*s)->fqsn,
						(*s)->keyvals, (*s)->kvlen)
						== RS_ERROR)
					return RS_ERROR;
				(*s)->flags |= TXT_ADDED;
			}
			ret |= RS_SEND;
		}
	}
	return ret;
}

/* Function checks if the received SRV query is for this device.
 * If the query matches with device's RR, device responds with following records
 * pertaining to this device
 * 1) A
 * 2) AAAA
 * 3) SRV
 * 4) TXT
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 0 (RS_NO_SEND) set if query is not for this device
 *        and/or bit 1 (RS_SEND) set if a response needs to be sent
 */
static inline int mdns_handle_srv_query(struct mdns_message *rx,
		struct mdns_message *tx, struct mdns_question *q,
		int config_idx)
{
	int ret = RS_NO_SEND;
	struct mdns_service **s;

	/* Add T_A record */
	if (mdns_add_a_response(tx, config_idx) == RS_ERROR)
		return RS_ERROR;

#ifdef CONFIG_IPV6
	/* Add T_AAAA record */
	if (mdns_add_aaaa_response(tx, config_idx) == RS_ERROR)
		return RS_ERROR;
#endif	/*	CONFIG_IPV6	*/

	for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++) {
		if (dname_cmp(rx->data, q->qname, NULL, (*s)->fqsn) == 0) {
			/* Add T_SRV record if not already added */
			if (!((*s)->flags & SRV_ADDED)) {
				if (mdns_add_srv_response(tx, (*s)->fqsn,
						(*s)->port, fqdn) == RS_ERROR)
					return RS_ERROR;
				ret |= RS_SEND;
				(*s)->flags |= SRV_ADDED;
			}

			/* Add T_TXT record if not already added */
			if ((*s)->keyvals && !((*s)->flags & TXT_ADDED)) {
				if (mdns_add_txt_response(tx, (*s)->fqsn,
						(*s)->keyvals, (*s)->kvlen) ==
						RS_ERROR)
					return RS_ERROR;
				ret |= RS_SEND;
				(*s)->flags |= TXT_ADDED;
			}
		}
	}
	return ret;
}

/* Function checks if the received TXT query is for this device.
 * If the query matches with device's RR, device responds with following records
 * pertaining to this device
 * 1) TXT
 * 2) PTR
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 0 (RS_NO_SEND) set if query is not for this device
 *        and/or bit 1 (RS_SEND) set if a response needs to be sent
 */
static inline int mdns_handle_txt_query(struct mdns_message *rx,
		struct mdns_message *tx, struct mdns_question *q,
		int config_idx)
{
	int ret = RS_NO_SEND;
	struct mdns_service **s;

	for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++) {
		if (dname_cmp(rx->data, q->qname, NULL, (*s)->fqsn) == 0) {
			/* Add T_TXT record if not already added */
			if ((*s)->keyvals && !((*s)->flags & TXT_ADDED)) {
				if (mdns_add_txt_response(tx, (*s)->fqsn,
						(*s)->keyvals, (*s)->kvlen) ==
						RS_ERROR)
					return RS_ERROR;
				ret |= RS_SEND;
			}
			/* Send T_PTR record explicitly for each TXT query */
			if (q->qtype == T_TXT) {
				if (mdns_add_ptr_response(tx, (*s)->ptrname,
						(*s)->fqsn) == RS_ERROR)
					return RS_ERROR;
				ret |= RS_SEND;
			}
		}
	}
	return ret;
}

/* Function prepares response message in tx to the query received in rx.
 * Return RS_ERROR (-1) if error occurred while adding the record
 *        or bit 0 (RS_NO_SEND) set if query is not for this device and no
 *        response is required to be sent
 *        and/or bit 1 (RS_SEND) set if a response needs to be sent
 *        and/or bit 2 (RS_SEND_DELAY) set if the response needs to be sent
 *        with a delay
 */
static int prepare_response(struct mdns_message *rx, struct mdns_message *tx,
		int config_idx)
{
	int i, err;
	int ret = RS_NO_SEND;
	struct mdns_question *q;
	struct mdns_service **s;
#ifdef CONFIG_BONJ_CONFORMANCE
	static bool last_tc_bit;
	bool cur_tc_bit = false;

	if (rx->header->flags.fields.qr != QUERY)
		return RS_NO_SEND;

	/* As per specification in RFC 6762, section 6.7 - Legacy Unicast
	 * Responses, if the UDP port in a received Multicast DNS query is not
	 * port 5353, C_FLUSH bit should not be set.
	 */
	if (unicast_resp)
		c_flush = C_IN;
	else
#endif /* CONFIG_BONJ_CONFORMANCE */
		c_flush = C_FLUSH;

	mr_stats.rx_queries++;
#ifdef CONFIG_BONJ_CONFORMANCE
	/* Message should not be re-inited if TC bit was set in the last query,
	 * as this means response is buffered for last query. */
	if (!last_tc_bit)
#endif /* CONFIG_BONJ_CONFORMANCE */
		mdns_response_init(tx);
#ifdef CONFIG_BONJ_CONFORMANCE
	cur_tc_bit = rx->header->flags.fields.tc;
	/* If the query contains any answers RR, or TC(truncate) bit is set,
	 * it indicates that querier knows answer to the given query, hence
	 * call known_answer_suppression.
	 * */
	if (rx->num_answers || cur_tc_bit) {
		ret = known_answer_suppression(rx, tx, config_idx);
		last_tc_bit = cur_tc_bit;
		/* Return from the function only if device's record is found
		 * in the known answer section. Otherwise process the query.
		 */
		if (ret != RS_ANS_NOT_FOUND) {
			mr_stats.rx_known_ans++;
			return ret;
		}
		ret = RS_NO_SEND;
	}
#endif /* CONFIG_BONJ_CONFORMANCE */
	tx->header->id = rx->header->id;

	/* To prevent service's SRV and TXT records from being added multiple
	 * times, SRV_ADDED and TXT_ADDED flags are used. Begin with flags
	 * cleared.
	 */
	for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++)
		(*s)->flags &= ~(SRV_ADDED | TXT_ADDED);

	/* Begin with a_added and aaaa_added flags cleared. These flags prevent
	 * duplication of A and AAAA records in a response. */
	a_added = false;
#ifdef CONFIG_IPV6
	aaaa_added = false;
#endif /* CONFIG_IPV6 */

	for (i = 0; i < rx->num_questions; i++) {
		q = &rx->questions[i];

		if (q->qtype == T_ANY || q->qtype == T_A) {
			err = mdns_handle_a_query(rx, tx, q, config_idx);
			if (err == RS_ERROR)
				return RS_ERROR;
			else
				ret |= err;
		}

#ifdef CONFIG_IPV6
		if (q->qtype == T_ANY || q->qtype == T_AAAA) {
			err = mdns_handle_aaaa_query(rx, tx, q, config_idx);
			if (err == RS_ERROR)
				return RS_ERROR;
			else
				ret |= err;
		}
#endif	/*	CONFIG_IPV6	*/


		if (q->qtype == T_ANY || q->qtype == T_PTR) {
			err = mdns_handle_ptr_query(rx, tx, q, config_idx);
			if (err == RS_ERROR)
				return RS_ERROR;
			else
				ret |= err;
		}

		if (q->qtype == T_ANY || q->qtype == T_SRV) {
			err = mdns_handle_srv_query(rx, tx, q, config_idx);
			if (err == RS_ERROR)
				return RS_ERROR;
			else
				ret |= err;
		}

		if (q->qtype == T_ANY || q->qtype == T_TXT) {
			err = mdns_handle_txt_query(rx, tx, q, config_idx);
			if (err == RS_ERROR)
				return RS_ERROR;
			else
				ret |= err;
		}
	}
	return ret;
}
/* Build a response packet to check that everything will fit in the packet.
 * First need to build the query and then will simulate the response.
 */
static int check_max_response(struct mdns_message *rx, struct mdns_message *tx,
		int config_idx)
{
	uint32_t my_ipaddr;
	static uint8_t in_addr_arpa[MDNS_INADDRARPA_LEN];

	mdns_query_init(rx);
	my_ipaddr = get_interface_ip(config_idx);
	ipaddr_to_inaddrarpa(ntohl(my_ipaddr), in_addr_arpa);
	if (mdns_add_question(rx, fqdn, T_ANY, C_IN) != 0 ||
	    mdns_add_question(rx, in_addr_arpa, T_ANY, C_IN) != 0 ||
	    responder_add_all_services(rx, MDNS_SECTION_QUESTIONS, 0, config_idx
		    , NULL) != 0) {
		MDNS_LOG("Resource records don't fit into query of response packet."
				"");
		return -1;
	}

	if (mdns_parse_message(rx, VALID_LENGTH(rx)) != 0) {
		MDNS_LOG("Failed to parse biggest expected query.");
		return -1;
	}

	/* This function calls prepare_response() with a dummy
	 * Rx packet to verify response packet.
	 * Hence decrement the Rx query count which is incremented
	 * in prepare_response() assuming that the query packet
	 * has been received
	 */
	mr_stats.rx_queries--;
	if (prepare_response(rx, tx, config_idx) < RS_NO_SEND) {
		MDNS_LOG("Resource records don't fit into response packet.");
		return -1;
	}

	return 0;
}

static int validate_service(struct mdns_service *s)
{
	int maxlen;

	if (!valid_label(s->servname)) {
		MDNS_LOG("Invalid service name: %s", s->servname);
		return ERR_MDNS_INVAL;
	}

	if (!valid_label(s->servtype)) {
		MDNS_LOG("Invalid service type: %s", s->servtype);
		return ERR_MDNS_INVAL;
	}

	if (s->proto != MDNS_PROTO_TCP && s->proto != MDNS_PROTO_UDP) {
		MDNS_LOG("Invalid proto: %d", s->proto);
		return ERR_MDNS_INVAL;
	}

	/* +1 byte for label len */
	maxlen = strlen(s->servname) + 1;
	/* +1 byte for label len and +1 byte for leading '_' */
	maxlen += strlen(s->servtype) + 2;
	/* +4 bytes for either "_tcp" or "_udp" string */
	maxlen += 4;
	maxlen += strlen(domname) + 1;
	/* If servname is not unique, '-2' or '-3' etc might be needed to be
	 * added */
	maxlen += 2;
	/* +1 byte for terminating 0 */
	maxlen += 1;
	if (maxlen > MDNS_MAX_NAME_LEN)
		return ERR_MDNS_TOOBIG;

	return 0;
}
int mdns_verify_service(struct mdns_service *services[], netif_t iface)
{
	int config_idx = -1, i, num_services = 0, ret;
	struct mdns_service **tmp_services = services;

	if (iface == INTERFACE_NONE || services == NULL)
		return ERR_MDNS_INVAL;

	/* Check if interface is already registered.
	 * A decrementing loop is required so that first config_idx is always
	 * 0. */
	for (i = MDNS_MAX_SERVICE_CONFIG - 1; i >= 0; i--) {
		if (config_g[i].iface_idx == iface) {
			config_idx = i;
			break;
		} else if (config_g[i].iface_idx == INTERFACE_NONE) {
			config_idx = i;
		}
	}

	if (config_idx == -1)
		/* No. of interfaces exhausted */
		return kGeneralErr;

	/* Get no. of services in an array to check if enough
	 * space is available to accommodate them */
	while (services[num_services++] != NULL)
		;

	if (--num_services > (MAX_MDNS_LST - config_g[config_idx].num_services))
		/* Service list is exhausted */
		return kGeneralErr;

	for (; *tmp_services != NULL; tmp_services++) {
		ret = validate_service(*tmp_services);
		if (ret != 0)
			return ret;

		reset_fqsn(*tmp_services);
	}

	reset_fqsn(&services_dnssd);
	ret = prepare_probe(&tx_msg, config_idx, services);
	if (ret == -1)
		return ERR_MDNS_TOOBIG;

	if (max_probe_growth(num_services) > TAILROOM(&tx_msg)) {
		MDNS_DBG("Insufficient space for host name and service names");
		return ERR_MDNS_TOOBIG;
	}

	ret = check_max_response(&rx_msg, &tx_msg, config_idx);
	if (ret == -1) {
		MDNS_DBG("Insufficient space for host name and service names in"
				"response");
		return ERR_MDNS_TOOBIG;
	}
	if (max_response_growth(num_services) > TAILROOM(&tx_msg)) {
		MDNS_DBG("Insufficient growth for host name and service names in"
				"response");
		return ERR_MDNS_TOOBIG;
	}

	return MICO_SUCCESS;

}


/* Function registers services as well as interface in config_g */
int mdns_add_service_iface(struct mdns_service *services[], netif_t iface)
{
	int i, config_idx = 255, srv_idx;

	if (iface == INTERFACE_NONE || services == NULL)
		return ERR_MDNS_INVAL;

	for (i = MDNS_MAX_SERVICE_CONFIG - 1; i >= 0; i--) {
		if (config_g[i].iface_idx == iface) {
			config_idx = i;
			break;
		} else if (config_g[i].iface_idx == INTERFACE_NONE) {
			config_idx = i;
		}
	}

	if( config_idx == 255 )  return ERR_MDNS_TOOBIG;

	config_g[config_idx].iface_idx = iface;

	/* Now add services to global config structure */
	for (srv_idx = 0; srv_idx < MAX_MDNS_LST && *services; srv_idx++) {
		if (config_g[config_idx].
				services[srv_idx] == NULL) {
			config_g[config_idx].services[srv_idx] = *services;
			config_g[config_idx].num_services++;
			services++;
		}
	}

	return MICO_SUCCESS;
}


int mdns_remove_service_iface(struct mdns_service *services[], netif_t iface)
{
	int config_idx;
	/* Pointer to service list inside mdns_service_config structure */
	struct mdns_service **services_g;

	if (iface == INTERFACE_NONE || services == NULL)
		return ERR_MDNS_INVAL;

	/* Get configuration index corresponding to given interface */
	config_idx = get_config_idx_from_iface(iface);

	if (config_idx == kGeneralErr)
		return ERR_MDNS_INVAL;

	/* Find service from array and remove it */
	for (; *services != NULL; services++) {
		/* Initialize it to the beginning of service list. */
		services_g = config_g[config_idx].services;
		for (; *services_g != NULL; services_g++) {
			if (*services == *services_g) {
				/* When services are deannounced, clear fqsn
				 * array for that particular service as well.
				 * If the service is reannounced with a new
				 * name, not clearing it might result in
				 * inappropriate name*/
				memset((*services)->fqsn, 0,
						sizeof((*services)->fqsn));
				/* Shift all valid services to one place left.
				 * This overwrites existing service with the
				 * next service in the list.
				 * This is required so that NULL in the list
				 * indicates end of list.
				 * It allows service traversing to be terminated
				 * at NULL, instead of looping through entire
				 * list. */
				for (; *services_g != NULL; services_g++)
					*services_g = *(services_g + 1);

				config_g[config_idx].num_services--;
				break;
			}
		}
	}

	if (config_g[config_idx].num_services <= 0) {
		interface_state[config_idx] = STOPPED;
		config_g[config_idx].iface_idx = INTERFACE_NONE;
	}

	return MICO_SUCCESS;
}

int mdns_remove_all_services(netif_t iface)
{
	int i, config_idx;

	if (iface == INTERFACE_NONE)
		return ERR_MDNS_INVAL;

	config_idx = get_config_idx_from_iface(iface);

	if (config_idx == kGeneralErr)
		return ERR_MDNS_INVAL;

	for (i = 0; i < MAX_MDNS_LST; i++) {
		config_g[config_idx].services[i] = NULL;
	}

	config_g[config_idx].num_services = 0;
	config_g[config_idx].iface_idx = INTERFACE_NONE;
	interface_state[config_idx] = STOPPED;

	return MICO_SUCCESS;
}

/* Whenever hostname conflict is encountered, increment
 * 1) hostname
 * 2) All registered service names.
 * \return 1, if appropriate host name is incremented successfully
 * \return -1, if error occurred when incrementing host name
 */
static int handle_hostname_conflict(int config_idx)
{
	int len, dname_inc, ret = 1;
	struct mdns_service **s;

	/* Increment domain name */
	if (dname_increment(fqdn) == -1)
		ret = -1;

	/* Increment all announced service name as well */
	for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++) {
		len = dname_size((*s)->fqsn);
		if (dname_increment((*s)->fqsn) == -1)
			ret = -1;
		/* remember that we must increment the pointer to the fully
		 * qualified service type if we added bytes to the service name.
		 */
		dname_inc = dname_size((*s)->fqsn) - len;
		if (dname_inc > 0)
			(*s)->ptrname += dname_inc;
	}

	mr_stats.rx_hn_conflicts++;
	return ret;
}

/* Whenever service name conflict is encountered, increment
 * 1) hostname
 * 2) service name to which conflict is received
 * \return 1, if appropriate service name is incremented successfully
 * \return -1, if error occurred when incrementing service name
 */
static int handle_srvname_conflict(struct mdns_service *s)
{
	int len, dname_inc, ret = 1;

	if (dname_increment(fqdn) == -1)
		ret = -1;

	len = dname_size(s->fqsn);
	if (dname_increment(s->fqsn) == -1)
		ret = -1;

	/* Set service name conflict flag */
	sn_conflict = true;

	/* remember that we must increment the pointer to the fully qualified
	 * service type if we added bytes to the service name.
	 */
	dname_inc = dname_size(s->fqsn) - len;
	if (dname_inc > 0)
		s->ptrname += dname_inc;

	mr_stats.rx_sn_conflicts++;
	return ret;
}

/* Resolve conflicts received in the form of service probe from other
 * devices.
 * \return 0, when no host/service name conflict is found
 * \return 1, if host/service name conflict is found and appropriate names are
 * incremented successfully
 * \return -1, if host/service name conflict is found but error occurred when
 * incrementing either record or we have tried all possible records
 */
static int fix_probe_conflicts(struct mdns_message *m, struct mdns_message *p,
		int config_idx, int state)
{
	struct mdns_question *q;
	struct mdns_service **s;
	int ret = 0, i, cmp;

	mr_stats.rx_queries++;
	/* Check the authorities of service names only once because
	 * it is quite costly.  SERVICE_CHECKED_FLAG is used to keep track if
	 * service has been checked or not.
	 */
	for (s = config_g[config_idx].services; s != NULL && *s != NULL; s++)
		(*s)->flags &= ~(SERVICE_CHECKED_FLAG);

	for (i = 0; i < m->num_questions; i++) {
		q = &m->questions[i];
		/* Other devices are allowed to have same PTR records.
		 * Hence ignore it.
		 */
		if (q->qtype == T_PTR)
			continue;

		/* Check for hostname conflict */
		if (dname_cmp(m->data, q->qname, NULL, fqdn) == 0) {
			/* When conflict is found, check authorities */
			cmp = authority_set_cmp(m, p, fqdn);
			if (cmp == 1)
				/* Other device's authoritative RR is greater
				 * than ours, hence increment our hostname. */
				ret = handle_hostname_conflict(config_idx);
		}

		/* Check for any service name conflict */
		for (s = config_g[config_idx].services; s != NULL && *s != NULL;
				s++) {
			if ((*s)->flags & SERVICE_CHECKED_FLAG)
				continue;
			if (dname_cmp(m->data, q->qname, NULL, (*s)->fqsn)
					== 0) {
				/* When conflict is found, check authorities */
				cmp = authority_set_cmp(m, p, (*s)->fqsn);
				if (cmp == 1)
					/* Other device's authoritative RR is
					 * greater than ours */
					ret = handle_srvname_conflict(*s);
				(*s)->flags |= SERVICE_CHECKED_FLAG;
			}
		}
	}
	return ret;
}

/* Resolve conflicts received in the form of service announcements from other
 * devices.
 * \return 0, when no host/service name conflict is found
 * \return 1, if host/service name conflict is found and appropriate names are
 * incremented successfully
 * \return -1, if host/service name conflict is found but error occurred when
 * incrementing either record or we have tried all possible records
 */
static int fix_announcement_conflicts(struct mdns_message *m,
		struct mdns_message *p, int config_idx, int state)
{
	struct mdns_resource *r;
	struct mdns_service **s;
	int ret = 0, i;
	bool defend = false;

	/* Conflict aries after services have been announced. Could be a n/w
	 * echo. Defend our original announcement. */
	if (state == READY_TO_RESPOND)
		defend = true;

	mr_stats.rx_answers++;
	for (i = 0; i < m->num_answers; i++) {
		r = &m->answers[i];
		/* Check for host name conflicts */
		if ((r->type == T_A
#ifdef CONFIG_IPV6
				|| r->type == T_AAAA
#endif /* CONFIG_IPV6 */
				) && (dname_cmp(m->data, r->name, NULL, fqdn)
				== 0)) {
			/* Defend original name first */
			if (defend)
				return 1;
			/* try a different name */
			ret = handle_hostname_conflict(config_idx);
		}
		/* Check for service name conflicts */
		if (r->type == T_SRV) {
			for (s = config_g[config_idx].services; s != NULL &&
					*s != NULL; s++) {
				if (dname_cmp(m->data, r->name, NULL,
							(*s)->fqsn) == 0) {
					/* Defend original name first */
					if (defend)
						return 1;
					/* try a different service name */
					ret = handle_srvname_conflict(*s);
				}
			}
		}
	}
	return ret;
}

/* Function resolves conflict of an incoming message m with our local resource
 * record. Conflict can be of two types:
 * 1) Query:
 *	- Other device is also querying for the same record
 *	- Function will serve to such conflicts only when device is in query
 *	  state.
 *	- Function will call fix_probe_conflicts to handle such conflicts.
 * 2) Response:
 *	- Other device has already announced the same record and replies with
 *	  appropriate response.
 *	- Function will serve to such conflicts either during query phase or
 *	  after device has already announced its records.
 *	- Function will call fix_announcement_conflicts to handle such conflicts
 */
static int fix_response_conflicts(struct mdns_message *m,
				  struct mdns_message *p, int config_idx,
				  int state)
{
	int ret = 0;

	if (m->header->flags.fields.qr == QUERY && m->num_authorities > 0)
		ret = fix_probe_conflicts(m, p, config_idx, state);
	else if (m->header->flags.fields.qr == RESPONSE)
		ret = fix_announcement_conflicts(m, p, config_idx, state);

	if (ret == 1) {
		MDNS_DBG("Responder detected conflict with this response:");
		debug_print_message(m);
		//MDNS_DBG("");
	}

	return ret;
}

static int send_init_probes(int idx, int *state, int *event,
		     struct timeval *probe_wait_time,
		     struct mdns_service *services[]);
static int responder_state_machine(struct sockaddr_storage from, int *state,
				   int *event, int config_idx,
				   struct timeval *probe_wait_time)
{
	int ret;
	int rand_time = 0;
	struct sockaddr_in *from_v4 = (struct sockaddr_in *)&from;

#ifdef CONFIG_BONJ_CONFORMANCE
	/* As per specification in RFC 6762, section 6.7 - Legacy Unicast
	 * Responses, if the UDP port in a received Multicast DNS query is not
	 * port 5353, this indicates that querier is simple resolver which does
	 * not fully implement all of Multicast DNS. In this case, Multicast
	 * DNS responder MUST send UDP response directly back to the querier.
	 */
	if (ntohs(from.sin_port) != 5353)
		unicast_resp = true;
	else
		unicast_resp = false;
#endif /* CONFIG_BONJ_CONFORMANCE */

	/* We will be in below state machine only after above is completed */
	switch (state[config_idx]) {
	case READY_TO_RESPOND:
		if (event[config_idx] == EVENT_RX) {
			/* If conflicting announcement is received after our
			 * announcement, we should check for conflict and resend
			 * initial probes again, if required. */
			if (rx_msg.header->flags.fields.qr == RESPONSE) {
				mr_stats.rx_answers++;
				int tt = fix_response_conflicts(&rx_msg,
						&tx_msg, config_idx,
						state[config_idx]);
				/* fix_response_conflicts returns 1 or -1 on a
				 * conflict 0 is returned when no conflict is
				 * found.*/
				if (tt != 0) {
					state[config_idx] = INIT;
					send_init_probes(config_idx, state,
							event, probe_wait_time,
							NULL);
				}
			}
			/* prepare a response if necessary */
			ret = prepare_response(&rx_msg, &tx_msg, config_idx);
			if (ret <= RS_NO_SEND) {
				break;
			}
			if (ret & RS_SEND_DELAY) {
				/* Implment random delay from 20-120msec */
				MDNS_DBG("delaying response\r\n");
				rand_time = mdns_rand_range(100);
				SET_TIMEOUT(&probe_wait_time[config_idx],
					    (rand_time + 20));
				state[config_idx] = READY_TO_SEND;
			} else if (ret & RS_SEND) {
				/* We send immedately */
				MDNS_DBG("responding to query:\r\n");
				debug_print_message(&rx_msg);
				mr_stats.tx_response++;
#ifdef CONFIG_BONJ_CONFORMANCE
				if (unicast_resp) {
					mdns_send_msg(&tx_msg, mc_sock, -1,
							from.sin_port,
							config_g[config_idx].
							iface_idx,
							from.sin_addr.s_addr);
				} else {
#endif /* CONFIG_BONJ_CONFORMANCE */
					mdns_send_msg(&tx_msg, mc_sock, from_v4->sin_port,
						config_g[config_idx].
						iface_idx, 0);
#ifdef CONFIG_BONJ_CONFORMANCE
				}
#endif /* CONFIG_BONJ_CONFORMANCE */
			}
		}
		break;

	case READY_TO_SEND:
			/* We send, no matter if triggered by timeout or RX */
		MDNS_DBG("responding to query:\r\n");
		debug_print_message(&rx_msg);
		mr_stats.tx_response++;
#ifdef CONFIG_BONJ_CONFORMANCE
		if (unicast_resp) {
			mdns_send_msg(&tx_msg, mc_sock, -1,
					from.sin_port,
					config_g[config_idx].iface_handle,
					from.sin_addr.s_addr);
		} else {
#endif /* CONFIG_BONJ_CONFORMANCE */
			mdns_send_msg(&tx_msg, mc_sock,
			              from_v4->sin_port, config_g[config_idx].
			              iface_idx, 0);
#ifdef CONFIG_BONJ_CONFORMANCE
		}
#endif /* CONFIG_BONJ_CONFORMANCE */

		if (event[config_idx] == EVENT_TIMEOUT) {
			/* Here due to timeout, so we're done, go back to RTR */
			SET_TIMEOUT(&probe_wait_time[config_idx], 0);
			state[config_idx] = READY_TO_RESPOND;
		} else if (event[config_idx] == EVENT_RX) {

			/* prepare a response if necessary */
			ret = prepare_response(&rx_msg, &tx_msg, config_idx);
			/* Error or otherwise no response, go back to RTR */
			if (ret <= RS_NO_SEND) {
				SET_TIMEOUT(&probe_wait_time[config_idx], 0);
				state[config_idx] = READY_TO_RESPOND;
				} else if (ret & RS_SEND_DELAY) {
				MDNS_DBG("delaying response\r\n");
				/* Implement random delay from 20-120msec */
				rand_time = mdns_rand_range(100);
				SET_TIMEOUT(&probe_wait_time[config_idx],
					    (rand_time + 20));
				state[config_idx] = READY_TO_SEND;
			} else if (ret & RS_SEND) {
				/* We send immedately */
				MDNS_DBG("responding to query:\r\n");
				debug_print_message(&rx_msg);
				mr_stats.tx_response++;
#ifdef CONFIG_BONJ_CONFORMANCE
				if (unicast_resp) {
					mdns_send_msg(&tx_msg, mc_sock, -1,
							from.sin_port,
							config_g[config_idx].
							iface_handle,
							from.sin_addr.s_addr);
				} else {
#endif /* CONFIG_BONJ_CONFORMANCE */
					mdns_send_msg(&tx_msg, mc_sock,
						from_v4->sin_port, config_g[config_idx].
						iface_idx, 0);
#ifdef CONFIG_BONJ_CONFORMANCE
				}
#endif /* CONFIG_BONJ_CONFORMANCE */

				/* No longer have a message queued up,
				 * so go back to RTR */
				SET_TIMEOUT(&probe_wait_time[config_idx], 0);
				state[config_idx] = READY_TO_RESPOND;
			}
		}
			break;
	}
	return 0;
}

/* We're in a probe state sending the probe tx, and we got a probe response
 * (rx).  Process it and return the next state.  Also, update the timeout with
 * the time until the next event.  state must be one of the probe states!
 */
static int process_probe_resp(struct mdns_message *tx, struct mdns_message *rx,
		int state, struct timeval *timeout, int config_idx)
{
	int ret;

	ret = fix_response_conflicts(rx, tx, config_idx, state);
	if (ret == 1) {
		SET_TIMEOUT(timeout, MDNS_INTER_PROBE_INTERVAL);
		return INIT;
	} else if (ret == -1) {
		/* We have tried more than 10 names. Now we have
		 * to limit the probe rate to 1probe per secound.
		 */
		MDNS_DBG("Responder tried more than 10 possible names. Limiting the"
		    " probe rate");
		SET_TIMEOUT(timeout, 1100);
		return INIT;
	} else {
		/* this was an unrelated message.  Remain in the same state.
		 * Assume the caller has recalculated the timeout properly.
		 */
		return state;
	}
}

int probe_state_machine(int idx, int *state, int *event,
		     struct timeval *probe_wait_time,
		     struct mdns_service *services[], struct sockaddr_storage from)
{
	int ret;
	struct sockaddr_in *from_v4 = (struct sockaddr_in *)&from;
	switch (state[idx]) {
	case INIT:
		if (event[idx] == EVENT_TIMEOUT) {
			mr_stats.tx_probes_curr = 0;
			mr_stats.tx_announce_curr = 0;
			mr_stats.probe_rx_events_curr = 0;
			MDNS_DBG("Sending probe #%d to index = %d\r\n", \
				state[idx] + 1, idx);
			prepare_probe(&tx_msg, idx, services);
			mr_stats.tx_probes++;
			mr_stats.tx_probes_curr++;
			mdns_send_msg(&tx_msg, mc_sock, htons(5353),
				config_g[idx]. iface_idx, 0);
			SET_TIMEOUT(&probe_wait_time[idx],
				MDNS_INTER_PROBE_INTERVAL);
			state[idx] = FIRST_PROBE_SENT;
		}
		break;

	case FIRST_PROBE_SENT:
	case SECOND_PROBE_SENT:
		if (event[idx] == EVENT_TIMEOUT) {
			MDNS_DBG("Sending probe #%d to index = %d\r\n", \
				state[idx] + 1, idx);
			prepare_probe(&tx_msg, idx, services);
			mr_stats.tx_probes++;
			mr_stats.tx_probes_curr++;
			mdns_send_msg(&tx_msg, mc_sock,	htons(5353),
				config_g[idx].iface_idx, 0);
			SET_TIMEOUT(&probe_wait_time[idx],
				MDNS_INTER_PROBE_INTERVAL);
			state[idx] = state[idx] + 1;
		} else if (event[idx] == EVENT_RX && from_v4->sin_addr.s_addr != get_interface_ip(idx)) {
			mr_stats.probe_rx_events_curr++;
			state[idx] =
				process_probe_resp(&tx_msg, &rx_msg, state[idx],
				&probe_wait_time[idx], idx);
		}
		break;

	case THIRD_PROBE_SENT:
		if (event[idx] == EVENT_TIMEOUT) {
			/* Okay.  We now own our name.  Announce it. */
			ret = prepare_announcement(&tx_msg, 255, idx,
					services);
			if (ret != 0)
				break;
			mr_stats.tx_announce++;
			mr_stats.tx_announce_curr++;
			/* Set Current timeout to infinite */
			SET_TIMEOUT(&probe_wait_time[idx], 0);
			state[idx] = READY_TO_RESPOND;

			mdns_send_msg(&tx_msg, mc_sock, htons(5353), config_g[idx].iface_idx, 0);
			mico_rtos_delay_milliseconds(100);
			mdns_send_msg(&tx_msg, mc_sock, htons(5353), config_g[idx].iface_idx, 0);
			mico_rtos_delay_milliseconds(200);
			mdns_send_msg(&tx_msg, mc_sock, htons(5353), config_g[idx].iface_idx, 0);
			mico_rtos_delay_milliseconds(500);
			mdns_send_msg(&tx_msg, mc_sock, htons(5353), config_g[idx].iface_idx, 0);
			mico_rtos_delay_milliseconds(1000);
			mdns_send_msg(&tx_msg, mc_sock, htons(5353), config_g[idx].iface_idx, 0);

		} else if (event[idx] == EVENT_RX && from_v4->sin_addr.s_addr != get_interface_ip(idx)) {
			mr_stats.probe_rx_events_curr++;
			state[idx] = process_probe_resp(&tx_msg, &rx_msg, state[idx], &probe_wait_time[idx], idx);
		}
		break;
	}
	return 0;
}
/*
 * Returns
 * -WM_FAIL if length of Rx packet is less than 0, which means there is some
 *	error in Rx
 * -WM_FAIL if there is any error in parsing Rx message
 * -WM_FAIL if the value of sock is invalid (neither mc_sock, nor mc_sock6)
 */
int process_inter_probe_rx(int sock, int *state, int *event,
	     struct timeval *probe_wait_time, struct mdns_service *services[],
	     struct sockaddr_storage from)
{
	int len, config_idx, ret;
	socklen_t in_size = sizeof(struct sockaddr_storage);
	struct sockaddr_in *from_v4 = (struct sockaddr_in *)&from;
#ifdef CONFIG_IPV6
	struct sockaddr_in6 *from_v6 = (struct sockaddr_in6 *)&from;
#endif

	len = recvfrom(sock, (char *)rx_msg.data, sizeof(rx_msg.data),
		MSG_DONTWAIT, (struct sockaddr *)&from, &in_size);
	if (len < 0) {
		MDNS_LOG("responder failed to recv packet");
		return kGeneralErr;
	}

#ifdef CONFIG_IPV6
	if (IS_IPV4_MAPPED_IPV6(from_v6)) {
	    UNMAP_IPV4_MAPPED_IPV6(from_v4, from_v6);
	    config_idx = get_config_idx_from_ip(from_v4->sin_addr.s_addr);
	} else {
	    config_idx = get_config_idx_from_iface(INTERFACE_STA);
	}
#else
	config_idx = get_config_idx_from_ip(from_v4->sin_addr.s_addr);
#endif

    if (config_idx == -1) {
		mr_stats.rx_errors++;
		return kGeneralErr;
    }
	mr_stats.total_rx++;

	ret = mdns_parse_message(&rx_msg, len);
	if (ret != 0) {
		mr_stats.rx_errors++;
		return kGeneralErr;
	}
	if (interface_state[config_idx] == STARTED ||
		interface_state[config_idx] == RUNNING)
		event[config_idx] = EVENT_RX;

	/* Check if packet is from interface where mdns is in RUNNING state*/
	if (interface_state[config_idx] == RUNNING) {
		responder_state_machine(from, state, event, config_idx,
			probe_wait_time);
		return MICO_SUCCESS;
	}
	return probe_state_machine(config_idx, state, event, probe_wait_time,
		services, from);
}

/* Probing and Announcing on Startup
 * RFC 6762 Section 8
 *
 * This function carries out initial probe/announcement sequence of mDNS
 * responder.
 * Here is the basic workflow:
 * - Device sends three mDNS probe packets separated by minimum time
 *   specified by RFC 6762 (250ms)
 * - If there exists a host with conflicting resource record(s), then
 *   that host would respond with conflicting mDNS response. Then the
 *   conflict resolution happens at device. And probe sequence restarts
 * - If there were no conflicts received during successive three probes,
 *   then send out couple of mDNS announcements
 * - During probe sequence, device must not respond to any mDNS queries.
 *
 * Notes:
 * - Following function prepares mDNS probe packet, and calls mdns_send_msg()
 *   which can send probe packet over IPv4 and IPv6 (if enabled) networks.
 * - Following are the important events which need to be handled by this
 *   function in order of priority:
 *   -- Timeout occurred an interface other than the one in probe sequence.
 *      This can happen when timeout of that interface is set in
 *      responder_state_machine() while processing Rx packet on that interface
 *   -- Time for transmitting next probe/announcement
 *   -- Incoming packet on IPv4 and IPv6 (if enabled) sockets
 *
 */
static int send_init_probes(int idx, int *state, int *event,
	struct timeval *probe_wait_time, struct mdns_service *services[])
{
	uint32_t start_wait, stop_wait;
	int rand_time = 0;
	int i, min_idx, active_fds;
	struct timeval *timeout;
	bool is_timeout[MDNS_MAX_SERVICE_CONFIG];
	fd_set fds;
	struct sockaddr_storage from;

	mico_rtos_lock_mutex(&mdns_mutex);
	/* Per RFC 6762 Section 8.1, wait for random ammount of time
	 * between 0 and 250 ms before the first probe.
	 */
	rand_time = mdns_rand_range(250);
	SET_TIMEOUT(&probe_wait_time[idx], rand_time);
	interface_state[idx] = STARTED;
	while (state[idx] != READY_TO_RESPOND) {
		FD_ZERO(&fds);
		FD_SET(mc_sock, &fds);

		min_idx = get_min_timeout(probe_wait_time,
				MDNS_MAX_SERVICE_CONFIG);
		if (get_msec_time(probe_wait_time[min_idx]) != 0)
			timeout = &probe_wait_time[min_idx];
		else
			timeout = NULL;

		for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++)
			is_timeout[i] = false;

		start_wait = mdns_time_ms();

		active_fds = select(mc_sock + 1, &fds, NULL, NULL, timeout);

		stop_wait = mdns_time_ms();
		if (active_fds < 0)
			MDNS_LOG("error: select() failed: %d", active_fds);

		/* Check whether the timeout has occurred on each of the
		 * interface */
		for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
			if ((timeout != NULL) && (stop_wait - start_wait) >=
				get_msec_time(probe_wait_time[i])) {
				is_timeout[i] = true;
			}
		}

		update_all_timeout(probe_wait_time, start_wait, stop_wait);
		/* Process the interface timeouts */
		for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
			if (is_timeout[i] == true) {
				event[idx] = EVENT_TIMEOUT;
				if (i == idx) {
					probe_state_machine(idx, state,
						event, probe_wait_time,
						services, from);
				} else if (interface_state[min_idx] == \
					RUNNING) {
					responder_state_machine(from, state, \
					event, min_idx, probe_wait_time);
				}
			}
		}

		/* Process the incoming packets */
		if (FD_ISSET(mc_sock, &fds)) {
			process_inter_probe_rx(mc_sock, state, event,
				probe_wait_time, services, from);
		}

	}
	mico_rtos_unlock_mutex(&mdns_mutex);
	interface_state[idx] = RUNNING;
	return event[idx];
}

static inline void mdns_ctrl_halt()
{
	int i, ret;
	/* Send the goodbye packet.  This is same as announcement, but with a
	 * TTL of 0
	 */
	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		if (interface_state[i] != STOPPED) {
			ret = prepare_announcement(&tx_msg, 0, i, NULL);
			if (ret != 0) {
				MDNS_LOG("Error halting responder.");
				return;
			}
			mr_stats.tx_bye++;
			mdns_send_msg(&tx_msg, mc_sock, htons(5353), config_g[i].iface_idx, 0);
		}
		interface_state[i] = STOPPED;
	}
	MDNS_LOG("responder done.");
	responder_enabled = 0;
}

static inline void mdns_ctrl_iface_join_group(netif_t iface)
{
    struct ip_mreq mc;
    unsigned char ttl = 255;

    if( mc_sock == -1 ) return;

    /* join multicast group */
    mc.imr_multiaddr = mdns_mquery_v4group.sin_addr;
    mc.imr_interface = in_addr_any;
    if (setsockopt(mc_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mc, sizeof(mc)) < 0) {
        MDNS_LOG("error: failed to join multicast group");
    };

    /* set other IP-level options */
    if (setsockopt(mc_sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl)) < 0) {
        MDNS_LOG("error: failed to set multicast TTL");
    }

#ifdef CONFIG_IPV6
    struct ipv6_mreq mc6;

    /* join IPv6 mld group */
    mc6.ipv6mr_interface = INTERFACE_STA;
    memcpy(&mc6.ipv6mr_multiaddr, &mdns_mquery_v6group.sin6_addr, sizeof(struct in6_addr));
    if (setsockopt(mc_sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mc6, sizeof(mc6)) < 0) {
        MDNS_LOG("error: failed to join IPv6 mld group");
    };
#endif
}

static inline void mdns_ctrl_iface_leave_group(netif_t iface)
{
    struct ip_mreq mc;
    unsigned char ttl = 255;

    if( mc_sock == -1 ) return;

    /* join multicast group */
    mc.imr_multiaddr = mdns_mquery_v4group.sin_addr;
    mc.imr_interface = in_addr_any;
    if (setsockopt(mc_sock, IPPROTO_IP, IP_DROP_MEMBERSHIP, &mc, sizeof(mc)) < 0) {
        MDNS_LOG("error: failed to join multicast group");
    };

    /* set other IP-level options */
    if (setsockopt(mc_sock, IPPROTO_IP, IP_MULTICAST_TTL, (char *)&ttl, sizeof(ttl)) < 0) {
        MDNS_LOG("error: failed to set multicast TTL");
    }

#ifdef CONFIG_IPV6
    struct ipv6_mreq mc6;

    /* join IPv6 mld group */
    mc6.ipv6mr_interface = INTERFACE_STA;
    memcpy(&mc6.ipv6mr_multiaddr, &mdns_mquery_v6group.sin6_addr, sizeof(struct in6_addr));
    if (setsockopt(mc_sock, IPPROTO_IPV6, IPV6_LEAVE_GROUP, &mc6, sizeof(mc6)) < 0) {
        MDNS_LOG("error: failed to join IPv6 mld group");
    };
#endif
}

static inline void mdns_ctrl_iface_up(netif_t iface, int *state, int *event,
		struct timeval *probe_wait_time)
{
	int config_idx = get_config_idx_from_iface(iface);
	if (config_idx == kGeneralErr) {
		MDNS_LOG("No interface found.");
		return;
	}
	if (interface_state[config_idx] != STOPPED) {
		MDNS_LOG("Interface already in RUNNING state.");
		return;
	}
	state[config_idx] = INIT;
	send_init_probes(config_idx, state, event, probe_wait_time, NULL);
}

static inline void mdns_ctrl_iface_down(netif_t iface)
{
	int config_idx = get_config_idx_from_iface(iface);
	if (config_idx == kGeneralErr) {
		MDNS_LOG("No interface found.");
		return;
	}
	if (interface_state[config_idx] == STOPPED) {
		MDNS_LOG("Interface already in STOPPED state.");
		return;
	}
	send_close_probe(config_idx, NULL);
	interface_state[config_idx] = STOPPED;
}

static inline void mdns_ctrl_reannounce(netif_t iface, int *state)
{
	int i, ret;
	int config_idx = get_config_idx_from_iface(iface);
	if (config_idx == kGeneralErr) {
		MDNS_LOG("No interface found.");
		return;
	}
	/* RFC 6762 Section 8.4 */
	ret = prepare_announcement(&tx_msg, 255, config_idx, NULL);
	if (ret != 0) {
		MDNS_LOG("Failed to reannounce service(s).");
		return;
	}
	/* Send atleast two announcements, 1 seconds apart */
	for (i = 0; i < 2; i++) {
		mr_stats.tx_reannounce++;
		mdns_send_msg(&tx_msg, mc_sock, htons(5353), config_g[config_idx].iface_idx, 0);
		if (i < 1)
			mico_rtos_delay_milliseconds(1000);
	}
	state[config_idx] = READY_TO_RESPOND;
}

static inline void mdns_ctrl_announce(void *service, netif_t iface, int *state,
		int *event, struct timeval *probe_wait_time)
{
	int config_idx;
	struct mdns_service *service_arr[2];

	service_arr[0] = service;
	service_arr[1] = NULL;

	mdns_add_service_iface(service_arr, iface);

	config_idx = get_config_idx_from_iface(iface);
	if (config_idx == kGeneralErr) {
		MDNS_LOG("Failed to announce the service. No interface found");
		return;
	}

    if ( IS_INTERFACE_DOWN(iface) ) {
        MDNS_LOG("Interface is down, skip probe");
        return;
    }

	state[config_idx] = INIT;
	send_init_probes(config_idx, state, event, probe_wait_time, NULL);
}

static inline void mdns_ctrl_deannounce(void *service, netif_t iface)
{
	int config_idx;
	struct mdns_service *service_arr[2];

	service_arr[0] = service;
	service_arr[1] = NULL;

	config_idx = get_config_idx_from_iface(iface);
	if (config_idx == kGeneralErr) {
		MDNS_LOG("Failed to deeannounce the service. No interface found."
				"");
		return;
	}

	send_close_probe(config_idx, service_arr);
	mdns_remove_service_iface(service_arr, iface);
}

static inline void mdns_ctrl_announce_arr(void *services, netif_t iface,
		int *state, int *event, struct timeval *probe_wait_time)
{
	int config_idx;

	mdns_add_service_iface(services, iface);

	config_idx = get_config_idx_from_iface(iface);
	if (config_idx == kGeneralErr) {
		MDNS_LOG("Failed to announce services. No interface found.");
		return;
	}

    if ( IS_INTERFACE_DOWN(iface) ) {
        MDNS_LOG("Interface is down, skip probe");
        return;
    }

	state[config_idx] = INIT;
	send_init_probes(config_idx, state, event, probe_wait_time, NULL);
}

static inline void mdns_ctrl_deannounce_arr(void *service, netif_t iface)
{
	int config_idx = get_config_idx_from_iface(iface);
	if (config_idx == kGeneralErr) {
		MDNS_LOG("Failed to deannounce services. No interface found.");
		return;
	}
	send_close_probe(config_idx, service);
	mdns_remove_service_iface(service, iface);
}

static inline void mdns_ctrl_deannounce_all(netif_t iface)
{
	int config_idx = get_config_idx_from_iface(iface);
	if (interface_state[config_idx] != STOPPED) {
		send_close_probe(config_idx, NULL);
		mdns_remove_all_services(iface);
		num_config_g--;
	}
}

/* This is the mdns thread function */
static void do_responder(void)
{
	int max_sock;
	mdns_ctrl_data msg;
	int ret;
	struct sockaddr_storage from;
	struct sockaddr_in *from_v4 = (struct sockaddr_in *)&from;
#ifdef CONFIG_IPV6
	struct sockaddr_in6 *from_v6 = (struct sockaddr_in6 *)&from;
#endif
	int active_fds;
	fd_set fds;
	int len = 0, i, config_idx = 0, min_idx;
	struct timeval *timeout;
	struct timeval probe_wait_time[MDNS_MAX_SERVICE_CONFIG];
	socklen_t in_size = sizeof(struct sockaddr_in);
	int state[MDNS_MAX_SERVICE_CONFIG] = {INIT, INIT};
	int event[MDNS_MAX_SERVICE_CONFIG];
	uint32_t start_wait, stop_wait;
	responder_enabled = 1;
	mico_queue_t *ctrl_responder_queue;

	ret = mico_rtos_init_mutex(&mdns_mutex);
	if (ret != MICO_SUCCESS)
		return;
	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		SET_TIMEOUT(&probe_wait_time[i], 0);
	}

	while (responder_enabled) {

		FD_ZERO(&fds);
		FD_SET(mc_sock, &fds);
		FD_SET(ctrl_sock, &fds);

		max_sock = ctrl_sock > mc_sock ? ctrl_sock : mc_sock;

		min_idx = get_min_timeout(probe_wait_time,
					  MDNS_MAX_SERVICE_CONFIG);
		if (get_msec_time(probe_wait_time[min_idx]) != 0)
			timeout = &probe_wait_time[min_idx];
		else
			timeout = NULL;
		start_wait = mdns_time_ms();
		active_fds = select(max_sock + 1, &fds, NULL, NULL,
				timeout);
		stop_wait = mdns_time_ms();

		if (active_fds < 0)
			MDNS_LOG("error: select() failed: %d", active_fds);

		if (FD_ISSET(ctrl_sock, &fds)) {
		    if( mdns_socket_queue(MDNS_CTRL_RESPONDER, &ctrl_responder_queue, 0) < 0 ) {
		        MDNS_LOG("error: loopback socket err");
		        continue;
		    }

		    ret = mico_rtos_pop_from_queue(ctrl_responder_queue, &msg, 0);
			if (ret == -1) {
				MDNS_LOG("Warning: responder failed to get control message");
			} else {
	            MDNS_DBG("Responder got control message = %d.\r\n", msg.cmd);
				if (msg.cmd == MDNS_CTRL_HALT) {
					mdns_ctrl_halt();
					continue;
				} else if (msg.cmd == MDNS_CTRL_IFACE_JOIN_GROUP) {
                    mdns_ctrl_iface_join_group(msg.iface);
                    continue;
                } else if (msg.cmd == MDNS_CTRL_IFACE_LEAVE_GROUP) {
                    mdns_ctrl_iface_leave_group(msg.iface);
                    continue;
                } else if (msg.cmd == MDNS_CTRL_IFACE_UP) {
					mdns_ctrl_iface_up(msg.iface, state,
							event, probe_wait_time);
					continue;
				} else if (msg.cmd == MDNS_CTRL_IFACE_DOWN) {
					mdns_ctrl_iface_down(msg.iface);
					continue;
				} else if (msg.cmd == MDNS_CTRL_REANNOUNCE) {
					mdns_ctrl_reannounce(msg.iface, state);
					continue;
				} else if (msg.cmd ==
						MDNS_CTRL_ANNOUNCE_SERVICE) {
				    MDNS_DBG("================mdns_announce_service=================\r\n");

					mdns_ctrl_announce(msg.service,
							msg.iface, state, event,
							probe_wait_time);
					continue;
				} else if (msg.cmd ==
						MDNS_CTRL_DEANNOUNCE_SERVICE) {
					mdns_ctrl_deannounce(msg.service,
							msg.iface);
					continue;
				} else if (msg.cmd ==
					MDNS_CTRL_ANNOUNCE_SERVICE_ARR) {
					mdns_ctrl_announce_arr(msg.service,
							msg.iface, state, event,
							probe_wait_time);
					continue;
				} else if (msg.cmd ==
					MDNS_CTRL_DEANNOUNCE_SERVICE_ARR) {
					mdns_ctrl_deannounce_arr(msg.service,
							msg.iface);
					continue;
				} else if (msg.cmd ==
						MDNS_CTRL_DEANNOUNE_ALL) {
					mdns_ctrl_deannounce_all(msg.iface);
					continue;
				}
			}
		}

		if (FD_ISSET(mc_sock, &fds)) {
		    in_size = sizeof(struct sockaddr_in);
		    len = recvfrom(mc_sock, (char *)rx_msg.data, sizeof(rx_msg.data), MSG_DONTWAIT,
		                   (struct sockaddr *)&from, &in_size);
				if (len < 0) {
					MDNS_LOG("responder failed to recv packet");
					continue;
				}
#ifdef CONFIG_IPV6
			    if (IS_IPV4_MAPPED_IPV6(from_v6)) {
			        UNMAP_IPV4_MAPPED_IPV6(from_v4, from_v6);
			        config_idx = get_config_idx_from_ip(from_v4->sin_addr.s_addr);
			    } else {
			        config_idx = get_config_idx_from_iface(INTERFACE_STA);
			    }
#else
				config_idx = get_config_idx_from_ip( from_v4->sin_addr.s_addr);
#endif
            if (config_idx == -1) {
                
        		continue;
            }
			mr_stats.total_rx++;
			ret = mdns_parse_message(&rx_msg, len);
			if (ret != 0) {
				mr_stats.rx_errors++;
				continue;
			}
			update_others_timeout(config_idx, probe_wait_time, start_wait, stop_wait);
			if (interface_state[config_idx] != RUNNING)
				continue;
			event[config_idx] = EVENT_RX;
		} else {
			update_others_timeout(min_idx, probe_wait_time, start_wait, stop_wait);
			if (interface_state[config_idx] != RUNNING)
				continue;
			else {
				event[min_idx] = EVENT_TIMEOUT;
				config_idx = min_idx;
			}
		}
		if (interface_state[config_idx] != STOPPED) {
			/* We will be in below state machine only after
			   above is completed */
			responder_state_machine(from, state, event, config_idx,
						probe_wait_time);
		}
	}
	if (!responder_enabled) {
		MDNS_LOG("Signalled to stop mdns_responder");
		mico_rtos_deinit_mutex(&mdns_mutex);
		mico_rtos_delete_thread(NULL);
	}
}

int responder_launch(const char *domain, char *hostname)
{
	int i;
//#ifdef CONFIG_IPV6
//	int ret;
//	ip6_addr_t mdns_ipv6_addr;
//#endif	/*	CONFIG_IPV6	*/

	/* Assign buffers to rx and tx messages */
	rx_msg.questions = rx_questions;
	rx_msg.answers = rx_answers;
	rx_msg.authorities = rx_authorities;

	tx_msg.questions = tx_questions;
	tx_msg.answers = tx_answers;
	tx_msg.authorities = tx_authorities;

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		interface_state[i] = STOPPED;
		config_g[i].iface_idx = INTERFACE_NONE;
	}
        /* populate the fqdn */
	if (domain == NULL)
#if CONFIG_XMDNS
		domain = "site";
#else
		domain = "local";
#endif
	if (!valid_label(hostname) || !valid_label(domain))
		return ERR_MDNS_INVAL;
	/* populate fqdn */
	hname = hostname;
	domname = domain;
	num_config_g = 0;
	/* number of mdns config opt will be used in do responder thread */
	reset_fqdn();
	mc_sock = mdns_socket_mcast();

	if (mc_sock < 0) {
		MDNS_LOG("error: unable to open multicast socket in responder");
		return mc_sock;
	}

#if 0
#ifdef CONFIG_IPV6
#if CONFIG_XMDNS
	ip6addr_aton("FF05::FB", &mdns_ipv6_addr);
#else
	ip6addr_aton("FF02::FB", &mdns_ipv6_addr);
#endif

	mc6_sock = mdns6_socket_mcast(mdns_ipv6_addr, htons(5353));
	if (mc6_sock < 0) {
		MDNS_LOG("error: unable to open multicast socket in responder");
		return mc6_sock;
	}
	ret = mld6_joingroup(IP6_ADDR_ANY, &mdns_ipv6_addr);
	if (ret < 0) {
		MDNS_LOG("error: unable to join IPv6 mDNS multicast group");
		return ret;
	}
#endif	/*	CONFIG_IPV6	*/
#endif
	/* create both ends of the control socket */
	ctrl_sock = mdns_socket_queue(MDNS_CTRL_RESPONDER, NULL, sizeof(mdns_ctrl_data));

	if (ctrl_sock < 0) {
		MDNS_LOG("Failed to create responder control socket: %d",
		    ctrl_sock);
		return kGeneralErr;
	}
	responder_thread =
	    mdns_thread_create(do_responder, MDNS_THREAD_RESPONDER);
	if (responder_thread == NULL)
		return kGeneralErr;
	return kNoErr;
}

static int signal_and_wait_for_responder_halt()
{
	int total_wait_time = 100 * 100;    /* 10 seconds */
	int check_interval = 100;   /* 100 ms */
	int num_iterations = total_wait_time / check_interval;

	if (!responder_enabled) {
		MDNS_LOG("Warning: mdns responder not running");
		return kNoErr;
	}

	while (responder_enabled && num_iterations--)
	    mico_rtos_delay_milliseconds(check_interval);

	if (!num_iterations)
		MDNS_LOG("Error: timed out waiting for mdns responder to stop");

	return responder_enabled ? kGeneralErr : kNoErr;
}


int responder_halt(void)
{
	int ret, i;

	ret = mdns_send_ctrl_msg(MDNS_CTRL_HALT, MDNS_CTRL_RESPONDER);
	if (ret != 0) {
		MDNS_LOG("Warning: failed to send HALT msg to mdns responder");
		return kGeneralErr;
	}
	ret = signal_and_wait_for_responder_halt();

	if (ret != kNoErr)
		MDNS_LOG("Warning: failed to HALT mdns responder");

	/* force a halt */
	if (responder_enabled != false) {
		MDNS_LOG("Warning: failed to halt mdns responder, forcing.");
		responder_enabled = false;
	}

	ret = mico_rtos_delete_thread(responder_thread);
	if (ret != kNoErr)
		MDNS_LOG("Warning: failed to delete thread.");

	for (i = 0; i < MDNS_MAX_SERVICE_CONFIG; i++) {
		mdns_remove_all_services(config_g[i].iface_idx);
		config_g[i].iface_idx = INTERFACE_NONE;
		interface_state[i] = STOPPED;
	}
	ret = mdns_socket_close(&ctrl_sock);
	ret = mdns_socket_close(&mc_sock);

	return ret;
}
