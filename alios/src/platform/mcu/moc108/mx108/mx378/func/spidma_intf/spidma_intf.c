#include "include.h"
#include "arm_arch.h"

#if CFG_USE_SPIDMA
#include "llc.h"

#include "spidma_pub.h"
#include "spidma_intf.h"
#include "spidma_intf_pub.h"

#include "drv_model_pub.h"
#include "mem_pub.h"

#include "app_lwip_tcp.h"
#include "app_lwip_udp.h"
#include "app_led.h"

#if CFG_GENERAL_DMA
#include "general_dma_pub.h"
#endif

#define SPIDMA_INTF_DEBUG
#include "uart_pub.h"
#ifdef SPIDMA_INTF_DEBUG
#define SPIDMA_INTF_PRT           os_printf
#define SPIDMA_INTF_WPRT          warning_prf
#define SPIDMA_INTF_FATAL         fatal_prf
#else
#define SPIDMA_INTF_PRT           null_prf
#define SPIDMA_INTF_WPRT          null_prf
#define SPIDMA_INTF_FATAL         null_prf
#endif

#define SPIDMA_UDP_TRANS        1
//#define SPIDMA_TCP_TRANS        1

#define RX_TIMEOUT_30US         10
#define RX_TIMEOUT_800US        300
#define FPGA_MAIN_CLK           70
#define SPIDMA_RXDATA_TIMEOUT   (FPGA_MAIN_CLK * RX_TIMEOUT_800US)

#ifndef SPIDMA_RXNODE_SIZE
#if SPIDMA_UDP_TRANS
#define SPIDMA_RXNODE_SIZE      1024//1472
#endif
#if SPIDMA_TCP_TRANS
#define SPIDMA_RXNODE_SIZE      1460//1024
#endif
#endif

#ifndef SPIDMA_RXDATA_TIMEOUT
#define SPIDMA_RXDATA_TIMEOUT  SPIDMA_DEF_RXDATA_TIMEOUT_VAL
#endif

#define SPIDATA_POOL_LEN    (SPIDMA_RXNODE_SIZE * 5)
#define SPIDMA_RXBUF_LEN    (SPIDMA_DEF_RXDATA_THRE_INT * 2)
#define SPIDMA_TXBUF_LEN    (1024 * 1)

#if ((SPIDMA_RXBUF_LEN != 1024)  && (SPIDMA_RXBUF_LEN != 2048)  &&  \
     (SPIDMA_RXBUF_LEN != 4096)  && (SPIDMA_RXBUF_LEN != 8192)  &&  \
     (SPIDMA_RXBUF_LEN != 16384) && (SPIDMA_RXBUF_LEN != 32768) &&  \
     (SPIDMA_RXBUF_LEN != 65536) )
#error "SPIDMA_RXBUF_LEN should be 1024/2048/4096/8192/16384/32768/65536."
#endif

UINT8 spidma_rxbuf[SPIDMA_RXBUF_LEN];
UINT8 spidma_txbuf[SPIDMA_TXBUF_LEN];

SPIDMA_DESC_ST spidma_intf;
DD_HANDLE spidma_hdl;

/*---------------------------------------------------------------------------*/
PROCESS(spidma_intfer_process, "spidma_intfer");
/*---------------------------------------------------------------------------*/
typedef struct spidma_elem_st
{
    struct co_list_hdr hdr;
    void *buf_start;
    UINT32 buf_len;
} SPIDMA_ELEM_ST, *SPIDMA_ELEM_PTR;

typedef struct spidma_pool_st
{
    UINT8  pool[SPIDATA_POOL_LEN];
    SPIDMA_ELEM_ST elem[SPIDATA_POOL_LEN / SPIDMA_RXNODE_SIZE];
    struct co_list free;
    struct co_list ready;  
} SPIDMA_POOL_ST, *SPIDMA_POOL_PTR;

SPIDMA_POOL_ST spidma_pool;

