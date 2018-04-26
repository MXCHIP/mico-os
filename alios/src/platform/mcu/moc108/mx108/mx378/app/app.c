/**
 ****************************************************************************************
 *
 * @file app.c
 *
 *
 * Copyright (C) Beken Corp 2011-2016
 *
 ****************************************************************************************
 */
#include "include.h"
#include "rwnx_config.h"
#include "app.h"

#if (NX_POWERSAVE)
#include "ps.h"
#endif //(NX_POWERSAVE)

#include "sa_ap.h"
#include "app_lwip_udp.h"
#include "sa_station.h"
#include "main_none.h"
#include "sm.h"
#include "uart_pub.h"

#include "mico_rtos.h"
#include "param_config.h"
#include "rxl_cntrl.h"
#include "lwip/pbuf.h"
#include "rw_msdu.h"
#include "txu_cntrl.h"

#include <hal/wifi.h>

mico_thread_t  init_thread_handle;
mico_thread_t  app_thread_handle;
uint32_t  init_stack_size = 2000;
uint32_t  app_stack_size = 4000;

static mico_semaphore_t app_sema = NULL;
WIFI_CORE_T g_wifi_core = {0};
volatile int32_t bmsg_rx_count = 0;

extern int application_start(int , char** );

#if (0 == CFG_MICO_DEMO)
void app_init(void)
{
#if CFG_READ_FLASH
    cfg_get_general_params();
#endif

#if CFG_WIFI_AP_MODE
#if CFG_READ_FLASH
    if(g_wlan_general_param->role == CONFIG_ROLE_AP)
    {
        cfg_get_ap_params();
#endif

        sa_ap_init();
        wpa_main_entry(2, 0);
        sm_build_broadcast_deauthenticate();

#if CFG_MODE_SWITCH
    }
#endif
#endif

#if CFG_WIFI_STATION_MODE
#if CFG_READ_FLASH
    if(g_wlan_general_param->role == CONFIG_ROLE_STA)
    {
        cfg_get_sta_params();
#endif

        sa_station_init();
        supplicant_main_entry(0);
#if CFG_MODE_SWITCH
    }
#endif
#endif
}
#else
void app_init(void)
{
#if CFG_READ_FLASH
    cfg_get_general_params();
#endif

	mxchipInit();
}
#endif // (0 == CFG_MICO_DEMO)

void app_set_sema(void)
{
	mico_rtos_set_semaphore(&app_sema);
}

static void kmsg_bk_thread_main( void *arg )
{
	OSStatus ret;

	while(1)
	{
		ret = mico_rtos_get_semaphore(&app_sema, 1000);
		//ASSERT(kNoErr == ret);        
		mr_kmsg_background_handle();
		ke_evt_none_core_scheduler();
	}
}

static void init_thread_main( void *arg )
{
//	GLOBAL_INT_START();     

	app_init();     
	os_printf("app_init finished\r\n");

	mico_rtos_delete_thread( NULL );
}

#ifndef DISABLE_CORE_THREAD
void bmsg_rx_handler(BUS_MSG_T *msg)
{
	GLOBAL_INT_DECLARATION();
	
	GLOBAL_INT_DISABLE();
	if(bmsg_rx_count > 0)
	{
		bmsg_rx_count -= 1;
	}
	GLOBAL_INT_RESTORE();
	
	rxl_cntrl_evt((int)msg->arg);
}

void bmsg_skt_tx_handler(BUS_MSG_T *msg)
{
	hapd_intf_ke_rx_handle(msg->arg);
}

void bmsg_tx_handler(BUS_MSG_T *msg)
{
	OSStatus ret;
	struct pbuf *p = (struct pbuf *)msg->arg;
	struct pbuf *q = p;
    
    if(p->next) {
        q = pbuf_coalesce(p, PBUF_RAW);
        if(q == p){
            // must be outof memery in pbuf malloc
            goto EXIT;  
        }
    }
	
	rwm_transfer(q->payload, q->len);
EXIT:	
	
	pbuf_free(q);
}

