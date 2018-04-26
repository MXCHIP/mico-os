/**
 ****************************************************************************************
 *
 * @file txl_frame_shared.c
 *
 * @brief Definition of the Tx frame area that has to be shared with MAC HW
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup TX_FRAME
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "include.h" 
#include "txl_buffer.h"
#include "txl_cntrl.h"
#include "compiler.h"

#if (NX_P2P_GO)
#include "p2p.h"
#endif //(NX_P2P_GO)

#if NX_TX_FRAME
/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
uint32_t txl_frame_pool[NX_TXFRAME_CNT][(sizeof(struct txl_buffer_tag) + NX_TXFRAME_LEN) / 4] ;

#if NX_BCN_AUTONOMOUS_TX
uint8_t txl_tim_ie_pool[NX_VIRT_DEV_MAX][MAC_TIM_BMP_OFT + 1] ;
uint8_t txl_tim_bitmap_pool[NX_VIRT_DEV_MAX][MAC_TIM_SIZE] ;
struct tx_pbd txl_tim_desc[NX_VIRT_DEV_MAX][2] ;

uint32_t txl_bcn_pool[NX_VIRT_DEV_MAX][(sizeof(struct txl_buffer_tag) + NX_BCNFRAME_LEN) / 4] ;
struct tx_hw_desc_s txl_bcn_hwdesc_pool[NX_VIRT_DEV_MAX] ;
struct tx_pbd txl_bcn_end_desc[NX_VIRT_DEV_MAX] ;

struct txl_buffer_control txl_bcn_buf_ctrl[NX_VIRT_DEV_MAX] ;

#if (NX_P2P_GO)
/// Pool of P2P NOA payload descriptors
struct tx_pbd txl_p2p_noa_desc[NX_VIRT_DEV_MAX];
/// Pool of P2P NOA Attributes
uint16_t txl_p2p_noa_ie_pool[NX_VIRT_DEV_MAX][P2P_NOA_IE_BUFFER_LEN];
#endif //(NX_P2P_GO)

#if (RW_MESH_EN)
// Payload Descriptor for additional IEs of Mesh Beacon
struct tx_pbd txl_mesh_add_ies_desc[RW_MESH_VIF_NB];
// Buffer containing the additional IEs of Mesh Beacon
struct mesh_add_ies_tag txl_mesh_add_ies[RW_MESH_VIF_NB];
#endif //(RW_MESH_EN)
#endif //(NX_BCN_AUTONOMOUS_TX)

struct tx_hw_desc_s txl_frame_hwdesc_pool[NX_TXFRAME_CNT] ;

/// Default buffer control structure for 2.4GHz band
struct txl_buffer_control txl_buffer_control_24G;
/// Default buffer control structure for 5GHz band
struct txl_buffer_control txl_buffer_control_5G;

#if (RW_BFMER_EN)
/// Default policy table for transmission of NDPA and BRP frames sent during Beamforming Sounding procedure
struct txl_buffer_control txl_buffer_control_ndpa_brp;
/// Default policy table for NDP frame sent during Beamforming Sounding procedure
struct txl_buffer_control txl_buffer_control_ndp;
#endif //(RW_BFMER_EN)

struct tx_policy_tbl txl_frame_pol_24G ;
struct tx_policy_tbl txl_frame_pol_5G ;
struct txl_buffer_control txl_frame_buf_ctrl[NX_TXFRAME_CNT] ;

#endif

/// @}
