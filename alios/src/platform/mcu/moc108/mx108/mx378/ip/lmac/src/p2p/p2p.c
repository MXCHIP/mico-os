/**
 ****************************************************************************************
 * @file p2p.c
 *
 * @brief Wi-Fi Peer-to-Peer (P2P) Management
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup P2P
 * @{
 ****************************************************************************************
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "p2p.h"

#if (NX_P2P)

#include "mac_ie.h"
#include "ps.h"
#include "mm.h"
#include "mm_bcn.h"
#include "vif_mgmt.h"

#if (NX_P2P_GO)
#include "td.h"
#endif //(NX_P2P_GO)

/**
 * DEBUG (DEFINES, MACROS, ...)
 ****************************************************************************************
 */

/**
 * Debug Configuration for P2P Module
 *      0 - Traces are disabled
 *      1:3 - Level of verbosity
 */
#define P2P_DEBUG_TRACES_EN    (0)

#if (P2P_DEBUG_TRACES_EN)
/// Function used to print module information
#define P2P_DEBUG_PRINT(lvl, format, ...)                           \
    do {                                                            \
        if (lvl <= P2P_DEBUG_TRACES_EN)                             \
        {                                                           \
        }                                                           \
    } while (0);
#else
#define P2P_DEBUG_PRINT(lvl, format, ...)
#endif //(P2P_DEBUG_TRACES_EN)

/**
 * GLOBAL VARIABLES DECLARATION
 ****************************************************************************************
 */

#if (NX_P2P_GO)
struct p2p_env_tag p2p_env;
#endif //(NX_P2P_GO)

struct p2p_info_tag p2p_info_tab[NX_P2P_VIF_MAX];

/**
 * PRIVATE FUNCTIONS DECLARATION
 ****************************************************************************************
 */

static void p2p_go_noa_get_intv_cnt(struct p2p_noa_info_tag *p_noa_entry);
static void p2p_cli_noa_cancel(struct p2p_noa_info_tag *p_noa_entry);

/**
 * PRIVATE FUNCTIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Look for specific attribute inside a provided information element (IE).
 *  For example, can be used to find Notice of Absence attribute inside P2P IE.
 *
 * @param[in]  ie_addr       IE Start Address
 * @param[in]  ie_total_len  IE Total Length
 * @param[in]  att_id        Attribute ID to look for
 *
 * @return  Start address of the attribute if found, else 0.
 ****************************************************************************************
 */
static uint32_t p2p_att_find(uint32_t ie_addr, uint16_t ie_total_len, uint8_t att_id)
{
    uint32_t ie_end = ie_addr + ie_total_len;

    // Loop as long as we do not go beyond the frame size
    while (ie_addr < ie_end)
    {
        // Check if the current Attrbute ID is the one looked for
        if (att_id == co_read8p(ie_addr))
        {
            // The Attribute ID matches, return the address of the element
            return (ie_addr);
        }

        // Move on to the next attribute
        ie_addr += co_read16p(ie_addr + P2P_ATT_LEN_OFT) + P2P_ATT_BODY_OFT;
    }

    return (0);
}

/**
 ****************************************************************************************
 * @brief Callback called once system entered in IDLE state before peer device absence.
 ****************************************************************************************
 */
static void p2p_goto_idle_cb(void)
{
    P2P_DEBUG_PRINT(3, D_CRT "p2p_goto_idle_cb\n");

    // Flush all the TX and RX queues
    mm_force_idle_req();

    // Go back to ACTIVE state
    mm_active();
}

/**
 ****************************************************************************************
 * @brief Check if at least one NOA absence is currently in progress. Also return the lowest
 *        NOA count.
 *
 * @param[in]     p_p2p_entry       P2P Entry
 * @param[in|out] low_noa_count     Lower found NOA count value
 *
 * @return true if at least NOA absence is found, else false.
 ****************************************************************************************
 */
static bool p2p_noa_wait_end_abs(struct p2p_info_tag *p_p2p_entry, uint8_t *low_noa_count)
{
    // NOA Counter
    uint8_t noa_counter;
    // Is one of the NOA status equal to P2P_NOA_TIMER_WAIT_END_ABS ?
    bool wait_end_abs = false;

    *low_noa_count = 255;

    for (noa_counter = 0; noa_counter < P2P_NOA_NB_MAX; noa_counter++)
    {
        struct p2p_noa_info_tag *p_noa_entry = &p_p2p_entry->noa[noa_counter];

        if ((p_noa_entry->noa_status == P2P_NOA_TIMER_WAIT_END_ABS)
            #if (NX_P2P_GO)
                && !(p_p2p_entry->noa_paused && p_noa_entry->dyn_noa && (p_noa_entry->noa_type == P2P_NOA_TYPE_NORMAL))
            #endif //(NX_P2P_GO)
                )
        {
            wait_end_abs = true;

            if (p_noa_entry->noa_init_counter < *low_noa_count)
            {
                *low_noa_count = p_noa_entry->noa_init_counter;
            }
        }
    }

    return (wait_end_abs);
}

/**
 ****************************************************************************************
 * @brief Send an MM_PS_CHANGE_IND message to the host in order to notice him about
 * peer device absence or presence. One message is sent for each STA linked to the
 * provided VIF.
 *
 * @param[in] vif_index         VIF Interface Index
 * @param[in] next_ps_state     Peer device PS state
 ****************************************************************************************
 */
