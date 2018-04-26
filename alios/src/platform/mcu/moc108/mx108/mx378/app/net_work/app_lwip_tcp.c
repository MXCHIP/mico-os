#include "include.h"

#if CFG_USE_LWIP_NETSTACK
#include "lwip/tcp.h"
#include "app_lwip_tcp.h"
#include "uart_pub.h"
#include "mem_pub.h"

#include "spidma_intf_pub.h"

#define LWIP_TCP_DEBUG
#ifdef LWIP_TCP_DEBUG
#define LWIP_TCP_PRT      warning_prf
#define LWIP_TCP_WARN     warning_prf
#define LWIP_TCP_FATAL    fatal_prf
#else
#define LWIP_TCP_PRT      null_prf
#define LWIP_TCP_WARN     null_prf
#define LWIP_TCP_FATAL    null_prf
#endif

#define LWIP_TCP_SERVICER     1
#if LWIP_TCP_SERVICER
#define LWIP_TCP_CLIENT       0
#define LWIP_TCP_STATIS       0
#else
#define LWIP_TCP_CLIENT       1
#define LWIP_TCP_STATIS       1
#endif

static struct tcp_pcb *lwip_tcp_pcb = NULL;

#if LWIP_TCP_CLIENT
const static char remote_ipaddr[4] = {192, 168, 4, 151};
static uint16_t remote_port = 7211;
#else
static uint16_t local_port = 8050;
#endif

#if LWIP_TCP_STATIS
static uint32_t total_tcppkt_cnt;
static uint32_t total_tcpbyte_cnt;
static uint32_t total_tcpsecd;
static uint32_t tcpbyte_per_secd;
#endif

const uint8_t tcp_app_tab[] = "abcdefghijklmnopqrstuvwyxz1234567890";

/*---------------------------------------------------------------------------*/
static err_t app_lwip_tcp_recv(void *arg, 
										struct tcp_pcb *tpcb,
										struct pbuf *p, 
										err_t err)
{
    if(err != ERR_OK)
    {
        if(p)
            pbuf_free(p);
        return ERR_ABRT;
    }

    if(lwip_tcp_pcb == tpcb)
    {
        char *src_ipaddr = (char *)&tpcb->remote_ip;
        char *data = p->payload;
        LWIP_TCP_PRT("src_ipaddr: %d.%d.%d.%d\r\n", src_ipaddr[0], src_ipaddr[1],
                     src_ipaddr[2], src_ipaddr[3]);
        LWIP_TCP_PRT("src_port: %d\r\n", tpcb->remote_port);
        LWIP_TCP_PRT("data%d:0x%x,0x%x,0x%x\r\n", p->len, data[0], data[1], data[2]);

        if(p && p->tot_len)
        {
            tcp_recved(tpcb, p->tot_len);
            app_lwip_tcp_send_packet((UINT8 *)tcp_app_tab, sizeof(tcp_app_tab));
#if LWIP_TCP_STATIS
            if(!total_tcppkt_cnt)
            {
                total_tcppkt_cnt = 0;
                total_tcpbyte_cnt = 0;
                total_tcpsecd = 0;
                tcpbyte_per_secd = 0;

                //etimer_set(&tcp_statis_timer, CLOCK_SECOND);
            }
            total_tcppkt_cnt++;
            tcpbyte_per_secd += p->tot_len;
#endif
        }
        else if(!p)
        {
            err = tcp_close(lwip_tcp_pcb);
            LWIP_TCP_WARN("tcp_state:%d\r\n", lwip_tcp_pcb->state);
            lwip_tcp_pcb = NULL;
            if(err != ERR_OK)
                LWIP_TCP_WARN("close fail\r\n");

#if CFG_USE_SPIDMA
            if(process_is_running(&spidma_intfer_process))
            {
                process_exit(&spidma_intfer_process);
            }
#endif
            return err;
        }
    }
    else
    {
        LWIP_TCP_PRT("tcp no match: old:%p--new:%d\r\n", lwip_tcp_pcb, tpcb);
    }
	
    if(p)
    {
        pbuf_free(p);
    }
	
    return ERR_OK;
}

static err_t app_lwip_tcp_sent(void *arg, struct tcp_pcb *tpcb, u16_t len)
{
    return ERR_OK;
}

/*---------------------------------------------------------------------------*/
#if LWIP_TCP_SERVICER
static err_t app_lwip_tcp_accept(void *arg, struct tcp_pcb *newpcb,
                                 err_t err)
{
    LWIP_TCP_PRT("tcp accept, param_err:%d\r\n", err);

    if(err != ERR_OK)
    {
        return ERR_ABRT;
    }

    if((lwip_tcp_pcb && lwip_tcp_pcb->state != ESTABLISHED) || (!lwip_tcp_pcb))
    {
        char *src_ipaddr = (char *)&newpcb->remote_ip;

        LWIP_TCP_PRT("old pcb:%p -- new pcb:%p\r\n", lwip_tcp_pcb, newpcb);
        LWIP_TCP_PRT("src_ipaddr: %d.%d.%d.%d\r\n", src_ipaddr[0], src_ipaddr[1],
                     src_ipaddr[2], src_ipaddr[3]);
        LWIP_TCP_PRT("src_port: %d\r\n", newpcb->remote_port);
        lwip_tcp_pcb = newpcb;
        tcp_recv(lwip_tcp_pcb, app_lwip_tcp_recv);
        tcp_sent(lwip_tcp_pcb, app_lwip_tcp_sent);

#if CFG_USE_SPIDMA
        if(!process_is_running(&spidma_intfer_process))
        {
            process_start(&spidma_intfer_process, NULL);
        }
#endif

        return ERR_OK;
    }
    else
    {
        return ERR_ISCONN;
    }
}
#endif

