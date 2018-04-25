#include "include.h"

#if CFG_USE_LWIP_NETSTACK
#include "lwip/udp.h"
#include "app_lwip_udp.h"
#include "uart_pub.h"
#include "mem_pub.h"
#include "arm_arch.h"
#include "arch/cc.h"
#include "app_led.h"
#include "spidma_intf_pub.h"

#define LWIP_UDP_DEBUG
#if CFG_SUPPORT_TIANZHIHENG_DRONE
#define SUPPORT_TZH_DRONE       1
#endif

#define LWIP_UDP_SERVICER       1

#if SUPPORT_TZH_DRONE
#include "drv_model_pub.h"
#define CMD_HEADER_CODE         0x66
#define CMD_TAIL_CODE           0x99
#define CMD_LEN                 8

#define CMD_IMG_HEADER          0x42
#define CMD_START_IMG           0x76
#define CMD_STOP_IMG            0x77
#endif

#ifdef LWIP_UDP_DEBUG
#define LWIP_UDP_PRT            warning_prf
#define LWIP_UDP_WARN           warning_prf
#define LWIP_UDP_FATAL          fatal_prf
#else
#define LWIP_UDP_PRT            null_prf
#define LWIP_UDP_WARN           null_prf
#define LWIP_UDP_FATAL          null_prf
#endif

#if LWIP_UDP_SERVICER
#define LWIP_UDP_CLIENT         0
#else
#define LWIP_UDP_CLIENT         1
#endif

static struct udp_pcb *lwip_udp_pcb = NULL;

#if LWIP_UDP_CLIENT
const static char remote_ipaddr[4] = {192, 168, 4, 151};
static uint16_t remote_port = 8080;
#else
static uint16_t local_port = 8080;
#endif

#if SUPPORT_TZH_DRONE
static struct udp_pcb *lwip_udp_cmd_pcb = NULL;
static uint16_t cmd_local_port = 8090;
static DD_HANDLE uart_hdl;
#endif

#if CFG_SUPPORT_UVC
extern void fuvc_test_init(void);
#endif

#if SUPPORT_TZH_DRONE
static void app_lwip_udp_config_uart(void);
static void app_lwip_udp_uart_exit(void);
static void app_lwip_udp_handle_cmd_data(UINT8 *data, UINT16 len);
#endif

/*---------------------------------------------------------------------------*/
static void app_lwip_udp_receiver(void *arg, struct udp_pcb *pcb,
                                  struct pbuf *p, ip_addr_t *addr, u16_t port)
{

#if LWIP_UDP_SERVICER

#if SUPPORT_TZH_DRONE
    char *data = p->payload;
    if(data[0] == CMD_IMG_HEADER)
    {
        if(data[1] == CMD_START_IMG)
        {
#endif
            if((!lwip_udp_pcb->remote_port) && (lwip_udp_pcb == pcb))
            {
                err_t err;
                char *src_ipaddr = (char *)addr;
                LWIP_UDP_PRT("src_ipaddr: %d.%d.%d.%d\r\n", src_ipaddr[0], src_ipaddr[1],
                             src_ipaddr[2], src_ipaddr[3]);
                LWIP_UDP_PRT("udp connect to new port:%d\r\n", port);
                err = udp_connect(lwip_udp_pcb, addr, port);
                if(err != ERR_OK)
                {
                    LWIP_UDP_PRT("udp_connect failed\r\n");
                    pbuf_free(p);
                    return;
                }

#if CFG_SUPPORT_UVC
                //fuvc_test_init();
#endif
#if  CFG_USE_SPIDMA
                if(!process_is_running(&spidma_intfer_process))
                {
                    process_start(&spidma_intfer_process, NULL);
                }
#endif
            }

#if SUPPORT_TZH_DRONE
        }
        else if(data[1] == CMD_STOP_IMG)
        {
            if((lwip_udp_pcb->remote_port) && (lwip_udp_pcb == pcb))
            {
                udp_disconnect(lwip_udp_pcb);
                udp_disconnect(lwip_udp_cmd_pcb);
#if CFG_SUPPORT_UVC
                //fuvc_test_init();
#endif
#if  CFG_USE_SPIDMA
                if(process_is_running(&spidma_intfer_process))
                {
                    process_exit(&spidma_intfer_process);
                }
#endif
            }
        }
    }
#endif

#endif
    pbuf_free(p);
}

#if SUPPORT_TZH_DRONE
static void app_lwip_udp_cmd_receiver(void *arg, struct udp_pcb *pcb,
                                      struct pbuf *p, ip_addr_t *addr, u16_t port)
{

    if((!lwip_udp_cmd_pcb->remote_port) && (lwip_udp_cmd_pcb == pcb))
    {
        err_t err;
        char *src_ipaddr = (char *)addr;
        LWIP_UDP_PRT("src_ipaddr: %d.%d.%d.%d\r\n", src_ipaddr[0], src_ipaddr[1],
                     src_ipaddr[2], src_ipaddr[3]);
        LWIP_UDP_PRT("udp cmd connect to new port:%d\r\n", port);
        err = udp_connect(lwip_udp_cmd_pcb, addr, port);
        if(err != ERR_OK)
        {
            LWIP_UDP_PRT("udp_connect failed\r\n");
            pbuf_free(p);
            return;
        }
    }

    app_lwip_udp_handle_cmd_data((UINT8 *)p->payload, p->tot_len);

    pbuf_free(p);
}
#endif