static void p2p_update_go_ps_state(struct p2p_info_tag *p_p2p_entry)
{
    // New P2P GO presence state. present by default
    bool is_go_present = true;
    // Lowest NOA Count value used by NOA timers
    uint8_t low_noa_count;

    /**
     * The order of precedence for determining P2P Group Owner power save state shall
     * be as follow:
     *      1 - Highest: Absence due to a non-periodic Notice of Absence (Count = 1)
     *      2 - Presence from TBTT until the end of Beacon frame transmission
     *      3 - Presence during the CTWindow
     *      4 - Lowest: Absence for a periodic Notice of Absence (Count > 1)
     */

    // Check if GO can be absent due to NOA
    if (p2p_noa_wait_end_abs(p_p2p_entry, &low_noa_count))
    {
        do
        {
            // Check if NOA is periodic
            if (low_noa_count != 1)
            {
                // Check if we are waiting for beacon reception
                if (p_p2p_entry->is_waiting_bcn)
                {
                    // Go is present
                    break;
                }

                // Check if the CTWindow is opened
                if (p_p2p_entry->oppps_status == P2P_OPPPS_TIMER_WAIT_END_CTW)
                {
                    // Go is present
                    break;
                }
            }

            is_go_present = false;
        } while (0);
    }
    else
    {
        do
        {
            // Check if Opportunistic PS is supported
            if (p_p2p_entry->oppps_ctw == 0)
            {
                // Go is present
                break;
            }

            // Check if the CTWindow is opened
            if (p_p2p_entry->oppps_status == P2P_OPPPS_TIMER_WAIT_END_CTW)
            {
                // Go is present
                break;
            }

            #if (NX_P2P_GO)
            /*
             * As GO we now have to check if all peer P2P client are in sleep mode. In that
             * case, we can enter in sleep mode until the next TBTT.
             * As CLI, consider the GO as absent only if PS mode is ON and not PAUSED
             */
            if (p_p2p_entry->role == P2P_ROLE_GO)
            {
                // Get VIF interface information
                struct vif_info_tag *p_vif_entry = &vif_info_tab[p_p2p_entry->vif_index];

                // Check if all P2P Client are sleeping
                if (p_vif_entry->u.ap.ps_sta_cnt != p_vif_entry->p2p_link_nb)
                {
                     break;
                }
            }
            else
            #endif //(NX_P2P_GO)
            {
                #if (NX_POWERSAVE)
                if (!ps_env.ps_on || (ps_env.prevent_sleep & PS_PSM_PAUSED))
                {
                    break;
                }
                #endif //(NX_POWERSAVE)
            }

            is_go_present = false;
        } while (0);
    }

    // Check if presence status has been modified
    if (is_go_present != p_p2p_entry->is_go_present)
    {
        #if (NX_CHNL_CTXT)
        // Get associated VIF
        struct vif_info_tag *p_vif_entry = &vif_info_tab[p_p2p_entry->vif_index];
        #endif //(NX_CHNL_CTXT)

        // We have to inform the driver in order to avoid sending of data during absence
        struct mm_p2p_vif_ps_change_ind *p_ind = KE_MSG_ALLOC(MM_P2P_VIF_PS_CHANGE_IND,
                                                              TASK_API, TASK_MM,
                                                              mm_p2p_vif_ps_change_ind);

        p_ind->vif_index = p_p2p_entry->vif_index;
        p_ind->ps_state  = (is_go_present) ? PS_MODE_OFF : PS_MODE_ON;

        // Send the request
        ke_msg_send(p_ind);

        // Store new status
        p_p2p_entry->is_go_present = is_go_present;

        #if (NX_CHNL_CTXT)
        // Notify chan module about the P2P state update
        chan_p2p_absence_update(p_vif_entry->chan_ctxt, !is_go_present);
        #endif //(NX_CHNL_CTXT)

        #if (NX_UAPSD)
        // Notify the PS module about the P2P state update
        ps_p2p_absence_update(p_vif_entry, !is_go_present);
        #endif //(NX_UAPSD)

        if (!is_go_present)
        {
            // Update prevent_sleep state
            #if (NX_P2P_GO && NX_POWERSAVE)
            if (p_vif_entry->type == VIF_AP)
            {
                // Allow to enter in doze mode
                p_vif_entry->prevent_sleep &= ~PS_VIF_P2P_GO_PRESENT;
            }
            #endif //(NX_P2P_GO && NX_POWERSAVE)

            // Require to enter to idle state in order to flush current TX packets
            struct mm_force_idle_req *req = KE_MSG_ALLOC(MM_FORCE_IDLE_REQ,
                                                         TASK_MM, TASK_NONE,
                                                         mm_force_idle_req);

            // Set the request parameters
            req->cb = p2p_goto_idle_cb;

            // Send the request
            ke_msg_send(req);
        }
        else
        {
            // Update prevent_sleep state
            #if (NX_P2P_GO && NX_POWERSAVE)
            if (p_vif_entry->type == VIF_AP)
            {
                // Disallow to enter in doze mode
                p_vif_entry->prevent_sleep |= PS_VIF_P2P_GO_PRESENT;
            }
            #endif //(NX_P2P_GO && NX_POWERSAVE)

            // GO is present again, try to send pending packets after beacon reception or transmission
            if (!p_p2p_entry->is_waiting_bcn)
            {
                vif_mgmt_send_postponed_frame(&vif_info_tab[p_p2p_entry->vif_index]);
            }
        }
    }

    P2P_DEBUG_PRINT(2, D_CRT "%s -> %d\n", __func__, is_go_present);
}

/**
 ****************************************************************************************
 * @brief Program the NOA Timer for a given P2P entry.
 *        Call p2p_noa_timer_end function once time expires.
 *
 * @param[in]  p_noa_entry  P2P NOA Entry for which the timer is programmed
 * @param[in]  time         NOA Timer Expiration Time
 ****************************************************************************************
 */
static void p2p_noa_timer_prog(struct p2p_noa_info_tag *p_noa_entry, uint32_t time)
{
    P2P_DEBUG_PRINT(3, D_CRT "p2p_noa_timer_prog p2p_idx=%d\n", p_noa_entry->p2p_index);
    P2P_DEBUG_PRINT(3, D_CRT "    time=%d\n", time);
    P2P_DEBUG_PRINT(3, D_CRT "    current_time=%d\n", ke_time());

    // Program the timer
    mm_timer_set(&p_noa_entry->noa_timer, time);
}

/**
 ****************************************************************************************
 * @brief Function called upon NOA timer expiration. Handle the timer state machine update
 *        and computation of next timer expiration time.
 *
 * @param[in]  p_env  P2P Entry for which timer was programmed.
 ****************************************************************************************
 */
static void p2p_noa_timer_end(void *p_env)
{
    // Get P2P NOA Info structure
    struct p2p_noa_info_tag *p_noa_entry = (struct p2p_noa_info_tag *)p_env;
    // Get P2P Info structure
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_noa_entry->p2p_index];
    // Get associated VIF Info structure
    struct vif_info_tag *p_vif_entry = &vif_info_tab[p_p2p_entry->vif_index];
    // Next NOA status
    uint8_t next_noa_status = p_noa_entry->noa_status;
    // Next NOA timer time
    uint32_t next_time;

    P2P_DEBUG_PRINT(2, D_CRT "p2p_noa_timer_end vif=%d, status=%d\n", p_p2p_entry->vif_index,
            p_noa_entry->noa_status);

    if (p_p2p_entry->role == P2P_ROLE_GO)
    {
        // Directly use timer time
        next_time = p_noa_entry->noa_timer.time;

        #if (NX_P2P_GO && NX_CHNL_CTXT)
        // Check if NOA Duration has to be updated
        if ((p_p2p_entry->role == P2P_ROLE_GO) && p_noa_entry->noa_next_dur_us)
        {
            if (next_noa_status == P2P_NOA_TIMER_WAIT_END_ABS)
            {
                // Apply delta duration
                next_time += (int32_t)(p_noa_entry->noa_next_dur_us - p_noa_entry->noa_dur_us);
            }

            p_noa_entry->noa_dur_us      = p_noa_entry->noa_next_dur_us;
            p_noa_entry->noa_next_dur_us = 0;
        }
        #endif //(NX_P2P_GO && NX_CHNL_CTXT)
    }
    else
    {
        // Get time based on peer TSF value.
        next_time = p_noa_entry->peer_next_noa_time;
    }

    // Loop in order to avoid programming in the past
    do
    {
        // React based on the current NOA status
        switch (next_noa_status)
        {
            case (P2P_NOA_TIMER_WAIT_NEXT_ABS):
            {
                next_noa_status = P2P_NOA_TIMER_WAIT_END_ABS;
                next_time      += (p_noa_entry->noa_dur_us);
            } break;

            case (P2P_NOA_TIMER_WAIT_END_ABS):
            {
                // Decrement number of remaining absences if not continuous
                if (p_noa_entry->noa_counter != P2P_NOA_CONTINUOUS_COUNTER)
                {
                    p_noa_entry->noa_counter--;
                }

                // Check if another absence is scheduled
                if (p_noa_entry->noa_counter)
                {
                    next_noa_status = P2P_NOA_TIMER_WAIT_NEXT_ABS;
                    next_time      += (p_noa_entry->noa_intv_us - p_noa_entry->noa_dur_us);

                    #if (NX_P2P_GO)
                    // If GO, update start time
                    if ((p_p2p_entry->role == P2P_ROLE_GO) && p_p2p_entry->is_noa_bcn)
                    {
                        // Next start time
                        p_noa_entry->noa_start_time += p_noa_entry->noa_intv_us;
                        // Decrement start time update counter
                        p_noa_entry->noa_time_upd_cnt--;

                        if (!p_noa_entry->noa_time_upd_cnt)
                        {
                            // Update NOA Init Counter
                            p_noa_entry->noa_init_counter = p_noa_entry->noa_counter;

                            // Recompute start time update counter
                            p2p_go_noa_get_intv_cnt(p_noa_entry);

                            // Update the beacon
                            mm_bcn_update_p2p_noa(p_vif_entry->index, P2P_BCN_UPD_OP_NOA_UPD);
                        }
                    }
                    #endif //(NX_P2P_GO)
                }
                else
                {
                    next_noa_status = P2P_NOA_TIMER_NOT_STARTED;

                    #if (NX_P2P_GO)
                    // Stop the NOA
                    if (p_p2p_entry->role == P2P_ROLE_GO)
                    {
                        p2p_go_noa_stop(p_vif_entry, p_noa_entry->noa_inst, false);
                    }
                    else
                    #endif //(NX_P2P_GO)
                    {
                        p2p_cli_noa_cancel(p_noa_entry);
                    }
                }
            } break;

            default:
            {
                // Should not happen
                ASSERT_ERR(0);
            }
        }
    } while (hal_machw_time_cmp(next_time, ke_time() + P2P_NOA_TIMER_MARGIN) &&
             (next_noa_status != P2P_NOA_TIMER_NOT_STARTED));

    // Reprogram NOA timer if needed
    if (next_noa_status != P2P_NOA_TIMER_NOT_STARTED)
    {
        if (p_p2p_entry->role == P2P_ROLE_CLIENT)
        {
            // Keep the next time based on peer TSF
            p_noa_entry->peer_next_noa_time = next_time;

            // Consert the peer time in local time
            next_time -= p_vif_entry->u.sta.last_tsf_offset;
        }
        
        p2p_noa_timer_prog(p_noa_entry, next_time);

        // Update the NOA status
        p_noa_entry->noa_status = next_noa_status;
    }

    // Update GO presence status
    p2p_update_go_ps_state(p_p2p_entry);

    #if (NX_P2P_GO && NX_POWERSAVE)
    /*
     * When doze mode is used while we are AP, TBTT interrupts are no more generated.
     * Hence to be sure to not be in Doze mode at this time, set P2P_GO_PRESENT bit at the
     * end of the NOA absence (occurs few us before the interrupt)
     */
    if ((p_vif_entry->type == VIF_AP) &&
        (next_noa_status == P2P_NOA_TIMER_WAIT_NEXT_ABS))
    {
        p_vif_entry->prevent_sleep |= PS_VIF_P2P_GO_PRESENT;
    }
    #endif //(NX_P2P_GO && NX_POWERSAVE)
}

