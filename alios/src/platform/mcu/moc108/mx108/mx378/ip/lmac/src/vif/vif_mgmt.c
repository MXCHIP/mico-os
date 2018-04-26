/**
 ****************************************************************************************
 *
 * @file vif_mgmt.c
 *
 * @brief Virtual Interface Management implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup VIF_MGMT
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "vif_mgmt.h"
#include "mm.h"
#include "chan.h"
#include "mm_bcn.h"
#include "co_utils.h"
#include "co_status.h"

#if (NX_CHNL_CTXT || NX_P2P)
#include "txl_cntrl.h"

#if (NX_P2P)
// P2P Definitions and Functions
#include "p2p.h"
#endif //(NX_P2P)

#if (NX_POWERSAVE)
#include "ps.h"
#endif //(NX_POWERSAVE)

#endif //(NX_CHNL_CTXT || NX_P2P)

#if (NX_TD)
// Traffic Detection Functions
#include "td.h"
#endif //(NX_TD)

#if (RW_UMESH_EN)
#include "mesh.h"
#endif //(RW_UMESH_EN)

#include "include.h"
#include "uart_pub.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct vif_mgmt_env_tag vif_mgmt_env;
struct vif_info_tag vif_info_tab[NX_VIRT_DEV_MAX];

/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */

#if (NX_P2P || NX_CHNL_CTXT)
/**
 ****************************************************************************************
 * @brief
 ****************************************************************************************
 */
static void vif_mgmt_bcn_to_evt(void *env)
{
    // Retrieve the VIF
    struct vif_info_tag *p_vif_entry = (struct vif_info_tag *)env;

    #if (NX_P2P)
    // Informed interested modules
    if (p_vif_entry->p2p)
    {
        p2p_bcn_evt_handle(p_vif_entry);
    }
    #endif //(NX_P2P)

    if (p_vif_entry->chan_ctxt)
    {
        chan_bcn_to_evt(p_vif_entry);
    }
}
#endif //(NX_P2P || NX_CHNL_CTXT)

/**
 ****************************************************************************************
 * @brief Initializes (resets) the content of a VIF entry
 *
 * @param[in] vif_entry A pointer to the VIF entry to reset
 ****************************************************************************************
 */
static void vif_mgmt_entry_init(struct vif_info_tag *vif_entry)
{
    // Reset table
    memset(vif_entry, 0, sizeof(*vif_entry));

    vif_entry->type = VIF_UNKNOWN;
    vif_entry->tx_power = VIF_UNDEF_POWER;
    vif_entry->user_tx_power = VIF_UNDEF_POWER;

    #if (NX_P2P || NX_CHNL_CTXT)
    // Initialize callback and environment for Beacon TimeOut timer
    vif_entry->tmr_bcn_to.cb  = vif_mgmt_bcn_to_evt;
    vif_entry->tmr_bcn_to.env = vif_entry;
    #endif //(NX_P2P || NX_CHNL_CTXT)
}

void vif_mgmt_init(void)
{
    int i;

    // Reset VIF management environment
    memset(&vif_mgmt_env, 0, sizeof(vif_mgmt_env));

    // Initialize VIF lists
    co_list_init(&vif_mgmt_env.free_list);
    co_list_init(&vif_mgmt_env.used_list);

    // push all the entries to the free list
    for(i = 0; i < NX_VIRT_DEV_MAX ; i++)
    {
        struct vif_info_tag *vif_entry = &vif_info_tab[i];
        // Init STA info table.
        vif_mgmt_entry_init(vif_entry);
        // Push to free list.
        co_list_push_back(&vif_mgmt_env.free_list, (struct co_list_hdr*)vif_entry);
    }
}

#if (NX_TX_FRAME)
void vif_mgmt_reset(void)
{
    struct vif_info_tag *p_vif_entry = (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);

    while (p_vif_entry)
    {
        // Try to push the frame marked as postponed
        vif_mgmt_send_postponed_frame(p_vif_entry);

        p_vif_entry = (struct vif_info_tag *)p_vif_entry->list_hdr.next;
    }
}
#endif //(NX_TX_FRAME)

