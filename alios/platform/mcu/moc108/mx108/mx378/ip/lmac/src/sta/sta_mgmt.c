/**
 ****************************************************************************************
 *
 * @file sta_mgmt.c
 *
 * @brief UMAC Station Management implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup STA_MGMT
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "sta_mgmt.h"
#include "mac_frame.h"
#include "co_utils.h"
#include "co_status.h"
#include "txl_buffer.h"
#include "txl_frame.h"
#include "vif_mgmt.h"
#include "bam.h"
#include "mm_task.h"
#if (NX_P2P)
#include "p2p.h"
#endif //(NX_P2P)

#if (RW_BFMER_EN)
/// TX Beamforming Definition
#include "bfr.h"
#endif //(RW_BFMER_EN)

#if (RW_UMESH_EN)
#include "mm.h"
#endif //(RW_UMESH_EN)

#include "include.h"
#include "mem_pub.h"
#include "uart_pub.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct sta_info_env_tag sta_info_env;
struct sta_info_tag sta_info_tab[STA_MAX];
#if (TDLS_ENABLE)
struct sta_tdls_tag sta_tdls;
#endif

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
/**
 ****************************************************************************************
 * @brief Initializes (resets) the content of a STA entry
 *
 * @param[in] sta_entry A pointer to the STA entry to reset
 ****************************************************************************************
 */
static void sta_mgmt_entry_init(struct sta_info_tag *sta_entry)
{
    uint8_t tid;
	
    #if (NX_TX_FRAME)
    // Free all pending descriptor
    while (!co_list_is_empty(&sta_entry->tx_desc_post))
    {
        // Get first TX desc in the list
        struct txdesc *p_txdesc = (struct txdesc *)co_list_pop_front(&sta_entry->tx_desc_post);

        // Insert it back in the list of free TX descriptors
        txl_frame_release(p_txdesc, true);
    }
    #endif //(NX_TX_FRAME)
	 
    // Reset table
    os_memset(sta_entry, 0, sizeof(*sta_entry));

    for (tid = 0; tid < TID_MAX; tid++)
    {
        sta_entry->ba_info[tid].bam_idx_rx = BAM_INVALID_TASK_IDX;
        sta_entry->ba_info[tid].bam_idx_tx = BAM_INVALID_TASK_IDX;
    }

    // Set the instance number to 0xFF, indicate the sta is free
    sta_entry->inst_nbr = 0xFF;
}

void sta_mgmt_init(void)
{
    int i;

    co_list_init(&sta_info_env.free_sta_list);

    // push all the entries to the free list
    for(i = 0; i < NX_REMOTE_STA_MAX ; i++)
    {
        struct sta_info_tag *sta_entry = &sta_info_tab[i];
        // Init STA info table.
        sta_mgmt_entry_init(sta_entry);
        // Push to free list.
        co_list_push_back(&sta_info_env.free_sta_list, (struct co_list_hdr*)sta_entry);
    }

    // Initialize the BC/MC pseudo stations
    for(i = 0; i < NX_VIRT_DEV_MAX ; i++)
    {
        uint8_t idx = VIF_TO_BCMC_IDX(i);
        struct sta_info_tag *sta_entry = &sta_info_tab[idx];
        struct vif_info_tag *vif = &vif_info_tab[i];

        // Init STA info table.
        sta_mgmt_entry_init(sta_entry);

        // Attach the station to its VIF
        sta_entry->inst_nbr = i;
        sta_entry->pol_tbl.buf_ctrl[0] = &txl_buffer_control_desc_bcmc[i];
        sta_entry->pol_tbl.buf_ctrl[1] = &txl_buffer_control_desc_bcmc[i];
        sta_entry->ctrl_port_state = PORT_CLOSED;
        sta_entry->sta_sec_info.cur_key = &vif->default_key;
    }
}