/**
 ****************************************************************************************
 *
 ****************************************************************************************
 */
static void p2p_oppps_timer_end(void *p_env)
{
    // Get P2P Info structure
    struct p2p_info_tag *p_p2p_entry = (struct p2p_info_tag *)p_env;

    P2P_DEBUG_PRINT(3, D_CRT "p2p_oppps_timer_end vif=%d status=%d\n", p_p2p_entry->vif_index,
            p_p2p_entry->oppps_status);

    if (p_p2p_entry->oppps_status == P2P_OPPPS_TIMER_WAIT_END_CTW)
    {
        // Update OppPS timer status
        p_p2p_entry->oppps_status = P2P_OPPPS_TIMER_NOT_STARTED;

        // Update GO presence status
        p2p_update_go_ps_state(p_p2p_entry);
    }
}

#if (NX_P2P_GO)
/**
 ****************************************************************************************
 * @brief Send a MM_P2P_NOA_UPD_IND to the host so that it can maintain a status about the
 * NoA scheme currently applied locally.
 *
 * @param[in] p_p2p_entry       P2P Entry
 * @param[in] noa_instance      NoA Instance Updated
 ****************************************************************************************
 */
static void p2p_go_send_noa_upd_ind(struct p2p_info_tag *p_p2p_entry, uint8_t noa_instance)
{
    // Get NoA Information
    struct p2p_noa_info_tag *p_noa_entry = &p_p2p_entry->noa[noa_instance];
    // Allocate indication message
    struct mm_p2p_noa_upd_ind *p_ind = KE_MSG_ALLOC(MM_P2P_NOA_UPD_IND, TASK_API,
                                                    TASK_MM, mm_p2p_noa_upd_ind);

    p_ind->vif_index = p_p2p_entry->vif_index;
    p_ind->noa_inst_nb = noa_instance;
    p_ind->count = (p_noa_entry->noa_status != P2P_NOA_TIMER_NOT_STARTED) ? p_noa_entry->noa_init_counter : 0;

    if (p_ind->count)
    {
        p_ind->noa_type = p_noa_entry->noa_type;
        p_ind->duration_us = p_noa_entry->noa_dur_us;
        p_ind->interval_us = p_noa_entry->noa_intv_us;
        p_ind->start_time = p_noa_entry->noa_start_time;
    }

    // Send the message
    ke_msg_send(p_ind);
}

/**
 ****************************************************************************************
 *
 ****************************************************************************************
 */
static uint8_t p2p_go_get_noa_inst(struct p2p_info_tag *p_p2p_entry)
{
    uint8_t noa_counter;

    for (noa_counter = 0; noa_counter < P2P_NOA_NB_MAX; noa_counter++)
    {
        struct p2p_noa_info_tag *p_noa_entry = &p_p2p_entry->noa[noa_counter];

        // Check if the NOA timer is active
        if (p_noa_entry->noa_status == P2P_NOA_TIMER_NOT_STARTED)
        {
            break;
        }
    }

    return (noa_counter);
}

/**
 ****************************************************************************************
 * @brief Compute the number of NOA absence period to have before next start time update.
 *
 * @param[in|out] p_noa_entry   P2P NOA Entry, update the value of noa_time_upd_cnt
 ****************************************************************************************
 */
static void p2p_go_noa_get_intv_cnt(struct p2p_noa_info_tag *p_noa_entry)
{
    /**
     * "The P2P Group Owner shall update the Start Time field [...] every 2^31us"
     * Compute number of absences before next start time update.
     * Update the start time one absence before.
     */
    p_noa_entry->noa_time_upd_cnt = ((2 << 31) / p_noa_entry->noa_intv_us) - 1;
}
#endif //(NX_P2P_GO)

/**
 ****************************************************************************************
 * @brief A frame containing a Notice of Absence attribute can be received after the
 * indicated start time.
 * Hence we have to compute how many absences have been missed since start time in order
 * to properly program the next absence time.
 *
 * @param[in]  p_p2p_entry   P2P Entry for which NOA attribute has been found
 * @param[in]  start_time    Start Time value read in the NOA attribute (4 lower bytes of TSF)
 * @param[in]  tsf_peer      TSF value provided by the peer device
 *
 * @return Peer device next absence time
 ****************************************************************************************
 */
