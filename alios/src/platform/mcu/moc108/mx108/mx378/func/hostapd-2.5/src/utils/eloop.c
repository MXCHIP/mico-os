/*
 * Event loop based on select() loop
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */
#include "include.h"
#include "ll.h"
#include "fake_clock_pub.h"
#include "uart_pub.h"
#include "includes.h"
#include "common.h"
#include "trace.h"
#include "eloop.h"
#include "signal.h"
#include "errno-base.h"
#include "sk_intf.h"
#include "main_none.h"
#include "mac.h"

#include "mico_rtos.h"


static struct eloop_data eloop;

int eloop_init(void)
{
	os_memset(&eloop, 0, sizeof(eloop));
	
	dl_list_init(&eloop.timeout);

	return 0;
}

static int eloop_sock_table_add_sock(struct eloop_sock_table *table,
                                     int sock, eloop_sock_handler handler,
                                     void *eloop_data, void *user_data)
{
	struct eloop_sock *tmp;
	int new_max_sock;

	if (sock > eloop.max_sock)
		new_max_sock = sock;
	else
		new_max_sock = eloop.max_sock;

	if (table == NULL)
		return -1;

	eloop_trace_sock_remove_ref(table);
	tmp = os_realloc_array(table->table, table->count + 1,
			       sizeof(struct eloop_sock));
	if (tmp == NULL) {
		eloop_trace_sock_add_ref(table);
		return -1;
	}

	tmp[table->count].sock = sock;
	tmp[table->count].eloop_data = eloop_data;
	tmp[table->count].user_data = user_data;
	tmp[table->count].handler = handler;
	wpa_trace_record(&tmp[table->count]);
	
	table->count++;
	table->table = tmp;
	eloop.max_sock = new_max_sock;
	eloop.count++;
	table->changed = 1;
	eloop_trace_sock_add_ref(table);

	return 0;
}


static void eloop_sock_table_remove_sock(struct eloop_sock_table *table,
                                         int sock)
{
	int i;

	if (table == NULL || table->table == NULL || table->count == 0)
		return;

	for (i = 0; i < table->count; i++) {
		if (table->table[i].sock == sock)
			break;
	}
	if (i == table->count)
		return;
	eloop_trace_sock_remove_ref(table);
	if (i != table->count - 1) {
		os_memmove(&table->table[i], &table->table[i + 1],
			   (table->count - i - 1) *
			   sizeof(struct eloop_sock));
	}
	table->count--;
	eloop.count--;
	table->changed = 1;
	eloop_trace_sock_add_ref(table);

	return;
}

static void eloop_sock_table_destroy(struct eloop_sock_table *table)
{
	if (table) {
		int i;
		for (i = 0; i < table->count && table->table; i++) {
			wpa_printf(MSG_INFO, "ELOOP: remaining socket: "
				   "sock=%d eloop_data=%p user_data=%p "
				   "handler=%p",
				   table->table[i].sock,
				   table->table[i].eloop_data,
				   table->table[i].user_data,
				   table->table[i].handler);
			wpa_trace_dump_funcname("eloop unregistered socket "
						"handler",
						table->table[i].handler);
			wpa_trace_dump("eloop sock", &table->table[i]);
		}
		
		os_free(table->table);
	}
}


int eloop_register_read_sock(int sock, eloop_sock_handler handler,
			     void *eloop_data, void *user_data)
{
	return eloop_register_sock(sock, EVENT_TYPE_READ, handler,
				   eloop_data, user_data);
}


void eloop_unregister_read_sock(int sock)
{
	eloop_unregister_sock(sock, EVENT_TYPE_READ);
}


static struct eloop_sock_table *eloop_get_sock_table(eloop_event_type type)
{
	switch (type) {
	case EVENT_TYPE_READ:
		return &eloop.readers;
	case EVENT_TYPE_WRITE:
		return &eloop.writers;
	case EVENT_TYPE_EXCEPTION:
		return &eloop.exceptions;
	}

	return NULL;
}