static void spidma_intfer_pool_init(void)
{
    UINT32 i = 0;
    os_memset(&spidma_pool.pool[0], 0, sizeof(UINT8)*SPIDATA_POOL_LEN);
    co_list_init(&spidma_pool.free);
    co_list_init(&spidma_pool.ready);

    for(i = 0; i < (SPIDATA_POOL_LEN / SPIDMA_RXNODE_SIZE); i++)
    {
        spidma_pool.elem[i].buf_start =
            (void *)&spidma_pool.pool[i * SPIDMA_RXNODE_SIZE];
        spidma_pool.elem[i].buf_len = 0;

        co_list_push_back(&spidma_pool.free,
                          (struct co_list_hdr *)&spidma_pool.elem[i].hdr);
    }
}

static SPIDMA_ELEM_PTR spidma_intfer_elem_output(void)
{
    SPIDMA_ELEM_PTR elem = NULL;
    elem = (SPIDMA_ELEM_PTR)co_list_pick(&spidma_pool.ready);
    if(!elem)
    {
        //SPIDMA_INTF_PRT("spidma_intfer_elem_output is NULL\r\n");
        return NULL;
    }
    co_list_pop_front(&spidma_pool.ready);
    co_list_push_back(&spidma_pool.free, (struct co_list_hdr *)&elem->hdr);

    return elem;
}

static SPIDMA_ELEM_PTR spidma_intfer_elem_input(void)
{
    SPIDMA_ELEM_PTR elem = NULL;
    elem = (SPIDMA_ELEM_PTR)co_list_pick(&spidma_pool.free);
    if(!elem)
    {
        return NULL;
    }
    co_list_pop_front(&spidma_pool.free);
    co_list_push_back(&spidma_pool.ready, (struct co_list_hdr *)&elem->hdr);

    return elem;
}

#if CFG_GENERAL_DMA
static void spidma_intfer_config_general_dma(void)
{
    GDMACFG_TPYES_ST cfg;

    cfg.dstdat_width = 32;
    cfg.srcdat_width = 32;
    cfg.dstptr_incr = 1;
    cfg.srcptr_incr = 1;
    cfg.src_start_addr = NULL;
    cfg.dst_start_addr = NULL;

    cfg.channel = GDMA_CHANNEL_1;
    cfg.prio = 0;
    cfg.u.type1.src_loop_start_addr = &spidma_rxbuf[0];
    cfg.u.type1.src_loop_end_addr = &spidma_rxbuf[SPIDMA_RXBUF_LEN];

    sddev_control(GDMA_DEV_NAME, CMD_GDMA_CFG_TYPE1, &cfg);
}

void *spidma_memcpy(void *out, const void *in, UINT32 n)
{
    GDMA_DO_ST do_st;
    do_st.channel = GDMA_CHANNEL_1;
    do_st.src_addr = (void*)in;
    do_st.length = n;
    do_st.dst_addr = out;
    sddev_control(GDMA_DEV_NAME, CMD_GDMA_ENABLE, &do_st);
    return out;
} 
#endif

static void spidma_intfer_rx_handler(void *curptr, UINT32 newlen)
{
    SPIDMA_ELEM_PTR elem = NULL;
    if(!newlen)
        return;
    elem = spidma_intfer_elem_input();
    if(elem)
    {
        if(newlen > SPIDMA_RXNODE_SIZE)
            newlen = SPIDMA_RXNODE_SIZE;
        
        #if CFG_GENERAL_DMA
        spidma_memcpy(elem->buf_start, curptr, newlen);
        #else
        os_memcpy(elem->buf_start, curptr, newlen);
        #endif
        elem->buf_len = newlen;
    }
    spidma_intf.rx_cnt++;
    if(process_is_running(&spidma_intfer_process))
    {
        process_poll(&spidma_intfer_process);
    }
}

static void spidma_intfer_tx_handler(void)
{

}

