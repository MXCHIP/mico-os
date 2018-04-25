/**
 ****************************************************************************************
 *
 * @file mm.c
 *
 * @brief MAC Management module implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup MM
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "mac_defs.h"
#include "mac_frame.h"
#include "mm.h"
#include "mm_timer.h"
#include "mm_bcn.h"
#include "co_endian.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "phy.h"
#include "rd.h"
#include "ps.h"
#include "rxl_cntrl.h"

#if (NX_P2P)
#include "p2p.h"
#endif //(NX_P2P)

#if (NX_TD)
#include "td.h"
#endif //(NX_TD)

#if (RW_BFMER_EN)
#include "bfr.h"
#endif //(RW_BFMER_EN)

#if (RW_UMESH_EN)
#include "mesh.h"
#endif //(RW_UMESH_EN)

#include "reg_mac_core.h"
#include "reg_mac_pl.h"

// For IE access functions
#include "mac_ie.h"
#if CFG_MODE_SWITCH
#include "param_config.h"
#endif

#include "include.h"
#include "uart_pub.h"

/// Event mask of the TBTT kernel events
#if NX_BEACONING
#define MM_TBTT_EVT_MASK (KE_EVT_PRIMARY_TBTT_BIT | KE_EVT_SECONDARY_TBTT_BIT)
#else
#define MM_TBTT_EVT_MASK KE_EVT_PRIMARY_TBTT_BIT
#endif

/// Beacon loss threshold above which we consider the connection as lost
#define MM_BEACON_LOSS_THD                  (30)  // wangzhilei TODO

/// Periodicity of keep-alive NULL frame transmission
#define MM_KEEP_ALIVE_PERIOD (30 * 1000000)   ///< 30s

/// Mask of the TBTT interrupts
#define MM_TBTT_IRQ_MASK (NXMAC_IMP_PRI_DTIM_BIT | NXMAC_IMP_PRI_TBTT_BIT)

/// Mask used for the MAC address compatibility checking
#define MM_MAC_ADDR_MSK  ((NX_VIRT_DEV_MAX - 1) << 8)

/// Macro returning the maximum duration of A-MPDUs according to the TXOP limit
#define TXOP(limit)  (((limit)==0) || ((limit) > MM_DEFAULT_MAX_AMPDU_DURATION))?       \
                                  MM_DEFAULT_MAX_AMPDU_DURATION:(limit);

#if NX_MULTI_ROLE
/// Wake up delay before TBTT is occurring
#define TBTT_DELAY   400  ///< 400us
#endif


/// Margin taken when checking if the computed TBTT is not in the past
#if NX_CHNL_CTXT
#define MM_TBTT_COMPUTE_MARGIN (CHAN_SWITCH_DELAY + 300)
#else
#define MM_TBTT_COMPUTE_MARGIN 300
#endif

/// Task identifier used for transmission of indication from MM to UMAC
#define TASK_IND  TASK_SM

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
///  Global data for maintaining BSS and STA information

/**  LMAC MM Context variable, used to store MM Context data
 */
struct mm_env_tag mm_env;


void mm_env_max_ampdu_duration_set(void)
{
    // Initialize the TXOP values
    mm_env.ampdu_max_dur[AC_BK] = TXOP(nxmac_tx_op_limit_0_getf());
    mm_env.ampdu_max_dur[AC_BE] = TXOP(nxmac_tx_op_limit_1_getf());
    mm_env.ampdu_max_dur[AC_VI] = TXOP(nxmac_tx_op_limit_2_getf());
    mm_env.ampdu_max_dur[AC_VO] = TXOP(nxmac_tx_op_limit_3_getf());

#if NX_BEACONING
    // For BCN queue, put same parameter as VO
    mm_env.ampdu_max_dur[AC_BCN] = mm_env.ampdu_max_dur[AC_VO];
#endif
}

void mm_env_init(void)
{
    memset(&mm_env, 0, sizeof(mm_env));

    mm_env.prev_mm_state = MM_IDLE;
    mm_env.prev_hw_state = HW_IDLE;
    mm_env.host_idle = 1;
    mm_env.rx_filter_lmac_enable = 0;
    
    mm_rx_filter_umac_set(MM_RX_FILTER_MONITOR);

    // Initialize the TXOP values
    mm_env_max_ampdu_duration_set();
}

/**
 * Initialize all MM related context and data etc....
 */
void mm_init(void)
{
    // TODO reset PHY

    // Initialize the MAC HW
    hal_machw_init();

    // Init All LMAC MM env data
    mm_env_init();

    // Initialize the VIF table
    vif_mgmt_init();

    // Initialize the peer station tables
    sta_mgmt_init();

#if (NX_TD)
    // Initialize the TD module
    td_init();
#endif //(NX_TD)

#if NX_POWERSAVE
    // Initialize the PS module
    ps_init();
#endif

#if (NX_P2P)
    // Initialize P2P module
    p2p_init();
#endif //(NX_P2P)

    // Initialize Tx
    txl_cntrl_init();

    // Initialize Rx
    rxl_init();

#if NX_RADAR_DETECT
    // Initialize radar
    rd_init();
#endif

#if NX_MM_TIMER
    mm_timer_init();
#endif

#if NX_HW_SCAN
    scan_init();
#endif

#if NX_CHNL_CTXT
    chan_init();
#endif

#if NX_GP_DMA
    hal_dma_init();
#endif

#if NX_BCN_AUTONOMOUS_TX
    // Init MM beacon module
    mm_bcn_init();
#endif

#if (RW_BFMER_EN)
    // Initialize Beamformer module
    bfr_init();
#endif //(RW_BFMER_EN)
}

