/**
 ******************************************************************************
 * @file    mdns_private.h
 * @author  William Xu
 * @version V1.0.0
 * @date    3-August-2017
 * @brief   Declare mdns internal functions and type defines
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

#ifndef __MDNS_PRIVATE_H__
#define __MDNS_PRIVATE_H__

#include "mdns.h"
#include "mdns_message.h"

enum mdns_status_t {
	INIT,
	FIRST_PROBE_SENT,	/* wait a random amount of time and probe */
	SECOND_PROBE_SENT,	/* wait 250ms and probe */
	THIRD_PROBE_SENT,	/* wait 250ms and probe */
	READY_TO_RESPOND,	/* we have claimed our name */
	READY_TO_SEND,		/* response prepared, waiting to send */
};

enum mdns_event_t {
	EVENT_RX,		/* recieved a DNS packet */
	EVENT_CTRL,		/* recieved a control message */
	EVENT_TIMEOUT,	/* timed out waiting for DNS packet */
};

/* internal control message types. */
enum mdns_commands_t {
	MDNS_CTRL_HALT = 0,
	MDNS_CTRL_MONITOR,
	MDNS_CTRL_UNMONITOR,
	MDNS_CTRL_IFACE_UP,
	MDNS_CTRL_IFACE_DOWN,
	MDNS_CTRL_IFACE_JOIN_GROUP,
	MDNS_CTRL_IFACE_LEAVE_GROUP,
	MDNS_CTRL_DEANNOUNE_ALL,
	MDNS_CTRL_ANNOUNCE_SERVICE,
	MDNS_CTRL_DEANNOUNCE_SERVICE,
	MDNS_CTRL_ANNOUNCE_SERVICE_ARR,
	MDNS_CTRL_DEANNOUNCE_SERVICE_ARR,
	MDNS_CTRL_REANNOUNCE,
};

/* mDNS responder statistics
 *
 * rx_queries: Number of Rx packets containing mDNS queries
 * rx_answers: Number of Rx packets containing mDNS answers
 *
 * rx_hn_conflicts: Number of hostname conflicts resolved
 * rx_sn_conflicts: Number of servicename conflicts resolved
 *
 * NOTE: A single mDNS packet can contain both queries and
 * answers, hence it is expected that (rx_queries + rx_answers)
 * can be greater than total_rx
 *
 * rx_errors: Number of received mDNS packets which have some kind of
 * error, which is found while parsing the packet
 *
 * rx_known_ans: Number of Rx packets containing answers which match
 * device's resource record(s) and have TTL greater than a specific
 * threshold (MDNS_TTL_THRESHOLD). No response is sent by the device
 * when such packets are received
 *
 */
typedef struct {
	int total_rx, rx_queries, rx_answers;
	int rx_errors, rx_known_ans;
	uint8_t rx_hn_conflicts, rx_sn_conflicts;
	int total_tx, tx_probes, tx_announce, tx_bye;
	uint8_t tx_probes_curr, tx_announce_curr;
	int tx_reannounce, tx_response;
	int tx_ipv4_err, tx_ipv6_err;
	uint8_t probe_rx_events_curr;
	uint8_t iface_up, iface_down, iface_reannounce;
} mdns_responder_stats;


typedef struct mdns_ctrl_sock_data {
	int cmd;
	netif_t iface;
	void *service;
} mdns_ctrl_data;

#ifdef MDNS_QUERY_API
enum sinst_state {
    SINST_STATE_INIT = 0,
    SINST_STATE_CLEAN,
    SINST_STATE_UPDATING,
};

enum sinst_event {
    SINST_EVENT_GOT_TXT = 0,
    SINST_EVENT_GOT_SRV,
    SINST_EVENT_GOT_AREC,
    SINST_EVENT_LOST_AREC,
#ifdef CONFIG_IPV6
    SINST_EVENT_GOT_AAAA_REC,
    SINST_EVENT_LOST_AAAA_REC,
#endif /* CONFIG_IPV6 */
    SINST_EVENT_ADD_QUESTIONS,
};

struct service_monitor;