static void spidma_intfer_config_desc(void)
{
    spidma_intf.rxbuf = &spidma_rxbuf[0];
    spidma_intf.txbuf = &spidma_txbuf[0];
    spidma_intf.rxbuf_len = SPIDMA_RXBUF_LEN;
    spidma_intf.txbuf_len = SPIDMA_TXBUF_LEN;

    spidma_intf.rx_handler = spidma_intfer_rx_handler;
    spidma_intf.rx_callback = NULL;
    spidma_intf.tx_handler = spidma_intfer_tx_handler;

    spidma_intf.mode = ((SPIDMA_DEF_RXDATA_THRE_INT & SPIDMA_DESC_RX_THRED_MASK)
                        << SPIDMA_DESC_RX_THRED_POSI);
    spidma_intf.timeout_val = SPIDMA_RXDATA_TIMEOUT;
    spidma_intf.node_len = SPIDMA_RXNODE_SIZE;

    spidma_intf.rx_cnt = 0;
    spidma_intf.rx_cnt_prev = 0;     
}

static void spidma_intfer_poll_handler(void)
{
    UINT32 send_len;
    SPIDMA_ELEM_PTR elem = NULL;

    do
    {
        elem = spidma_intfer_elem_output();
        if(elem)
        {
            #if SPIDMA_UDP_TRANS
            send_len = app_lwip_udp_send_packet(elem->buf_start, elem->buf_len);
            #endif

            #if SPIDMA_TCP_TRANS
            send_len = app_lwip_tcp_send_packet(elem->buf_start, elem->buf_len);
            #endif
        }
    }
    while(elem);

    //SPIDMA_INTF_PRT("rx handler event: rxbuf_rdptr:0x%p\r\n", rxbuf_rdptr);
    //SPIDMA_INTF_PRT("rx handler event: rxbuf_newlen:%d\r\n", rxbuf_newlen);
    //app_lwip_udp_send_packet (rxbuf_rdptr, rxbuf_newlen);

    if(spidma_intf.rx_callback != NULL)
        spidma_intf.rx_callback(send_len);
}

static void spidma_intfer_timer_handler(void)
{
    if(etimer_expired(&spidma_intf.timer)) {
        #if CFG_SUPPORT_TIANZHIHENG_DRONE
        if(spidma_intf.rx_cnt_prev == spidma_intf.rx_cnt) {
            process_post(&app_led_process, PROCESS_EVENT_POLL, (process_data_t)NO_SPI_DATA);
        } else {
            process_post(&app_led_process, PROCESS_EVENT_POLL, (process_data_t)DET_SPI_DATA);
        }
        #endif
        spidma_intf.rx_cnt_prev = spidma_intf.rx_cnt;
        etimer_reset(&spidma_intf.timer);
    }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(spidma_intfer_process, ev, data)
{
    UINT32 status;
    PROCESS_BEGIN();

    spidma_intfer_pool_init();
    spidma_intfer_config_desc();
#if CFG_GENERAL_DMA
    spidma_intfer_config_general_dma();
#endif

    spidma_hdl = ddev_open(SPIDMA_DEV_NAME, &status, (UINT32)&spidma_intf);

    if(DD_HANDLE_UNVALID == spidma_hdl)
    {
        SPIDMA_INTF_FATAL("sdidma open failed, exit process\r\n");
        PROCESS_EXIT();
    }

    etimer_set(&spidma_intf.timer, SPIDMA_TIMER_INTVAL);	

    while(1)
    {
        PROCESS_YIELD();
        if(ev == PROCESS_EVENT_EXIT)
        {
            SPIDMA_INTF_PRT("spidma_intfer_process exit process\r\n");
            etimer_stop(&spidma_intf.timer);
            ddev_close(spidma_hdl);
        }
        else if(ev == PROCESS_EVENT_EXITED)
        {
            struct process *exit_p = (struct process *)data;
            SPIDMA_INTF_PRT("%s exit in spidma_intfer_process\r\n",
                            PROCESS_NAME_STRING(exit_p));
        }
        else if(ev == PROCESS_EVENT_POLL)
        {
            spidma_intfer_poll_handler();
        }
        else if(ev == PROCESS_EVENT_TIMER) {
            spidma_intfer_timer_handler();
        }  
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/

#endif  // CFG_USE_SPIDMA

