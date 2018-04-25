/**
 ****************************************************************************************
 *
 * @file txl_buffer.c
 *
 * @brief Management of Tx payload buffers
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup TX_BUFFER
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "co_int.h"
#include "co_bool.h"
#include "txl_buffer.h"
#include "txl_cntrl.h"
#include "arch.h"
#include "co_utils.h"
#include "txu_cntrl.h"

#if (RW_BFMER_EN)
#include "bfr.h"
#endif //(RW_BFMER_EN)

#include "include.h"
#include "arm_arch.h"
#include "mem_pub.h"
#include "uart_pub.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
/// Environment of the Tx buffer management module
struct txl_buffer_env_tag txl_buffer_env;
uint32_t pd_pattern = TX_PAYLOAD_DESC_PATTERN;

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void txl_buffer_reinit(void)
{
    int i, j;

    for (i=0; i<NX_TXQ_CNT; i++)
    {
        txl_buffer_env.list[i].first = NULL;
        txl_buffer_env.list[i].last = NULL;
        for (j = 0; j<RW_USER_MAX; j++)
        {
            struct txl_buffer_idx_tag *idx = &txl_buffer_env.buf_idx[i][j];
            idx->free = 0;
            idx->used_area = 0;
            idx->free_size = TX_BUFFER_POOL_SIZE;
            idx->last = TX_BUFFER_NULL;
            idx->count = 0;
            idx->buf_size = 0;
            if ((i != AC_BCN) || (j == 0))
            {
                struct txl_buffer_hw_desc_tag *hwdesc = &txl_buffer_hw_desc[i*RW_USER_MAX + j];

                hwdesc->pbd.upatterntx = TX_PAYLOAD_DESC_PATTERN;
                hwdesc->pbd.bufctrlinfo = 0;
                hwdesc->pbd.next = 0;

                idx->desc = hwdesc;
            }
            else
            {
                idx->desc = NULL;
            }
						
            idx->next_needed = (uint32_t)-1;
        }
    }
}

void txl_buffer_init(void)
{
    txl_buffer_reinit();

    for (int i = 0; i<NX_REMOTE_STA_MAX; i ++)
    {
        for (int j = 0; j < 2; j ++)
        {
            struct tx_policy_tbl *pol;

            pol = &txl_buffer_control_desc[i][j].policy_tbl;
            pol->upatterntx         = POLICY_TABLE_PATTERN;
            pol->phycntrlinfo1      = phy_get_ntx() << NX_TX_PT_OFT;
            pol->phycntrlinfo2      = TX_NTX_2_ANTENNA_SET(phy_get_ntx());
            pol->maccntrlinfo1      = 0;
            pol->maccntrlinfo2      = 0xFFFF0704;

            pol->ratecntrlinfo[0]   = HW_RATE_6MBPS << MCS_INDEX_TX_RCX_OFT;
            pol->ratecntrlinfo[1]   = HW_RATE_6MBPS << MCS_INDEX_TX_RCX_OFT;
            pol->ratecntrlinfo[2]   = HW_RATE_6MBPS << MCS_INDEX_TX_RCX_OFT;
            pol->ratecntrlinfo[3]   = HW_RATE_6MBPS << MCS_INDEX_TX_RCX_OFT;
            
            pol->powercntrlinfo[0]  = nxmac_ofdm_max_pwr_level_getf() << TX_PWR_LEVEL_PT_RCX_OFT;
            pol->powercntrlinfo[1]  = nxmac_ofdm_max_pwr_level_getf() << TX_PWR_LEVEL_PT_RCX_OFT;
            pol->powercntrlinfo[2]  = nxmac_ofdm_max_pwr_level_getf() << TX_PWR_LEVEL_PT_RCX_OFT;
            pol->powercntrlinfo[3]  = nxmac_ofdm_max_pwr_level_getf() << TX_PWR_LEVEL_PT_RCX_OFT;
            
            txl_buffer_control_desc[i][j].mac_control_info = EXPECTED_ACK_NORMAL_ACK | LOW_RATE_RETRY;
            txl_buffer_control_desc[i][j].phy_control_info = 63 << GID_TX_OFT;
        }
    }
	
    for (int i = 0; i < NX_VIRT_DEV_MAX; i++)
    {
        struct tx_policy_tbl *pol = &txl_buffer_control_desc_bcmc[i].policy_tbl;
        pol->upatterntx         = POLICY_TABLE_PATTERN;
        pol->phycntrlinfo1      = phy_get_ntx() << NX_TX_PT_OFT;
        pol->phycntrlinfo2      = TX_NTX_2_ANTENNA_SET(phy_get_ntx());
        pol->maccntrlinfo1      = 0;
        pol->maccntrlinfo2      = 0xFFFF0704;
        pol->ratecntrlinfo[0]   = HW_RATE_6MBPS << MCS_INDEX_TX_RCX_OFT;
        pol->ratecntrlinfo[1]   = 0;
        pol->ratecntrlinfo[2]   = 0;
        pol->ratecntrlinfo[3]   = 0;
        pol->powercntrlinfo[0]  = nxmac_ofdm_max_pwr_level_getf() << TX_PWR_LEVEL_PT_RCX_OFT;
        pol->powercntrlinfo[1]  = 0;
        pol->powercntrlinfo[2]  = 0;
        pol->powercntrlinfo[3]  = 0;

        txl_buffer_control_desc_bcmc[i].mac_control_info = EXPECTED_ACK_NO_ACK;
        txl_buffer_control_desc_bcmc[i].phy_control_info = 63 << GID_TX_OFT;
    }
}

void txl_buffer_reset(uint8_t access_category)
{
    int i;
	struct txl_buffer_idx_tag *idx;

    txl_buffer_env.list[access_category].first = NULL;
    txl_buffer_env.list[access_category].last = NULL;
	
    for (i = 0; i < RW_USER_MAX; i++)
    {
        idx = &txl_buffer_env.buf_idx[access_category][i];

        idx->free = 0;
        idx->used_area = 0;
        idx->free_size = TX_BUFFER_POOL_SIZE;
        idx->last = TX_BUFFER_NULL;
        idx->count = 0;
        idx->buf_size = 0;
        idx->next_needed = (uint32_t)-1;
    }
}

#if NX_AMSDU_TX
/**
 ****************************************************************************************
 * @brief Get the padding value from the SW control information field
 *
 * @param[in] txdesc The pointer to the TX descriptor for which the THD has to be updated
 * @param[in] access_category The corresponding queue index
 * @param[in] pkt_idx Index of the MSDU inside the A-MSDU
 *
 * @return    The padding value
 *
 ****************************************************************************************
 */