int eloop_register_sock(int sock, eloop_event_type type,
			eloop_sock_handler handler,
			void *eloop_data, void *user_data)
{
	struct eloop_sock_table *table;

	assert(sock >= 0);
	table = eloop_get_sock_table(type);
	return eloop_sock_table_add_sock(table, sock, handler,
					 eloop_data, user_data);
}


void eloop_unregister_sock(int sock, eloop_event_type type)
{
	struct eloop_sock_table *table;

	table = eloop_get_sock_table(type);
	eloop_sock_table_remove_sock(table, sock);
}

int eloop_register_timeout(unsigned int secs, 
			unsigned int usecs,
			eloop_timeout_handler handler,
			void *eloop_data, 
			void *user_data)
{
	os_time_t now_sec;	
	uint32_t clk_time;
    OSStatus err = kNoErr;
	struct eloop_timeout *timeout, *tmp;

	ASSERT(handler);
	
	timeout = os_zalloc(sizeof(*timeout));
	if (timeout == NULL)
	{
		os_printf("------------eloop_register_malloc_failed\r\n");
		return -1;
	}
	
	if (os_get_reltime(&timeout->time) < 0) 
	{
		os_free(timeout);
		os_printf("???? os_get_reltimeErr\r\n");
		return -1;
	}
	
	now_sec = timeout->time.sec;
	timeout->time.sec += secs;
	if (timeout->time.sec < now_sec) {
		/*
		 * Integer overflow - assume long enough timeout to be assumed
		 * to be infinite, i.e., the timeout would never happen.
		 */
		os_printf("------------ELOOP: Too long timeout\r\n");
		os_free(timeout);
		return 0;
	}
	
	timeout->time.usec += usecs;
	while (timeout->time.usec >= 1000000) {
		timeout->time.sec++;
		timeout->time.usec -= 1000000;
	}
	timeout->eloop_data = eloop_data;
	timeout->user_data = user_data;
	timeout->handler = handler;

	dl_list_for_each(tmp, &eloop.timeout, struct eloop_timeout, list) {
		if (os_reltime_before(&timeout->time, &tmp->time)) {
			dl_list_add(tmp->list.prev, &timeout->list);
			return 0;
		}
	}
	dl_list_add_tail(&eloop.timeout, &timeout->list);
	
	return 0;
}

void eloop_remove_timeout(struct eloop_timeout *timeout)
{	
	dl_list_del(&timeout->list);
	os_free(timeout);
}

int eloop_cancel_timeout(eloop_timeout_handler handler,
			 void *eloop_data, void *user_data)
{
	struct eloop_timeout *timeout, *prev;
	int removed = 0;

	dl_list_for_each_safe(timeout, prev, &eloop.timeout,
						      struct eloop_timeout, list) 
	{
		if (timeout->handler == handler &&
		    (timeout->eloop_data == eloop_data ||
		     eloop_data == ELOOP_ALL_CTX) &&
		    (timeout->user_data == user_data ||
		     user_data == ELOOP_ALL_CTX)) 
		{
			eloop_remove_timeout(timeout);
			removed++;
		}
	}

	return removed;
}


int eloop_cancel_timeout_one(eloop_timeout_handler handler,
			     void *eloop_data, void *user_data,
			     struct os_reltime *remaining)
{
	struct eloop_timeout *timeout, *prev;
	int removed = 0;
	struct os_reltime now;

	os_get_reltime(&now);
	remaining->sec = remaining->usec = 0;

	dl_list_for_each_safe(timeout, prev, &eloop.timeout,
			      struct eloop_timeout, list){
		if (timeout->handler == handler &&
		    (timeout->eloop_data == eloop_data) &&
		    (timeout->user_data == user_data)) {
			removed = 1;
			if (os_reltime_before(&now, &timeout->time))
				os_reltime_sub(&timeout->time, &now, remaining);
			eloop_remove_timeout(timeout);
			break;
		}
	} 
   
	return removed;
}

