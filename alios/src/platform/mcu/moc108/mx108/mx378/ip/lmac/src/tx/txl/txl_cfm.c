/**
 ****************************************************************************************
 *
 * @file txl_cfm.c
 *
 * @brief Tx confirmation module.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/*
 ****************************************************************************************
 * @addtogroup TX_CFM
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stddef.h>
#include "txl_cntrl.h"
#include "txl_cfm.h"
#include "ke_event.h"
#include "mac_frame.h"
#include "txl_buffer.h"
#include "txl_frame.h"
#include "co_utils.h"
#include "ps.h"

#if NX_AMPDU_TX
#include "mm.h"
#include "rxl_cntrl.h"
#include "rx_swdesc.h"
#endif

#include "me_utils.h"
#include "txu_cntrl.h"
#include "rwnx.h"

#if (RW_BFMER_EN)
#include "bfr.h"
#endif //(RW_BFMER_EN)

#include "include.h"
#include "arm_arch.h"

#include "sdio_pub.h"
#include "sdio_intf_pub.h"

/*
 * GLOBAL VARIABLE DEFINITION
 ****************************************************************************************
 */
struct txl_cfm_env_tag txl_cfm_env;

const uint32_t txl_cfm_evt_bit[NX_TXQ_CNT] =
{
    KE_EVT_TXCFM_AC0_BIT,  // [AC_BK] =
    KE_EVT_TXCFM_AC1_BIT,   // [AC_BE] =
    KE_EVT_TXCFM_AC2_BIT,   // [AC_VI] =
    KE_EVT_TXCFM_AC3_BIT,   // [AC_VO] =

#if NX_BEACONING
    KE_EVT_TXCFM_BCN_BIT,   // [AC_BCN] =
#endif
};

/// Offset of the status element in the TX buffer
#define CFM_STATUS_OFFSET   offsetof_b(struct txl_buffer_control, status)

/// Number of prepared confirmations after which we program the DMA to transmit them
#define TXL_CFM_CNT_THRESHOLD   8



/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Prepare the confirmation by adding it to the queue
 *
 * @param[in] first  Pointer to the first CFM DMA descriptor location
 * @param[in] curr   Pointer to the current CFM DMA descriptor location, updated
 * @param[in] txdesc Pointer to the Tx descriptor for which a CFM is prepared
 *
 ****************************************************************************************
 */
static void txl_cfm_prep(uint8_t access_category,
                         struct dma_desc **first,
                         struct dma_desc **curr,
                         uint32_t *cfm_ind,
                         struct txdesc *txdesc)
{
    struct dma_desc *desc = &txdesc->lmac.hw_desc->dma_desc;

    // Fill in the confirmation DMA descriptor
    desc->dest = txdesc->host.status_desc_addr;
    desc->ctrl = 0;

#if NX_POWERSAVE
    // Decrease the number of packets in the TX path
    txl_cntrl_env.pck_cnt--;
#endif

#if (RW_MUMIMO_TX_EN)
    /// Set user position bit that needs to be confirmed
    *cfm_ind |= CO_BIT(get_user_pos(txdesc) + access_category * RW_USER_MAX);
#endif

    // Check if the TX descriptor is the first to be confirmed
    if (*first == NULL)
    {
        *first = desc;
    }
    else
    {
        (*curr)->next = CPU2HW(desc);
    }

    // Update current descriptor pointer
    *curr = desc;
}

/**
 ****************************************************************************************
 * @brief Program a confirmation  - DMA transfer and interrupt - on a certain AC
 *
 * @param[in] access_category  Access category value
 * @param[in] first            Pointer to the first CFM DMA descriptor
 * @param[in] last             Pointer to the last CFM DMA descriptor
 *
 ****************************************************************************************
 */
static void txl_cfm_prog(uint8_t access_category,
                         struct dma_desc *first,
                         struct dma_desc *last,
                         uint32_t cfm_ind)
{
    #if NX_POWERSAVE
    GLOBAL_INT_DECLARATION();
    #endif
    
    // Check if some confirmations have to be programmed
    if (first != NULL)
    {
        // Enable an interrupt on the AC
        last->ctrl = 0;
#if NX_POWERSAVE
        GLOBAL_INT_DISABLE();
        ps_env.prevent_sleep |= PS_TX_CFM_UPLOADING;
#endif

        // Set the confirmation bits into the array
        //ASSERT_ERR(txl_cfm_env.user_cfm[txl_cfm_env.in_idx] == 0);
        txl_cfm_env.user_cfm[txl_cfm_env.in_idx] = cfm_ind;
        txl_cfm_env.in_idx = (txl_cfm_env.in_idx + 1) & TXL_CFM_IDX_MSK;

        // Program the DMA
        dma_push(first, last, IPC_DMA_CHANNEL_DATA_RX);

#if NX_POWERSAVE
        GLOBAL_INT_RESTORE();
#endif
    }
}