static uint32_t p2p_cli_noa_get_abs_time(struct p2p_noa_info_tag *p_noa_entry, uint8_t vif_index,
                                         uint32_t start_time, uint32_t tsf_peer)
{
    // VIF Info structure
    struct vif_info_tag *p_vif_entry = &vif_info_tab[vif_index];
    // First start time
    uint32_t absence_time;
    // Need to know how many absences have been missed since start_time (on peer side)
    uint32_t missed_intv;

    P2P_DEBUG_PRINT(2, D_CRT "p2p_cli_noa_get_abs_time vif=%d\n", p_vif_entry->index);
    P2P_DEBUG_PRINT(3, D_CRT "    Start Time=%d\n", start_time);
    P2P_DEBUG_PRINT(3, D_CRT "    TSF Peer=%d\n", tsf_peer);

    if (tsf_peer >= start_time)
    {
        // If only one absence is scheduled it is too late.
        if (p_noa_entry->noa_counter == 1)
        {
            return (0);
        }

        missed_intv = ((tsf_peer - start_time) / p_noa_entry->noa_intv_us) + 1;
    }
    else
    {
        missed_intv = 0;
    }

    P2P_DEBUG_PRINT(3, D_CRT "    -> Missed Intv=%d\n", missed_intv);

    // Compute next absence time in Peer Time
    absence_time = start_time + (missed_intv * p_noa_entry->noa_intv_us);
    // Keep this time in mind
    p_noa_entry->peer_next_noa_time = absence_time;
    // Remove TSF Offset in order to retrieve this time in local tine
    absence_time -= p_vif_entry->u.sta.last_tsf_offset;

    if (p_noa_entry->noa_counter != P2P_NOA_CONTINUOUS_COUNTER)
    {
        p_noa_entry->noa_counter -= co_min(missed_intv, p_noa_entry->noa_counter);
    }

    // Check that computed time is not in the past
    while (hal_machw_time_past(absence_time) && p_noa_entry->noa_counter)
    {
        absence_time += p_noa_entry->noa_intv_us;

        if (p_noa_entry->noa_counter != P2P_NOA_CONTINUOUS_COUNTER)
        {
            // Decrease number of remaining absences
            p_noa_entry->noa_counter--;
        }
    }

    if (!p_noa_entry->noa_counter)
    {
        // No more absence are coming
        return (0);
    }

    P2P_DEBUG_PRINT(3, D_CRT "    Last TSF Offset=%d\n", p_vif_entry->u.sta.last_tsf_offset);
    P2P_DEBUG_PRINT(3, D_CRT "    -> Absence Time=%d\n", absence_time);

    return (absence_time);
}

/**
 ****************************************************************************************
 * @brief Start NOA procedure on P2P client side.
 *
 * @param[in]  p_p2p_entry  P2P Entry
 * @param[in]  start_time   NOA Start Time value read in the received Beacon
 * @param[in]  tsf_peer     Peer TSF at Beacon Transmission
 ****************************************************************************************
 */
static bool p2p_cli_noa_start(struct p2p_noa_info_tag *p_noa_entry, uint8_t vif_index,
                              uint32_t start_time, uint32_t tsf_peer)
{
    // Compute first absence time
    uint32_t first_abs_time = p2p_cli_noa_get_abs_time(p_noa_entry, vif_index, start_time, tsf_peer);

    if (first_abs_time)
    {
        // Program the noa timer with the computed time
        p2p_noa_timer_prog(p_noa_entry, first_abs_time);
        // And Update the NOA status
        p_noa_entry->noa_status = P2P_NOA_TIMER_WAIT_NEXT_ABS;
    }

    return ((bool)first_abs_time);
}

/**
 ****************************************************************************************
 * @brief Cancel NOA procedure on P2P client side.
 *
 * @param[in]  p_p2p_entry  P2P Entry
 ****************************************************************************************
 */
static void p2p_cli_noa_cancel(struct p2p_noa_info_tag *p_noa_entry)
{
    P2P_DEBUG_PRINT(1, D_CRT "p2p_cli_noa_cancel p2p=%d\n", p_noa_entry->p2p_index);

    if (p_noa_entry->noa_status != P2P_NOA_TIMER_NOT_STARTED)
    {
        // Stop NOA timer
        mm_timer_clear(&p_noa_entry->noa_timer);

        // Reset NOA status
        p_noa_entry->noa_status = P2P_NOA_TIMER_NOT_STARTED;
    }
}

/**
 ****************************************************************************************
 * @brief Handle the received NOA attribute.
 *
 * @param[in]  p2p_index    Index of the P2P entry for which NOA attribute has been found
 * @param[in]  a_noa_att    Address of received NOA attribute
 * @param[in]  tsf_peer     Peer TSF at Beacon Transmission
 ****************************************************************************************
 */
static void p2p_cli_noa_handle_att(uint8_t p2p_index,
                                   uint32_t a_noa_att,
                                   uint32_t tsf_peer)
{
    // Get P2P information structure
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p2p_index];
    // NOA Counter
    uint8_t noa_counter;

    P2P_DEBUG_PRINT(2, D_CRT "p2p_cli_noa_handle_att p2p=%d, NOA=%d\n", p2p_index,
                        (a_noa_att != 0));

    if (a_noa_att)
    {
        // Number of NoA descriptors
        uint8_t nb_noa_desc;

        do
        {
            // Read NoA Index
            uint8_t index = co_read8p(a_noa_att + P2P_NOA_ATT_INDEX_OFFSET);
            // Information Element Length (Element ID and Length not included
            uint8_t length;
            // Opportunistic PS Information
            uint8_t ctw_oppps;

            P2P_DEBUG_PRINT(3, D_CRT "    Index=%d, Last Index=%d\n", index,
                                                                      p_p2p_entry->index);

            // Check if a one of the NOA fields has been updated since last beacon
            if (p_p2p_entry->is_noa_bcn && (p_p2p_entry->index == index))
            {
                // Nothing changed, skip this one
                break;
            }

            // Keep in mind that NOA attribute was part of last received Beacon
            p_p2p_entry->is_noa_bcn = true;
            // Keep received NoA index
            p_p2p_entry->index    = index;

            // Check if a NOA procedure is already known and in progress
            if (p_p2p_entry->noa_nb)
            {
                for (noa_counter = 0; noa_counter < P2P_NOA_NB_MAX; noa_counter++)
                {
                    // Cancel NOA procedure
                    p2p_cli_noa_cancel(&p_p2p_entry->noa[noa_counter]);

                    p_p2p_entry->noa_nb = 0;
                }

                // Update Go PS state
                p2p_update_go_ps_state(p_p2p_entry);
            }

            // Compute number of NoA Descriptor from length of NoA attribute
            length = co_read8p(a_noa_att + P2P_NOA_ATT_LENGTH_OFFSET);
            nb_noa_desc = (length - 2) / 13;

            for (noa_counter = 0; noa_counter < nb_noa_desc; noa_counter++)
            {
                struct p2p_noa_info_tag *p_noa_entry = &p_p2p_entry->noa[noa_counter];
                // Get address of NoA descriptor in the memory
                uint32_t a_noa_desc = a_noa_att + P2P_NOA_ATT_NOA_DESC_OFFSET
                        + (noa_counter * P2P_NOA_DESC_LENGTH);

                // Extract NOA Counter
                p_noa_entry->noa_init_counter = co_read8p(a_noa_desc + P2P_NOA_DESC_COUNT_OFFSET);

                // Check if GO is using NOA
                if (p_noa_entry->noa_init_counter != 0)
                {
                    // Read absence interval
                    uint32_t noa_intv = co_read32p(a_noa_desc + P2P_NOA_DESC_INTV_OFFSET);
                    
                    // Verify that interval is not 0 if count different than 1
                    if ((p_noa_entry->noa_init_counter > 1) && !noa_intv)
                    {
                        continue;
                    }

                    // Extract NOA parameters
                    p_noa_entry->noa_dur_us  = co_read32p(a_noa_desc + P2P_NOA_DESC_DUR_OFFSET);
                    p_noa_entry->noa_intv_us = noa_intv;
                    p_noa_entry->noa_counter = p_noa_entry->noa_init_counter;

                    // Restart NOA procedure
                    if (p2p_cli_noa_start(p_noa_entry, p_p2p_entry->vif_index,
                                          co_read32p(a_noa_desc + P2P_NOA_DESC_START_OFFSET), tsf_peer))
                    {
                        p_p2p_entry->noa_nb++;
                    }
                }
            }

            ctw_oppps = co_read8p(a_noa_att + P2P_NOA_ATT_CTW_OPPPS_OFFSET);

            // Check if GO is using OppPS
            if ((ctw_oppps & P2P_OPPPS_MASK) == P2P_OPPPS_MASK)
            {
                // Extract CTWindow
                p_p2p_entry->oppps_ctw = ctw_oppps & P2P_CTWINDOW_MASK;
            }
            else
            {
                // 0 value will indicate that the OppPS is not used
                p_p2p_entry->oppps_ctw = 0;
            }
        } while(0);

    }
    else
    {
        p_p2p_entry->is_noa_bcn = false;

        // Check if a NOA procedure is already known and in progress
        if (p_p2p_entry->noa_nb)
        {
            for (noa_counter = 0; noa_counter < P2P_NOA_NB_MAX; noa_counter++)
            {
                // Cancel NOA procedure
                p2p_cli_noa_cancel(&p_p2p_entry->noa[noa_counter]);

                p_p2p_entry->noa_nb = 0;
            }

            // Update Go PS state
            p2p_update_go_ps_state(p_p2p_entry);
        }

        // Stop OppPS procedure
        p_p2p_entry->oppps_ctw = 0;
    }
}