int eloop_is_timeout_registered(eloop_timeout_handler handler,
				void *eloop_data, void *user_data)
{
	struct eloop_timeout *tmp;

	dl_list_for_each(tmp, &eloop.timeout, struct eloop_timeout, list) 
	{
		if (tmp->handler == handler 
			&& tmp->eloop_data == eloop_data 
		    && tmp->user_data == user_data)
		{
			return 1;
		}	
	}

	return 0;
}

int eloop_deplete_timeout(unsigned int req_secs, unsigned int req_usecs,
			  eloop_timeout_handler handler, void *eloop_data,
			  void *user_data)
{
	struct os_reltime now, requested, remaining;
	struct eloop_timeout *tmp;

	dl_list_for_each(tmp, &eloop.timeout, struct eloop_timeout, list) {
		if (tmp->handler == handler &&
			tmp->eloop_data == eloop_data &&
			tmp->user_data == user_data) {
			requested.sec = req_secs;
			requested.usec = req_usecs;
			os_get_reltime(&now);
			os_reltime_sub(&tmp->time, &now, &remaining);
			if (os_reltime_before(&requested, &remaining)) {
				eloop_cancel_timeout(handler, eloop_data,
							 user_data);
				eloop_register_timeout(requested.sec,
							   requested.usec,
							   handler, eloop_data,
							   user_data);
				return 1;
			}
			return 0;
		}
	}

	return -1;
}

int eloop_replenish_timeout(unsigned int req_secs, 
				unsigned int req_usecs,
			    eloop_timeout_handler handler, 
			    void *eloop_data,
			    void *user_data)
{
	struct os_reltime now, requested, remaining;
	struct eloop_timeout *tmp;

	dl_list_for_each(tmp, &eloop.timeout, struct eloop_timeout, list) {
		if (tmp->handler == handler &&
			tmp->eloop_data == eloop_data &&
			tmp->user_data == user_data) {
			requested.sec = req_secs;
			requested.usec = req_usecs;
			os_get_reltime(&now);
			os_reltime_sub(&tmp->time, &now, &remaining);
			if (os_reltime_before(&remaining, &requested)) {
				eloop_cancel_timeout(handler, eloop_data,
							 user_data);
				eloop_register_timeout(requested.sec,
							   requested.usec,
							   handler, eloop_data,
							   user_data);
				return 1;
			}
			return 0;
		}
	}

	return -1;
}

static void eloop_handle_alarm(int sig)
{
	wpa_printf(MSG_ERROR, "eloop: could not process SIGINT or SIGTERM in "
		   "two seconds. Looks like there\n"
		   "is a bug that ends up in a busy loop that "
		   "prevents clean shutdown.\n"
		   "Killing program forcefully.\n");
}

void eloop_handle_signal(int sig)
{
	int i;
	GLOBAL_INT_DECLARATION();

#ifndef CONFIG_NATIVE_WINDOWS
	if ((sig == SIGINT || sig == SIGTERM) && !eloop.pending_terminate) {
		/* Use SIGALRM to break out from potential busy loops that
		 * would not allow the program to be killed. */
		eloop.pending_terminate = 1;
		signal_bk(SIGALRM, eloop_handle_alarm);
		alarm(2);
	}
#endif /* CONFIG_NATIVE_WINDOWS */

	GLOBAL_INT_DISABLE();
	eloop.signaled++;
	for (i = 0; i < eloop.signal_count; i++) {
		if (eloop.signals[i].sig == sig) {
			eloop.signals[i].signaled++;
			break;
		}
	}

	GLOBAL_INT_RESTORE();
}

int eloop_signal_is_registered(int sig)
{
	int i;
	int hit_flag = 0;	
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	for (i = 0; i < eloop.signal_count; i++) {
		if (sig == eloop.signals[i].sig) {
			hit_flag = 1;
			break;
		}
	}
	GLOBAL_INT_RESTORE();

	return hit_flag;
}