uint8_t sta_mgmt_register(struct mm_sta_add_req const *param,
                          uint8_t *sta_idx)
{
    struct sta_info_tag *sta_entry;
    struct vif_info_tag *vif = &vif_info_tab[param->inst_nbr];
    uint32_t time;

    // get a free table from the free table list
    sta_entry = (struct sta_info_tag*)co_list_pop_front(&sta_info_env.free_sta_list);

    // no entries available return immediately
    if (sta_entry == NULL)
        // there are no free tables
        return CO_FAIL;

    // Initialize some fields from the parameters
    sta_entry->mac_addr = param->mac_addr;
    sta_entry->ampdu_spacing_min = co_max(param->ampdu_spacing_min, NX_TX_MPDU_SPACING);
    sta_entry->ampdu_size_max_ht = param->ampdu_size_max_ht;
    sta_entry->ampdu_size_max_vht = param->ampdu_size_max_vht;
    sta_entry->inst_nbr = param->inst_nbr;

    // Get the station index
    *sta_idx = CO_GET_INDEX(sta_entry, sta_info_tab);
    sta_entry->staid = *sta_idx;
    #if (TDLS_ENABLE)
    sta_entry->tdls_sta =  param->tdls_sta;
    if ((vif->type == VIF_STA) && (sta_entry->tdls_sta))
    {
        if (!vif->u.sta.sta_tdls)
        {
            vif->u.sta.sta_tdls = &sta_tdls;
        }
        vif->u.sta.sta_tdls->active = true;
        vif->u.sta.sta_tdls->sta_idx = sta_entry->staid;
    }
    #endif

    sta_entry->bcn_int = 100 * TU_DURATION;

    #if (NX_P2P)
    if (vif->p2p)
    {
        // Increase number of P2P STAs registered for the VIF
        vif->p2p_link_nb++;
    }
    #endif //(NX_P2P)

    sta_entry->rx_nqos_last_seqcntl  = 0xFFFF;

    for (int i = 0; i < TID_MAX; i++)
    {
        sta_entry->rx_qos_last_seqcntl[i] = 0xFFFF;
    }
	
    sta_entry->pol_tbl.buf_ctrl[0] = &txl_buffer_control_desc[*sta_idx][0];
    sta_entry->pol_tbl.buf_ctrl[1] = &txl_buffer_control_desc[*sta_idx][1];
    sta_entry->ctrl_port_state = PORT_CLOSED;
    
    if ((vif->flags & WPA_WPA2_IN_USE)
    #if (RW_MESH_EN)
            || (vif->type == VIF_MESH_POINT)
    #endif //(RW_MESH_EN)
        )
    {
        struct sta_mgmt_sec_info *sec = &sta_entry->sta_sec_info;
        sec->cur_key = &sec->pairwise_key;
    }
    else
    {
        struct sta_mgmt_sec_info *sec = &sta_entry->sta_sec_info;
        sec->cur_key = &vif->default_key;
    }
    time = hal_machw_time();
	
    for(int i = 0; i < TID_MAX; i++)
    {
        sta_entry->ba_info[i].last_tx_time = time;
        sta_entry->ba_info[i].last_ba_add_time = time - BAM_ADDBA_REQ_INTERVAL;
    }

    #if NX_MFP
    if (vif->flags & MFP_IN_USE)
    {
        sta_entry->info.capa_flags |= STA_MFP_CAPA;
    }
    #endif //(NX_MFP)

    // Link the STA to its associated VIF
    co_list_push_back(&vif->sta_list, &sta_entry->list_hdr);

    // The station is now considered as active
    sta_entry->valid = true;

    // return the allocated station entry index
    return CO_OK;
}

void sta_mgmt_unregister(uint8_t sta_idx)
{
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct vif_info_tag *p_vif_entry = &vif_info_tab[sta_entry->inst_nbr];

    #if (NX_P2P)
    if (p_vif_entry->p2p)
    {
        // Decrease number of P2P STAs registered for the VIF
        p_vif_entry->p2p_link_nb--;
    }
    #endif //(NX_P2P)

    #if (RW_BFMER_EN)
    if (bfr_is_enabled())
    {
        bfr_del_sta_ind(sta_idx);
    }
    #endif //(RW_BFMER_EN)

    co_list_extract(&p_vif_entry->sta_list, &sta_entry->list_hdr);

    // Fully reset the entry
    sta_mgmt_entry_init(sta_entry);

    // Push back the station entry in the free list
    co_list_push_back(&sta_info_env.free_sta_list, (struct co_list_hdr*)sta_entry);
}