#if NX_MULTI_ROLE
static void mm_tbtt_compute(struct bcn_frame *bcn, uint16_t len, struct rx_hd *rhd,
                            struct vif_info_tag *vif_entry,
                            struct sta_info_tag *p_sta_entry, uint32_t tim)
{
    // Get peer AP information
    uint64_t tsf_start_local = ((uint64_t)rhd->tsflo) | (((uint64_t)rhd->tsfhi) << 32);
    uint64_t tsf_start_peer = bcn->tsf;
    int64_t tsf_offset;
    uint32_t next_tbtt;
    uint32_t duration_to_timestamp;
    uint32_t duration_of_frame;
    uint64_t next_tbtt_tsf;
    uint64_t tbtt_tsf;
    uint32_t bcn_int = (bcn->bcnint << 10);
    uint16_t interval;
    uint32_t drift = 0;

#if NX_POWERSAVE
    if (vif_entry->u.sta.listen_interval)
    {
        interval = vif_entry->u.sta.listen_interval;
    }
    else
#endif
    {
        // Get DTIM information
        interval = co_read8p(tim + MAC_TIM_CNT_OFT);
        if (interval == 0)
            interval = co_read8p(tim + MAC_TIM_PERIOD_OFT);
    }

#if (NX_POWERSAVE)
    drift = p_sta_entry->drift * interval;

#if (NX_P2P)
    vif_entry->u.sta.ctw_add_dur = drift + TBTT_DELAY;
#endif //(NX_P2P)
#endif //(NX_POWERSAVE)

    // Compute the local time at first bit of timestamp
    duration_of_frame = hal_machw_rx_duration(rhd, len);
    duration_to_timestamp = hal_machw_rx_duration(rhd, MAC_BEACON_TIMESTAMP_OFT);
    tsf_start_local -= (duration_of_frame - duration_to_timestamp);

    // Compute the TSF offset between the peer and the local counters
    tsf_offset = tsf_start_peer - tsf_start_local;

#if (NX_P2P)
    vif_entry->u.sta.last_tsf_offset = tsf_offset;
#endif //(NX_P2P)

    // Compute what was the current TBTT time
    tbtt_tsf = (tsf_start_peer / bcn_int) * bcn_int;

    // Check if the beacon was not sent too early
    if (tbtt_tsf > (tsf_start_peer - duration_to_timestamp))
        tbtt_tsf = tsf_start_peer - duration_to_timestamp;

    // Compute the next TBTT time at peer time
    next_tbtt_tsf = tbtt_tsf + interval * bcn_int;

    // Compute the next TBTT at local time based on the TSF offset
    next_tbtt = (uint32_t)(next_tbtt_tsf - tsf_offset) - TBTT_DELAY - drift;

    // Check if next TBTT is in the past
    if (hal_machw_time_past(next_tbtt - MM_TBTT_COMPUTE_MARGIN))
    {
        next_tbtt += bcn_int;
    }

    // Convert TSF time to local time
    next_tbtt += (ke_time() - nxmac_tsf_lo_get());

#if (RW_UMESH_EN)
    if (vif_entry->type == VIF_MESH_POINT)
    {
        // Forward extracted TBTT information to the Mesh module
        mesh_update_tbtt_info(vif_entry, p_sta_entry, next_tbtt);
    }
    else
#endif //(RW_UMESH_EN)
    {
        // Program the next TBTT
        if (next_tbtt != vif_entry->tbtt_timer.time)
        {
            mm_timer_set(&vif_entry->tbtt_timer, next_tbtt);
        }
    }
}
#endif

#if NX_CONNECTION_MONITOR
/**
 ****************************************************************************************
 * @brief Callback function indicating the completion of the NULL frame transmission used
 * to probe the AP in case we don't receive any more beacons
 *
 * @param[in] env     Pointer to the VIF entry
 * @param[in] status  Status of the transmission
 ****************************************************************************************
 */
static void mm_ap_probe_cfm(void *env, uint32_t status)
{
    struct vif_info_tag *vif_entry = (struct vif_info_tag *) env;

    // Check if NULL frame was acknowledged
    if (status & FRAME_SUCCESSFUL_TX_BIT)
    {
        // Frame was acknowledged, reset the beacon loss counter
        vif_entry->u.sta.beacon_loss_cnt = 0;
    }
    else
    {
        // Otherwise, we consider that the connection is lost
        mm_send_connection_loss_ind(vif_entry);
    }
}

/**
 ****************************************************************************************
 * @brief Function computing the CRC of the received beacon
 * The function skips the information elements handled in the LMAC (such as the TIM). It
 * also returns the address of the TIM in the beacon buffer.
 *
 * @param[in] bcn     Pointer to the BCN frame
 * @param[in] len     Length of the BCN frame
 * @param[out] tim    Variable to which the TIM address will be written
 *
 * @return The computed CRC
 ****************************************************************************************
 */
static uint32_t mm_compute_beacon_crc(struct bcn_frame *bcn, uint16_t len, uint32_t *tim)
{
    uint32_t crc;
    uint32_t bcn_addr = CPU2HW(bcn);
    uint32_t addr = bcn_addr + MAC_BEACON_VARIABLE_PART_OFT;

    // Only beacon interval and capability information are of interest in the constant part
    crc = co_crc32(bcn_addr + MAC_BEACON_INTERVAL_OFT, 4, 0);

    // Remove the length of the constant part + MAC header
    len -= MAC_BEACON_VARIABLE_PART_OFT;

    // By default we consider that we won't find the TIM IE
    *tim = 0;

    // Now compute the CRC on the variable part IEs, skipping the ones we handle internally
    while (len >= MAC_INFOELT_INFO_OFT)
    {
        uint8_t ie_id = co_read8p(addr++);
        uint8_t ie_len = co_read8p(addr++);

        // Ensure IE is complete
        if ((ie_len + MAC_INFOELT_INFO_OFT) > len)
            break;

        // Check if we compute the CRC on this IE or not
        switch (ie_id)
        {
        case MAC_ELTID_TIM:
            // Skip this element, so we do nothing
            *tim = addr - MAC_INFOELT_INFO_OFT;
            break;
        default:
            // Compute the CRC on this element
            crc = co_crc32(addr, ie_len, crc);
            break;
        }

        // Decrease the length and increase the pointer
        len -= ie_len + MAC_INFOELT_INFO_OFT;
        addr += ie_len;
    }

    return (crc);
}
#endif