uint8_t vif_mgmt_register(struct mac_addr const *mac_addr,
                          uint8_t  vif_type,
                          bool p2p,
                          uint8_t *vif_idx)
{
    struct vif_info_tag *vif_entry;
    uint8_t status = CO_FAIL;

    do
    {
        // Check if there are still some free VIFs
        if (co_list_is_empty(&vif_mgmt_env.free_list))
            // All possible VIFs are active. Reject the new VIF.
            break;

        #if (NX_P2P)
        if (p2p)
        {
            #if (NX_P2P_GO)
            if (p2p_env.nb_p2p_go)
            {
                // Only one P2P GO VIF is supported
                break;
            }
            #else
            if (vif_type == VIF_AP)
            {
                // Do not create a P2P GO VIF if not supported
                break;
            }
            #endif //(NX_P2P_GO)
        }
        #endif //(NX_P2P)

        // Check if an interface is already defined or not
        if (co_list_is_empty(&vif_mgmt_env.used_list))
        {
            #if NX_MULTI_ROLE
            // Configure the HW properly for the first interface added
            mm_hw_info_set(mac_addr);
            #else
            #if NX_BEACONING
            // Depending on the interface type, check if the beaconing has to be enabled
            if ((vif_type == VIF_AP) || (vif_type == VIF_IBSS))
            {
                mm_env.beaconing = true;
            }
            else
            {
                mm_env.beaconing = false;
            }
            #endif
			
            // Configure the HW properly
            mm_hw_interface_info_set(vif_type, mac_addr);
            #endif
        }
        else
        {
            uint32_t vif_addr_low, vif_addr_high;

            #if (!NX_MULTI_ROLE)
            struct vif_info_tag *vif_entry = (struct vif_info_tag *)
                                                  co_list_pick(&vif_mgmt_env.used_list);

            // We can only have multiple APs. In all other cases, we reject the new VIF
            if ((vif_entry->type != VIF_AP) || (vif_type != VIF_AP))
            {
                break;
            }
            #endif

            #if (NX_P2P)
            if (p2p)
            {
                // Check if a new P2P VIF can be created
                if (vif_mgmt_env.nb_p2p_vifs == NX_P2P_VIF_MAX)
                {
                    break;
                }
            }
            #endif //(NX_P2P)

            // Check if VIF MAC address matches with the mask - i.e. only MSB might differ
            vif_addr_low = mac_addr->array[0] | (((uint32_t)mac_addr->array[1]) << 16);
            vif_addr_high = mac_addr->array[2];
            if ((vif_addr_low != nxmac_mac_addr_low_get()) ||
               (((vif_addr_high ^ nxmac_mac_addr_hi_get()) & ~nxmac_mac_addr_hi_mask_get()) != 0))
            {
                break;
            }

            #if (NX_MULTI_ROLE)
            // More than one entity will now be available, so disable HW filtering on BSSID
            mm_rx_filter_lmac_enable_set(NXMAC_ACCEPT_OTHER_BSSID_BIT);
            #endif //(NX_MULTI_ROLE)
        }

        // get a free element from the free VIF list
        vif_entry = (struct vif_info_tag*)co_list_pop_front(&vif_mgmt_env.free_list);

        // Initialize some fields from the parameters
        vif_entry->type = vif_type;
        vif_entry->mac_addr = *mac_addr;
        vif_entry->index = CO_GET_INDEX(vif_entry, vif_info_tab);
        vif_entry->txq_params[AC_BK] = NXMAC_EDCA_AC_0_RESET;
        vif_entry->txq_params[AC_BE] = NXMAC_EDCA_AC_1_RESET;
        vif_entry->txq_params[AC_VI] = NXMAC_EDCA_AC_2_RESET;
        vif_entry->txq_params[AC_VO] = NXMAC_EDCA_AC_3_RESET;
		
        #if (NX_CHNL_CTXT)
        vif_entry->chan_ctxt     = NULL;
        vif_entry->tbtt_switch.vif_index = vif_entry->index;
        #endif //(NX_CHNL_CTXT)

        #if (NX_P2P)
        vif_entry->p2p = p2p;
        vif_entry->u.sta.sp_paused = false;
        #if (NX_CHNL_CTXT)
        if (p2p)
        {
            // Update number of registered P2P VIFs
            vif_mgmt_env.nb_p2p_vifs++;
        }
        #endif //(NX_CHNL_CTXT)
        #endif //(NX_P2P)

        #if (NX_MULTI_ROLE || NX_BCN_AUTONOMOUS_TX || NX_REORD)
        switch (vif_type)
        {
            case (VIF_STA):
            {
                #if NX_MULTI_ROLE
                // One more STA VIF
                vif_mgmt_env.vif_sta_cnt++;

                // Initialize TBTT timer
                vif_entry->tbtt_timer.cb = mm_sta_tbtt;
                vif_entry->tbtt_timer.env = vif_entry;
                #endif

                vif_entry->u.sta.ap_id = INVALID_STA_IDX;

                vif_entry->u.sta.csa_count = 0;
                vif_entry->u.sta.csa_occured = false;
            } break;

            #if (RW_MESH_EN)
            case (VIF_MESH_POINT):
            {
                #if (RW_UMESH_EN)
                vif_entry->mvif_idx = MESH_INVALID_MESH_IDX;
                #endif //(RW_UMESH_EN)

                vif_mgmt_env.vif_mp_cnt++;

                // Disable filtering on other BSSID as 4 adresses are used for mesh QoS data
                mm_rx_filter_lmac_enable_set(NXMAC_ACCEPT_OTHER_BSSID_BIT);
            } // no break;
            #endif //(RW_MESH_EN)
            case (VIF_AP):
            {
                #if NX_MULTI_ROLE
                // Check if this is the first AP VIF created
                if (!vif_mgmt_env.vif_ap_cnt)
                    mm_hw_ap_info_set();

                // One more AP VIF
                vif_mgmt_env.vif_ap_cnt++;
                #endif

                #if (NX_P2P_GO && NX_POWERSAVE)
                if (vif_entry->p2p)
                {
                    // Initialize TBTT timer
                    vif_entry->tbtt_timer.cb = mm_ap_pre_tbtt;
                    vif_entry->tbtt_timer.env = vif_entry;
                }
                #endif //(NX_P2P_GO && NX_POWERSAVE)

                #if (NX_BCN_AUTONOMOUS_TX)
                mm_bcn_init_vif(vif_entry);
                #endif
            } break;

            default:
            {
                break;
            }
        }
        #endif

        #if (NX_P2P)
        if (p2p)
        {
            // Check if a new P2P interface can be created
            vif_entry->p2p_index = p2p_create(vif_entry->index,
                                              (vif_type == VIF_STA) ? P2P_ROLE_CLIENT : P2P_ROLE_GO);

            // Sanity Check
            ASSERT_ERR(vif_entry->p2p_index != P2P_INVALID_IDX);
        }
        #endif //(NX_P2P)

        #if (NX_TD)
        // Start Traffic Detection on this VIF
        td_start(vif_entry->index);
        #endif //(NX_TD)

        // Get the station index
        *vif_idx = vif_entry->index;

        // Push the VIF entry to the used list
        co_list_push_back(&vif_mgmt_env.used_list, &vif_entry->list_hdr);

        status = CO_OK;
    } while (0);

    // return the allocated station entry index
    return status;
}