#if NX_AMPDU_TX
/**
 ****************************************************************************************
 * @brief Recover the access category value corresponding to a received BA frame by
 *        extracting the TID from its payload.
 *
 * @param[in] rxdesc  Pointer to Rx descriptor for BA frame for which AC is determined
 *
 * @return One of the 4 access category values if any correspond, invalid value if not.
 ****************************************************************************************
 */
static uint8_t txl_ba_ac_get(struct rx_swdesc *rxdesc)
{
    // Get the status information from the RX header descriptor
    uint32_t statinfo = rxdesc->dma_hdrdesc->hd.statinfo;

    // And extract the access category from it
    return (uint8_t)((statinfo & IMM_RSP_AC_MSK) >> IMM_RSP_AC_OFT);
}

/**
 ****************************************************************************************
 * @brief This function checks if the received BA content match the A-MPDU it
 * acknowledges, i.e if TA and TID correspond to the respective parameters of the A-MPDU
 * we sent.
 *
 * @param[in] badesc  Pointer to Rx descriptor of BA frame
 * @param[in] sta_idx Index of the peer STA to which we just sent an A-MPDU
 * @param[in] tid     TID of the A-MPDU we just sent
 *
 * @return true if TA and TID of the BA frame match the respective parameters the A-MPDU,
 *         false otherwise.
 ****************************************************************************************
 */
static bool txl_is_ba_valid(struct rx_swdesc *badesc, uint8_t sta_idx, uint8_t tid)
{
    uint8_t ba_sta_idx;
    uint8_t ba_tid;
    uint16_t key_idx_hw;
    struct ba_frame *ba_payl;
    struct rx_hd *hdrdesc = &badesc->dma_hdrdesc->hd;
    struct rx_pbd *pd = HW2CPU(hdrdesc->first_pbd_ptr);
    uint32_t statinfo = hdrdesc->statinfo;

    // Check if BA frame length is correct
    if (hdrdesc->frmlen != BA_FRM_LEN)
        // Length not correct
        return false;

    // Check if the Key index is valid, i.e. if the TA is known
    if (!(statinfo & KEY_IDX_VALID_BIT))
        // TA not known, we consider the BA as invalid
        return false;

    // Get the key index
    key_idx_hw = (uint16_t)((statinfo & KEY_IDX_MSK) >> KEY_IDX_OFT);

    // Sanity check
    ASSERT_REC_VAL(key_idx_hw >= MM_SEC_DEFAULT_KEY_COUNT, false);

    // Get the STA index from the key index
    ba_sta_idx = (uint8_t)(key_idx_hw - MM_SEC_DEFAULT_KEY_COUNT);

    // Check if both STA indexes match
    if (ba_sta_idx != sta_idx)
        // No match, we consider BA as invalid
        return false;

    // Extract the TID
    ba_payl = (struct ba_frame *)HW2CPU(pd->datastartptr);
    // Byte 1 is MSB of BA Control field, its upper 4 bits = TID
    ba_tid = (ba_payl->ba_cntrl & BA_PAYL_TID_BIT_MSK) >> BA_PAYL_TID_BIT_OFT;

    // Check if both TID match
    if (ba_tid != tid)
        // No match, we consider BA as invalid
        return false;

    // All checks passed, BA is valid
    return true;
}

/**
 ****************************************************************************************
 * @brief Returns true if the first SN is the the min of two sequence numbers
 *
 * @param[in] sn1     First sequence number to be compared
 * @param[in] sn2     Second sequence number to be compared
 *
 * @return True if sn1 <= sn2
 ****************************************************************************************
 */
//ret true if sn1 is min of the two
static bool txl_comp_sn(uint16_t sn1, uint16_t sn2)
{
    if (((uint16_t)((sn1 - sn2) & 0x0FFF)) < 0x07FF)
        return false;
    else
        return true;
}