#if NX_CONNECTION_MONITOR || NX_MULTI_ROLE
bool mm_check_beacon(struct rx_hd *rhd, struct vif_info_tag *vif_entry,
                     struct sta_info_tag *p_sta_entry, uint32_t *tim)
{
    struct rx_pbd *pbd = HW2CPU(rhd->first_pbd_ptr);
    struct bcn_frame *bcn = HW2CPU(pbd->datastartptr);
    uint16_t len = rhd->frmlen;

#if NX_CONNECTION_MONITOR
    int8_t rssi;
    uint32_t crc_prev = vif_entry->u.sta.mon_last_crc;

    // Reset the beacon loss count
    vif_entry->u.sta.beacon_loss_cnt = 0;

    if (vif_entry->u.sta.csa_occured)
    {
        mm_send_csa_traffic_ind(vif_entry->index, true);
        vif_entry->u.sta.csa_occured = false;
    }

#if (NX_P2P_GO)
    // Remind that at least one beacon has been received
    vif_entry->u.sta.bcn_rcved = true;
#endif //(NX_P2P_GO)

    // Check if we need to send a keep-alive frame
    if ((ke_time_past(vif_entry->u.sta.mon_last_tx + MM_KEEP_ALIVE_PERIOD)) &&
            (txl_frame_send_null_frame(vif_entry->u.sta.ap_id, NULL, NULL) == CO_OK))
    {
        // Update the keep-alive time
        vif_entry->u.sta.mon_last_tx = ke_time();
    }

    // Retrieve the RSSI value from the RX vector
    rssi = (rhd->recvec1c >> 24) & 0xFF;
    // Check if RSSI is below or above the threshold
    mm_check_rssi(vif_entry, rssi);

    // Compute the beacon CRC to check if some fields have changed
    vif_entry->u.sta.mon_last_crc = mm_compute_beacon_crc(bcn, len, tim);
#endif

#if NX_MULTI_ROLE
    // Compute the time of the next TBTT
    mm_tbtt_compute(bcn, len, rhd, vif_entry, p_sta_entry, *tim);
#endif

#if NX_CONNECTION_MONITOR
    return (crc_prev != vif_entry->u.sta.mon_last_crc);
#else
    return (true);
#endif
}
#endif

void mm_reset(void)
{
    // Check what was the state of the MM when the error occurred and behave accordingly
    switch (ke_state_get(TASK_MM))
    {
    case MM_ACTIVE:
        // MM was active or doze, so put back the HW in active state
        mm_active();
        break;

    default:
        // MM was IDLE, or going to IDLE, so set its state to IDLE
        ke_state_set(TASK_MM, MM_IDLE);
        break;
    }
}

void mm_active(void)
{
    // Put the HW in active state
    nxmac_next_state_setf(HW_ACTIVE);
    ke_state_set(TASK_MM, MM_ACTIVE);
}

#if NX_POWERSAVE || NX_CONNECTION_MONITOR || NX_MULTI_ROLE
void mm_sta_tbtt(void *env)
{
    // Get the VIF entry from the env pointer
    struct vif_info_tag *vif_entry = (struct vif_info_tag *)env;
#if (NX_MULTI_ROLE || NX_CHNL_CTXT)
    // TBTT Time
    uint32_t tbtt_time;
#endif //(NX_MULTI_ROLE || NX_CHAN_CTXT)

    do
    {
        if (!vif_entry->active)
            // STA is not associated, exit immediately
            break;

        if (vif_entry->u.sta.csa_count)
        {
            vif_entry->u.sta.csa_count--;
            if (vif_entry->u.sta.csa_count <= 1)
            {
                vif_mgmt_switch_channel(vif_entry);
                break;
            }
            else if (vif_entry->u.sta.csa_count == 2)
            {
                mm_send_csa_traffic_ind(vif_entry->index, false);
            }
        }

#if (NX_MULTI_ROLE || NX_CHNL_CTXT)
        tbtt_time = vif_entry->tbtt_timer.time + sta_info_tab[vif_entry->u.sta.ap_id].bcn_int;
#endif //(NX_MULTI_ROLE || NX_CHNL_CTXT)

#if NX_MULTI_ROLE
        // Program next TBTT based on previous TBTT
        mm_timer_set(&vif_entry->tbtt_timer, tbtt_time);
#endif

#if (NX_P2P || NX_CHNL_CTXT)
        vif_mgmt_bcn_to_prog(vif_entry);
#endif //(NX_P2P || NX_CHNL_CTXT)

#if (NX_P2P)
        p2p_tbtt_handle(vif_entry);
#endif //(NX_P2P)

#if (RW_BFMER_EN)
        if (bfr_is_enabled())
        {
            // Check if calibration is required
            bfr_calibrate(vif_entry);
        }
#endif //(RW_BFMER_EN)

#if (NX_CHNL_CTXT)
        // Program TBTT switch timer
        chan_tbtt_switch_update(vif_entry, tbtt_time);

        // Check if we are on our channel or not
        if (!chan_is_on_channel(vif_entry))
            break;
#endif //(NX_CHNL_CTXT)

#if NX_POWERSAVE
        vif_entry->prevent_sleep |= PS_VIF_WAITING_BCN;
#endif

#if NX_CONNECTION_MONITOR
        // Increase the beacon loss count (it will be reset upon beacon reception)
        vif_entry->u.sta.beacon_loss_cnt++;

        // Check if we reached the beacon loss threshold
        if (vif_entry->u.sta.beacon_loss_cnt > MM_BEACON_LOSS_THD)
        {
            // No beacons received for a long time, send a NULL frame to the AP
            vif_entry->u.sta.beacon_loss_cnt = 0;
            txl_frame_send_null_frame(vif_entry->u.sta.ap_id, mm_ap_probe_cfm, vif_entry);
        }
#if (NX_CHNL_CTXT)
        else if (vif_entry->u.sta.beacon_loss_cnt > (MM_BEACON_LOSS_THD - 1))
        {
            // Try to spend more time on channel in order to catch a beacon
            chan_bcn_detect_start(vif_entry);
        }
#endif //(NX_CHNL_CTXT)
#endif
    }
    while(0);
}
#endif