/* Each service instance that we detect is stored in a service instance */
struct service_instance {
    SLIST_ENTRY(service_instance) list_item;
    struct mdns_service service;
    char sname[MDNS_MAX_LABEL_LEN + 1];
    char stype[MDNS_MAX_LABEL_LEN + 1];
    char domain[MDNS_MAX_LABEL_LEN + 1];
    char keyvals[MDNS_MAX_KEYVAL_LEN + 1];
    char rawkeyvals[MDNS_MAX_KEYVAL_LEN + 1];
    int rawkvlen;
     SLIST_ENTRY(service_instance) alist_item;
    struct arec *arec;
#ifdef CONFIG_IPV6
     SLIST_ENTRY(service_instance) aaaa_list_item;
    struct aaaa_rec *aaaa_rec;
#endif
    /* we keep ttls for each record.  Note that these ttls are in milliseconds,
     * not seconds.  If a received ttl is too big, we simply make it as big as
     * possible.  Note that we have to regularly refresh the srv and txt
     * records, so we keep enough details around to refresh it properly.  In
     * practice, the txt and srv will probably have the same ttl, so we just
     * refresh with T_ANY when the srv expires.
     */
    uint32_t ptr_ttl;
    uint32_t srv_ttl;
    uint32_t srv_ttl0;
    uint32_t next_refresh;
    int ttl_percent;
    enum sinst_state state;
    struct service_monitor *smon;
};

SLIST_HEAD(sinst_list, service_instance);

/* We maintain a linked list of services that we are monitoring. */
struct service_monitor {
    SLIST_ENTRY(service_monitor) list_item;
    struct sinst_list sinsts;

    uint8_t fqst[MDNS_MAX_NAME_LEN + 1];
    /* save memory by adding a dname pointer to the fqst and fqsn instead of
     * copying them over and over.  These offsets can be used while
     * constructing packets, but aren't expected to be relevant in any other
     * context.
     */
    uint16_t fqst_offset;
    uint16_t fqsn_offset;
    mdns_query_cb cb;
    void *cbdata;

#ifdef CONFIG_DNSSD_QUERY
    struct in_addr dns_addr;
    bool is_unicast;
#endif
    /* this is the time in milliseconds between service refreshes.  It starts
     * at 1s and doubles after each refresh until it reaches 60s, where it
     * stays.
     */
    uint32_t refresh_period;

    /* this is the time until the next refresh in ms.  It gets updated every
     * time we wake up to do anything.  If it drops to 0, we refresh the
     * service.
     */
    uint32_t next_refresh;
};

SLIST_HEAD(smons_list, service_monitor);

/* Commands sent over the control socket must at least have a command (e.g.,
 * MDNS_CTRL_HALT, etc.) and a length.  Commands that send data (e.g.,
 * MDNS_CTRL_MONITOR, etc.) will also have a data member.  Note that the length
 * is the total length of the command, not the length of the data.
 */
union query_ctrl_data {
    char raw[0];
    struct service_monitor smon;
    uint8_t fqst[MDNS_MAX_NAME_LEN + 1];
};

typedef struct  {
    int command;
    int length;
    union query_ctrl_data data;
} query_ctrl_msg;
#endif

/* mdns service config structure
 *
 * config structure contains interface handle and service list to be published
 * on that interface. config->iface_handle holds the interface handle and
 * config->services holds pointers to list of service. config->num_services
 * holds no. of services announced per interface.
 */
struct mdns_service_config {
    /** interface index */
    uint8_t   iface_idx;
	/** list of services announced on given interface */
	struct mdns_service *services[MAX_MDNS_LST + 1];
	/** No. of services registered */
	int num_services;
};

/* Return values from mdns_prepare_response */
#define RS_ERROR		-1		/* error in querry or response prep */
#define RS_NO_SEND		0x01		/* nothing to send */
#define RS_SEND			0x02	/* send */
#define RS_SEND_DELAY	0x04	/* this is a service query, delay 20-120ms */
/* No record corresponding to the service(s) published by device is found
 * in answer section of incoming query */
#define RS_ANS_NOT_FOUND	0x08

#if CONFIG_MDNS_DEBUG
extern char *statenames[];
extern char *eventnames[];

/* logging helpers */
void debug_print_message(struct mdns_message *m);
void debug_print_name(struct mdns_message *m, uint8_t *name);
#define LOG(M, ...) MICO_LOG(CONFIG_MDNS_DEBUG, "MDNS LOG", M, ##__VA_ARGS__)
#define DBG(M, ...) MICO_PRINT(CONFIG_MDNS_DEBUG, M, ##__VA_ARGS__)
#else
#define debug_print_message(m) do {} while (0)
#define debug_print_name(m, n) do {} while (0)
#define DBG(...) do {} while (0)
#define LOG(...) do {} while (0)
#endif