void bmsg_tx_raw_handler(BUS_MSG_T *msg)
{
    OSStatus ret;
    uint8_t *pkt = (uint8_t *)msg->arg;
    uint16_t len = msg->len;
    MSDU_NODE_T *node;
    UINT8 *content_ptr;
    UINT32 queue_idx = AC_VI;
    struct txdesc *txdesc_new;
    struct umacdesc *umac;

    node = rwm_tx_node_alloc(len);
    if (node == NULL) {
        goto exit;
    }

    rwm_tx_msdu_renew(pkt, len, node->msdu_ptr);
    content_ptr = rwm_get_msdu_content_ptr(node);

    txdesc_new = tx_txdesc_prepare(queue_idx);
    if(txdesc_new == NULL || TXDESC_STA_USED == txdesc_new->status) {
        rwm_node_free(node);
        goto exit;
    }

    txdesc_new->status = TXDESC_STA_USED;
    txdesc_new->host.flags = TXU_CNTRL_MGMT;
    txdesc_new->host.orig_addr = (UINT32)node->msdu_ptr;
    txdesc_new->host.packet_addr = (UINT32)content_ptr;
    txdesc_new->host.packet_len = len;
    txdesc_new->host.status_desc_addr = (UINT32)content_ptr;
    txdesc_new->host.tid = 0xff;

    umac = &txdesc_new->umac;
    umac->payl_len = len;
    umac->head_len = 0;
    umac->tail_len = 0;
    umac->hdr_len_802_2 = 0;

    umac->buf_control = &txl_buffer_control_24G;

    txdesc_new->lmac.agg_desc = NULL;
    txdesc_new->lmac.hw_desc->cfm.status = 0;

    rwm_push_tx_list(node);
    txl_cntrl_push(txdesc_new, queue_idx);

exit:
    aos_free(pkt);
}

void bmsg_ioctl_handler(BUS_MSG_T *msg)
{
	ke_msg_send(msg->arg);
}
	
void bmsg_skt_tx_sender(void *arg)
{
	OSStatus ret;
	BUS_MSG_T msg;

	msg.type = BMSG_SKT_TX_TYPE;
	msg.arg = (uint32_t)arg;
	msg.len = 0;
	msg.sema = NULL;
	
	ret = mico_rtos_push_to_queue(&g_wifi_core.io_queue, &msg, MICO_NO_WAIT);
	if(kNoErr != ret)
	{
		os_printf("bmsg_rx_sender_failed\r\n");
	}
}

void bmsg_null_sender(void)
{
	OSStatus ret;
	BUS_MSG_T msg;

	msg.type = BMSG_NULL_TYPE;
	msg.arg = 0;
	msg.len = 0;
	msg.sema = NULL;
	
	if(!mico_rtos_is_queue_empty(&g_wifi_core.io_queue))
	{
		return;
	}
	
	ret = mico_rtos_push_to_queue(&g_wifi_core.io_queue, &msg, MICO_NO_WAIT);
	if(kNoErr != ret)
	{
		os_printf("bmsg_null_sender_failed\r\n");
	}
}

void bmsg_rx_sender(void *arg)
{
	OSStatus ret;
	BUS_MSG_T msg;

	msg.type = BMSG_RX_TYPE;
	msg.arg = (uint32_t)arg;
	msg.len = 0;
	msg.sema = NULL;
	if(bmsg_rx_count >= 2)
	{
		return;
	}
	
	bmsg_rx_count += 1;
	ret = mico_rtos_push_to_queue(&g_wifi_core.io_queue, &msg, MICO_NO_WAIT);
	if(kNoErr != ret)
	{
		APP_PRT("bmsg_rx_sender_failed\r\n");
	}
}

void bmsg_rx_lsig(uint16_t len, uint8_t rssi)
{
	BUS_MSG_T msg;

	msg.type = BMSG_RX_LSIG;
	msg.arg = (uint32_t)((len << 16) | rssi);
	msg.len = mico_rtos_get_time();
	msg.sema = NULL;

	mico_rtos_push_to_queue(&g_wifi_core.io_queue, &msg, MICO_NO_WAIT);
}

void bmsg_rx_lsig_handler(BUS_MSG_T *msg)
{
	lsig_input((msg->arg&0xFFFF0000)>>16, msg->arg&0xFF, msg->len);
}

int bmsg_tx_sender(struct pbuf *p)
{
	OSStatus ret;
	BUS_MSG_T msg;

	msg.type = BMSG_TX_TYPE;
	msg.arg = (uint32_t)p;
	msg.len = 0;
	msg.sema = NULL;

	pbuf_ref(p);
	ret = mico_rtos_push_to_queue(&g_wifi_core.io_queue, &msg, 1*SECONDS);
	if(kNoErr != ret)
	{
		APP_PRT("bmsg_tx_sender failed\r\n");
		pbuf_free(p);
	} 

	return ret;
}

int bmsg_tx_raw_sender(uint8_t *payload, uint16_t length)
{
    OSStatus ret;
    BUS_MSG_T msg;

    msg.type = BMSG_TX_RAW_TYPE;
    msg.arg = (uint32_t)payload;
    msg.len = length;
    msg.sema = NULL;

    ret = mico_rtos_push_to_queue(&g_wifi_core.io_queue, &msg, 1*SECONDS);

    if(ret != kNoErr) {
        APP_PRT("bmsg_tx_sender failed\r\n");
        aos_free(payload);
    }

    return ret;
}

