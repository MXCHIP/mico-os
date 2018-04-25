#include "include.h"
#include "rw_msdu.h"
#include "rw_pub.h"

#include "mem_pub.h"
#include "txu_cntrl.h"
#include "vif_mgmt.h"

#include "lwip/pbuf.h"

#include "arm_arch.h"
#if CFG_GENERAL_DMA
#include "general_dma_pub.h"
#endif
#if CFG_MODE_SWITCH
#include "param_config.h"
#endif

void ethernetif_input(int iface, struct pbuf *p);

LIST_HEAD_DEFINE(msdu_tx_list);
LIST_HEAD_DEFINE(msdu_rx_list);

void rwm_push_rx_list(MSDU_NODE_T *node)
{
    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();
    list_add_tail(&node->hdr, &msdu_rx_list);
    GLOBAL_INT_RESTORE();
}

MSDU_NODE_T *rwm_pop_rx_list(void)
{
    LIST_HEADER_T *tmp;
    LIST_HEADER_T *pos;
    MSDU_NODE_PTR node;

    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();

    node = NULLPTR;
    list_for_each_safe(pos, tmp, &msdu_rx_list)
    {
        list_del(pos);
        node = list_entry(pos, MSDU_NODE_T, hdr);

        break;
    }

    GLOBAL_INT_RESTORE();

    return node;
}

void rwm_push_tx_list(MSDU_NODE_T *node)
{
    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();
    list_add_tail(&node->hdr, &msdu_tx_list);
    GLOBAL_INT_RESTORE();
}

void rwm_flush_rx_list(void)
{
    MSDU_NODE_T *node_ptr;

	while(1)
	{
	    node_ptr = rwm_pop_rx_list();
		if(node_ptr)
		{
	    	os_free(node_ptr);
		}
		else
		{
			break;
		}
	}
}

void rwm_flush_tx_list(void)
{
    MSDU_NODE_T *node;

	while(1)
	{
	    node = rwm_pop_tx_list();
		if(node)
		{
	    	rwm_node_free(node);
		}
		else
		{
			break;
		}
	}
}

MSDU_NODE_T *rwm_pop_tx_list(void)
{
    LIST_HEADER_T *tmp;
    LIST_HEADER_T *pos;
    MSDU_NODE_PTR node;

    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();

    node = NULLPTR;
    list_for_each_safe(pos, tmp, &msdu_tx_list)
    {
        list_del(pos);
        node = list_entry(pos, MSDU_NODE_T, hdr);

        break;
    }

    GLOBAL_INT_RESTORE();

    return node;
}

void rwm_tx_confirm(void)
{
    MSDU_NODE_T *node;

    node = rwm_pop_tx_list();

    rwm_node_free(node);
}

void rwm_tx_msdu_renew(UINT8 *buf, UINT32 len, UINT8 *orig_addr)
{
#if CFG_GENERAL_DMA
    gdma_memcpy((void *)((UINT32)orig_addr + CFG_MSDU_RESV_HEAD_LEN), buf, len);
#else
    os_memmove((void *)((UINT32)orig_addr + CFG_MSDU_RESV_HEAD_LEN), buf, len);
#endif
}

UINT8 *rwm_get_msdu_content_ptr(MSDU_NODE_T *node)
{
    return (UINT8 *)((UINT32)node->msdu_ptr + CFG_MSDU_RESV_HEAD_LEN);
}

void rwm_txdesc_copy(struct txdesc *dst_local, ETH_HDR_PTR eth_hdr_ptr)
{
    struct hostdesc *host_ptr;

    host_ptr = &dst_local->host;

    os_memcpy(&host_ptr->eth_dest_addr, &eth_hdr_ptr->e_dest, sizeof(host_ptr->eth_dest_addr));
    os_memcpy(&host_ptr->eth_src_addr, &eth_hdr_ptr->e_src, sizeof(host_ptr->eth_src_addr));
}

MSDU_NODE_T *rwm_tx_node_alloc(UINT32 len)
{
    UINT8 *buff_ptr;
    MSDU_NODE_T *node_ptr = 0;

    node_ptr = (MSDU_NODE_T *)os_malloc(sizeof(MSDU_NODE_T)
                                        + CFG_MSDU_RESV_HEAD_LEN
                                        + len
                                        + CFG_MSDU_RESV_TAIL_LEN);

    if(NULL == node_ptr)
    {
        goto alloc_exit;
    }

    buff_ptr = (UINT8 *)((UINT32)node_ptr + sizeof(MSDU_NODE_T));

    node_ptr->msdu_ptr = buff_ptr;
    node_ptr->len = len;

alloc_exit:
    return node_ptr;
}

void rwm_node_free(MSDU_NODE_T *node)
{
    ASSERT(node);
    os_free(node);
}

UINT8 *rwm_rx_buf_alloc(UINT32 len)
{
    return (UINT8 *)os_malloc(len);
}

