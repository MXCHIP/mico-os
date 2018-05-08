/**
 ******************************************************************************
 * @file    mdns_debug.h
 * @author  William Xu
 * @version V1.0.0
 * @date    3-August-2017
 * @brief   mdns debug functions
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
#include "mdns_private.h"

#if CONFIG_MDNS_DEBUG

char *statenames[] = {
	"INIT",
	"FIRST_PROBE_SENT",
	"SECOND_PROBE_SENT",
	"THIRD_PROBE_SENT",
	"READY_TO_RESPOND",
	"READY_TO_SEND",
};

char *eventnames[] = {
	"EVENT_RX",
	"EVENT_CTRL",
	"EVENT_TIMEOUT",
};

void debug_print_ip(uint32_t ip)
{
	MDNS_DBG("%lu.%lu.%lu.%lu",
	    ip >> 24,
	    ((ip & 0x00FF0000) >> 16),
	    ((ip & 0x0000FF00) >> 8), ip & 0x000000FF);
}

void debug_print_txt(char *txt, uint16_t len)
{
	uint16_t i;
	for (i = 0; i < len; i++)
		if (txt[i] < ' ') {
			MDNS_DBG("\\%d", txt[i]);
		} else {
			MDNS_DBG("%c", txt[i]);
		}
}

/* print a RFC-1035 format domain name */
void debug_print_name(struct mdns_message *m, uint8_t * name)
{
	uint8_t *s = name;
	int first = 1;

	while (*s) {
		if (IS_POINTER(*s)) {	/* pointer */
			if (m == NULL)
				break;
			/* go print at start of message+offset */
			s = m->data + POINTER(s);
			continue;
		} else {	/* label */
			if (!first) {
				MDNS_DBG(".");
			}
			first = 0;
			debug_print_txt((char *)s + 1, *s);
			s += *s;
		}
		s++;
	}
}

/* print question (query) data */
void debug_print_question(struct mdns_message *m, struct mdns_question *q)
{
	debug_print_name(m, q->qname);
	MDNS_DBG(" (type %u, class %u)\r\n", q->qtype, q->qclass);
}

/* print resource (answer) and associated RR */
void debug_print_resource(struct mdns_message *m, struct mdns_resource *r)
{
	struct rr_srv *srv;

	debug_print_name(m, r->name);
	MDNS_DBG(" (class %u%s, ttl=%lu, len=%u) ",
	    r->class & 0x8000 ? r->class & ~(0x8000) : r->class,
	    r->class & 0x8000 ? " FLUSH" : "", r->ttl, r->rdlength);
	switch (r->type) {
	case T_A:
		MDNS_DBG("A ");
		debug_print_ip(ntohl(*((uint32_t *) r->rdata)));
		break;
	case T_NS:
		MDNS_DBG("NS ");
		debug_print_name(m, (uint8_t *) r->rdata);
		break;
	case T_CNAME:
		MDNS_DBG("CNAME ");
		debug_print_name(m, (uint8_t *) r->rdata);
		break;
	case T_SRV:
		MDNS_DBG("SRV ");
		srv = (struct rr_srv *)r->rdata;
		MDNS_DBG("prior: %u, weight: %u, port: %u, target: \"",
		    ntohs(srv->priority), ntohs(srv->weight), ntohs(srv->port));
		debug_print_name(m, srv->target);
		MDNS_DBG("\"\r\n");
		break;
	case T_PTR:
		MDNS_DBG("PTR ");
		debug_print_name(m, (uint8_t *) r->rdata);
		break;
	case T_TXT:
		MDNS_DBG("TXT \"");
		debug_print_txt((char *)r->rdata, r->rdlength);
		MDNS_DBG("\"\r\n");
		break;
	default:
		MDNS_DBG("\tunknown\r\n");
		break;
	}
	MDNS_DBG("\r\n");
}

void debug_print_header(struct mdns_header *h)
{
	MDNS_DBG("ID=%u, QR=%u, AA=%d OPCODE=%u\r\n"
	    "QDCOUNT=%u, ANCOUNT=%u, NSCOUNT=%u, ARCOUNT=%u\r\n",
	    ntohs(h->id), h->flags.fields.qr,
	    h->flags.fields.aa, h->flags.fields.opcode,
	    ntohs(h->qdcount), ntohs(h->ancount),
	    ntohs(h->nscount), ntohs(h->arcount));
}

/* print information about a message: header, questions, answers */
void debug_print_message(struct mdns_message *m)
{
	int i;

	MDNS_DBG("########################################################\r\n");
	MDNS_DBG("HEADER;\r\n");
	debug_print_header(m->header);

	MDNS_DBG("QUESTION;\r\n");
	for (i = 0; i < m->num_questions; i++)
		debug_print_question(m, &m->questions[i]);

	MDNS_DBG("ANSWER;\r\n");
	for (i = 0; i < m->num_answers; i++)
		debug_print_resource(m, &m->answers[i]);

	MDNS_DBG("AUTHORITY;\r\n");
	for (i = 0; i < m->num_authorities; i++)
		debug_print_resource(m, &m->authorities[i]);
	MDNS_DBG("########################################################\r\n");
}
#endif