#if NX_BEACONING
static void mm_ap_tbtt(uint32_t evt)
{
#if NX_CHNL_CTXT
    struct vif_info_tag *vif_entry = (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);
#endif

    GLOBAL_INT_DECLARATION();

    // Protect from interrupt while flushing the queue
    GLOBAL_INT_DISABLE();

    // Halt the beacon queue
    txl_cntrl_halt_ac(AC_BCN);

    // Flush the beacon queue in case some old packets are still blocked
    txl_cntrl_flush_ac(AC_BCN, DESC_DONE_SW_TX_BIT);

    while (vif_entry != NULL)
    {
        switch (vif_entry->type)
        {
        case (VIF_AP):
#if (RW_MESH_EN)
        case (VIF_MESH_POINT):
#endif //(RW_MESH_EN)
        {
#if (NX_CHNL_CTXT || NX_P2P_GO)
            uint32_t beacon_int;
            uint32_t next_tbtt;
#endif //(NX_CHNL_CTXT || NX_P2P_GO)

            vif_entry->u.ap.bcn_tbtt_cnt--;

            // Check if VIF has to send a beacon on this TBTT
            if (vif_entry->u.ap.bcn_tbtt_cnt)
            {
                break;
            }

            // Reset the TBTT counter
            vif_entry->u.ap.bcn_tbtt_cnt = vif_entry->u.ap.bcn_tbtt_ratio;

#if (NX_CHNL_CTXT || NX_P2P_GO)
            // Program the Beacon Timeout timer
            vif_mgmt_bcn_to_prog(vif_entry);
#endif //(NX_P2P || NX_CHNL_CTXT)

#if (NX_P2P_GO)
            p2p_tbtt_handle(vif_entry);
#endif //(NX_P2P_GO)

#if (NX_CHNL_CTXT || NX_P2P_GO)
            beacon_int = (uint32_t)vif_entry->u.ap.bcn_int << 10;
            next_tbtt = ke_time() + ((uint32_t)nxmac_next_tbtt_get() << 5)
                        + beacon_int;
#endif //(NX_CHNL_CTXT || NX_P2P_GO)

#if (NX_CHNL_CTXT)
            if (vif_entry->chan_ctxt != NULL)
            {
                chan_tbtt_switch_update(vif_entry, next_tbtt);
            }
#endif //(NX_CHNL_CTXT)

#if (NX_P2P_GO)
            if (vif_entry->p2p)
            {
                /*
                 * When HW is in doze mode, it does not trigger the TBTT interrupt anymore. Hence we use a
                 * timer in order to wake up slightly before AP_TBTT occurs
                 */
                mm_timer_set(&vif_entry->tbtt_timer,
                             next_tbtt - HAL_MACHW_BCN_TX_DELAY_US - MM_PRE_AP_TBTT_DELAY_US);
            }
#endif //(NX_P2P_GO)

#if (RW_BFMER_EN)
            if (bfr_is_enabled())
            {
                // Check if calibration is required
                bfr_calibrate(vif_entry);
            }
#endif //(RW_BFMER_EN)
        }
        break;

        default:
        {
        } break;
        }

        // Go to next entry
        vif_entry = vif_mgmt_next(vif_entry);
    }

#if NX_BCN_AUTONOMOUS_TX
    // Transmit the beacon(s)
    mm_bcn_transmit();
#endif

    // Reenable the interrupts
    GLOBAL_INT_RESTORE();
}
#endif

#if (NX_P2P_GO && NX_POWERSAVE)
void mm_ap_pre_tbtt(void *env)
{
    // Get the VIF on which the TBTT will occur soon
    struct vif_info_tag *p_vif_entry = (struct vif_info_tag *)env;

    // Inform the P2P module about the coming TBTT
    p2p_go_pre_tbtt(p_vif_entry);
}
#endif //(NX_P2P_GO && NX_POWERSAVE)

#if NX_BEACONING && (!NX_MULTI_ROLE)
void mm_tbtt_evt(int dummy)
{
    uint32_t evt = ke_evt_get() & MM_TBTT_EVT_MASK;

    // Get the current VIF entry. For the moment we support only one VIF in STA
    // mode, so the first of the list is this one
    struct vif_info_tag *vif_entry =
        (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);

    // Sanity check - Primary and secondary TBTT events should not be active
    // at the same time
    if(evt == MM_TBTT_EVT_MASK)
    {
    	os_printf("tt1\r\n");
    }

    // Clear the event
    ke_evt_clear(evt);

#if NX_POWERSAVE || NX_CONNECTION_MONITOR
    // Check if VIF is of STA type
    if (vif_entry->type == VIF_STA)
    {
        // Call the specific STA TBTT handler
        mm_sta_tbtt(vif_entry);
    }
#if NX_BEACONING
    else
#endif
#endif
    {
#if NX_BEACONING
        // Check if beaconing is enabled
        if (mm_env.beaconing)
        {
            mm_ap_tbtt(evt);
        }
#endif
    }

}
#elif (!NX_MULTI_ROLE)
void mm_tbtt_evt(int dummy)
{
    uint32_t evt = ke_evt_get() & MM_TBTT_EVT_MASK;
    // Get the current VIF entry. For the moment we support only one VIF in STA
    // mode, so the first of the list is this one
    struct vif_info_tag *vif_entry =
        (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);

    // Sanity check - Primary and secondary TBTT events should not be active
    // at the same time
    if(evt == MM_TBTT_EVT_MASK)
    {
    	os_printf("tt2\r\n");
    }

    // Clear the event
    ke_evt_clear(evt);

    // Call the specific STA TBTT handler
    mm_sta_tbtt(vif_entry);
}
#elif NX_BEACONING
void mm_tbtt_evt(int dummy)
{
    uint32_t evt = ke_evt_get() & MM_TBTT_EVT_MASK;

    // Sanity check - Primary and secondary TBTT events should not be active
    // at the same time
    if(evt == MM_TBTT_EVT_MASK)
    {
    	os_printf("tt3\r\n");
    }

    // Clear the event
    ke_evt_clear(evt);

    // Call the AP specific TBTT handler
    mm_ap_tbtt(evt);
}
#endif