/* helpers for accessing elements.  value arguments are in host byte order */
#define get_uint16_t(p) ((*(uint8_t *)(p) << 8) | *((uint8_t *)(p) + 1))
#define get_uint32_t(p) ((*(uint8_t *)(p) << 24) | \
					   (*((uint8_t *)(p) + 1) << 16) | \
					   (*((uint8_t *)(p) + 2) << 8) |  \
					   (*((uint8_t *)(p) + 3)))
#define set_uint16_t(p, v) do { \
	*(uint8_t *)(p) = ((v) >> 8) & 0xff;	 \
	*((uint8_t *)(p) + 1) = (v) & 0xff;		 \
	} while (0)
#define set_uint32_t(p, v)  do { \
	*(uint8_t *)(p) = ((v) >> 24) & 0xff;	 \
	*((uint8_t *)(p) + 1) = ((v) >> 16) & 0xff;		\
	*((uint8_t *)(p) + 2) = ((v) >> 8) & 0xff;		\
	*((uint8_t *)(p) + 3) = (v) & 0xff;				\
	} while (0)

/* helpers for handling dns names */
uint8_t *dname_put_label(uint8_t *dst, const char *label);
int dname_size(uint8_t *dname);
int dname_increment(uint8_t *name);
int dname_cmp(uint8_t *p1, uint8_t *n1, uint8_t *p2, uint8_t *n2);
int dnameify(char *name, uint16_t kvlen, uint8_t sep, uint8_t *dest);
int dname_copy(uint8_t *dst, uint8_t *p, uint8_t *src);
uint8_t *dname_label_to_c(char *dst, uint8_t *p, uint8_t *src,
						  int keepuscores);
int dname_label_cmp(uint8_t *p1, uint8_t *l1, uint8_t *p2, uint8_t *l2);
uint8_t *dname_label_next(uint8_t *p, uint8_t *n);
void txt_to_c_ncpy(char *dst, int dlen, uint8_t *txt, int tlen);
int dname_overrun(uint8_t *p, uint8_t *e, uint8_t *n);
/* d points to a dname pointer in a message.  return the offset in the
 * packet.
 */
#define POINTER(d) ((((*(d) & ~0xC0) << 8) | *((d) + 1)) & 0xFFFFU)
#define IS_POINTER(c) ((c) & 0xC0)

void dname_tests(void);

#ifdef MDNS_TESTS

#define FAIL_UNLESS(condition, ...) \
	do { \
		if (!(condition)) { \
			mdns_log("FAIL: %s: %d: ", __FILE__, __LINE__); \
			mdns_log(__VA_ARGS__); \
			mdns_log("\r\n"); \
		} else { \
			mdns_log("PASS: %s: %d\r\n", __FILE__, __LINE__); \
		} \
	} while (0)


#define test_title(s) mdns_log("========= %s =========\r\n", s)

#endif


/* internal flags for service data */
#define SERVICE_CHECKED_FLAG	1
#define SERVICE_HAS_A_FLAG		2
#ifdef CONFIG_IPV6
#define SERVICE_HAS_AAAA_FLAG		4
#endif /* CONFIG_IPV6 */
#define SERVICE_HAS_SRV_FLAG	8
#define SRV_ADDED				16
#define TXT_ADDED				32

#ifdef CONFIG_IPV6
#define SERVICE_IS_READY(s) \
	(((s)->flags & (SERVICE_HAS_A_FLAG|SERVICE_HAS_SRV_FLAG)) ==	\
	 (SERVICE_HAS_A_FLAG|SERVICE_HAS_SRV_FLAG) || \
	 ((s)->flags & (SERVICE_HAS_AAAA_FLAG|SERVICE_HAS_SRV_FLAG)) ==	\
	 (SERVICE_HAS_AAAA_FLAG|SERVICE_HAS_SRV_FLAG))
#else
#define SERVICE_IS_READY(s) \
	(((s)->flags & (SERVICE_HAS_A_FLAG|SERVICE_HAS_SRV_FLAG)) ==	\
	 (SERVICE_HAS_A_FLAG|SERVICE_HAS_SRV_FLAG))
#endif /* CONFIG_IPV6 */

/* internal API functions for responder */
int responder_launch(const char *domain, char *hostname);
int responder_halt(void);

/* internal API functions for querier */
int query_launch();
int query_halt(void);