/**
 ****************************************************************************************
 * @brief Extract the ACK status of an MPDU from a BA frame bitmap
 * @param[in] txdesc   Pointer to Tx SW descriptor of the MPDU whose status will be extracted
 * @param[in] ba_desc  Pointer to Rx SW descriptor of the BA frame
 * @param[out] agg_ok  Variable to be incremented if the MPDU is confirmed
 * @return The status bits to be put in the descriptor status field
 ****************************************************************************************
 */
static uint32_t txl_ba_extract_ack(struct txdesc *txdesc, struct rx_swdesc *ba_desc,
                                   int *agg_ok)
{
    //payload
    struct rx_hd *hdrdesc = &ba_desc->dma_hdrdesc->hd;
    struct rx_pbd *pd = HW2CPU(hdrdesc->first_pbd_ptr);
    struct ba_frame *ba_payl = (struct ba_frame *)HW2CPU(pd->datastartptr);
    //byte 2 is LSB of Start Sequence control, at bit 4 the SSN starts
    uint16_t ssn = ba_payl->ssc >> SN_IN_SSC_BIT_OFT;
    uint16_t bit_pos;
    uint8_t word_idx;
    uint8_t bit_in_word_pos;

    //txdesc is below the start of the BA window - bad, means we moved the window! can never get an ACK for it
    if (txl_comp_sn(txdesc->host.sn, ssn))
    {
        return BA_FRAME_RECEIVED_BIT;
    }

    bit_pos = (txdesc->host.sn - ssn) & 0xFFF;
    word_idx = bit_pos / 16;
    bit_in_word_pos = bit_pos % 16;

    //make sure SN is in window - shouldn't be possible
    if (word_idx > 3)
    {
        return BA_FRAME_RECEIVED_BIT;
    }

    if (((ba_payl->bitmap[word_idx] >> bit_in_word_pos) & 0x01) == 0x01)
    {
        // Increment the number of MPDUs successfully transmitted
        (*agg_ok)++;
        return (BA_FRAME_RECEIVED_BIT | FRAME_SUCCESSFUL_TX_BIT);
    }
    else
    {
        return BA_FRAME_RECEIVED_BIT;
    }
}

/**
 ****************************************************************************************
 * @brief Release all AMPDU related space; rx BA, BAR, aggregate control and its AMPDU THD
 *
 * @param[in] ba_desc  Pointer to Rx SW descriptor of the BA frame
 * @param[in] ac       Access category needed to recover the aggregates list
 ****************************************************************************************
 */
static void txl_agg_release(struct rx_swdesc *ba_desc, uint8_t ac)
{
    struct tx_agg_desc *agg_desc =
        (struct tx_agg_desc *)co_list_pop_front(&(txl_cntrl_env.txlist[ac].aggregates));

    //get new sn min we can use and free BA rx frame
    if (NULL != ba_desc)
    {
        //free BA frame
        rxl_frame_release(ba_desc);
    }

    // Decrease the user count
    agg_desc->user_cnt--;

    // Free the descriptor
    if (agg_desc->user_cnt == 0)
    {
        txl_agg_desc_free(agg_desc, ac);
    }
}
#endif


void txl_cfm_init(void)
{
    int i;

    memset(&txl_cfm_env, 0, sizeof(txl_cfm_env));

    for (i = 0; i < NX_TXQ_CNT; i++)
    {
        co_list_init(&txl_cfm_env.cfmlist[i]);
    }
}

void txl_cfm_push(struct txdesc *txdesc, uint32_t status, uint8_t access_category)
{
    txdesc->lmac.hw_desc->cfm.status = status;

    // Push the descriptor in the confirmation queue
    co_list_push_back(&txl_cfm_env.cfmlist[access_category], (struct co_list_hdr *)txdesc);

#if NX_AMPDU_TX
    //call the CFM event only if it is a singleton, for MPDUs in AMPDU CFM event is set when BAR is done
    if (!is_mpdu_agg(txdesc))
#endif

        // Set an event to handle the confirmation in background
        ke_evt_set(txl_cfm_evt_bit[access_category]);
}