void txl_buffer_update_tbd(struct txdesc *txdesc,
                                    uint8_t access_category,
                                    uint8_t pkt_idx)
{
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];
    struct txl_buffer_tag *buffer = txdesc->lmac.buffer[pkt_idx];
    struct tx_pbd *tbd = &buffer->tbd;
    uint32_t pkt_len = txdesc->host.packet_len[pkt_idx];
    uint32_t start = CPU2HW(buffer->payload) + buffer->padding;
    uint32_t end = start + pkt_len - 1;
    struct tx_pbd *pbd = NULL;
    struct tx_pbd *last = tbd;

    if ((pkt_idx + 1) == txdesc->host.packet_cnt) {
       pkt_len += txdesc->umac.tail_len;
       end += txdesc->umac.tail_len;
    }

    // By default we consider that the buffer is not split
    buffer->flags &= ~BUF_SPLIT;

    // Set start and end pointers to the beginning and end of the frame
    tbd->datastartptr = start;
    tbd->dataendptr = end;
    tbd->next = CPU2HW(pbd);
    tbd->bufctrlinfo = 0;

    if ((buffer->flags & BUF_FRONTIER) ||
        (is_mpdu_last(txdesc) && ((pkt_idx + 1) == txdesc->host.packet_cnt)))
        // Enable interrupt on the last PBD
        last->bufctrlinfo = TBD_INTERRUPT_EN;
    else
        last->bufctrlinfo = 0;

    // Chain the descriptor(s) to the previous one(s)
    ASSERT_ERR(txlist->last_pbd[buffer->user_idx] != NULL);
    txlist->last_pbd[buffer->user_idx]->next = CPU2HW(tbd);

    // Store the new value of last PBD
    last->next = 0;
    txlist->last_pbd[buffer->user_idx] = last;
}