static void eloop_process_pending_signals(void)
{
	int i,sig;
	int sig_count;
	void *user_data;
	eloop_signal_handler handler;
	GLOBAL_INT_DECLARATION();

	GLOBAL_INT_DISABLE();
	if (eloop.signaled == 0)
	{
		GLOBAL_INT_RESTORE();
		return;
	}
	eloop.signaled = 0;
	sig_count = eloop.signal_count;

	if (eloop.pending_terminate) {
#ifndef CONFIG_NATIVE_WINDOWS
		alarm(0);
#endif /* CONFIG_NATIVE_WINDOWS */
		eloop.pending_terminate = 0;
	}
	GLOBAL_INT_RESTORE();
	
	for (i = 0; i < sig_count; i++) {	
		GLOBAL_INT_DISABLE();
		if (eloop.signals[i].signaled) {
			eloop.signals[i].signaled = 0;
			
			handler = eloop.signals[i].handler;
			sig = eloop.signals[i].sig;
			user_data = eloop.signals[i].user_data;
			ASSERT(handler);
			GLOBAL_INT_RESTORE();
			
			handler(sig,user_data);
		}else{
			GLOBAL_INT_RESTORE();
		}		
	}
}

int eloop_register_signal(int sig, eloop_signal_handler handler,
			  void *user_data)
{
	struct eloop_signal *tmp;
	GLOBAL_INT_DECLARATION();

	if(eloop_signal_is_registered(sig))
	{
		return 0;
	}

	GLOBAL_INT_DISABLE();
	tmp = os_realloc_array(eloop.signals, eloop.signal_count + 1,
			       sizeof(struct eloop_signal));
	if (tmp == NULL)
	{
		GLOBAL_INT_RESTORE();
		return -1;
	}

	tmp[eloop.signal_count].sig = sig;
	tmp[eloop.signal_count].user_data = user_data;
	tmp[eloop.signal_count].handler = handler;
	tmp[eloop.signal_count].signaled = 0;
	
	eloop.signal_count++;
	eloop.signals = tmp;
	
	signal_bk(sig, eloop_handle_signal);

	GLOBAL_INT_RESTORE();

	return 0;
}

int eloop_register_signal_terminate(eloop_signal_handler handler,
				    void *user_data)
{
	int ret = eloop_register_signal(SIGINT, handler, user_data);
	if (ret == 0)
		ret = eloop_register_signal(SIGTERM, handler, user_data);
	return ret;
}

int eloop_register_signal_reconfig(eloop_signal_handler handler,
				   void *user_data)
{
#ifdef CONFIG_NATIVE_WINDOWS
	return 0;
#else /* CONFIG_NATIVE_WINDOWS */
	return eloop_register_signal(SIGHUP, handler, user_data);
#endif /* CONFIG_NATIVE_WINDOWS */
}

void eloop_reader_dispatch(int type)
{
	int i;
	int sk;	
	void *eloop_data = 0;
	void *user_data = 0;
	eloop_sock_handler handler;
	
	if(type == HOSTAPD_MGMT){
	sk = mgmt_get_socket_num();
	}else{
		sk = data_get_socket_num();
	}

	handler = 0;
	for(i = 0; i < eloop.readers.count; i ++)
	{
		if(sk == eloop.readers.table[i].sock)
		{
			handler = eloop.readers.table[i].handler;
			eloop_data = eloop.readers.table[i].eloop_data;
			user_data = eloop.readers.table[i].user_data;
			
			break;
		}
	}

	if(handler)
	{
		handler(sk, eloop_data, user_data);
	}
}

void eloop_timeout_run(void)
{	
	struct os_reltime now;
	
	if (!eloop.terminate 
			&& (!dl_list_empty(&eloop.timeout))) {	
		struct eloop_timeout *timeout, *prv;
		
		/* check if some registered timeouts have occurred */
		dl_list_for_each_safe(timeout, prv, &eloop.timeout, struct eloop_timeout, list)
		{
			os_get_reltime(&now);
			if (!os_reltime_before(&now, &timeout->time)) {
				void *eloop_data = timeout->eloop_data;
				void *user_data = timeout->user_data;
				eloop_timeout_handler handler =
					timeout->handler;
				
				eloop_remove_timeout(timeout);
				
				handler(eloop_data, user_data);
			}else{
				break;
			}
		}
	}
}