void vif_mgmt_unregister(uint8_t vif_idx)
{
    struct vif_info_tag *vif_entry = &vif_info_tab[vif_idx];

    // Extract the VIF entry from the used list
    co_list_extract(&vif_mgmt_env.used_list, &vif_entry->list_hdr);

    #if NX_MULTI_ROLE
    // Check if it was a STA or AP VIF
    switch (vif_entry->type)
    {
        case (VIF_STA):
        {
            // One less STA VIF
            vif_mgmt_env.vif_sta_cnt--;
        } break;

        #if (RW_MESH_EN)
        case (VIF_MESH_POINT):
        {
            vif_mgmt_env.vif_mp_cnt--;
        } //no break;
        #endif //(RW_MESH_EN)
        case (VIF_AP):
        {
            // One less AP VIF
            vif_mgmt_env.vif_ap_cnt--;

            // Check if there are still some APs enabled
            if (!vif_mgmt_env.vif_ap_cnt)
                mm_hw_ap_info_reset();
        } break;

        default:
        {

        } break;
    }

    // Check if there is only one entity again
    if ((vif_mgmt_used_cnt() == 1)
        #if (RW_MESH_EN)
            && (!vif_mgmt_env.vif_mp_cnt)
        #endif //(RW_MESH_EN)
            )
    {
        struct vif_info_tag *vif = (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);

        // More than one entity will now be available, so reenable HW filtering on BSSID
        mm_rx_filter_lmac_enable_clear(NXMAC_ACCEPT_OTHER_BSSID_BIT);
        // Only one VIF used, so put the BSSID in the HW
        // write lower 4 bytes of BSSID
        nxmac_bss_id_low_setf(vif->bssid.array[0] | (((uint32_t)vif->bssid.array[1]) << 16));

        // write higher 2 bytes of BSSID
        nxmac_bss_id_high_setf(vif->bssid.array[2]);
    }
    #endif

    #if (NX_MULTI_ROLE || NX_CHNL_CTXT || NX_P2P_GO)
    // Clear TBTT Timer
    mm_timer_clear(&vif_entry->tbtt_timer);
    #endif //(NX_MULTI_ROLE || NX_CHNL_CTXT || NX_P2P_GO)

    #if (NX_CHNL_CTXT || NX_P2P)
    // Clear Beacon Timeout Timer
    mm_timer_clear(&vif_entry->tmr_bcn_to);
    #endif //(NX_CHNL_CTXT || NX_P2P)

    #if (NX_P2P)
    if (vif_entry->p2p)
    {
        p2p_cancel(vif_entry->p2p_index, true);

        #if (NX_CHNL_CTXT)
        // Update number of registered P2P VIFs
        vif_mgmt_env.nb_p2p_vifs--;
        #endif //(NX_CHNL_CTXT)
    }
    #endif //(NX_P2P)

    #if (NX_TD)
    // Stop Traffic Detection on this VIF
    td_reset(vif_entry->index);
    #endif //(NX_TD)

    // Fully reset the entry
    vif_mgmt_entry_init(vif_entry);

    // Push back the station entry in the free list
    co_list_push_back(&vif_mgmt_env.free_list, (struct co_list_hdr*)vif_entry);
}