struct txl_buffer_tag *txl_buffer_alloc(struct txdesc *txdesc, 
										uint8_t access_category,
										uint8_t user_idx, 
										uint8_t pkt_idx)
{
    struct txl_buffer_tag *buf = NULL;
    uint16_t add_len = 0;
    uint16_t head_len = 0;
    uint16_t size = txdesc->host.packet_len[pkt_idx];
    uint32_t packet_addr = txdesc->host.packet_addr[pkt_idx];
    struct txl_buffer_idx_tag *idx = &txl_buffer_env.buf_idx[access_category][user_idx];
    uint32_t free = idx->free;
    uint32_t freesz = idx->free_size;
    uint32_t needed;
    uint16_t dma_oft;
    uint16_t pad_size;
    struct tx_pbd *tbd;
	
	os_printf("txl_buffer_alloc\r\n");

    if (pkt_idx == 0)
    {
        add_len = txdesc->umac.head_len;
        head_len = txdesc->umac.head_len;
    }
    if ((pkt_idx + 1) == txdesc->host.packet_cnt)
    {
        add_len += txdesc->umac.tail_len;
    }

    needed = CO_ALIGN4_HI(size + add_len + sizeof_b(struct txl_buffer_tag)) >> 2;

    // Sanity checks
    ASSERT_ERR(freesz <= TX_BUFFER_POOL_SIZE);
    ASSERT_ERR(free <= TX_BUFFER_POOL_SIZE);

    do
    {
        // By default we consider that the allocation will fail
        idx->next_needed = needed;

        // Check if have enough free space
        if (freesz < needed)
            break;

        // Compute the different parameters of the download
        dma_oft = head_len + offsetof_b(struct txl_buffer_tag, payload);
        // For Management frames make sure we allocate all the frame in a contiguous space
        // Add add_len in case of protected mgmt frame (if not protected add_len = 0)
        if (txdesc->host.flags & TXU_CNTRL_MGMT)
        {
            #if NX_MFP
            if (head_len) {
                // head_len > 0 means unicast protected frame. In this case the "offset"
                // must be added between MAC header and MDPU body (by using 2 dma desc)
                dma_oft -= head_len;
            }
            #endif
            head_len = size + add_len;
        }

        // Allocate the buffer
        pad_size = sizeof(UINT32) - (packet_addr & (sizeof(UINT32) - 1));
        buf = (struct txl_buffer_tag *)(packet_addr - dma_oft - pad_size);
        buf->length = needed;
        buf->padding = pad_size;
        buf->flags = 0;
        buf->user_idx = user_idx;
        idx->last = free;
        idx->next_needed = (uint32_t)-1;
        freesz -= needed;

        // Prepare the DMA descriptor for the pattern
        tbd = &buf->tbd;
        tbd->upatterntx = TX_PAYLOAD_DESC_PATTERN;

        if (pkt_idx > 0)
        {
            // Check if we need to perform some actions once this payload is transfered
            #if NX_AMPDU_TX
            if (is_mpdu_agg(txdesc))
            {
                struct tx_agg_desc *agg_desc = txdesc->lmac.agg_desc;

                agg_desc->available_len += size;

                if (!(agg_desc->status & AGG_ALLOC) &&
                     (agg_desc->available_len > TX_BUFFER_MIN_AMPDU_DWNLD))
                {
                    // Enough data is now considered as allocated for this
                    // A-MPDU, so no need to get DMA interrupts anymore
                    agg_desc->status |= AGG_ALLOC;
                    buf->flags |= BUF_ALLOC_OK | BUF_INT_MSDU;

                    txl_buffer_push(access_category, buf);
                }
            }
            #endif
        }
        else
        {
            if (!(txdesc->host.flags & TXU_CNTRL_MGMT))
            {
                // Call the UMAC for the header formatting
                txu_cntrl_frame_build(txdesc, CPU2HW(buf->payload) + buf->padding + txdesc->umac.head_len);
            }

            // Check if we need to perform some actions once this payload is transfered
            if (is_mpdu_agg(txdesc))
            {
                struct tx_agg_desc *agg_desc = txdesc->lmac.agg_desc;

                agg_desc->available_len += size;

                if (is_mpdu_first(txdesc))
                {
                    txl_buffer_push(access_category, buf);

                    if (agg_desc->available_len > TX_BUFFER_MIN_AMPDU_DWNLD)
                    {
                        // Enough data is now considered as allocated for this
                        // A-MPDU, so no need to get DMA interrupts anymore
                        agg_desc->status |= AGG_ALLOC;
                        buf->flags |= BUF_ALLOC_OK;
                    }
                }
                else if (!(agg_desc->status & AGG_ALLOC) &&
                        ((agg_desc->available_len > TX_BUFFER_MIN_AMPDU_DWNLD)
                                                   || is_mpdu_last(txdesc)))
                {
                    // Enough data is now considered as allocated for this
                    // A-MPDU, so no need to get DMA interrupts anymore
                    agg_desc->status |= AGG_ALLOC;
                    buf->flags |= BUF_ALLOC_OK;
                    txl_buffer_push(access_category, buf);
                }
            }
            else
            {
                buf->flags |= BUF_ALLOC_OK;
                txl_buffer_push(access_category, buf);
            }
        }

        // Start the transfer
        dma_push(0, 0, IPC_DMA_CHANNEL_DATA_TX);

        // Update the free index and used area
        idx->free = free;
        idx->free_size = freesz;
        idx->used_area += needed;
        idx->buf_size += needed;
        if (idx->buf_size >= TX_BUFFER_MIN_SIZE)
        {
            // One more super buffer available
            idx->count++;
            // We set this buffer as a frontier
            buf->flags |= BUF_FRONTIER;
            // We start the new one
            idx->buf_size = 0;
        }
        else
        {
            // This buffer is not a frontier between 2 super buffers
            buf->flags &= ~BUF_FRONTIER;
        }
    } while(0);

    return(buf);
}                                        
#else
struct txl_buffer_tag *txl_buffer_alloc(struct txdesc *txdesc, 
                                            uint8_t access_category,
                                            uint8_t user_idx)
{
    uint16_t dma_oft;
    uint16_t pad_size;
    uint16_t add_len = 0;
    uint16_t head_len = 0;
    struct txl_buffer_tag *buf = NULL;
    uint16_t size = txdesc->host.packet_len;
    uint32_t packet_addr = txdesc->host.packet_addr;
    struct txl_buffer_idx_tag *idx = &txl_buffer_env.buf_idx[access_category][user_idx];
    uint32_t free = idx->free;
    uint32_t freesz = idx->free_size;
    uint32_t needed;
    struct tx_pbd *tbd;

    add_len = txdesc->umac.head_len + txdesc->umac.tail_len;
    head_len = txdesc->umac.head_len;
    needed = CO_ALIGN4_HI(size + add_len 
                                + sizeof_b(struct txl_buffer_tag)) >> 2;

    do
    {
        // By default we consider that the allocation will fail
        idx->next_needed = needed;

        // Compute the different parameters of the download
        dma_oft = head_len + offsetof_b(struct txl_buffer_tag, payload);
        // For Management frames make sure we allocate all the frame in a contiguous space
        // Add add_len in case of protected mgmt frame (if not protected add_len = 0)
        if (txdesc->host.flags & TXU_CNTRL_MGMT)
        {
            #if NX_MFP
            if (head_len) {
                // head_len > 0 means unicast protected frame. In this case the "offset"
                // must be added between MAC header and MDPU body (by using 2 dma desc)
                dma_oft -= head_len;
            }
            #endif
            head_len = size + add_len;
        }

        // Allocate the buffer
        ASSERT((packet_addr - txdesc->host.orig_addr) > dma_oft);

        pad_size = (sizeof(UINT32) - (packet_addr & (sizeof(UINT32) - 1))) % sizeof(UINT32);
        buf = (struct txl_buffer_tag *)(packet_addr - dma_oft - pad_size);
        buf->length = needed;
        buf->padding = pad_size;
        buf->flags = 0;
        buf->user_idx = user_idx;
        idx->last = free;
        idx->next_needed = (uint32_t)-1;
        freesz -= needed;

        // Prepare the DMA descriptor for the pattern
        tbd = &buf->tbd;
        tbd->upatterntx = TX_PAYLOAD_DESC_PATTERN;

        if (!(txdesc->host.flags & TXU_CNTRL_MGMT))
        {
            txu_cntrl_frame_build(txdesc, CPU2HW(buf->payload) + buf->padding + txdesc->umac.head_len);
        }

        txl_buffer_push(access_category, buf);

        dma_push(0, 0, IPC_DMA_CHANNEL_DATA_TX);

        // Update the free index and used area
        idx->free = free;
        idx->free_size = freesz;
        idx->used_area += needed;
        idx->buf_size += needed;
        if (idx->buf_size >= TX_BUFFER_MIN_SIZE)
        {
            // One more super buffer available
            idx->count++;
            // We set this buffer as a frontier
            buf->flags |= BUF_FRONTIER;
            // We start the new one
            idx->buf_size = 0;
        }
        else
        {
            // This buffer is not a frontier between 2 super buffers
            buf->flags &= ~BUF_FRONTIER;
        }
    } while(0);

    return(buf);
}
#endif