/*---------------------------------------------------------------------------*/
void lwip_udp_stop(void)
{	
	LWIP_UDP_PRT("lwip_udp_stop exit process\r\n");
	if(lwip_udp_pcb)
	{		
		udp_disconnect(lwip_udp_pcb);
		udp_remove(lwip_udp_pcb);
		lwip_udp_pcb = NULL;
	}
	
#if SUPPORT_TZH_DRONE
	if(lwip_udp_cmd_pcb)
	{
		udp_disconnect(lwip_udp_cmd_pcb);
		udp_remove(lwip_udp_cmd_pcb);
		lwip_udp_cmd_pcb = NULL;
		app_lwip_udp_uart_exit();
	}
#endif
}

void lwip_udp_start(void)
{
    err_t err;

    lwip_udp_pcb = udp_new();
    if(!lwip_udp_pcb )
    {
        LWIP_UDP_PRT("udp_new failed, exit process\r\n");
        return;
    }

#if SUPPORT_TZH_DRONE
    lwip_udp_cmd_pcb = udp_new();
    if(!lwip_udp_cmd_pcb)
    {
        LWIP_UDP_PRT("udp_new failed, exit process\r\n");
        return;
    }
#endif

#if LWIP_UDP_SERVICER
    {
        err = udp_bind(lwip_udp_pcb, IP_ADDR_ANY, local_port);
        if(err != ERR_OK)
        {
            LWIP_UDP_PRT("udp_bind failed, exit process\r\n");
            udp_remove(lwip_udp_pcb);
            lwip_udp_pcb = NULL;
            return;
        }

#if SUPPORT_TZH_DRONE
        err = udp_bind(lwip_udp_cmd_pcb, IP_ADDR_ANY, cmd_local_port);
        if(err != ERR_OK)
        {
            LWIP_UDP_PRT("udp_bind failed, exit process\r\n");
            udp_remove(lwip_udp_cmd_pcb);
            lwip_udp_cmd_pcb = NULL;
            return;
        }
#endif
    }
#endif

    udp_recv(lwip_udp_pcb, app_lwip_udp_receiver, NULL);

#if SUPPORT_TZH_DRONE
    udp_recv(lwip_udp_cmd_pcb, app_lwip_udp_cmd_receiver, NULL);

    app_lwip_udp_config_uart();
    app_led_intial();

    //process_post(&app_led_process, PROCESS_EVENT_POLL, (process_data_t)POWER_ON);
#endif

#if LWIP_UDP_CLIENT
    {
        ip_addr_t ipaddr;
        IP4_ADDR(&ipaddr, 
						remote_ipaddr[0], 
						remote_ipaddr[1],
						remote_ipaddr[2], 
						remote_ipaddr[3]);
        err = udp_connect(lwip_udp_pcb, &ipaddr, remote_port);
        if(err != ERR_OK)
        {
            LWIP_UDP_PRT("udp_connect failed\r\n");
        }
    }
#endif
}

/*---------------------------------------------------------------------------*/
int app_lwip_udp_send_packet (UINT8 *data, UINT32 len)
{
    int send_byte = 0;

    if(lwip_udp_pcb->flags & UDP_FLAGS_CONNECTED)
    {
        err_t err;
        struct pbuf *p, *q;

        p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
        if(!p)
        {
            LWIP_UDP_PRT("udp_send failed, no PBUF_POOL\r\n");
            return 0;
        }
        for(q = p; q != NULL; q = q->next)
        {
            os_memcpy((u8_t *)q->payload, data + send_byte, q->len);
            send_byte += q->len;
        }
        err = udp_send(lwip_udp_pcb, p);
        if(err != ERR_OK)
        {
            LWIP_UDP_PRT("udp_send failed, err code:%d\r\n", err);
            pbuf_free(p);
            return 0;
        }
        pbuf_free(p);
    }

    return send_byte;
}
/*---------------------------------------------------------------------------*/

#if SUPPORT_TZH_DRONE
static void app_lwip_udp_config_uart(void)
{
    UINT32 status;
    uart_hdl = ddev_open(UART_DEV_NAME, &status, 0);
    LWIP_UDP_PRT("udp drone cmd send config uart done!\r\n");
}

static void app_lwip_udp_uart_exit(void)
{
    ddev_close(uart_hdl);
    LWIP_UDP_PRT("udp drone cmd send uart closed!\r\n");
}

static void app_lwip_udp_handle_cmd_data(UINT8 *data, UINT16 len)
{
    uint8_t crc_cal;
    uint32_t wrlen = 0;

    if((data[0] != 0x66) && (len != CMD_LEN))
        return;

    crc_cal = (data[1] ^ data[2] ^ data[3] ^ data[4] ^ data[5]);

    if(crc_cal != data[6])
    {
        if(((crc_cal == CMD_HEADER_CODE) || (crc_cal == CMD_TAIL_CODE))
                && (crc_cal + 1 == data[6]))
            // drop this paket for crc is the same with Header or Tailer
            return;
        else // change to right crc
            data[6] = crc_cal;
    }

    wrlen = ddev_write(uart_hdl, (char *)data, len, 0);
    if(wrlen != len)
    {
        LWIP_UDP_WARN("can't send all of the drone cmd\r\n");
    }
    ddev_control(uart_hdl, CMD_SEND_BACKGROUND, 0);

}
#endif  // SUPPORT_TZH_DRONE
#endif  // CFG_USE_LWIP_NETSTACK

// EOF

