/**
 ****************************************************************************************
 *
 * @file tx_swdesc.c
 *
 * @brief Implementation of Tx SW descriptors functions
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup TX_SWDESC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "co_int.h"
#include "co_bool.h"
#include "co_list.h"
#include "txl_hwdesc.h"
#include "tx_swdesc.h"
#include "ke_event.h"
#include "co_utils.h"

#include "include.h"
#include "mem_pub.h"
#include "txl_cntrl.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
/// Descriptors for the BK queue
struct txdesc       txdesc_array0[RW_USER_MAX * NX_TXDESC_CNT0];
/// Descriptors for the BE queue
struct txdesc       txdesc_array1[RW_USER_MAX * NX_TXDESC_CNT1];
/// Descriptors for the VI queue
struct txdesc       txdesc_array2[RW_USER_MAX * NX_TXDESC_CNT2];
/// Descriptors for the VO queue
struct txdesc       txdesc_array3[RW_USER_MAX * NX_TXDESC_CNT3];
#if (NX_BEACONING)
/// Descriptors for the BCN queue
struct txdesc       txdesc_array4[NX_TXDESC_CNT4];
#endif

/// Array of pointer to the different TX descriptor pools
const struct txdesc *txdesc_array_per_q[NX_TXQ_CNT] =
{
    txdesc_array0,
    txdesc_array1,
    txdesc_array2,
    txdesc_array3,
    
    #if (NX_BEACONING)
    txdesc_array4,
    #endif
};

/// Array of pointer to the different TX descriptor pools
struct txdesc *txdesc_array[NX_TXQ_CNT][RW_USER_MAX];

/// Array of pointer to the different TX HW descriptor pools
const struct tx_hw_desc_s *tx_hw_desc[NX_TXQ_CNT] =
{
    tx_hw_desc0,
    tx_hw_desc1,
    tx_hw_desc2,
    tx_hw_desc3,
    
    #if (NX_BEACONING)
    tx_hw_desc4,
    #endif
};

const int nx_txdesc_cnt[] =
{
    NX_TXDESC_CNT0,
    NX_TXDESC_CNT1,
    NX_TXDESC_CNT2,
    NX_TXDESC_CNT3,
    
    #if (NX_BEACONING)
    NX_TXDESC_CNT4,
    #endif
};

uint32_t tx_desc_idx[NX_TXQ_CNT] = {0};
const int nx_txdesc_cnt_msk[NX_TXQ_CNT] = 
{
    NX_TXDESC_CNT0 - 1,
    NX_TXDESC_CNT1 - 1,
    NX_TXDESC_CNT2 - 1,
    NX_TXDESC_CNT3 - 1,
    
    #if (NX_BEACONING)
    NX_TXDESC_CNT4 - 1,
    #endif
};

const int nx_txuser_cnt[] =
{
    RW_USER_MAX,
    RW_USER_MAX,
    RW_USER_MAX,
    RW_USER_MAX,
	
    #if (NX_BEACONING)
    1,
    #endif
};

/*
 * INTERNAL FUNCTION PROTOTYPES
 ****************************************************************************************
 */
void tx_txdesc_init(void)
{    
    // Attach the TX confirmation descriptors to the TX descriptors
    for (int i = 0; i < NX_TXQ_CNT; i++)
    {
        for (int j = 0; j < nx_txuser_cnt[i]; j++)
        {
            txdesc_array[i][j] = (struct txdesc *)&txdesc_array_per_q[i][j * nx_txdesc_cnt[i]];

            os_memset((void *)txdesc_array[i][j], 0, nx_txdesc_cnt[i] * sizeof(struct txdesc));
            for (int k = 0; k < nx_txdesc_cnt[i]; k++)
            {
                struct lmacdesc *desc;
                struct tx_hw_desc_s *hw_desc;

                desc = (struct lmacdesc *)&txdesc_array[i][j][k].lmac;
                hw_desc = (struct tx_hw_desc_s *)&tx_hw_desc[i][j * nx_txdesc_cnt[i] + k];
                
                // Attach the TX confirmation descriptor
                desc->hw_desc = hw_desc;

                // Initialize some fields of the TX confirmation descriptor
                hw_desc->dma_desc.src = CPU2HW(&hw_desc->cfm);
                hw_desc->dma_desc.length = sizeof_b(hw_desc->cfm);

                // Initialize some fields of the THDs
                hw_desc->thd.optlen[0] = 0;
                hw_desc->thd.optlen[1] = 0;
                hw_desc->thd.optlen[2] = 0;
            }
        }
    }
}

struct txdesc *tx_txdesc_prepare(UINT32 ac)
{
    struct txdesc *prepare_txd_ptr;

prepare:
    prepare_txd_ptr = (struct txdesc *) &txdesc_array[ac][0][tx_desc_idx[ac] 
                                                    & nx_txdesc_cnt_msk[ac]];
    if(txl_check_hd_is_current(ac, prepare_txd_ptr))
    {
        tx_desc_idx[ac] ++;

        goto prepare;
    }

    return prepare_txd_ptr;
}

void tx_txdesc_obtain(struct txdesc *desc, UINT32 ac)
{
    desc->status = TXDESC_STA_USED;

    tx_desc_idx[ac] ++;
}

/// @} // end of group TX_SWDESC