#if NX_AMPDU_TX
void txl_ba_push(struct rx_swdesc *rxdesc)
{
    uint8_t ac;
    struct tx_agg_desc *agg_desc;


    // Get the access category value
    ac = txl_ba_ac_get(rxdesc);

    // Set BA in appropriate aggregate descriptor
    agg_desc = (struct tx_agg_desc *)co_list_pick(&(txl_cntrl_env.txlist[ac].aggregates));
    while (1)
    {
        // Sanity check - There shall be an AGG descriptor for this BA, because unexpected
        // BAs are filtered earlier in the RX chain
        ASSERT_REC(agg_desc != NULL);

        // Check if we already received the BA for this A-MPDU
        if (!(agg_desc->status & AGG_DONE) && (agg_desc->ba_desc == NULL))
            // BA not already received, so the current BA is for this A-MPDU
            break;

        // Aggregate already has its needed BA frame, go to next one
        agg_desc = (struct tx_agg_desc *)agg_desc->list_hdr.next;
    }

    // Set BA frame pointer into agg desc
    agg_desc->ba_desc = rxdesc;

    // Check if the received BA is valid
    if (txl_is_ba_valid(rxdesc, agg_desc->sta_idx, agg_desc->tid))
        agg_desc->status |= AGG_BA;
}
#endif

