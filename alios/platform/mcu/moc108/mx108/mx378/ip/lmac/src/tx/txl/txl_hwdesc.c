/**
 ****************************************************************************************
 *
 * @file txl_hwdesc.c
 *
 * @brief Implementation of the API function used to initialize the pools.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "txl_hwdesc.h"
#include "tx_swdesc.h"
#include "txl_cntrl.h"
#include "mac_frame.h"
#include "co_utils.h"
#include "hal_desc.h"

/*
 * GLOBAL VARIABLE DEFINITIONS
 ****************************************************************************************
 */
#if NX_AMPDU_TX
/// Table of pointers to the different aggregation descriptor arrays
const struct tx_agg_desc *tx_agg_desc_array[NX_TXQ_CNT] =
{
    tx_agg_desc_array0,
    tx_agg_desc_array1,
    tx_agg_desc_array2,
    tx_agg_desc_array3,
    #if (NX_BEACONING)
    tx_agg_desc_array4,
    #endif
};

/// Number of aggregation descriptor per queue
const int nx_aggdesc_cnt[NX_TXQ_CNT] =
{
    TX_MAX_AMPDU_NB_PER_AC0,
    TX_MAX_AMPDU_NB_PER_AC1,
    TX_MAX_AMPDU_NB_PER_AC2,
    TX_MAX_AMPDU_NB_PER_AC3,
    #if (NX_BEACONING)
    TX_MAX_AMPDU_NB_PER_AC4,
    #endif
};

struct co_list tx_agg_desc_pool[NX_TXQ_CNT];
#endif

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
void txl_hwdesc_init(void)
{
    #if NX_AMPDU_TX
    int i, j;
    
    // Check structure size modulo 4 is null
    SIZEOFSTRUCT_ASSERT(tx_hd);
    SIZEOFSTRUCT_ASSERT(dma_desc);

    //reset the TX Aggregation descriptor array elements
    memset(tx_agg_desc_pool, 0, sizeof(tx_agg_desc_pool));

    // Initialize the descriptor pools
    for (i = 0; i < NX_TXQ_CNT; i++)
    {
        struct co_list *list = &tx_agg_desc_pool[i];

        memset((void *)tx_agg_desc_array[i], 0, nx_aggdesc_cnt[i] * sizeof(struct tx_agg_desc));
        for (j = 0; j < nx_aggdesc_cnt[i]; j++)
        {
            struct tx_agg_desc *agg_desc = (struct tx_agg_desc *)&tx_agg_desc_array[i][j];
            struct tx_hd *a_thd = &agg_desc->a_thd;
            struct tx_hd *bar_thd = &agg_desc->bar_thd;
            struct bar_frame *bar_payl = &agg_desc->bar_payl;
            struct tx_policy_tbl *bar_pol = &agg_desc->bar_pol_tbl;

            // Fill payload
            bar_payl->h.fctl = (MAC_FCTRL_CTRL_T | MAC_FCTRL_BAR_ST);
            bar_payl->h.durid = 0x0500;

            // Initialize default A-MPDU descriptor
            a_thd->upatterntx = TX_HEADER_DESC_PATTERN;
            a_thd->frmlifetime = 0;
            a_thd->macctrlinfo2 = WHICHDESC_AMPDU_EXTRA | INTERRUPT_EN_TX;
            a_thd->optlen[0] = 0;
            a_thd->optlen[1] = 0;
            a_thd->optlen[2] = 0;

            // Initialize BAR descriptor
            bar_thd->upatterntx = TX_HEADER_DESC_PATTERN;
            bar_thd->frmlifetime = 0;
            bar_thd->nextmpdudesc_ptr = 0;
            bar_thd->policyentryaddr = CPU2HW(&agg_desc->bar_pol_tbl);
            bar_thd->datastartptr = CPU2HW(bar_payl);
            bar_thd->dataendptr = (uint32_t)bar_thd->datastartptr + BAR_FRM_LEN_WITH_FCS - 1;
            bar_thd->first_pbd_ptr = 0;
            bar_thd->optlen[0] = 0;
            bar_thd->optlen[1] = 0;
            bar_thd->optlen[2] = 0;

            // Set frame length
            bar_thd->frmlen = BAR_FRM_LEN_WITH_FCS;
            // Set PHY control information
            bar_thd->phyctrlinfo = 0;
            // Set MAC control information 1
            bar_thd->macctrlinfo1 = EXPECTED_ACK_COMPRESSED_BLOCK_ACK;
            // Set WhichDescriptor field
            bar_thd->macctrlinfo2 = INTERRUPT_EN_TX | WHICHDESC_UNFRAGMENTED_MSDU |
                                            FRM_TYPE_CNTL | FRM_CNTL_SUBTYPE_BAR| TS_VALID_BIT;

            // Initialize BAR policy table
            bar_pol->upatterntx = POLICY_TABLE_PATTERN;
            bar_pol->phycntrlinfo1 = phy_get_ntx() << NX_TX_PT_OFT;
            bar_pol->phycntrlinfo2 = TX_NTX_2_ANTENNA_SET(phy_get_ntx());
            bar_pol->maccntrlinfo1 = 0;
            bar_pol->maccntrlinfo2 = 0xFFFF0704;

            // Push the descriptor into the list
            co_list_push_back(list, (struct co_list_hdr *) agg_desc);
        }
    }
    #endif
}

void txl_hwdesc_reset(void)
{
    #if NX_AMPDU_TX
    int i, j;
    
    // Initialize the descriptor pools
    for (i = 0; i < NX_TXQ_CNT; i++)
    {
        struct co_list *list = &tx_agg_desc_pool[i];

        // Initialize the list
        co_list_init(list);

        for (j = 0; j < nx_aggdesc_cnt[i]; j++)
        {
            struct tx_agg_desc *agg_desc = (struct tx_agg_desc *)&tx_agg_desc_array[i][j];

            // Push the descriptor into the list
            co_list_push_back(list, (struct co_list_hdr *) agg_desc);
        }
    }
    #endif
}

/// @}