void bmsg_ioctl_sender(void *arg)
{
	OSStatus ret;
	BUS_MSG_T msg;

	msg.type = BMSG_IOCTL_TYPE;
	msg.arg = (uint32_t)arg;
	msg.len = 0;
	
    ret = mico_rtos_init_semaphore(&msg.sema, 0);
    ASSERT(kNoErr == ret);
	
	ret = mico_rtos_push_to_queue(&g_wifi_core.io_queue, &msg, MICO_NO_WAIT);
	if(kNoErr != ret)
	{
		APP_PRT("bmsg_ioctl_sender_failed\r\n");
	} 
	else 
	{
		APP_PRT("bmsg_ioctl_sender\r\n");
		ret = mico_rtos_get_semaphore(&msg.sema, MICO_WAIT_FOREVER);
		ASSERT(kNoErr == ret);     
	}

	ret = mico_rtos_deinit_semaphore(&msg.sema);
	ASSERT(kNoErr == ret);   
}

static void core_thread_main( void *arg )
{
    OSStatus ret;
	BUS_MSG_T msg;

    while(1)
    {	
        ret = mico_rtos_pop_from_queue(&g_wifi_core.io_queue, &msg, MICO_WAIT_FOREVER);
        if(kNoErr == ret)
        {
        	switch(msg.type)
        	{
        		case BMSG_RX_TYPE:
					APP_PRT("bmsg_rx_handler\r\n");
					bmsg_rx_handler(&msg);
					break;
					
        		case BMSG_TX_TYPE:
					APP_PRT("bmsg_tx_handler\r\n");
					bmsg_tx_handler(&msg);
					break;
					
        		case BMSG_SKT_TX_TYPE:
					APP_PRT("bmsg_skt_tx_handler\r\n");
					bmsg_skt_tx_handler(&msg);
					break;
					
        		case BMSG_IOCTL_TYPE:
					APP_PRT("bmsg_ioctl_handler\r\n");
					bmsg_ioctl_handler(&msg);
					break;
					
                case BMSG_TX_RAW_TYPE:
                    bmsg_tx_raw_handler(&msg);
                    break;
                case BMSG_RX_LSIG:
                    bmsg_rx_lsig_handler(&msg);
                    break;
        		default:
					APP_PRT("unknown_msg\r\n");
					break;
        	}

			if (msg.sema != NULL) {
				mico_rtos_set_semaphore(&msg.sema);
			}
			ke_evt_core_scheduler();
        }
    }
}

void core_thread_init(void)
{
	OSStatus ret;
	
	g_wifi_core.queue_item_count = CORE_QITEM_COUNT;
	g_wifi_core.stack_size = CORE_STACK_SIZE;
	
	ret = mico_rtos_init_queue(&g_wifi_core.io_queue, 
							"core_queue",
							sizeof(BUS_MSG_T),
							g_wifi_core.queue_item_count);
	if (kNoErr != ret) 
	{
		os_printf("Create io queue failed\r\n");
		goto fail;
	}

    ret = mico_rtos_create_thread(&g_wifi_core.handle, 
            THD_CORE_PRIORITY,
            "core_thread", 
            core_thread_main, 
            (unsigned short)g_wifi_core.stack_size, 
            0);
	if (kNoErr != ret) 
	{
		os_printf("Create core thread failed\r\n");
		goto fail;
	}
	
	return;
	
fail:
	core_thread_uninit();
	
	return;
}

void core_thread_uninit(void)
{
	if(g_wifi_core.handle)
	{
		mico_rtos_delete_thread(&g_wifi_core.handle);
		g_wifi_core.handle = 0;
	}
	
	if(g_wifi_core.io_queue)
	{
		mico_rtos_deinit_queue(&g_wifi_core.io_queue);
		g_wifi_core.io_queue = 0;
	}
	
	g_wifi_core.queue_item_count = 0;
	g_wifi_core.stack_size = 0;
}
#endif

void app_start(void)
{
    OSStatus ret; 
    
    ret = mico_rtos_init_semaphore(&app_sema, 0);
    ASSERT(kNoErr == ret);

	
    ret = mico_rtos_create_thread(&app_thread_handle, 
            THD_APPLICATION_PRIORITY,
            "kmsgbk", 
            kmsg_bk_thread_main, 
            (unsigned short)app_stack_size, 
            0);
    ASSERT(kNoErr == ret);
    
    ret = mico_rtos_create_thread(&init_thread_handle, 
            THD_INIT_PRIORITY,
            "init_thread", 
            init_thread_main, 
            (unsigned short)init_stack_size, 
            0);
    ASSERT(kNoErr == ret);
	
	core_thread_init();
}


// eof