void txl_cfm_evt(int access_category)
{
#if NX_AMPDU_TX
    struct tx_cfm_tag *cfm;
    struct tx_agg_desc *agg_desc;
#endif

    struct txdesc *txdesc = NULL;
    uint32_t evt_bit = txl_cfm_evt_bit[access_category];
    struct co_list *cfm_list = &txl_cfm_env.cfmlist[access_category];
    struct dma_desc *first = NULL;
    struct dma_desc *curr = NULL;
#if RW_MUMIMO_TX_EN
    uint32_t cfm_ind = 0;
#else
    uint32_t cfm_ind = CO_BIT(access_category);
#endif
    int cfm_cnt = 0;

#if NX_AMPDU_TX
    int agg_txed = 0;
    int agg_ok = 0;
#endif

    GLOBAL_INT_DECLARATION();

    // Check that the event is correctly set
    ASSERT_ERR(ke_evt_get() & evt_bit);

    do
    {
        #if NX_AMPDU_TX
        cfm = &txdesc->lmac.hw_desc->cfm;
        #endif

        // Pick the first desc from the cfm queue (either 1st of AMPDU or singleton)
        GLOBAL_INT_DISABLE();
        txdesc = (struct txdesc *)co_list_pick(cfm_list);
        if (NULL == txdesc)
        {
            // No more descriptors to confirm, so exit the loop
            ke_evt_clear(evt_bit);
            GLOBAL_INT_RESTORE();

            break;
        }
        GLOBAL_INT_RESTORE();


#if NX_AMPDU_TX
        cfm = &txdesc->lmac.hw_desc->cfm;
        agg_desc = txdesc->lmac.agg_desc;

        // Check if the packet was part of an A-MPDU
        if (agg_desc)
        {
            uint8_t status = agg_desc->status;

            // Check if the A-MPDU status is ready
            if (!(status & AGG_DONE))
                break;

            // One more aggregated MPDU was transmitted
            agg_txed++;

            //clear FRM successful bit
            cfm->status &= ~FRAME_SUCCESSFUL_TX_BIT;

            if (status & AGG_BA)
            {
                //set ACK state extracted from BA bitmap in FRM successful bit position
                cfm->status |= txl_ba_extract_ack(txdesc, agg_desc->ba_desc, &agg_ok);
            }

#if (RW_BFMER_EN)
            if (bfr_is_enabled())
            {
                // Check if the frame was beamformed
                bfr_tx_cfm(txdesc);
            }
#endif //(RW_BFMER_EN)

            // Indicate the packet status to the upper MAC
            txu_cntrl_cfm(txdesc);

            // Prepare the confirmation to upper MAC
            txl_cfm_prep(access_category, &first, &curr, &cfm_ind, txdesc);

            // Protect the access to the lists from interrupt preemption
            GLOBAL_INT_DISABLE();

            // Extract the TX descriptor from the confirmation list
            co_list_pop_front(cfm_list);

            if (is_mpdu_last(txdesc))
            {
                // Update the RC about the status
                me_tx_cfm_ampdu(agg_desc->sta_idx, agg_txed, agg_ok,
                                ((cfm->status & (TX_STATUS_SW_RETRY_REQUIRED | TX_STATUS_RETRY_REQUIRED)) > 0));

                // Reset the packet counters
                agg_txed = 0;
                agg_ok = 0;

                // Free all aggregation related frames and control (BA, BAR, AGG_CNTRL)
                txl_agg_release(agg_desc->ba_desc, access_category);
            }

            // End of interrupt protection
            GLOBAL_INT_RESTORE();
        }
        //singleton MPDU - confirm directly
        else
        {
#endif
            // Protect the access to the list from interrupt preemption
            GLOBAL_INT_DISABLE();

            //only picked it, must pop it now
            co_list_pop_front(cfm_list);

            // End of interrupt protection
            GLOBAL_INT_RESTORE();

#if (RW_BFMER_EN)
            if (bfr_is_enabled())
            {
                // Check if the frame was beamformed
                bfr_tx_cfm(txdesc);
            }
#endif //(RW_BFMER_EN)

            // Update the RC about the TX status
            // WARNING!!! Must be done before the call to txu_cntrl_cfm()!!!
            me_tx_cfm_singleton(txdesc);

            // Indicate the packet status to the upper MAC
            txu_cntrl_cfm(txdesc);

            // Send the confirmation to upper MAC, already has status
            txl_cfm_prep(access_category, &first, &curr, &cfm_ind, txdesc);

            if(g_rwnx_connector.tx_confirm_func)
            {
                (*g_rwnx_connector.tx_confirm_func)();
            }

            txdesc->status = TXDESC_STA_IDLE;

#if NX_AMPDU_TX
        }
#endif

        // Increment the number of confirmed descriptors
        cfm_cnt++;

        // Check if we have reached the threshold
        if (cfm_cnt >= TXL_CFM_CNT_THRESHOLD)
        {
            // Send the prepared confirmations
            txl_cfm_prog(access_category, first, curr, cfm_ind);

            // Reset the counter and the first and current pointers
            cfm_cnt = 0;
            first = NULL;
            curr = NULL;
        }
    }
    while(0);

    // Send the prepared confirmations
    txl_cfm_prog(access_category, first, curr, cfm_ind);
}

    void txl_cfm_flush_desc(uint8_t access_category, struct txdesc * txdesc, uint32_t status)
    {
        struct dma_desc *first = NULL;
        struct dma_desc *curr = NULL;
#if RW_MUMIMO_TX_EN
        uint32_t cfm_ind = 0;
#else
        uint32_t cfm_ind = CO_BIT(access_category);
#endif
#if NX_AMSDU_TX
        int i;
#endif
#if NX_TX_FRAME
        uint32_t packet_addr;
#endif

    // Put the status in the descriptor, only if it was not already done by the HW
    if (txdesc->lmac.agg_desc != NULL)
        txdesc->lmac.hw_desc->cfm.status = status | A_MPDU_LAST;
    else if (!(txdesc->lmac.hw_desc->cfm.status & DESC_DONE_TX_BIT))
        txdesc->lmac.hw_desc->cfm.status = status;

    #if NX_TX_FRAME
    #if NX_AMSDU_TX
    packet_addr = txdesc->host.packet_addr[0];
    #else
    packet_addr = txdesc->host.packet_addr;
    #endif
    // Check if the frame was generated internally
    if (packet_addr == 0)
    {
        // Put the descriptor in the CFM queue
        txl_frame_cfm(txdesc);
    }
    else
    #endif
    {
        // Indicate the packet status to the upper MAC
        txu_cntrl_cfm(txdesc);

        #if (RW_BFMER_EN)
        if (bfr_is_enabled())
        {
            // Check if the frame was beamformed
            bfr_tx_cfm(txdesc);
        }
        #endif //(RW_BFMER_EN)

        // Prepare the confirmation to the host
        txl_cfm_prep(access_category, &first, &curr, &cfm_ind, txdesc);

        #if NX_AMSDU_TX
        // Free all payloads associated to this MPDU
        for (i = 0; i < txdesc->host.packet_cnt; i++)
        {
            struct txl_buffer_tag *buf = txdesc->lmac.buffer[i];

            // Free the buffer associated with the descriptor
            if (buf != NULL)
            {
                // Free the buffer
                txl_buffer_free(buf, access_category);

                // Reset the buffer pointer for this index
                txdesc->lmac.buffer[i] = NULL;
            }
        };
        #else
        if (txdesc->lmac.buffer != NULL)
        {
            // Free the buffer associated with the descriptor
            txl_buffer_free(txdesc->lmac.buffer, access_category);
            txdesc->lmac.buffer = NULL;
        }
        #endif

        if(g_rwnx_connector.tx_confirm_func)
        {
            (*g_rwnx_connector.tx_confirm_func)();
        }

        txdesc->status = TXDESC_STA_IDLE;
    }

    #if NX_TX_FRAME
    // Execute the internal frame confirmation event to complete the flush
    txl_frame_evt(0);
    #endif

    // Check if some confirmations have to be transfered
    if (first != NULL)
    {
        // Send the prepared confirmations
        txl_cfm_prog(access_category, first, curr, cfm_ind);

        // Poll for the completion of the transfer

        // Execute the interrupt handler to forward the interrupt to the upper MAC
        txl_cfm_dma_int_handler();
    }
}