bool txl_buffer_free(struct txl_buffer_tag *buf, uint8_t access_category)
{
    struct txl_buffer_idx_tag *idx = &txl_buffer_env.buf_idx[access_category][buf->user_idx];

    // Update the used area
    idx->used_area -= buf->length;
    idx->free_size += buf->length;
    if (buf->flags & BUF_FRONTIER)
        idx->count--;

    // Check if we freed the last buffer
    if (idx->used_area == 0)
    {
        idx->free = 0;
        idx->last = TX_BUFFER_NULL;
        idx->count = 0;
        idx->buf_size = 0;
    }

    return ((buf->flags & BUF_FRONTIER) || (idx->free_size >= idx->next_needed));
}

/// @name External API
/// @{
void txl_buffer_push(uint8_t access_category, struct txl_buffer_tag *buf)
{
    struct txl_buffer_list_tag *list = &txl_buffer_env.list[access_category];

    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();
    // check if list is empty
    if (list->first == NULL)
    {
        // list empty => pushed element is also head
        list->first = buf;
    }
    else
    {
        // list not empty => update next of last
        list->last->next = buf;
    }

    // add element at the end of the list
    list->last = buf;
    buf->next = NULL;
    
    GLOBAL_INT_RESTORE();
}

struct txl_buffer_tag *txl_buffer_pop(uint8_t access_category)
{
    struct txl_buffer_list_tag *list = &txl_buffer_env.list[access_category];
    struct txl_buffer_tag *buf;

