/*
 *  Copyright (C) 2015, Marvell International Ltd.
 *  All Rights Reserved.
 */

#include "mdns_private.h"

#if MDNS_DEBUG
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
	DBG("%lu.%lu.%lu.%lu",
	    ip >> 24,
	    ((ip & 0x00FF0000) >> 16),
	    ((ip & 0x0000FF00) >> 8), ip & 0x000000FF);
}

void debug_print_txt(char *txt, uint16_t len)
{
	uint16_t i;
	for (i = 0; i < len; i++)
		if (txt[i] < ' ') {
			DBG("\\%d", txt[i]);
		} else {
			DBG("%c", txt[i]);
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
				DBG(".");
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
	DBG(" (type %u, class %u)\r\n", q->qtype, q->qclass);
}

/* print resource (answer) and associated RR */
void debug_print_resource(struct mdns_message *m, struct mdns_resource *r)
{
	struct rr_srv *srv;

	debug_print_name(m, r->name);
	DBG(" (class %u%s, ttl=%lu, len=%u) ",
	    r->class & 0x8000 ? r->class & ~(0x8000) : r->class,
	    r->class & 0x8000 ? " FLUSH" : "", r->ttl, r->rdlength);
	switch (r->type) {
	case T_A:
		DBG("A ");
		debug_print_ip(ntohl(*((uint32_t *) r->rdata)));
		break;
	case T_NS:
		DBG("NS ");
		debug_print_name(m, (uint8_t *) r->rdata);
		break;
	case T_CNAME:
		DBG("CNAME ");
		debug_print_name(m, (uint8_t *) r->rdata);
		break;
	case T_SRV:
		DBG("SRV ");
		srv = (struct rr_srv *)r->rdata;
		DBG("prior: %u, weight: %u, port: %u, target: \"",
		    ntohs(srv->priority), ntohs(srv->weight), ntohs(srv->port));
		debug_print_name(m, srv->target);
		DBG("\"\r\n");
		break;
	case T_PTR:
		DBG("PTR ");
		debug_print_name(m, (uint8_t *) r->rdata);
		break;
	case T_TXT:
		DBG("TXT \"");
		debug_print_txt((char *)r->rdata, r->rdlength);
		DBG("\"\r\n");
		break;
	default:
		DBG("\tunknown\r\n");
		break;
	}
	DBG("\r\n");
}

void debug_print_header(struct mdns_header *h)
{
	DBG("ID=%u, QR=%u, AA=%d OPCODE=%u\r\n"
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

	DBG("########################################################\r\n");
	DBG("HEADER;\r\n");
	debug_print_header(m->header);

	DBG("QUESTION;\r\n");
	for (i = 0; i < m->num_questions; i++)
		debug_print_question(m, &m->questions[i]);

	DBG("ANSWER;\r\n");
	for (i = 0; i < m->num_answers; i++)
		debug_print_resource(m, &m->answers[i]);

	DBG("AUTHORITY;\r\n");
	for (i = 0; i < m->num_authorities; i++)
		debug_print_resource(m, &m->authorities[i]);
	DBG("########################################################\r\n");
}
#endif