void txl_cfm_flush(uint8_t access_category, struct co_list *list, uint32_t status)
{
    struct dma_desc *first = NULL;
    struct dma_desc *curr = NULL;
    #if RW_MUMIMO_TX_EN
    uint32_t cfm_ind = 0;
    #else
    uint32_t cfm_ind = CO_BIT(access_category);
    #endif

    // Go through the descriptors in the list
    while (1)
    {
        // Get the first descriptor from the list
        struct txdesc *txdesc = (struct txdesc *)co_list_pop_front(list);
        #if NX_AMSDU_TX
        int i;
        #endif
		
        #if NX_TX_FRAME
        uint32_t packet_addr;
        #endif

        // Check if we reached the end of the list
        if (txdesc == NULL)
            break;

        // Put the status in the descriptor, only if it was not already done by the HW
        if (txdesc->lmac.agg_desc != NULL)
            txdesc->lmac.hw_desc->cfm.status = status | A_MPDU_LAST;
        else if (!(txdesc->lmac.hw_desc->cfm.status & DESC_DONE_TX_BIT))
            txdesc->lmac.hw_desc->cfm.status = status;

#if NX_TX_FRAME
#if NX_AMSDU_TX
        packet_addr = txdesc->host.packet_addr[0];
#else
        packet_addr = txdesc->host.packet_addr;
#endif
        // Check if the frame was generated internally
        if (packet_addr == 0)
        {
            // Put the descriptor in the CFM queue
            txl_frame_cfm(txdesc);
        }
        else
#endif
        {
            // Indicate the packet status to the upper MAC
            txu_cntrl_cfm(txdesc);

#if (RW_BFMER_EN)
            if (bfr_is_enabled())
            {
                // Check if the frame was beamformed
                bfr_tx_cfm(txdesc);
            }
#endif //(RW_BFMER_EN)

            // Prepare the confirmation to the host
            txl_cfm_prep(access_category, &first, &curr, &cfm_ind, txdesc);

#if NX_AMSDU_TX
            // Free all payloads associated to this MPDU
            for (i = 0; i < txdesc->host.packet_cnt; i++)
            {
                struct txl_buffer_tag *buf = txdesc->lmac.buffer[i];

                // Free the buffer associated with the descriptor
                if (buf != NULL)
                {
                    // Free the buffer
                    txl_buffer_free(buf, access_category);

                    // Reset the buffer pointer for this index
                    txdesc->lmac.buffer[i] = NULL;
                }
            };
#else
            if (txdesc->lmac.buffer != NULL)
            {
                // Free the buffer associated with the descriptor
                txl_buffer_free(txdesc->lmac.buffer, access_category);
                txdesc->lmac.buffer = NULL;
            }

            if(g_rwnx_connector.tx_confirm_func)
            {
                (*g_rwnx_connector.tx_confirm_func)();
            }

            txdesc->status = TXDESC_STA_IDLE;
#endif
        }
    }


    #if NX_TX_FRAME
    // Execute the internal frame confirmation event to complete the flush
    txl_frame_evt(0);
    #endif

        // Check if some confirmations have to be transfered
        if (first != NULL)
        {
            // Send the prepared confirmations
            txl_cfm_prog(access_category, first, curr, cfm_ind);

            // Poll for the completion of the transfer

            // Execute the interrupt handler to forward the interrupt to the upper MAC
            txl_cfm_dma_int_handler();
        }
    }


    void txl_cfm_dma_int_handler(void)
    {
    }



    /// @}