    GLOBAL_INT_DECLARATION();

    GLOBAL_INT_DISABLE();
    // check if list is empty
    buf = list->first;
    if (buf != NULL)
    {
        // The list isn't empty : extract the first element
        list->first = list->first->next;
    }

    GLOBAL_INT_RESTORE();
    
    return buf;
}

struct txl_buffer_tag *txl_buffer_pick(uint8_t access_category)
{
    struct txl_buffer_list_tag *list = &txl_buffer_env.list[access_category];

    return list->first;
}

bool txl_buffer_list_empty(uint8_t access_category)
{
    struct txl_buffer_list_tag *list = &txl_buffer_env.list[access_category];

    return (list->first == NULL);
}


/**
 ****************************************************************************************
 * @brief Get the pointer to the buffer control structure
 *
 * @param[in]   txdesc The pointer to the TX descriptor
 *
 * @return    The pointer to the buffer control structure
 *
 ****************************************************************************************
 */
struct txl_buffer_tag *txl_buffer_get(struct txdesc *txdesc)
{
    #if NX_AMSDU_TX
    struct txl_buffer_tag *buffer = txdesc->lmac.buffer[0];
    #else
    struct txl_buffer_tag *buffer = txdesc->lmac.buffer;
    #endif
    return (buffer);
}


/**
 ****************************************************************************************
 * @brief Get the pointer to the buffer control structure
 *
 * @param[in]   txdesc The pointer to the TX descriptor
 *
 * @return    The pointer to the buffer control structure
 *
 ****************************************************************************************
 */
struct txl_buffer_control *txl_buffer_control_get(struct txdesc *txdesc)
{
    return (txdesc->umac.buf_control);
}

/**
 ****************************************************************************************
 * @brief Get the padding value from the SW control information field
 *
 * @param[in]   txdesc   The pointer to the TX descriptor for which the THD has to be
 * updated
 * @param[in]   access_category The corresponding queue index
 *
 ****************************************************************************************
 */