/**
 * PUBLIC FUNCTIONS
 ****************************************************************************************
 */

void p2p_init(void)
{
    uint8_t counter;

    P2P_DEBUG_PRINT(1, D_CRT "p2p_init\n");

    #if (NX_P2P_GO)
    memset(&p2p_env, 0, sizeof(struct p2p_env_tag));
    #endif //(NX_P2P_GO)

    /*
     * Initialize all vif_index to INVALID_VIF_IDX in order to indicate that the p2p
     * structure is unused
     */
    for (counter = 0; counter < NX_P2P_VIF_MAX; counter++)
    {
        struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[counter];

        p_p2p_entry->vif_index = INVALID_VIF_IDX;
    }
}

uint8_t p2p_create(uint8_t vif_index, uint8_t role)
{
    // Returned P2P Index
    uint8_t p2p_index = P2P_INVALID_IDX;
    uint8_t counter;

    // Look for an available P2P Entry structure
    for (counter = 0; counter < NX_P2P_VIF_MAX; counter++)
    {
        struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[counter];

        if (p_p2p_entry->vif_index == INVALID_VIF_IDX)
        {
            uint8_t noa_counter;

            // Initialize content
            memset(p_p2p_entry, 0, sizeof(struct p2p_info_tag));

            // Return current index
            p2p_index = counter;

            // Store the provided vif index and the role
            p_p2p_entry->vif_index = vif_index;
            p_p2p_entry->role      = role;

            #if (NX_P2P_GO)
            if (p_p2p_entry->role == P2P_ROLE_GO)
            {
                p2p_env.nb_p2p_go++;
            }
            #endif //(NX_P2P_GO)

            // Pre-configure NOA timers
            for (noa_counter = 0; noa_counter < P2P_NOA_NB_MAX; noa_counter++)
            {
                struct p2p_noa_info_tag *p_noa_entry = &p_p2p_entry->noa[noa_counter];

                p_noa_entry->noa_timer.cb  = p2p_noa_timer_end;
                p_noa_entry->noa_timer.env = p_noa_entry;
                p_noa_entry->p2p_index = counter;
                p_noa_entry->noa_inst = noa_counter;
            }

            // Pre-configure OPPPS timer
            p_p2p_entry->oppps_timer.cb  = p2p_oppps_timer_end;
            p_p2p_entry->oppps_timer.env = p_p2p_entry;

            // Initiate GO present status, will be present (no noa or oppps yet)
            p2p_update_go_ps_state(p_p2p_entry);

            // Stop looping
            break;
        }
    }

    P2P_DEBUG_PRINT(1, D_CRT "p2p_create vif_idx=%d, role=%d, p2p_idx=%d\n",
            vif_index, role, p2p_index);

    return (p2p_index);
}

void p2p_cancel(uint8_t p2p_index, bool vif_del)
{
    // Get P2P Information Structure
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p2p_index];

    P2P_DEBUG_PRINT(1, D_CRT "p2p_cancel p2p_idx=%d\n", p2p_index);

    // Verify that the structure is used
    if (p_p2p_entry->vif_index != INVALID_VIF_IDX)
    {
        // Cancel Beacon Timeout Timer if needed
        if (p_p2p_entry->is_waiting_bcn)
        {
        }

        #if (NX_P2P_GO)
        if (p_p2p_entry->role == P2P_ROLE_GO)
        {
            p2p_env.nb_p2p_go--;
        }
        #endif //(NX_P2P_GO)

        if (vif_del)
        {
            // Reset VIF index
            p_p2p_entry->vif_index = INVALID_VIF_IDX;
        }
    }
}

void p2p_set_vif_state(struct vif_info_tag *p_vif_entry, bool active)
{
    P2P_DEBUG_PRINT(1, D_CRT "%s vif=%d a=%d\n", __func__, p_vif_entry->index, active);

    // Check that provided VIF is a P2P VIF
    if (p_vif_entry->p2p)
    {
        // Get P2P entry
        struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];

        if (p_vif_entry->type == VIF_STA)
        {
            if (!active)
            {
                for (int noa_counter = 0; noa_counter < P2P_NOA_NB_MAX; noa_counter++)
                {
                    p2p_cli_noa_cancel(&p_p2p_entry->noa[noa_counter]);
                }

                p_p2p_entry->oppps_ctw = 0;

                // Cancel OppPS timer if needed
                if (p_p2p_entry->oppps_status != P2P_OPPPS_TIMER_NOT_STARTED)
                {
                    mm_timer_clear(&p_p2p_entry->oppps_timer);
                    p_p2p_entry->oppps_status = P2P_OPPPS_TIMER_NOT_STARTED;
                }

                p2p_update_go_ps_state(p_p2p_entry);
            }
        }
    }
}

bool p2p_is_present(uint8_t p2p_index)
{
    // Get P2P Information Structure
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p2p_index];

    return (p_p2p_entry->is_go_present);
}

void p2p_tbtt_handle(struct vif_info_tag *p_vif_entry)
{
    P2P_DEBUG_PRINT(2, D_CRT "p2p_tbtt_handle vif_index=%d\n", p_vif_entry->index);

    // Check that VIF is a P2P VIF
    if (p_vif_entry->p2p)
    {
        // P2P Information
        struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];

        // Keep in mind we are waiting for a beacon
        p_p2p_entry->is_waiting_bcn = true;

        // Check if Opportunistic Power Save mode is used -> Start timer monitoring the CTWindow
        if (p_p2p_entry->oppps_ctw)
        {
            // Compute end time of CT Window (oppps_ctw is in TUs -> multiply by 1024)
            uint32_t ctw_end = ke_time() + (p_p2p_entry->oppps_ctw << 10);

            if (p_p2p_entry->role == P2P_ROLE_CLIENT)
            {
                // When STA, TBTT is computed so that it happens drift + TBTT_DELAY before supposed beacon tx
                ctw_end += p_vif_entry->u.sta.ctw_add_dur;
            }
            else
            {
                // When GO, AP TBTT interrupt is raised few ms before the TBTT
                ctw_end += HAL_MACHW_BCN_TX_DELAY_US;
            }

            // Program the timer
            mm_timer_set(&p_p2p_entry->oppps_timer, ctw_end);

            // Update OppPS timer state
            p_p2p_entry->oppps_status = P2P_OPPPS_TIMER_WAIT_END_CTW;
        }

        // Update GO Power Save state
        p2p_update_go_ps_state(p_p2p_entry);
    }
}