void vif_mgmt_add_key(struct mm_key_add_req const *param, uint8_t hw_key_idx)
{
    struct vif_info_tag *vif = &vif_info_tab[param->inst_nbr];
    struct key_info_tag *key = &vif->key_info[param->key_idx];

    // Store the key information
    key->hw_key_idx = hw_key_idx;
    key->cipher = param->cipher_suite;
    key->key_idx = param->key_idx;

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
            break;
        #endif
        #if NX_MFP
        case MAC_RSNIE_CIPHER_AES_CMAC:
            memcpy(key->u.mfp.key, param->key.array, sizeof(key->u.mfp.key));
            key->tx_pn = 0;
            break;
        #endif
        default:
            key->tx_pn = 0;
            break;
    }

    // Key is now valid
    key->valid = true;

    #if NX_MFP
    if (key->cipher == MAC_RSNIE_CIPHER_AES_CMAC) {
        vif->default_mgmt_key = key;
    }
    else
    #endif
    // This key is now the one used by default
    vif->default_key = key;
}

void vif_mgmt_del_key(struct vif_info_tag *vif, uint8_t keyid)
{
    int i;
    struct key_info_tag *key = &vif->key_info[keyid];

    // Key is now invalid
    key->valid = false;

    // Check if we still have a valid default key
    if (vif->default_key == key)
    {
        vif->default_key = NULL;
        for (i = 0; i < MAC_DEFAULT_KEY_COUNT; i++)
        {
            key = &vif->key_info[i];
            if (key->valid)
            {
                vif->default_key = key;
                return;
            }
        }
    }
    #if NX_MFP
    if (vif->default_mgmt_key == key)
    {
        vif->default_mgmt_key = NULL;
        for (i = MAC_DEFAULT_KEY_COUNT; i < MAC_DEFAULT_MFP_KEY_COUNT; i++)
        {
            key = &vif->key_info[i];
            if (key->valid)
            {
                vif->default_mgmt_key = key;
                return;
            }
        }
    }
    #endif
}