void sta_mgmt_add_key(struct mm_key_add_req const *param, uint8_t hw_key_idx)
{
    struct sta_info_tag *sta = &sta_info_tab[param->sta_idx];
    struct sta_mgmt_sec_info *sec = &sta->sta_sec_info;
    struct key_info_tag *key;
    #if RW_WAPI_EN
    struct vif_info_tag *vif = &vif_info_tab[sta->inst_nbr];
    #endif

    #if (RW_UMESH_EN)
    if (hw_key_idx >= MM_SEC_MAX_MFP_KEY_NBR)
    {
        key = &sec->key_mfp_mesh_info[MM_MESH_MFP_KEY_TO_KEYID(hw_key_idx)];
    }
    else
    #endif //(RW_UMESH_EN)
    {
        key = &sec->key_info;
    }

    // Store the key information
    key->hw_key_idx = hw_key_idx;
    key->cipher = param->cipher_suite;
    key->key_idx = param->key_idx;
	
	os_printf("sta_mgmt_add_key\r\n");
    // Reset the replay counters
    memset(key->rx_pn, 0, TID_MAX * sizeof(uint64_t));

    // Check which encryption type has to be used
    switch(key->cipher)
    {
        case MAC_RSNIE_CIPHER_WEP40:
        case MAC_RSNIE_CIPHER_WEP104:
            key->tx_pn = co_rand_word() & 0xFFFFFF;
            break;
			
        case MAC_RSNIE_CIPHER_TKIP:
            key->tx_pn = 0;
            key->u.mic.tx_key[0] = param->key.array[4];
            key->u.mic.tx_key[1] = param->key.array[5];
            key->u.mic.rx_key[0] = param->key.array[6];
            key->u.mic.rx_key[1] = param->key.array[7];
            break;
			
        #if RW_WAPI_EN
        case MAC_RSNIE_CIPHER_WPI_SMS4:
            key->tx_pn = 0x5c365c365c365c36ULL;
            if (vif->type == VIF_AP)
                key->tx_pn++;
            break;
        #endif
		
        #if (RW_UMESH_EN)
        #if NX_MFP
        case MAC_RSNIE_CIPHER_AES_CMAC:
            memcpy(key->u.mfp.key, param->key.array, sizeof(key->u.mfp.key));
            key->tx_pn = 0;
            break;
        #endif
        #endif //(RW_UMESH_EN)
		
        default:
            key->tx_pn = 0;
            break;
    }

    // Key is now valid
    key->valid = true;

    #if (RW_UMESH_EN)
    if (hw_key_idx < MM_SEC_MAX_MFP_KEY_NBR)
    #endif //(RW_UMESH_EN)
    {
        // Set the pairwise key and current key pointers
        sec->pairwise_key = key;
    }
}

void sta_mgmt_del_key(struct sta_info_tag *sta)
{
    struct sta_mgmt_sec_info *sec = &sta->sta_sec_info;
    struct key_info_tag *key = &sec->key_info;

    // Key is now invalid
    key->valid = false;
    sec->pairwise_key = NULL;

	os_printf("sta_mgmt_del_key\r\n");

    // Port is now controlled
    sta->ctrl_port_state = PORT_CONTROLED;
}

struct sta_info_tag *sta_mgmt_get_sta(const uint8_t *mac)
{
	int i;
	struct sta_info_tag *sta;

	for(i=0; i<NX_REMOTE_STA_MAX; i++){
		sta = &sta_info_tab[i];
		if(os_memcmp((void *)&sta->mac_addr, mac, 6) == 0){
			break;
		}
	}

	if(i == NX_REMOTE_STA_MAX){
		return NULL;
	}
	return sta;
}
// eof