uint8_t mm_sec_machwaddr_wr(uint8_t sta_idx, uint8_t inst_nbr)
{
    uint8_t hw_sta_idx;
    struct sta_info_tag *sta = &sta_info_tab[sta_idx];

    // Compute the HW STA index
    hw_sta_idx = MM_SEC_DEFAULT_KEY_COUNT + sta_idx;

    // Copy the MAC addr
    nxmac_encr_mac_addr_low_set(sta->mac_addr.array[0] | (((uint32_t)sta->mac_addr.array[1]) << 16));
    nxmac_encr_mac_addr_high_set(sta->mac_addr.array[2]);

    // Reset the key data
    nxmac_encr_key_0_set(0);
    nxmac_encr_key_1_set(0);
    nxmac_encr_key_2_set(0);
    nxmac_encr_key_3_set(0);

    // Write control field
    // newRead, newWrite, newSearch, SearchError, keyIdx, cType, vlanIdx, spp, usedefkey, cLen
    nxmac_encr_cntrl_pack(0, 1, 0, 0, hw_sta_idx, 0, inst_nbr, 0, 1, 0);

    // Poll for the completion of the write
    while(nxmac_new_write_getf());

    return (hw_sta_idx);
}

uint8_t mm_sec_machwkey_wr(struct mm_key_add_req const *param)
{
    uint8_t clen = 1;
    uint8_t ctype = MM_SEC_CTYPE_NULL;
    uint8_t key_idx_hw;
    uint8_t sta_idx = param->sta_idx;
    struct mac_sec_key const *key = &param->key;
    uint8_t vlan_idx = param->inst_nbr;

    // Get index to be written in the HW table
    if (sta_idx == INVALID_STA_IDX)
    {
#if NX_MFP
        // For now Keys for MFP are not installed in hw
        if (param->cipher_suite == MAC_RSNIE_CIPHER_AES_CMAC)
        {
            key_idx_hw = MM_VIF_TO_MFP_KEY(param->key_idx, param->inst_nbr);
            vif_mgmt_add_key(param, key_idx_hw);
            return key_idx_hw;
        }
#endif

        // Default key index
        key_idx_hw = MM_VIF_TO_KEY(param->key_idx, param->inst_nbr);

        // Put an invalid MAC addr
        nxmac_encr_mac_addr_low_set(0xFFFFFFFF);
        nxmac_encr_mac_addr_high_set(0xFFFFFFFF);

        // Set the key parameters to the VIF
        vif_mgmt_add_key(param, key_idx_hw);
    }
    else
    {
#if (RW_MESH_EN)
        // Get VIF Information
        struct vif_info_tag *p_vif_entry = &vif_info_tab[param->inst_nbr];
#endif //(RW_MESH_EN)
        struct sta_info_tag *sta = &sta_info_tab[sta_idx];

        // Sanity check
        ASSERT_ERR(sta_idx < STA_MAX);

        // Pairwise key index
        key_idx_hw = MM_STA_TO_KEY(sta_idx);

#if (RW_MESH_EN)
        if (p_vif_entry->type == VIF_MESH_POINT)
        {
            vlan_idx = sta->mlink_idx + NX_VIRT_DEV_MAX;

            if (!param->pairwise)
            {
                if (param->cipher_suite == MAC_RSNIE_CIPHER_CCMP)
                {
                    // Peer Mesh Group Key Index
                    key_idx_hw = MM_MLINK_TO_KEY(param->key_idx + 1, sta->mlink_idx);

                    // Put an invalid MAC addr
                    nxmac_encr_mac_addr_low_set(0xFFFFFFFF);
                    nxmac_encr_mac_addr_high_set(0xFFFFFFFF);
                }
                else
                {
                    key_idx_hw = MM_STA_TO_MESH_MFP_KEY(param->key_idx, sta_idx);

                    sta_mgmt_add_key(param, key_idx_hw);

                    return (key_idx_hw);
                }
            }
        }

        if (param->pairwise)
#endif //(RW_MESH_EN)
        {
            // Set the key parameters to the STA
            sta_mgmt_add_key(param, key_idx_hw);

            // Copy MAC addr
            nxmac_encr_mac_addr_low_set(sta->mac_addr.array[0] | (((uint32_t)sta->mac_addr.array[1]) << 16));
            nxmac_encr_mac_addr_high_set(sta->mac_addr.array[2]);
        }
    }

    // Check which encryption type has to be used
    switch(param->cipher_suite)
    {
    case MAC_RSNIE_CIPHER_WEP40:
        clen = 0;
    case MAC_RSNIE_CIPHER_WEP104:
        ctype = MM_SEC_CTYPE_WEP;
        break;
    case MAC_RSNIE_CIPHER_TKIP:
        ctype = MM_SEC_CTYPE_TKIP;
        break;
    case MAC_RSNIE_CIPHER_CCMP:
        ctype = MM_SEC_CTYPE_CCMP;
        break;
#if RW_WAPI_EN
    case MAC_RSNIE_CIPHER_WPI_SMS4:
        ctype = MM_SEC_CTYPE_WPI_SMS4;
        break;
#endif
    default:
        break;
    }

    // Copy key data
    nxmac_encr_key_0_set(key->array[0]);
    nxmac_encr_key_1_set(key->array[1]);
    nxmac_encr_key_2_set(key->array[2]);
    nxmac_encr_key_3_set(key->array[3]);

#if RW_WAPI_EN
    if (ctype == MM_SEC_CTYPE_WPI_SMS4)
    {
        nxmac_encr_wpi_int_key_0_set(key->array[4]);
        nxmac_encr_wpi_int_key_1_set(key->array[5]);
        nxmac_encr_wpi_int_key_2_set(key->array[6]);
        nxmac_encr_wpi_int_key_3_set(key->array[7]);
    }
#endif

    // Write control field
    // newRead, newWrite, newSearch, SearchError, keyIdx, cType, vlanIdx, spp, usedefkey, cLen
    nxmac_encr_cntrl_pack(0, 1, 0, 0, key_idx_hw, ctype, vlan_idx, param->spp, 0, clen);

    // Poll for the completion of the write
    while(nxmac_new_write_getf());

    return (key_idx_hw);
}