void p2p_bcn_evt_handle(struct vif_info_tag *p_vif_entry)
{
    // P2P Information
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];

    P2P_DEBUG_PRINT(2, D_CRT "p2p_bcn_evt_handle vif_index=%d\n", p_vif_entry->index);

    // Update beacon reception status
    p_p2p_entry->is_waiting_bcn = false;

    // Update GO Power Save state
    p2p_update_go_ps_state(p_p2p_entry);
}

uint32_t p2p_cli_bcn_check_noa(struct vif_info_tag *p_vif_entry,
                               struct rx_pbd *p_pyld_desc, struct rx_dmadesc *p_dma_hdrdesc)
{
    // NOA Position
    uint32_t noa_pos = 0;
    // Read address
    uint32_t rd_addr;
    // End address of Beacon variable part
    uint32_t end_addr;
    // Beacon Frame content
    struct bcn_frame *bcn;

    do
    {
        // Check if STA role is used
        if (p_vif_entry->type != VIF_STA)
        {
            break;
        }

        // Check if the interface is a p2p interface
        if (!p_vif_entry->p2p)
        {
            break;
        }

        ASSERT_ERR(p_vif_entry->p2p_index < NX_P2P_VIF_MAX);

        P2P_DEBUG_PRINT(1, D_CRT "p2p_cli_bcn_check_noa vif_index=%d, p2p_index=%d\n", p_vif_entry->index,
                p_vif_entry->p2p_index);

        rd_addr  = p_pyld_desc->datastartptr + MAC_BEACON_VARIABLE_PART_OFT;
        end_addr = rd_addr + p_dma_hdrdesc->hd.frmlen - MAC_BEACON_VARIABLE_PART_OFT;

        // Look for P2P IE(s)
        while (rd_addr < end_addr)
        {
            // Keep the current address before computing the next one
            uint32_t addr = rd_addr;

            rd_addr += co_read8p(addr + MAC_INFOELT_LEN_OFT) + MAC_INFOELT_INFO_OFT;

            // Check if the current IE is the one we are looking for
            if (co_read8p(addr) != P2P_ELMT_ID)
            {
                // Move on to the next IE
                continue;
            }

            // Check if OUI type is P2P
            if (co_read8p(addr + P2P_IE_NOA_OUI_TYPE_OFFSET) != P2P_OUI_TYPE_P2P)
            {
                // Move on to the next IE
                continue;
            }

            P2P_DEBUG_PRINT(2, D_CRT "    -> P2P Element Found\n");

            // Look for NOA attribute
            noa_pos = p2p_att_find(addr + P2P_IE_NOA_ATT_ID_OFFSET,
                                   co_read8p(addr + P2P_IE_NOA_LENGTH_OFFSET) - 4,
                                   P2P_ATT_ID_NOTICE_OF_ABSENCE);

            if (!noa_pos)
            {
                // NOA has not been found, continue. Another P2P element could contain NOA
                continue;
            }

            P2P_DEBUG_PRINT(2, D_CRT "    -> NOA Found\n");

            break;
        }

        // Get beacon data
        bcn = (struct bcn_frame *)HW2CPU(
                       ((struct rx_pbd *)HW2CPU(p_dma_hdrdesc->hd.first_pbd_ptr))->datastartptr);

        p2p_cli_noa_handle_att(p_vif_entry->p2p_index, noa_pos, bcn->tsf);
    } while (0);

    return (noa_pos);
}

void p2p_cli_handle_action(struct vif_info_tag *p_vif_entry, uint32_t a_frame, uint16_t length, uint32_t rx_tsf)
{
    // Address (skip MAC Header)
    uint32_t addr = a_frame + MAC_SHORT_MAC_HDR_LEN;
    // Remaining Length
    uint16_t len = length - MAC_SHORT_MAC_HDR_LEN;

    do
    {
        P2P_DEBUG_PRINT(1, D_CRT "%s vif_index=%d\n", __func__, p_vif_entry->index);

        // Check Category Code, should be Vendor Specific (127)
        if (co_read8p(addr + MAC_ACTION_CATEGORY_OFT) != MAC_VENDOR_ACTION_CATEGORY)
        {
            break;
        }

        // Read P2P Action Subtype and react accordingly
        if (co_read8p(addr + MAC_ACTION_P2P_ACTION_OFT) == MAC_P2P_ACTION_NOA_SUBTYPE)
        {
            // Move to the tagged parameters
            addr += MAC_ACTION_P2P_TAGGED_OFT;
            len -= MAC_ACTION_P2P_TAGGED_OFT;

            // Check if the current IE is the one we are looking for
            if (co_read8p(addr + P2P_IE_NOA_ELMT_ID_OFFSET) != P2P_ELMT_ID)
            {
                break;
            }

            // Check if OUI type is P2P
            if (co_read8p(addr + P2P_IE_NOA_OUI_TYPE_OFFSET) != P2P_OUI_TYPE_P2P)
            {
                break;
            }

            P2P_DEBUG_PRINT(2, D_CRT "    -> P2P Element Found\n");

            if (co_read8p(addr + P2P_IE_NOA_ATT_ID_OFFSET) != P2P_ATT_ID_NOTICE_OF_ABSENCE)
            {
                break;
            }

            P2P_DEBUG_PRINT(2, D_CRT "    -> NOA Found\n");

            // Handle the NoA Attribute (local rx time converted into peer time)
            p2p_cli_noa_handle_att(p_vif_entry->p2p_index,
                                   addr + P2P_IE_NOA_ATT_ID_OFFSET,
                                   rx_tsf + p_vif_entry->u.sta.last_tsf_offset);
        }
    } while (0);
}

/**
 * --------------------------------------------------------------------------
 * |           FUNCTION TO BE USED FOR P2P GO BEACON MANAGEMENT             |
 * --------------------------------------------------------------------------
 */

#if (NX_P2P_GO)
#if (NX_POWERSAVE)
bool p2p_go_check_ps_mode(void)
{
    bool p2p_sleep_enabled = false;

    /*
     * Check if P2P module authorizes the system to enter in deep sleep when GO.
     * Note that if we are here, it is considered that ps_env.ps_on value is false
     * meaning that Legacy Power Save mode is disabled.
     * If ps_env.ps_on is true, P2P_GO_PRESENT bit in vif_mgmt_tag prevent sleep
     * value is enough.
     */
    do
    {
        // P2P GO VIF
        struct vif_info_tag *p_vif_entry;

        // Check if we have a GO VIF
        if (!p2p_env.nb_p2p_go)
        {
            break;
        }

        // Check if we have at least one STA VIFs or several VIF AP
        #if (NX_MULTI_ROLE)
        if (vif_mgmt_env.vif_sta_cnt || (vif_mgmt_env.vif_ap_cnt > 1))
        {
            break;
        }
        #endif //(NX_MULTI_ROLE)

        p_vif_entry = (struct vif_info_tag *)co_list_pick(&vif_mgmt_env.used_list);

        while (p_vif_entry)
        {
            if ((p_vif_entry->type == VIF_AP) && p_vif_entry->p2p)
            {
                // Get P2P Informations
                struct p2p_info_tag *p_p2p_info = &p2p_info_tab[p_vif_entry->p2p_index];

                // Check if at least OppPS or NoA (no Concurrent Mode) is used
                if (p_p2p_info->oppps_ctw || (p_p2p_info->noa_nb - p_p2p_info->noa_cm_nb))
                {
                    // Can go to deep sleep if GO is not present
                    p2p_sleep_enabled = true;

                    break;
                }
            }

            p_vif_entry = (struct vif_info_tag *)vif_mgmt_next(p_vif_entry);
        }
    } while (0);

    return (p2p_sleep_enabled);
}
#endif //(NX_POWERSAVE)

