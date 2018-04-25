/**
 ****************************************************************************************
 *
 * @file hal_desc.c
 *
 * @brief File containing the definition of HW descriptors.
 *
 * File containing the definition of the structure and API function used to initialize
 * the pool.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 *****************************************************************************************
 * @addtogroup HWDESC
 * @{
 *****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "hal_desc.h"
#include "tx_swdesc.h"
#include "hal_machw_mib.h"
#include "compiler.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
#if NX_AMPDU_TX 
struct tx_agg_desc tx_agg_desc_array0[TX_MAX_AMPDU_NB_PER_AC0] ;
struct tx_agg_desc tx_agg_desc_array1[TX_MAX_AMPDU_NB_PER_AC1] ;
struct tx_agg_desc tx_agg_desc_array2[TX_MAX_AMPDU_NB_PER_AC2] ;
struct tx_agg_desc tx_agg_desc_array3[TX_MAX_AMPDU_NB_PER_AC3] ;

#if (NX_BEACONING)
struct tx_agg_desc tx_agg_desc_array4[TX_MAX_AMPDU_NB_PER_AC4] ;
#endif

#endif

#if NX_RADAR_DETECT
struct radar_event_desc radar_event_desc_array[RADAR_EVENT_MAX] ;
#endif

struct rx_dmadesc       rx_dma_hdrdesc[NX_RXDESC_CNT] ;
struct rx_payloaddesc   rx_payload_desc[NX_RX_PAYLOAD_DESC_CNT] ;

struct tx_hw_desc_s       tx_hw_desc0[RW_USER_MAX * NX_TXDESC_CNT0] ;
struct tx_hw_desc_s       tx_hw_desc1[RW_USER_MAX * NX_TXDESC_CNT1] ;
struct tx_hw_desc_s       tx_hw_desc2[RW_USER_MAX * NX_TXDESC_CNT2] ;
struct tx_hw_desc_s       tx_hw_desc3[RW_USER_MAX * NX_TXDESC_CNT3] ;

#if (NX_BEACONING)
struct tx_hw_desc_s       tx_hw_desc4[NX_TXDESC_CNT4] ;
#endif

#if NX_BCN_AUTONOMOUS_TX
struct dma_desc bcn_dwnld_desc; /*IPC DMA control structure for beacon download*/
#endif

struct hal_host_rxdesc  hal_host_rxdesc_pool[HAL_RXDESC_CNT];
struct dma_desc hal_me_dma_desc;

/// @}  // end of group