void mm_sec_machwkey_del(uint8_t hw_key_idx)
{

#if NX_MFP
    if (hw_key_idx >= MM_SEC_MAX_KEY_NBR)
    {
        vif_mgmt_del_key(&vif_info_tab[MM_MFP_KEY_TO_VIF(hw_key_idx)],
                         MM_MFP_KEY_TO_KEYID(hw_key_idx));
        return;
    }
#endif

    // Get index to be written in the HW table
    if (hw_key_idx >= MM_SEC_DEFAULT_KEY_COUNT)
    {
        struct sta_info_tag *sta;
        // Pairwise key index
        uint8_t sta_idx = MM_KEY_TO_STA(hw_key_idx);

        // Copy MAC addr
        sta = &sta_info_tab[sta_idx];
        nxmac_encr_mac_addr_low_set(sta->mac_addr.array[0] | (((uint32_t)sta->mac_addr.array[1]) << 16));
        nxmac_encr_mac_addr_high_set(sta->mac_addr.array[2]);

        sta_mgmt_del_key(sta);
    }
    else
    {
        // Put an invalid MAC addr
        nxmac_encr_mac_addr_low_set(0xFFFFFFFF);
        nxmac_encr_mac_addr_high_set(0xFFFFFFFF);

#if (RW_MESH_EN)
        if (hw_key_idx < MM_SEC_DEFAULT_VIF_KEY_COUNT)
#endif //(RW_MESH_EN)
        {
            vif_mgmt_del_key(&vif_info_tab[MM_KEY_TO_VIF(hw_key_idx)],
                             MM_KEY_TO_KEYID(hw_key_idx));
        }
    }

    // Reset the key data
    nxmac_encr_key_0_set(0);
    nxmac_encr_key_1_set(0);
    nxmac_encr_key_2_set(0);
    nxmac_encr_key_3_set(0);

    // Write control field
    // newRead, newWrite, newSearch, SearchError, keyIdx, cType, vlanIdx, spp, usedefkey, cLen
    nxmac_encr_cntrl_pack(0, 1, 0, 0, hw_key_idx, 0, 0, 0, 0, 0);

    // Poll for the completion of the write
    while(nxmac_new_write_getf());
}

void mm_sec_machwaddr_del(uint8_t sta_idx)
{
    uint8_t hw_sta_idx;

    // Compute the HW STA index
    hw_sta_idx = MM_SEC_DEFAULT_KEY_COUNT + sta_idx;

    // Put an invalid MAC addr
    nxmac_encr_mac_addr_low_set(0xFFFFFFFF);
    nxmac_encr_mac_addr_high_set(0xFFFFFFFF);

    // Reset the key data
    nxmac_encr_key_0_set(0);
    nxmac_encr_key_1_set(0);
    nxmac_encr_key_2_set(0);
    nxmac_encr_key_3_set(0);

    // Write control field
    // newRead, newWrite, newSearch, SearchError, keyIdx, cType, vlanIdx, spp, usedefkey, cLen
    nxmac_encr_cntrl_pack(0, 1, 0, 0, hw_sta_idx, 0, 0, 0, 0, 0);

    // Poll for the completion of the write
    while(nxmac_new_write_getf());
}

void mm_hw_idle_evt(int dummy)
{
    // Clear the event
    ke_evt_clear(KE_EVT_HW_IDLE_BIT);

    // Put the MM task in IDLE state
    ke_state_set(TASK_MM, MM_IDLE);
}

#if NX_MULTI_ROLE
void mm_hw_info_set(struct mac_addr const *mac_addr)
{
    // By default we are configured as STA
    nxmac_ap_setf(0);
    nxmac_bss_type_setf(1);
    nxmac_mac_addr_hi_mask_set(MM_MAC_ADDR_MSK);

    // Reset TSF
    nxmac_tsf_lo_set(0);
    nxmac_tsf_hi_set(0);

    // Set the MAC address of the interface to the MAC HW
    nxmac_mac_addr_low_set(mac_addr->array[0] | (((uint32_t)mac_addr->array[1]) << 16));
    nxmac_mac_addr_hi_set(mac_addr->array[2]);

    // Now that we have a MAC Address we are supposed to reply
    nxmac_mac_cntrl_1_set(nxmac_mac_cntrl_1_get() & ~(NXMAC_DISABLE_ACK_RESP_BIT
                          | NXMAC_DISABLE_CTS_RESP_BIT | NXMAC_DISABLE_BA_RESP_BIT));

    // Enable reception of useful frames only
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_AP){
		mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE | NXMAC_ACCEPT_PROBE_REQ_BIT);
	}else if(g_wlan_general_param->role == CONFIG_ROLE_STA){
		mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE);
	}