/* internal common functions used by responder and querier */
/* return the amount of tail room in the message m */
#define TAILROOM(m) ((m)->end - (m)->cur + 1)

/* ensure that message m has at least l bytes of room left */
#define CHECK_TAILROOM(m, l)										  \
	do {															  \
		if (TAILROOM(m) < l) {										  \
			DBG("Warning: truncated mdns message (%d).\r\n", __LINE__); \
			return -1;												  \
		}															  \
	} while (0)

/* return the number of valid bytes in message m's buffer */
#define VALID_LENGTH(m) ((m)->cur - (m)->data)

/* dumb macro to set a struct timeval to "ms" milliseconds. */
#define SET_TIMEOUT(t, ms)								\
	do {												\
		(t)->tv_sec = (ms)/1000;						\
		(t)->tv_usec = ((ms)%1000) * 1000;				\
	} while (0)

/* mDNS Interface multicast group State */
enum iface_mc_group_state {
    JOIN = 0,
    LEAVE,
};


/* mdns_iface_group_state_change: Join or leave multicast group for a net interface */
int mdns_iface_group_state_change(netif_t iface, enum iface_mc_group_state state);

/* sock6 should be passed as -1, when message is required to be sent only
 * on IPv4 address.
 * to_addr should be passed as 0, when message is required to be sent on
 * mDNS multicast address, otherwise unicast reply would be sent on to_addr.
 */
int mdns_send_msg(struct mdns_message *m, int sock, unsigned short port, netif_t out_interface, in_addr_t to_addr);
int mdns_send_ctrl_msg_uint32(uint16_t port, int msg);
int mdns_send_ctrl_msg(uint16_t port, void *msg);
int mdns_add_srv_ptr_txt(struct mdns_message *m, struct mdns_service *s,
			 uint8_t *fqdn, int section, uint32_t ttl);
int mdns_add_srv(struct mdns_message *m, uint16_t priority,
		 uint16_t weight, uint16_t port, uint8_t *target);
int mdns_add_txt(struct mdns_message *m, char *txt, uint16_t len);
int mdns_add_question(struct mdns_message *m, uint8_t *qname, uint16_t qtype,
					  uint16_t qclass);
int mdns_add_answer(struct mdns_message *m, uint8_t *name, uint16_t type,
					uint16_t class, uint32_t ttl);
int mdns_add_answer_o(struct mdns_message *m, uint16_t offset, uint16_t type,
					  uint16_t class, uint32_t ttl);
int mdns_add_answer_lo(struct mdns_message *m, uint8_t *label, uint16_t offset,
		       uint16_t type, uint16_t class, uint32_t ttl);
int mdns_add_authority(struct mdns_message *m, uint8_t *name, uint16_t type,
					   uint16_t class, uint32_t ttl);
int mdns_add_uint32_t(struct mdns_message *m, uint32_t i);
#ifdef CONFIG_IPV6
int mdns_add_aaaa(struct mdns_message *m, void *i);
#endif	/*	CONFIG_IPV6	*/
int mdns_add_name(struct mdns_message *m, uint8_t *name);
int mdns_add_name_lo(struct mdns_message *m, uint8_t *label, uint16_t offset);
int mdns_query_init(struct mdns_message *m);
int mdns_parse_message(struct mdns_message *m, int mlen);
int mdns_response_init(struct mdns_message *m);
#if CONFIG_DNSSD_QUERY
int dns_send_msg(struct mdns_message *m, int sock, unsigned short port,
		 void *out_interface, struct in_addr out_addr);
int dns_socket_close(int s);
int dns_socket_ucast(uint16_t port);
#endif
uint32_t interval(uint32_t start, uint32_t stop);
void recalc_timeout(struct timeval *t, uint32_t start, uint32_t stop,
					uint32_t target);
int mdns_verify_service(struct mdns_service *services[], netif_t iface);

#ifndef ASSERT
#ifdef MDNS_DBG
#define ASSERT(condition) do { \
	if (!(condition)) { \
		DBG("%s: %d: ASSERTION FAILED\r\n", __FILE__, __LINE__); \
		while (1); \
	} \
} while (0)

#else
#define ASSERT(condition) do {} while (0)
#endif
#endif /* ! ASSERT */

/* mDNS test APIs */
void dname_size_tests(void);
void dname_cmp_tests(void);
void txt_to_c_ncpy_tests(void);
void increment_name_tests(void);

#endif /* __MDNS_PRIVATE_H__ */