#if (NX_TX_FRAME)
void vif_mgmt_send_postponed_frame(struct vif_info_tag *p_vif_entry)
{
    // STA Information entry
    struct sta_info_tag *p_sta_entry;
    // Counter
    uint8_t cnt;

    // Go through list of STA
    for (cnt = 0; cnt < STA_MAX; cnt++)
    {
        p_sta_entry = &sta_info_tab[cnt];

        // Check if STA in linked with provided VIF
        if (p_sta_entry->inst_nbr != p_vif_entry->index)
        {
            // Go to next STA
            continue;
        }

        // Go through list of packets pending for transmission
        while (!co_list_is_empty(&p_sta_entry->tx_desc_post))
        {
            struct txdesc *p_txdesc;
            struct txl_frame_desc_tag *p_frame_desc;						
			GLOBAL_INT_DECLARATION();

            GLOBAL_INT_DISABLE();

            // Check that the packet can be transmitted
            if (!txl_cntrl_tx_check(p_vif_entry))
            {
                break;
            }

            p_txdesc = (struct txdesc *)co_list_pop_front(&p_sta_entry->tx_desc_post);
            p_frame_desc = (struct txl_frame_desc_tag *)p_txdesc;

            // Keep in mind that the frame has been postponed
            p_frame_desc->postponed = false;

            // Send the frame
            // In this case, Access Category has been stored in TID field
            txl_cntrl_push_int(p_txdesc, p_txdesc->host.tid);

            GLOBAL_INT_RESTORE();
        }
    }
}
#endif //(NX_TX_FRAME)

#if (NX_CHNL_CTXT || NX_P2P)
void vif_mgmt_bcn_to_prog(struct vif_info_tag *p_vif_entry)
{
    mm_timer_set(&p_vif_entry->tmr_bcn_to, ke_time() + VIF_MGMT_BCN_TO_DUR);
}

void vif_mgmt_bcn_recv(struct vif_info_tag *p_vif_entry)
{
    do
    {
        #if (NX_POWERSAVE)
        // If PS not used, wait for timeout
        if (!ps_env.ps_on)
        {
            break;
        }

        // Check PS is paused due to traffic, wait for timeout
        if (ps_env.prevent_sleep & PS_PSM_PAUSED)
        {
            break;
        }

        // Check if we are waiting for data
        if (p_vif_entry->prevent_sleep)
        {
            break;
        }
        #endif //(NX_POWERSAVE)

        // Stop Timeout Timer
        mm_timer_clear(&p_vif_entry->tmr_bcn_to);

        // End Timeout period
        vif_mgmt_bcn_to_evt((void *)p_vif_entry);
    } while (0);
}
#endif //(NX_CHNL_CTXT || NX_P2P)

void vif_mgmt_set_ap_bcn_int(struct vif_info_tag *p_vif_entry, uint16_t bcn_int)
{
    // Beacon Interval to be used for the VIF
    uint16_t set_bcn_int = bcn_int;
    GLOBAL_INT_DECLARATION();

    #if NX_BCN_AUTONOMOUS_TX
    // Store the Beacon Interval
    p_vif_entry->u.ap.bcn_int = bcn_int;
    #endif

    #if (NX_BEACONING)
    // Protect from AP TBTT interrupt
    GLOBAL_INT_DISABLE();

    // Check if first AP/MP interface
    if (vif_mgmt_env.vif_ap_cnt > 1)
    {
        struct vif_info_tag *p_rd_vif_entry;
        // Cache lowest Beacon Interval currently in use
        uint16_t lowest_bcn_int = vif_info_tab[vif_mgmt_env.low_bcn_int_idx].u.ap.bcn_int;

        if (bcn_int < lowest_bcn_int)
        {
            // Provided VIF is now the one with the lowest beacon interval
            vif_mgmt_env.low_bcn_int_idx = p_vif_entry->index;
        }
        else
        {
            set_bcn_int = lowest_bcn_int;
        }

        // Loop over the AP/MP VIFs
        p_rd_vif_entry = (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);

        while (p_rd_vif_entry)
        {
            // Get number of time we see a TBTT between two beacon transmissions
            uint8_t ratio = (uint8_t)(p_rd_vif_entry->u.ap.bcn_int / set_bcn_int);

            // Store the ratio
            p_rd_vif_entry->u.ap.bcn_tbtt_ratio = ratio;
            // Initialize counter to our next TBTT
            p_rd_vif_entry->u.ap.bcn_tbtt_cnt = 1;

            // Get next VIF
            p_rd_vif_entry = (struct vif_info_tag *)(p_rd_vif_entry->list_hdr.next);
        }
    }
    else
    {
        // Store the VIF index
        vif_mgmt_env.low_bcn_int_idx = p_vif_entry->index;
        // And initialize beacon transmission values
        p_vif_entry->u.ap.bcn_tbtt_ratio = 1;
        p_vif_entry->u.ap.bcn_tbtt_cnt = 1;
    }
    #endif

    // If we are here update the beacon interval in the HW
    os_printf("beacon_int_set:%d TU\r\n", set_bcn_int);
    nxmac_beacon_int_setf(set_bcn_int);

    #if (NX_BEACONING)
    // Restore interrupts
    GLOBAL_INT_RESTORE();
    #endif
}