#else
    mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE);
#endif
}

void mm_hw_ap_info_set(void)
{
    // Enable the AP mode. This will trigger the beacon transmission at TBTT.
    nxmac_ap_setf(1);

    // Configure the RX filter to receive PS-poll and beacons from other networks
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_AP){
		mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE | NXMAC_ACCEPT_PS_POLL_BIT | NXMAC_ACCEPT_PROBE_REQ_BIT);
	}else if(g_wlan_general_param->role == CONFIG_ROLE_STA){
		mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE | NXMAC_ACCEPT_PS_POLL_BIT);
	}
#else
    mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE | NXMAC_ACCEPT_PS_POLL_BIT);
                          //| NXMAC_ACCEPT_ALL_BEACON_BIT);
#endif

    // Enable TBTT HW interrupt
    nxmac_gen_int_ack_clear(MM_TBTT_IRQ_MASK);
    nxmac_gen_int_enable_set(nxmac_gen_int_enable_get() | MM_TBTT_IRQ_MASK);
}

void mm_hw_ap_info_reset(void)
{
    // Disable the AP mode
    nxmac_ap_setf(0);

    // Configure the RX filter to discard PS-poll and beacons from other networks
#if CFG_MODE_SWITCH
	if(g_wlan_general_param->role == CONFIG_ROLE_AP){
		mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE | NXMAC_ACCEPT_PROBE_REQ_BIT);
	}else if(g_wlan_general_param->role == CONFIG_ROLE_STA){
		mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE);
	}
#else
    mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE);
#endif

    // Disable TBTT HW interrupt
    nxmac_gen_int_ack_clear(MM_TBTT_IRQ_MASK);
    nxmac_gen_int_enable_set(nxmac_gen_int_enable_get() & ~MM_TBTT_IRQ_MASK);
}
#else
void mm_hw_interface_info_set(uint8_t type, struct mac_addr const *mac_addr)
{
    // Configure the modes according to the interface type
    switch (type)
    {
    case VIF_AP:
        nxmac_ap_setf(1);
        nxmac_bss_type_setf(1);
        nxmac_mac_addr_hi_mask_set(MM_MAC_ADDR_MSK);
        nxmac_gen_int_ack_clear(MM_TBTT_IRQ_MASK);
        nxmac_gen_int_enable_set(nxmac_gen_int_enable_get() | MM_TBTT_IRQ_MASK);
        break;
    case VIF_IBSS:
        nxmac_ap_setf(0);
        nxmac_bss_type_setf(0);
        nxmac_mac_addr_hi_mask_set(0);
        nxmac_gen_int_enable_set(nxmac_gen_int_enable_get() | MM_TBTT_IRQ_MASK);
        break;
    case VIF_STA:
        nxmac_ap_setf(0);
        nxmac_bss_type_setf(1);
        nxmac_mac_addr_hi_mask_set(0);
        nxmac_gen_int_ack_clear(MM_TBTT_IRQ_MASK);
        nxmac_gen_int_enable_set(nxmac_gen_int_enable_get() | MM_TBTT_IRQ_MASK);
        break;
    default:
        break;
    }

    // Reset TSF
    nxmac_tsf_lo_set(0);
    nxmac_tsf_hi_set(0);

    // Set the MAC address of the interface to the MAC HW
    nxmac_mac_addr_low_set(mac_addr->array[0] | (((uint32_t)mac_addr->array[1]) << 16));
    nxmac_mac_addr_hi_set(mac_addr->array[2]);

    // Now that we have a MAC Address we are supposed to reply
    nxmac_mac_cntrl_1_set(nxmac_mac_cntrl_1_get() & ~(NXMAC_DISABLE_ACK_RESP_BIT
                          | NXMAC_DISABLE_CTS_RESP_BIT | NXMAC_DISABLE_BA_RESP_BIT));

    // Enable reception of useful frames only
    mm_rx_filter_umac_set(MM_RX_FILTER_ACTIVE);
}
#endif

#if (NX_AMPDU_TX)
bool mm_ba_agmt_tx_exists(uint8_t sta_idx, uint8_t tid)
{
    return (sta_mgmt_get_tx_bam_idx(sta_idx, tid) != BAM_INVALID_TASK_IDX);
}
#endif //(NX_AMPDU_TX)

bool mm_ba_agmt_rx_exists(uint8_t sta_idx, uint8_t tid)
{
    return (sta_mgmt_get_rx_bam_idx(sta_idx, tid) != BAM_INVALID_TASK_IDX);
}

void mm_back_to_host_idle(void)
{
    // Sanity check - This function shall be called while HW is IDLE
    ASSERT_ERR(ke_state_get(TASK_MM) == MM_HOST_BYPASSED);

    // Change the state of the MM
    if (mm_env.host_idle == 0)
        mm_active();
    else
        ke_state_set(TASK_MM, MM_IDLE);
}

/**
 ****************************************************************************************
 * @brief This function performs all the initializations of the MAC SW.
 *
 * It first initializes the heap, then the message queues and the events. Then if required
 * it initializes the trace.
 *
 ****************************************************************************************
 */
void mm_force_idle_req(void)
{
    GLOBAL_INT_DECLARATION();
    
    // Disable interrupts
    GLOBAL_INT_DISABLE();

    // Reset the MAC HW (this will reset the PHY too)
    hal_machw_reset();

    // Reset the RX path
    rxl_reset();

    // Reset the TX path
    txl_reset();

    // Reset the MM state
    ke_state_set(TASK_MM, MM_HOST_BYPASSED);
    mm_env.prev_mm_state = MM_IDLE;
    mm_env.prev_hw_state = HW_IDLE;

    // Restore the interrupts
    GLOBAL_INT_RESTORE();
}