UINT32 rwm_get_rx_valid(void)
{
    UINT32 count = 0;
    LIST_HEADER_T *tmp;
    LIST_HEADER_T *pos;
    LIST_HEADER_T *head = &msdu_rx_list;

    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();
    list_for_each_safe(pos, tmp, head)
    {
        count ++;
    }
    GLOBAL_INT_RESTORE();

    return ((count >= MSDU_RX_MAX_CNT) ? 0 : 1);
}

UINT32 rwm_get_rx_valid_node_len(void)
{
    UINT32 len = 0;
    LIST_HEADER_T *tmp;
    LIST_HEADER_T *pos;
    MSDU_NODE_PTR node;

    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();

    node = NULLPTR;
    list_for_each_safe(pos, tmp, &msdu_rx_list)
    {
        node = list_entry(pos, MSDU_NODE_T, hdr);
        len = node->len;
        break;
    }

    GLOBAL_INT_RESTORE();

    return len;
}

UINT32 rwm_transfer(UINT8 *buf, UINT32 len)
{
    UINT8 tid;
    UINT32 ret;
    UINT32 queue_idx;
    UINT8 *content_ptr;
    MSDU_NODE_T *node;
    ETH_HDR_PTR eth_hdr_ptr;
    struct txdesc *txdesc_new;

	//os_printf("rwm_transfer\r\n");
    ret = RW_FAILURE;
    node = rwm_tx_node_alloc(len);
    if(NULL == node)
    {
        goto tx_exit;
    }
    rwm_tx_msdu_renew(buf, len, node->msdu_ptr);
    content_ptr = rwm_get_msdu_content_ptr(node);
    eth_hdr_ptr = (ETH_HDR_PTR)content_ptr;

    tid = 0xff;
    queue_idx = AC_VI;
    txdesc_new = tx_txdesc_prepare(queue_idx);
    if(TXDESC_STA_USED == txdesc_new->status)
    {
        rwm_node_free(node);

        goto tx_exit;
    }

    txdesc_new->status = TXDESC_STA_USED;
    rwm_txdesc_copy(txdesc_new, eth_hdr_ptr);

    txdesc_new->host.flags            = 0;
#if NX_AMSDU_TX
    txdesc_new->host.orig_addr[0]     = (UINT32)node->msdu_ptr;
    txdesc_new->host.packet_addr[0]   = (UINT32)content_ptr + 14;
    txdesc_new->host.packet_len[0]    = node->len - 14;
    txdesc_new->host.packet_cnt       = 1;
#else
    txdesc_new->host.orig_addr        = (UINT32)node->msdu_ptr;
    txdesc_new->host.packet_addr      = (UINT32)content_ptr + 14;
    txdesc_new->host.packet_len       = node->len - 14;
#endif
    txdesc_new->host.status_desc_addr = (UINT32)content_ptr + 14;
    txdesc_new->host.ethertype        = eth_hdr_ptr->e_proto;
    txdesc_new->host.tid              = tid;
    txdesc_new->host.vif_idx          = 0;

#if CFG_WIFI_AP_MODE
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_AP)
#endif
	{
	    if(is_broadcast_eth_addr(eth_hdr_ptr->e_dest))
	    {
	        txdesc_new->host.staid = VIF_TO_BCMC_IDX(txdesc_new->host.vif_idx);
	    }
	    else
	    {
	        txdesc_new->host.staid = 0;
	    }
	}
#endif

#if CFG_WIFI_STATION_MODE
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_STA)
#endif
	{
		uint32_t ret;
		uint32_t staid = 0;
		
		ret = vif_mgmt_first_used_staid(&staid);
		if(ret)
		{
			os_printf("trMsduStrange\r\n");
		}
		
    	txdesc_new->host.staid	  = staid;
	}
#endif

    txdesc_new->lmac.agg_desc = NULL;
    txdesc_new->lmac.hw_desc->cfm.status = 0;

    rwm_push_tx_list(node);
    txu_cntrl_push(txdesc_new, queue_idx);

tx_exit:
    return ret;
}

UINT32 rwm_get_rx_free_node(struct pbuf **p_ret, UINT32 len)
{
    struct pbuf *p;

	p = pbuf_alloc(PBUF_RAW, len, PBUF_RAM);
	*p_ret = p;
	
    return RW_SUCCESS;
}

UINT32 rwm_upload_data(struct pbuf *p, UINT32 len)
{
	ethernetif_input(0, p);

    return RW_SUCCESS;
}

UINT32 rwm_uploaded_data_handle(UINT8 *upper_buf, UINT32 len)
{
    UINT32 count;
    UINT32 ret = RW_FAILURE;
    MSDU_NODE_T *node_ptr;

    node_ptr = rwm_pop_rx_list();
    if(node_ptr)
    {
        count = MIN(len, node_ptr->len);
#if CFG_GENERAL_DMA
        gdma_memcpy(upper_buf, node_ptr->msdu_ptr, count);
#else
        os_memcpy(upper_buf, node_ptr->msdu_ptr, count);
#endif
        ret = count;

        os_free(node_ptr);
        node_ptr = NULL;
    }

    return ret;
}
// eof