void p2p_go_td_evt(uint8_t vif_index, uint8_t new_status)
{
    // Get VIF index
    struct vif_info_tag *p_vif_entry = &vif_info_tab[vif_index];

    if (p_vif_entry->p2p && (p_vif_entry->type == VIF_AP))
    {
        // Get associated P2P Information
        struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];

        if (new_status)
        {
            p_p2p_entry->noa_paused = true;
        }
        else
        {
            p_p2p_entry->noa_paused = false;
        }

        if (p_p2p_entry->is_noa_bcn)
        {
            // Update the NoA IE in the Beacon
            mm_bcn_update_p2p_noa(p_p2p_entry->vif_index, P2P_BCN_UPD_OP_NOA_UPD);
        }

        // Update GO PS state
        p2p_update_go_ps_state(p_p2p_entry);
    }
}

void p2p_go_oppps_start(struct vif_info_tag *p_vif_entry, uint8_t ctw)
{
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];

    P2P_DEBUG_PRINT(1, D_CRT "p2p_go_oppps_start vif=%d\n", p_vif_entry->index);
    P2P_DEBUG_PRINT(1, D_CRT "    ctw=%d\n", ctw);

    // Initialize OPPPS parameters
    p_p2p_entry->oppps_ctw = ctw;
    // Consider the CTW as opened before next TBTT
    p_p2p_entry->oppps_status = P2P_OPPPS_TIMER_WAIT_END_CTW;

    // Update the NOA attribute, will be added in the beacon (see p2p_go_bcn_op_done)
    mm_bcn_update_p2p_noa(p_p2p_entry->vif_index, P2P_BCN_UPD_OP_NOA_UPD);

    // Update GO PS state
    p2p_update_go_ps_state(p_p2p_entry);
}

void p2p_go_oppps_stop(struct vif_info_tag *p_vif_entry)
{
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];

    P2P_DEBUG_PRINT(1, D_CRT "p2p_go_oppps_stop vif=%d\n", p_vif_entry->index);

    // Reset OPPPS parameters
    p_p2p_entry->oppps_ctw = 0;

    // Stop timer if needed
    if (p_p2p_entry->oppps_status != P2P_OPPPS_TIMER_NOT_STARTED)
    {
        p_p2p_entry->oppps_status = P2P_OPPPS_TIMER_NOT_STARTED;

        // Stop timer
        mm_timer_clear(&p_p2p_entry->oppps_timer);
    }

    // Update Go PS state
    p2p_update_go_ps_state(p_p2p_entry);

    if (!p_p2p_entry->noa_nb)
    {
        // If NOA is not used, remove NOA part of the Beacon
        mm_bcn_update_p2p_noa(p_p2p_entry->vif_index, P2P_BCN_UPD_OP_NOA_RMV);
    }
    else
    {
        // Update NOA attribute content
        mm_bcn_update_p2p_noa(p_p2p_entry->vif_index, P2P_BCN_UPD_OP_NOA_UPD);
    }
}

void p2p_go_ps_state_update(struct vif_info_tag *p_vif_entry)
{
    do
    {
        // Continue only if the interface is a P2P interface
        if (!p_vif_entry->p2p)
        {
            break;
        }

        P2P_DEBUG_PRINT(2, D_CRT "p2p_go_ps_state_update vif=%d\n", p_vif_entry->index);

        p2p_update_go_ps_state(&p2p_info_tab[p_vif_entry->p2p_index]);
    } while (0);
}

bool p2p_go_noa_start(struct vif_info_tag *p_vif_entry, bool concurrent, bool dyn_noa,
                      uint8_t counter, uint32_t intv_us, uint32_t dur_us, uint32_t start_time)
{
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];
    // Get free NOA Instance
    uint8_t noa_inst = P2P_NOA_NB_MAX;

    P2P_DEBUG_PRINT(1, D_CRT "p2p_go_noa_start vif=%d\n", p_vif_entry->index);
    P2P_DEBUG_PRINT(1, D_CRT "    counter=%d, intv=%d, dur=%d, start=%d\n",
                                                    counter, intv_us, dur_us, start_time);

    // Verify that NOA is not already started
    if (p_p2p_entry->role == P2P_ROLE_GO)
    {
        // Get available NOA instance
        noa_inst = p2p_go_get_noa_inst(p_p2p_entry);

        if (noa_inst < P2P_NOA_NB_MAX)
        {
            struct p2p_noa_info_tag *p_noa_entry = &p_p2p_entry->noa[noa_inst];

            // Initialize NOA parameters
            p_noa_entry->noa_counter      = counter;
            p_noa_entry->noa_init_counter = counter;
            p_noa_entry->noa_intv_us      = intv_us;
            p_noa_entry->noa_dur_us       = dur_us;
            p_noa_entry->noa_start_time   = start_time;
            p_noa_entry->dyn_noa          = dyn_noa;
            p_noa_entry->noa_type         = (concurrent) ? P2P_NOA_TYPE_CONCURRENT : P2P_NOA_TYPE_NORMAL;

            p_p2p_entry->noa_nb++;

            if (concurrent)
            {
                p_p2p_entry->noa_cm_nb++;
            }

            // Compute number of absences before next start time update
            p2p_go_noa_get_intv_cnt(p_noa_entry);

            // Program the noa timer with the computed start time
            p2p_noa_timer_prog(p_noa_entry, p_noa_entry->noa_start_time);
            // And Update the NOA status
            p_noa_entry->noa_status = P2P_NOA_TIMER_WAIT_NEXT_ABS;

            // Update the NOA attribute, will be added in the beacon (see p2p_go_bcn_op_done)
            mm_bcn_update_p2p_noa(p_p2p_entry->vif_index, P2P_BCN_UPD_OP_NOA_UPD);

            // Inform the host about the newly created NoA
            p2p_go_send_noa_upd_ind(p_p2p_entry, noa_inst);
            // Update Go PS state
            p2p_update_go_ps_state(p_p2p_entry);
        }
    }

    // Return NOA index
    return (noa_inst < P2P_NOA_NB_MAX);
}

bool p2p_go_noa_stop(struct vif_info_tag *p_vif_entry, uint8_t noa_inst, bool host_req)
{
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];
    // Status
    uint8_t status = CO_FAIL;

    P2P_DEBUG_PRINT(1, D_CRT "p2p_go_noa_stop vif=%d host_req=%d\n", p_vif_entry->index, host_req);

    // Verify that the NOA procedure is well started
    if ((p_p2p_entry->role == P2P_ROLE_GO) && (noa_inst < P2P_NOA_NB_MAX))
    {
        struct p2p_noa_info_tag *p_noa_entry = &p_p2p_entry->noa[noa_inst];

        if ((p_noa_entry->noa_status != P2P_NOA_TIMER_NOT_STARTED) &&
            !(host_req && (p_noa_entry->noa_type == P2P_NOA_TYPE_CONCURRENT)))
        {
            // Stop NOA timer
            mm_timer_clear(&p_noa_entry->noa_timer);

            // Reset NOA status
            p_noa_entry->noa_status = P2P_NOA_TIMER_NOT_STARTED;

            // Update Go PS state
            p2p_update_go_ps_state(p_p2p_entry);

            p_p2p_entry->noa_nb--;

            if (p_noa_entry->noa_type == P2P_NOA_TYPE_NORMAL)
            {
                p_p2p_entry->noa_cm_nb--;
            }

            // Check if NOA attribute has to be removed from the beacon
            if ((p_p2p_entry->oppps_ctw == 0) && (p_p2p_entry->noa_nb == 0))
            {
                // If OppPS is not used, remove NOA part of the Beacon
                mm_bcn_update_p2p_noa(p_p2p_entry->vif_index, P2P_BCN_UPD_OP_NOA_RMV);
            }
            else
            {
                // Update NOA attribute content
                mm_bcn_update_p2p_noa(p_p2p_entry->vif_index, P2P_BCN_UPD_OP_NOA_UPD);
            }

            // Inform the host that a NoA has been stopped
            p2p_go_send_noa_upd_ind(p_p2p_entry, noa_inst);

            status = CO_OK;
        }
    }

    return (status);
}