uint8_t mm_sta_add(struct mm_sta_add_req const *param, uint8_t  *sta_idx,
                   uint8_t *hw_sta_idx)
{
    uint8_t status;

    // Register the new station
    status = sta_mgmt_register(param, sta_idx);

    // Check if the registration was successful
    if (status == CO_OK)
    {
        struct vif_info_tag *vif_entry = &vif_info_tab[param->inst_nbr];

        // Compute the HW STA index
        *hw_sta_idx = mm_sec_machwaddr_wr(*sta_idx, param->inst_nbr);

        // Check if the VIF is of STA type
#if TDLS_ENABLE
        if ((vif_entry->type == VIF_STA) && (!param->tdls_sta))
#else
        if (vif_entry->type == VIF_STA)
#endif
        {
            // We save the AP index for later use
            vif_entry->u.sta.ap_id = *sta_idx;
        }
#if (RW_UMESH_EN)
        else if (vif_entry->type == VIF_MESH_POINT)
        {
            // Inform the Mesh Module that the STA is now registered
            mesh_add_sta_cfm(param->inst_nbr, *sta_idx);
        }
#endif //(RW_UMESH_EN)
    }

    return (status);
}

void mm_sta_del(uint8_t sta_idx)
{
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct vif_info_tag *vif_entry = &vif_info_tab[sta_entry->inst_nbr];

    // Check if the VIF is of STA type
#if TDLS_ENABLE
    if ((vif_entry->type == VIF_STA) && (!sta_entry->tdls_sta))
#else
    if (vif_entry->type == VIF_STA)
#endif
    {
        // We save the AP index for later use
        vif_entry->u.sta.ap_id = INVALID_STA_IDX;
    }
#if (NX_UMAC_PRESENT || NX_P2P_GO)
    else
    {
#if (RW_UMESH_EN)
        if (vif_entry->type == VIF_MESH_POINT)
        {
            // Inform the Mesh Module that the STA has been unregistered
            mesh_del_sta_cfm(sta_entry->mlink_idx);
        }
#endif //(RW_UMESH_EN)

        // Check if the station is in PS or not
        if (sta_entry->ps_state == PS_MODE_ON)
        {
            // Update the number of PS stations
            vif_entry->u.ap.ps_sta_cnt--;

#if (NX_UMAC_PRESENT)
            if (!vif_entry->u.ap.ps_sta_cnt)
            {
                mm_ps_change_ind(VIF_TO_BCMC_IDX(vif_entry->index), PS_MODE_OFF);
            }
#endif //(NX_UMAC_PRESENT)
        }
    }
#endif //(NX_UMAC_PRESENT || NX_P2P_GO)

#if (TDLS_ENABLE)
    if ((vif_entry->type == VIF_STA) && (sta_entry->tdls_sta))
    {
        if (vif_entry->u.sta.sta_tdls->chsw_active)
        {
            // Clear channel switch request timer
            mm_timer_clear(&vif_entry->u.sta.sta_tdls->chsw_req_timer);
            vif_entry->u.sta.sta_tdls->chsw_active = false;
        }
        // Set TDLS station not active
        vif_entry->u.sta.sta_tdls->active = false;
        vif_entry->u.sta.sta_tdls->sta_idx = INVALID_STA_IDX;
    }
#endif

    // Delete the station MAC address from the key storage
    mm_sec_machwaddr_del(sta_idx);

    // Unregister the station
    sta_mgmt_unregister(sta_idx);
}

#if (NX_CONNECTION_MONITOR)
void mm_send_connection_loss_ind(struct vif_info_tag *p_vif_entry)
{
    // Otherwise, we consider that the connection is lost
    struct mm_connection_loss_ind *ind =
        KE_MSG_ALLOC(MM_CONNECTION_LOSS_IND, TASK_IND, TASK_MM, mm_connection_loss_ind);

    // Fill-in the indication message parameters
    ind->inst_nbr = p_vif_entry->index;

    // Send the indication to the upper layers
    ke_msg_send(ind);
}
#endif //(NX_CONNECTION_MONITOR)

void mm_check_rssi(struct vif_info_tag *vif_entry, int8_t rssi)
{
    int8_t rssi_old = vif_entry->u.sta.rssi;
    int8_t rssi_thold = vif_entry->u.sta.rssi_thold;
    int8_t rssi_hyst = vif_entry->u.sta.rssi_hyst;
    bool rssi_status = vif_entry->u.sta.rssi_status;

    // Update current RSSI
    vif_entry->u.sta.rssi = rssi;

    // Check if threshold is set
    if (rssi_thold == 0)
        return;

    // Check RSSI
    if ((rssi_status == 0) && (rssi < rssi_old) && (rssi < (rssi_thold - rssi_hyst)))
    {
        rssi_status = 1;
    }
    else if ((rssi_status == 1) && (rssi > rssi_old) && (rssi > (rssi_thold + rssi_hyst)))
    {
        rssi_status = 0;
    }

    if (rssi_status != vif_entry->u.sta.rssi_status)
    {
        // If RSSI status is changed send MM_RSSI_STATUS_IND message
        struct mm_rssi_status_ind *ind =
            KE_MSG_ALLOC(MM_RSSI_STATUS_IND, TASK_API, TASK_MM, mm_rssi_status_ind);

        // Fill-in the indication message parameters
        ind->vif_index = vif_entry->index;
        ind->rssi_status = rssi_status;

        // Send the indication to the upper layers
        ke_msg_send(ind);
    }

    // Update current RSSI status
    vif_entry->u.sta.rssi_status = rssi_status;
}

void mm_send_csa_traffic_ind(uint8_t vif_index, bool enable)
{
    struct mm_csa_traffic_ind *ind = KE_MSG_ALLOC(MM_CSA_TRAFFIC_IND, TASK_API, TASK_MM,
                                     mm_csa_traffic_ind);
    ind->vif_index = vif_index;
    ind->enable = enable;

	os_printf("mm_send_csa_traffic_ind\r\n");
	
    ke_msg_send(ind);
}

/// @} end of group