/*---------------------------------------------------------------------------*/
#if LWIP_TCP_CLIENT
static err_t app_lwip_tcp_connected(void *arg, struct tcp_pcb *tpcb,
                                    err_t err)
{
    char *src_ipaddr = (char *)&tpcb->remote_ip;
    if(lwip_tcp_pcb != tpcb)
    {
        return ERR_ABRT;
    }

    LWIP_TCP_PRT("tcp connected to servicer\r\n");
    LWIP_TCP_PRT("src_ipaddr: %d.%d.%d.%d\r\n", src_ipaddr[0], src_ipaddr[1],
                 src_ipaddr[2], src_ipaddr[3]);
    LWIP_TCP_PRT("src_port: %d\r\n", tpcb->remote_port);

    return ERR_OK;
}
#endif

/*---------------------------------------------------------------------------*/
#if LWIP_TCP_STATIS
static void app_lwip_tcp_statis_timer_handler (void)
{
    float rate_per_secd = 0, total_rate = 0;

    if(etimer_expired(&tcp_statis_timer))
    {
        total_tcpsecd++;
        if(tcpbyte_per_secd)
        {
            total_tcpbyte_cnt += tcpbyte_per_secd;

            rate_per_secd = (tcpbyte_per_secd * 8.0) / (1 * 1000 * 1000.0);
            total_rate = (total_tcpbyte_cnt * 8.0) / (total_tcpsecd * 1000 * 1000.0);

            LWIP_TCP_PRT("%d:%d,rs:%f--rt:%f Mbit/s\r\n",
                         total_tcpsecd, total_tcppkt_cnt, 
                         rate_per_secd, total_rate);

            tcpbyte_per_secd = 0;
        }
        else
        {
            total_tcppkt_cnt = 0;
            total_tcpbyte_cnt = 0;
            total_tcpsecd = 0;
            tcpbyte_per_secd = 0;
        }
    }
}
#endif

/*---------------------------------------------------------------------------*/
void lwip_tcp_start(void)
{
#if LWIP_TCP_SERVICER
    err_t err;
#endif

    lwip_tcp_pcb = tcp_new();
    if(!lwip_tcp_pcb)
    {
        LWIP_TCP_PRT("tcp_new failed, exit process\r\n");
        return;
    }

#if LWIP_TCP_SERVICER
    {
        err = tcp_bind(lwip_tcp_pcb, IP_ADDR_ANY, local_port);
        if(err != ERR_OK)
        {
            LWIP_TCP_PRT("tcp_bind failed, errcode:%d\r\n", err);
            tcp_close(lwip_tcp_pcb);
            lwip_tcp_pcb = NULL;
            return;
        }

        lwip_tcp_pcb = tcp_listen(lwip_tcp_pcb);
        if(!lwip_tcp_pcb)
        {
            LWIP_TCP_PRT("tcp_listen failed\r\n");
            tcp_close(lwip_tcp_pcb);
            lwip_tcp_pcb = NULL;
            return;
        }
    }
    tcp_accept(lwip_tcp_pcb, app_lwip_tcp_accept);
#endif

#if LWIP_TCP_CLIENT
    tcp_recv(lwip_tcp_pcb, app_lwip_tcp_recv);
    tcp_sent(lwip_tcp_pcb, app_lwip_tcp_sent);
#endif

#if LWIP_TCP_STATIS
    total_tcppkt_cnt = 0;
    total_tcpbyte_cnt = 0;
    total_tcpsecd = 0;
    tcpbyte_per_secd = 0;
#endif
}

void lwip_tcp_stop(void)
{
	if(lwip_tcp_pcb)
	{
        tcp_close(lwip_tcp_pcb);
        lwip_tcp_pcb = NULL;
	}
}

/*---------------------------------------------------------------------------*/
void app_lwip_tcp_connect_to_servicer(void)
{
#if LWIP_TCP_CLIENT
    err_t err;
    ip_addr_t ipaddr;
    IP4_ADDR(&ipaddr, remote_ipaddr[0], remote_ipaddr[1],
             remote_ipaddr[2], remote_ipaddr[3]);
    err = tcp_connect(lwip_tcp_pcb, &ipaddr, remote_port, app_lwip_tcp_connected);
    if(err != ERR_OK)
    {
        LWIP_TCP_PRT("tcp_connect failed, errcode:%d\r\n", err);
    }
#endif
}

/*---------------------------------------------------------------------------*/
int app_lwip_tcp_send_packet(UINT8 *data, UINT32 len)
{
    int send_byte = 0;

    if(lwip_tcp_pcb && lwip_tcp_pcb->state == ESTABLISHED)
    {
        err_t err;
        if(tcp_sndbuf(lwip_tcp_pcb) >= len)
        {
            tcp_write(lwip_tcp_pcb, data, len, TCP_WRITE_FLAG_COPY);
            send_byte = len;
        }
        else
        {
            //LWIP_TCP_PRT("sndbuf_lwn:%d\r\n", tcp_sndbuf(lwip_tcp_pcb));
        }
        err = tcp_output(lwip_tcp_pcb);
        if(err != ERR_OK)
        {
            LWIP_TCP_PRT("tcp_send failed, err code:%d\r\n", err);
        }
    }
    return send_byte;
}
/*---------------------------------------------------------------------------*/

#endif  // CFG_USE_LWIP_NETSTACK
// EOF