void vif_mgmt_switch_channel(struct vif_info_tag *p_vif_entry)
{
    struct mm_csa_finish_ind *ind = KE_MSG_ALLOC(MM_CSA_FINISH_IND, TASK_API,
                                                 TASK_MM, mm_csa_finish_ind);
    struct mac_bss_info *bss = &p_vif_entry->bss_info;
    struct mm_chan_ctxt_add_req *dest = &p_vif_entry->csa_channel;
    uint8_t chan_idx = 0xFF;
    uint8_t res;

    // Unlink the VIF from its current channel context
    chan_ctxt_unlink(p_vif_entry->index);

    // Update bss info regarding channel
    bss->chan = me_freq_to_chan_ptr(dest->band, dest->prim20_freq);
    bss->center_freq1 = dest->center1_freq;
    bss->center_freq2 = dest->center2_freq;
    bss->phy_bw = dest->type;
    bss->bw = (dest->type== PHY_CHNL_BW_80P80) ? BW_160MHZ : dest->type;

    if (bss->chan)
    {
        dest->tx_power = bss->chan->tx_power;
    }
    else
    {
        dest->tx_power = p_vif_entry->bss_info.chan->tx_power;
    }

    // Create a channel context for new channel
    res = chan_ctxt_add(dest, &chan_idx);
    ind->status = res;
    ind->chan_idx = chan_idx;

    if (p_vif_entry->type == VIF_STA)
    {
        p_vif_entry->u.sta.csa_count = 0;

        if (res == CO_OK)
        {
            struct sta_info_tag *p_sta_entry = &sta_info_tab[p_vif_entry->u.sta.ap_id];

            chan_ctxt_link(p_vif_entry->index, chan_idx);

            // reset beacon related timer and counter. Traffic will be re-enabled
            // once a beacon is detected on the new channel.
            mm_timer_clear(&p_vif_entry->tmr_bcn_to);
            mm_timer_set(&p_vif_entry->tbtt_timer, ke_time() + p_sta_entry->bcn_int);
            p_vif_entry->u.sta.beacon_loss_cnt = 0;
            p_vif_entry->u.sta.csa_occured = true;
        }
        else
        {
            mm_send_connection_loss_ind(p_vif_entry);
        }
    }
    #if NX_BCN_AUTONOMOUS_TX // if not defined this function is not called for AP itf
    else if (p_vif_entry->type == VIF_AP)
    {
        p_vif_entry->u.ap.csa_count = 0;

        if (res == CO_OK)
        {
            chan_ctxt_link(p_vif_entry->index, chan_idx);

            // prevent beacon transmission until it has been updated
            mm_bcn_env.update_ongoing = true;
        }
    }
    #endif

    ke_msg_send(ind);
}

uint32_t vif_mgmt_first_used_staid(uint32_t *id)
{	
	uint32_t ret = 1;
	struct vif_info_tag *p_vif_entry;
	
	p_vif_entry = vif_mgmt_first_used();
	if(p_vif_entry)
	{
		ret = 0;
		*id = p_vif_entry ->u.sta.ap_id;
	}
	else
	{
		*id = 0;
	}
	
	return ret;
}
/// @}