void eloop_run(void)
{
	int type;
	struct os_reltime tv, now;
	int timeout_ms = MICO_WAIT_FOREVER;
	
	while(!eloop.terminate &&
	       (!dl_list_empty(&eloop.timeout) || eloop.readers.count > 0 ||
		eloop.writers.count > 0 || eloop.exceptions.count > 0))
	{
		struct eloop_timeout *timeout;
		
		if (eloop.pending_terminate) {
			/*
			 * This may happen in some corner cases where a signal
			 * is received during a blocking operation. We need to
			 * process the pending signals and exit if requested to
			 * avoid hitting the SIGALRM limit if the blocking
			 * operation took more than two seconds.
			 */
			eloop_process_pending_signals();
			if (eloop.terminate)
				break;
		}

		timeout = dl_list_first(&eloop.timeout, struct eloop_timeout,
					list);
		if (timeout) {
			os_get_reltime(&now);
			if (os_reltime_before(&now, &timeout->time))
				os_reltime_sub(&timeout->time, &now, &tv);
			else
				tv.sec = tv.usec = 0;
			timeout_ms = tv.sec * 1000 + tv.usec / 1000;
		}

		wpa_sem_wait(timeout_ms);

		eloop_process_pending_signals();

		/* check if some registered timeouts have occurred */
		timeout = dl_list_first(&eloop.timeout, struct eloop_timeout,
					list);
		if (timeout) {
			os_get_reltime(&now);
			if (!os_reltime_before(&now, &timeout->time)) {
				void *eloop_data = timeout->eloop_data;
				void *user_data = timeout->user_data;
				eloop_timeout_handler handler =
					timeout->handler;
				eloop_remove_timeout(timeout);
				handler(eloop_data, user_data);
			}

		}

		if(ws_mgmt_peek_rxed_next_payload_size()){
			eloop_reader_dispatch(HOSTAPD_MGMT);
		}

		if (ws_data_peek_rxed_next_payload_size()){
			eloop_reader_dispatch(HOSTAPD_DATA);
		}
		
	}

	eloop.terminate = 0;

	
	return;
}

void eloop_terminate(void)
{
	eloop.terminate = 1;
}

void eloop_destroy(void)
{
	struct eloop_timeout *timeout, *prev;
	struct os_reltime now;

	os_get_reltime(&now);
	dl_list_for_each_safe(timeout, prev, &eloop.timeout,
			      struct eloop_timeout, list) {
		int sec, usec;
		sec = timeout->time.sec - now.sec;
		usec = timeout->time.usec - now.usec;
		if (timeout->time.usec < now.usec) {
			sec--;
			usec += 1000000;
		}
		wpa_printf(MSG_INFO, "ELOOP: remaining timeout: %d.%06d "
			   "eloop_data=%p user_data=%p handler=%p",
			   sec, usec, timeout->eloop_data, timeout->user_data,
			   timeout->handler);
		wpa_trace_dump_funcname("eloop unregistered timeout handler",
					timeout->handler);
		wpa_trace_dump("eloop timeout", timeout);
		eloop_remove_timeout(timeout);
	}
	eloop_sock_table_destroy(&eloop.readers);
	eloop_sock_table_destroy(&eloop.writers);
	eloop_sock_table_destroy(&eloop.exceptions);
	os_free(eloop.signals);

#ifdef CONFIG_ELOOP_POLL
	os_free(eloop.pollfds);
	os_free(eloop.pollfds_map);
#endif /* CONFIG_ELOOP_POLL */
#ifdef CONFIG_ELOOP_EPOLL
	os_free(eloop.epoll_table);
	os_free(eloop.epoll_events);
	close(eloop.epollfd);
#endif /* CONFIG_ELOOP_EPOLL */
}


int eloop_terminated(void)
{
	return eloop.terminate || eloop.pending_terminate;
}


void eloop_wait_for_read_sock(int sock)
{
}
//eof