void txl_buffer_update_thd(struct txdesc *txdesc,
                                    uint8_t access_category)
{
    uint16_t add_len = 0;
    struct tx_hd *thd = &txdesc->lmac.hw_desc->thd;
    
    #if NX_AMSDU_TX
    struct txl_list *txlist = &txl_cntrl_env.txlist[access_category];
    struct txl_buffer_tag *buffer = txdesc->lmac.buffer[0];
    uint32_t pkt_len = txdesc->host.packet_len[0] + add_len;
    #else
    struct txl_buffer_tag *buffer = txdesc->lmac.buffer;
    uint32_t pkt_len = txdesc->host.packet_len + add_len;
    #endif
    
    uint32_t start = CPU2HW(buffer->payload) + buffer->padding;
    uint32_t end = start + pkt_len - 1;
    struct tx_pbd *pbd = &buffer->tbd;
    struct tx_pbd *last_pbd = pbd;

    add_len = txdesc->umac.head_len;
    #if NX_AMSDU_TX
    if (txdesc->host.packet_cnt == 1)
    #endif
        add_len += txdesc->umac.tail_len;

    pkt_len += add_len;
    end += add_len;

    // By default we consider that the buffer is not split
    buffer->flags &= ~BUF_SPLIT;

    // Set start and end pointers to the beginning and end of the frame
    pbd->datastartptr = start;
    pbd->dataendptr = end;
    pbd->next = 0;
    pbd->bufctrlinfo = 0;

    thd->first_pbd_ptr = CPU2HW(pbd);
    last_pbd->bufctrlinfo = 0;
	
    #if NX_AMPDU_TX
    if ((buffer->flags & BUF_FRONTIER) 
            || is_mpdu_last(txdesc) 
            || !is_mpdu_agg(txdesc))
    {
        if (!(buffer->flags & BUF_FRONTIER))
        {
            #if NX_AMSDU_TX
            if (txdesc->host.packet_cnt == 1)
            {
            #endif
                buffer->flags |= BUF_FRONTIER;
            #if NX_AMSDU_TX
            }
            #endif
            // Set WhichDescriptor field
            thd->macctrlinfo2 |= INTERRUPT_EN_TX;
        }
        else
        {
            #if NX_AMSDU_TX
            if (txdesc->host.packet_cnt > 1)
            {
                // Enable interrupt on the last PBD
                last_pbd->bufctrlinfo = TBD_INTERRUPT_EN;

                // For singleton we need to enable the IRQ on packet completion
                if (!is_mpdu_agg(txdesc))
                    thd->macctrlinfo2 |= INTERRUPT_EN_TX;
            }
            else
            #endif
            {
                // Set WhichDescriptor field
                thd->macctrlinfo2 |= INTERRUPT_EN_TX;
            }
        }
    }
    #else
    // Set WhichDescriptor field
    thd->macctrlinfo2 = WHICHDESC_UNFRAGMENTED_MSDU | INTERRUPT_EN_TX;
    #endif
    last_pbd->next = 0;

    #if NX_AMSDU_TX
    // Store the new value of last PBD
    txlist->last_pbd[buffer->user_idx] = last_pbd;
    #endif
}

/**
 ****************************************************************************************
 * @brief Get the padding value from the SW control information field
 *
 * @param[in]   buf   The pointer to the buffer for which the padding value has to be
 *                    retrieved
 *
 * @return    The padding value
 *
 ****************************************************************************************
 */
void txl_buffer_mic_compute(struct txdesc *txdesc,
                                     uint32_t *mic_key,
                                     uint32_t start,
                                     uint32_t len,
                                     uint8_t access_category)
{
    struct hostdesc *host = &txdesc->host;
    struct mic_calc mic;

    uint32_t end = start + len - 1;
    uint32_t clen = len;
    uint32_t mic_start = end + 1, mic_end;
    uint32_t mic_addr = CPU2HW(&mic.mic_key_least);

    // Initialize MIC computation
    me_mic_init(&mic, mic_key, &host->eth_dest_addr, &host->eth_src_addr, host->tid);

    me_mic_calc(&mic, start, clen);

    // End the MIC computation
    me_mic_end(&mic);
    
    mic_end = mic_start + 7;
    clen = 8;
    
    // Copy the remaining part of the MIC in the TX buffer
    co_copy8p(mic_start, mic_addr, clen);
    (void)mic_end;
}

/**
 ****************************************************************************************
 * @brief Returns the amount of data currently allocated in the TX buffer
 *
 * @param[in] access_category   Access category for which the buffer data is checked
 * @param[in] user_idx          User index (for MU-MIMO TX only, 0 otherwise)
 *
 * @return The allocated data size, in 32-bit words
 *
 ****************************************************************************************
 */
uint32_t txl_buffer_used(uint8_t access_category, uint8_t user_idx)
{
    return txl_buffer_env.buf_idx[access_category][user_idx].used_area;
}

/**
 ****************************************************************************************
 * @brief Indicates if the TX buffer pool for this queue is full or not
 *
 * @param[in] access_category   Access category for which the buffer data is checked
 * @param[in] user_idx          User index (for MU-MIMO TX only, 0 otherwise)
 *
 * @return true if the buffer pool is full, false otherwise
 *
 ****************************************************************************************
 */
uint8_t txl_buffer_count(uint8_t access_category, uint8_t user_idx)
{
    struct txl_buffer_idx_tag *idx = &txl_buffer_env.buf_idx[access_category][user_idx];

    return(idx->count);
}



/// @}  // end of group BUFFER