#if (NX_CHNL_CTXT)
void p2p_go_noa_update_duration(struct vif_info_tag *p_vif_entry, uint8_t noa_inst, uint32_t noa_dur_us)
{
    // Get P2P entry
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];
    // Get NOA entry
    struct p2p_noa_info_tag *p_noa_entry = &p_p2p_entry->noa[noa_inst];

    if (p_noa_entry->noa_status != P2P_NOA_TIMER_NOT_STARTED)
    {
        // Keep next absence duration
        p_noa_entry->noa_next_dur_us = noa_dur_us;
    }
}
#endif //(NX_CHNL_CTXT)

uint8_t p2p_go_bcn_get_noa_len(uint8_t p2p_index)
{
    // NOA IE Length
    uint8_t noa_ie_len = 0;
    // P2P Entry
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p2p_index];

    if ((p_p2p_entry->vif_index != INVALID_VIF_IDX) && p_p2p_entry->is_noa_bcn)
    {
        noa_ie_len = P2P_IE_NOA_NO_NOA_DESC_LENGTH
                       + p_p2p_entry->noa_in_bcn_nb * P2P_NOA_DESC_LENGTH;
    }

    P2P_DEBUG_PRINT(2, D_CRT "p2p_go_bcn_get_noa_len p2p_idx=%d, len=%d\n", p2p_index, noa_ie_len);

    return (noa_ie_len);
}

void p2p_go_bcn_init_noa_pyld(uint32_t a_noa_ie_elt)
{
    co_write8p(a_noa_ie_elt + P2P_IE_NOA_ELMT_ID_OFFSET, P2P_ELMT_ID);
    co_write8p(a_noa_ie_elt + P2P_IE_NOA_OUI_OFFSET, P2P_OUI_WIFI_ALL_BYTE0);
    co_write8p(a_noa_ie_elt + P2P_IE_NOA_OUI_OFFSET + 1, P2P_OUI_WIFI_ALL_BYTE1);
    co_write8p(a_noa_ie_elt + P2P_IE_NOA_OUI_OFFSET + 2, P2P_OUI_WIFI_ALL_BYTE2);
    co_write8p(a_noa_ie_elt + P2P_IE_NOA_OUI_TYPE_OFFSET, P2P_OUI_TYPE_P2P);
    co_write8p(a_noa_ie_elt + P2P_IE_NOA_ATT_ID_OFFSET, P2P_ATT_ID_NOTICE_OF_ABSENCE);
}

void p2p_go_bcn_upd_noa_pyld(uint8_t p2p_index, uint32_t a_noa_ie_elt)
{
    // Get P2P entry
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p2p_index];
    // NOA Counter, Number of processed active NoA
    uint8_t noa_counter, noa_proc_nb = 0;
    // Attribute length
    uint16_t att_length;

    P2P_DEBUG_PRINT(2, D_CRT "p2p_go_bcn_upd_noa_pyld p2p_idx=%d\n", p2p_index);

    co_write8p(a_noa_ie_elt + P2P_IE_NOA_INDEX_OFFSET, p_p2p_entry->index);

    // Increment NOA index
    p_p2p_entry->index++;

    if (p_p2p_entry->oppps_ctw)
    {
        // If CTWindow is not zero, opportunistic PS is used
        co_write8p(a_noa_ie_elt + P2P_IE_NOA_CTW_OPPPS_OFFSET,
                   p_p2p_entry->oppps_ctw | P2P_OPPPS_MASK);
    }
    else
    {
        co_write8p(a_noa_ie_elt + P2P_IE_NOA_CTW_OPPPS_OFFSET, 0);
    }

    for (noa_counter = 0; noa_counter < P2P_NOA_NB_MAX; noa_counter++)
    {
        struct p2p_noa_info_tag *p_noa_entry = &p_p2p_entry->noa[noa_counter];

        /*
         * Check if the NoA instance is active
         * Do not insert non concurrent mode noa if paused due to traffic.
         */
        if ((p_noa_entry->noa_status != P2P_NOA_TIMER_NOT_STARTED) &&
            !(p_p2p_entry->noa_paused && p_noa_entry->dyn_noa && (p_noa_entry->noa_type == P2P_NOA_TYPE_NORMAL)))
        {
            // Get address of the NoA descriptor
            uint32_t a_noa_desc = a_noa_ie_elt + P2P_IE_NOA_NOA_DESC_OFFSET
                                               + (P2P_NOA_DESC_LENGTH * noa_counter);

            noa_proc_nb++;

            co_write8p(a_noa_desc + P2P_NOA_DESC_COUNT_OFFSET, p_noa_entry->noa_init_counter);
            co_write32p(a_noa_desc + P2P_NOA_DESC_DUR_OFFSET, p_noa_entry->noa_dur_us);
            co_write32p(a_noa_desc + P2P_NOA_DESC_INTV_OFFSET, p_noa_entry->noa_intv_us);

            /*
             * Stored start time value is based on Monoatomic counter, value sent within the beacon is
             * a TSF value.
             */
            co_write32p(a_noa_desc + P2P_NOA_DESC_START_OFFSET,
                        p_noa_entry->noa_start_time - (ke_time() - nxmac_tsf_lo_get()));
        }
    }

    // Keep number of NoA in the beacon
    p_p2p_entry->noa_in_bcn_nb = noa_proc_nb;

    // Update the length fields
    att_length = 2 + noa_proc_nb * P2P_NOA_DESC_LENGTH;

    co_write16p(a_noa_ie_elt + P2P_IE_NOA_ATT_LENGTH_OFFSET, att_length);
    co_write8p(a_noa_ie_elt + P2P_IE_NOA_LENGTH_OFFSET, 7 + (uint8_t)att_length);
}

void p2p_go_bcn_op_done(uint8_t p2p_index, uint8_t operation)
{
    // Get P2P entry
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p2p_index];

    P2P_DEBUG_PRINT(2, D_CRT "p2p_go_bcn_op_done p2p_idx=%d, op=%d\n", p2p_index, operation);

    switch (operation)
    {
        case (P2P_BCN_UPD_OP_NOA_ADD):
        {
            p_p2p_entry->is_noa_bcn = true;
        } break;

        case (P2P_BCN_UPD_OP_NOA_RMV):
        {
            p_p2p_entry->is_noa_bcn = false;
        } break;

        case (P2P_BCN_UPD_OP_NOA_UPD):
        {
            // If the NOA attribute is not part of the beacon, insert it
            if (!p_p2p_entry->is_noa_bcn)
            {
                mm_bcn_update_p2p_noa(p_p2p_entry->vif_index, P2P_BCN_UPD_OP_NOA_ADD);
            }
            // else nothing to do
        } break;

        default:
        {
            ASSERT_ERR(0);
        }
    }
}

#if (NX_POWERSAVE)
void p2p_go_pre_tbtt(struct vif_info_tag *p_vif_entry)
{
    // Check if VIF is still active
    if (p_vif_entry->p2p && p_vif_entry->active)
    {
        // Set the GO present bit in order to be sure to be awoken for AP_TBTT interrupt
        p_vif_entry->prevent_sleep |= PS_VIF_P2P_GO_PRESENT;
    }
}
#endif //(NX_POWERSAVE)
#endif //(NX_P2P_GO)

#endif //(NX_P2P)

/// @} end of group
