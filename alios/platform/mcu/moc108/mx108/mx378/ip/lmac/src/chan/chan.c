/**
 ****************************************************************************************
 *
 * @file chan.c
 *
 * @brief MAC Management channel management implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup CHAN
 * @{
 ****************************************************************************************
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "chan.h"
#include "mac_defs.h"
#include "mac_frame.h"
#include "mm.h"
#include "mm_timer.h"
#include "co_endian.h"
#include "sta_mgmt.h"
#include "vif_mgmt.h"
#include "phy.h"
#include "rd.h"
#include "ps.h"
#include "txl_cntrl.h"
#include "txl_frame.h"
#include "rxl_cntrl.h"
#include "hal_machw.h"
#include "td.h"
#include "scan.h"
#include "tpc.h"
#include "me.h"

#if (NX_MDM_VER >= 20)
#include "karst/reg_riu.h"
#endif //(NX_MDM_VER >= 20)

#include "tdls.h"

#include "include.h"
#include "uart_pub.h"

#if (NX_CHNL_CTXT)

/**
 * DEBUG (DEFINES, MACROS, ...)
 ****************************************************************************************
 */

/// Enable or disable TBTT Switch scheduling debug
#define CHAN_DEBUG_TBTT_EN      (0)

/**
 * Debug Configuration for CHAN Module
 *      0 - Traces are disabled
 *      1:3 - Level of verbosity
 */
#define CHAN_DEBUG_TRACES_EN    (0)

#if (CHAN_DEBUG_TRACES_EN)
/// Function used to print module information
#define CHAN_DEBUG_PRINT(lvl, format, ...)                       \
    do {                                                            \
        if (lvl <= CHAN_DEBUG_TRACES_EN)                         \
        {                                                           \
        }                                                           \
    } while (0);
#else
#define CHAN_DEBUG_PRINT(lvl, format, ...)
#endif //(CHAN_DEBUG_TRACES_EN)

/**
 * DEFINES
 ****************************************************************************************
 */

#define CHAN_ROC_SCAN_PENDING_MASK     (CO_BIT(CHAN_ENV_ROC_BIT) | CO_BIT(CHAN_ENV_SCAN_BIT))

/**
 * GLOBAL VARIABLES
 ****************************************************************************************
 */

/// MM Channel module context variable
struct chan_env_tag chan_env;

/// Pool of channel contexts
struct chan_ctxt_tag chan_ctxt_pool[CHAN_CHAN_CTXT_CNT];

/**
 * PRIVATE FUNCTIONS DECLARATION
 ****************************************************************************************
 */

static void chan_switch_start(struct chan_ctxt_tag *p_chan_entry);
static void chan_switch_channel(void);

/**
 * PRIVATE FUNCTIONS DEFINITION
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Compute number of slots elapsed between time1 and time2. If time1 > time2,
 *        returned value will be a positive value.
 *
 * @param[in] time1     Time value in us
 * @param[in] time2     Time value in us
 *
 * @return Computed number of slots (@see CHAN_SLOT_DURATION_US value)
 ****************************************************************************************
 */
static int16_t chan_get_nb_slots(uint32_t time1, uint32_t time2)
{
    // Get time difference in us
    int32_t diff = time1 - time2;

    // Return time difference in slots
    return (int16_t)(diff / CHAN_SLOT_DURATION_US);
}

#if (NX_P2P)
/**
 ****************************************************************************************
 * @brief Loop over the traffic channels in order to find a channel linked with a P2P VIF,
 * on which we have to switch with priority.
 * It takes care of the number of slots remaining for this channel and it
 * also prevent from switching on a channel on which the GO device is not present.
 *
 * @return P2P channel on which we have to switch.
 ****************************************************************************************
 */
static struct chan_ctxt_tag *chan_get_next_p2p_chan(void)
{
    // Next P2P channel context entry, none by default
    struct chan_ctxt_tag *p_p2p_chan_entry = NULL;
    // Highest found number of remaining slots
    uint8_t high_rem_slots = 0;

    // Check if at least one P2P VIF has been created
    if (vif_mgmt_env.nb_p2p_vifs)
    {
        // Get first scheduled channel
        struct chan_ctxt_tag *p_chan_entry = (struct chan_ctxt_tag *)co_list_pick(&chan_env.list_sched_ctxt);

        // Loop on scheduled traffic channels
        while (p_chan_entry)
        {
            // Number of available slots
            uint8_t ava_slots;

            do
            {
                // Check if there is a P2P presence on the channel
                if (!p_chan_entry->nb_p2p_presence)
                {
                    break;
                }

                if (p_chan_entry->nb_rem_slots > p_chan_entry->nb_res_slots)
                {
                    ava_slots = p_chan_entry->nb_rem_slots - p_chan_entry->nb_res_slots;
                }
                else
                {
                    ava_slots = 0;
                }

                // Check if channel has enough available slots, if not do not switch on it
                if (ava_slots < CHAN_MIN_PRES_DUR)
                {
                    break;
                }

                // Try to jump on channel with higher number of available slots
                if (ava_slots > high_rem_slots)
                {
                    high_rem_slots = ava_slots;
                    p_p2p_chan_entry = p_chan_entry;
                }
            } while (0);

            // Get next scheduled channel
            p_chan_entry = (struct chan_ctxt_tag *)p_chan_entry->list_hdr.next;
        }
    }

    CHAN_DEBUG_PRINT(3, "chan_get_next_p2p_chan -> %d\n",
                           (p_p2p_chan_entry != NULL) ? p_p2p_chan_entry->idx : 0xFF);

    return (p_p2p_chan_entry);
}
#endif //(NX_P2P)

/**
 ****************************************************************************************
 * @brief Select the next traffic channel after following events:
 *              - Start of new Channel Distribution Event
 *              - End of TBTT Presence due to end of Beacon/UC/MC/BC reception/transmission
 *              - End of Channel Duration
 *              - P2P Go Start/End of Absence (if P2P features are supported)
*         Try to jump first on P2P channels on which a GO device is present in order to be
*         sure to use available slots outside NOA periods.
 *
 * @return Channel context entry of next traffic channel
 ****************************************************************************************
 */
static struct chan_ctxt_tag *chan_get_next_chan(void)
{
    // By default, stay on current channel
    struct chan_ctxt_tag *p_next_chan_entry = chan_env.current_channel;
    // Get first element of the TBTT switch list
    struct chan_tbtt_tag *p_tbtt_entry = (struct chan_tbtt_tag *)co_list_pick(&chan_env.list_tbtt);
    // Get current time
    uint32_t current_time = ke_time();
    // Counter
    uint8_t counter;
    // Number of remaining slots
    uint8_t nb_rem_slots;

    #if (NX_P2P)
    struct chan_ctxt_tag *p_p2p_chan_entry;
    #endif //(NX_P2P)

    do
    {
        if (chan_env.current_channel)
        {
            // Check if a TBTT presence is in progress
            if (p_tbtt_entry && (p_tbtt_entry->status == CHAN_TBTT_PRESENCE))
            {
                // Stay on current channel
                break;
            }

            // Check that enough slots are remaining in the current CDE
            if (!hal_machw_time_cmp(current_time + (CHAN_MIN_PRES_DUR * CHAN_SLOT_DURATION_US), chan_env.tmr_cde.time))
            {
                // Stay on current channel
                break;
            }

            #if (NX_P2P)
            // Check if there is a channel used for P2P on which we could switch with priority
            p_p2p_chan_entry = chan_get_next_p2p_chan();

            /*
             * If we have found a P2P channel, we switch on it with priority whatever the status of current channel
             */
            if (p_next_chan_entry->nb_linked_p2p_vif || !p_p2p_chan_entry)
            #endif //(NX_P2P)
            {
                // Check if end of channel timer is running
                if (p_next_chan_entry->status == CHAN_WAITING_END)
                {
                    // Wait for end of current channel
                    break;
                }
            }
        }
        #if (NX_P2P)
        else
        {
            p_p2p_chan_entry = chan_get_next_p2p_chan();
        }
        #endif //(NX_P2P)

        // Check if next TBTT will occur during current CDE
        if (p_tbtt_entry && hal_machw_time_cmp(p_tbtt_entry->time, chan_env.tmr_cde.time))
        {
            // Number of slots that can be used by the channel
            uint8_t ava_slots;

            // Get Channel Context for the TBTT
            p_next_chan_entry = vif_info_tab[p_tbtt_entry->vif_index].chan_ctxt;

            // Sanity Check
            ASSERT_ERR(p_next_chan_entry);

            // Check if TBTT will occur in less than minimal presence duration
            if (!hal_machw_time_cmp(current_time + (CHAN_MIN_PRES_DUR * CHAN_SLOT_DURATION_US), p_tbtt_entry->time))
            {
                // Jump on TBTT channel
                break;
            }

            #if (NX_P2P)
            if (p_p2p_chan_entry)
            {
                p_next_chan_entry = p_p2p_chan_entry;
                break;
            }

            // If TBTT channel is used for P2P, do not jump (p_p2p_chan_entry would not have been NULL)
            if (!p_next_chan_entry->nb_linked_p2p_vif)
            #endif //(NX_P2P)
            {
                if (p_next_chan_entry->nb_rem_slots >= p_next_chan_entry->nb_res_slots)
                {
                    ava_slots = p_next_chan_entry->nb_rem_slots - p_next_chan_entry->nb_res_slots;
                }
                else
                {
                    ava_slots = 0;
                }

                // Check if channel context has enough slots to reach the TBTT
                if (ava_slots >= chan_get_nb_slots(p_tbtt_entry->time, current_time))
                {
                    // Jump on TBTT channel
                    break;
                }
            }
        }
        #if (NX_P2P)
        else
        {
            if (p_p2p_chan_entry)
            {
                // Switch on P2P channel
                p_next_chan_entry = p_p2p_chan_entry;
                break;
            }
        }
        #endif //(NX_P2P)

        // No need to switch for a special operation, get channels with higher remaining number of slots
        nb_rem_slots = 0;

        for (counter = 0; counter < CHAN_TRAF_CTXT_CNT; counter++)
        {
            // Get channel context
            struct chan_ctxt_tag *p_chan_entry = &chan_ctxt_pool[counter];

            if ((p_chan_entry->status != CHAN_NOT_SCHEDULED) &&
                (p_chan_entry->nb_rem_slots >= nb_rem_slots))
            {
                #if (NX_P2P)
                if (p_p2p_chan_entry == NULL)
                {
                    // Do not consider P2P channels here
                    if (p_chan_entry->nb_linked_p2p_vif)
                    {
                        continue;
                    }
                }
                #endif //(NX_P2P)

                p_next_chan_entry = p_chan_entry;
                nb_rem_slots = p_chan_entry->nb_rem_slots;
            }
        }
    } while (0);

    CHAN_DEBUG_PRINT(3, "chan_get_next_chan -> %d\n", p_next_chan_entry->idx);

    // Sanity check
    ASSERT_ERR(p_next_chan_entry);

    return (p_next_chan_entry);
}

/**
 ****************************************************************************************
 * @brief Update the number of remaining slots for provided channel context.
 *        This function compute duration elapsed since last CDE event.
 *
 * @param[in] p_chan_entry  Channel context entry
 * @param[in] current_time  Current Time, allowing to compute number of used slots
 ****************************************************************************************
 */
static void chan_upd_nb_rem_slots(struct chan_ctxt_tag *p_chan_entry, uint32_t current_time)
{
    // Get number of slots since last CDE event
    uint8_t nb_slots = chan_get_nb_slots(current_time, chan_env.cde_time);

    // Update remaining number of slots
    p_chan_entry->nb_rem_slots -= co_min(nb_slots, p_chan_entry->nb_rem_slots);
}

/**
 ****************************************************************************************
 * @brief Update the status of a provided channel context and program the Channel Context
 *        Operation timer (Switch or End of Channel) accordingly with the new state.
 *
 *        Few rules applies:
 *          - When next state is CHAN_GOTO_IDLE, timer duration is set to CHAN_SWITCH_TO_DUR
 *            only for traffic channel (no switch timeout for RoC/Scan channels)
 *          - Timer expiration time is not updated is next state is CHAN_WAIT_NOA_CFM
 *          - If next state is CHAN_WAITING_END, end of channel timer is always
 *            programmed for RoC/Scan channels using the channel duration.
 *          - If next state is CHAN_WAITING_END, end of channel timer is programmed for
 *            traffic channel is following condition are true:
 *                -> CDE is used
 *                -> Enough slots are remaining in order to compute end of channel expiration
 *                   time
 *            If timer is not programmed, next state is updated to CHAN_PRESENT.
 *
 * @param[in] p_chan_entry  Channel Context entry
 * @param[in] next_status   New status of the provided channel context
 ****************************************************************************************
 */
static void chan_upd_ctxt_status(struct chan_ctxt_tag *p_chan_entry, uint8_t next_status)
{
    uint32_t tmr_exp_time = 0;
    uint32_t current_time = ke_time();

    CHAN_DEBUG_PRINT(2, "chan_upd_ctxt_status i=%d, s=%d\n", p_chan_entry->idx, next_status);

    if (next_status == CHAN_GOTO_IDLE)
    {
        // Do not start timer for timeout if RoC or Scan channel
        if (p_chan_entry->idx < CHAN_TRAF_CTXT_CNT)
        {
            tmr_exp_time = current_time + CHAN_SWITCH_TO_DUR;
        }        
    }
    else if (next_status == CHAN_WAITING_END)
    {
        do
        {
            CHAN_DEBUG_PRINT(3, "    -> rem=%d, res=%d\n",
                                    p_chan_entry->nb_rem_slots, p_chan_entry->nb_res_slots);

            if (p_chan_entry->idx < CHAN_TRAF_CTXT_CNT)
            {
                // Number of slots to be used for End of Channel Timer programming
                uint8_t ava_slots;

                // Program end of channel timer only if CDE is running
                if (chan_env.nb_sched_ctxt < 2)
                {
                    next_status = CHAN_PRESENT;
                    break;
                }

                // Number of slots available for the channel
                ava_slots = (p_chan_entry->nb_rem_slots >= p_chan_entry->nb_res_slots)
                            ? (p_chan_entry->nb_rem_slots - p_chan_entry->nb_res_slots) : 0;

                /*
                 * Do not program end of channel timer, if remaining number of slots is too low
                 * and wait for next TBTT switch or CDE event
                 */
                if (ava_slots < CHAN_MIN_PRES_DUR)
                {
                    next_status = CHAN_PRESENT;
                    break;
                }

                // Update CDE time in order to be sure to not program in the past
                chan_env.cde_time = current_time;

                tmr_exp_time = chan_env.cde_time + (ava_slots * CHAN_SLOT_DURATION_US);
            }
            else
            {
                tmr_exp_time = current_time + (p_chan_entry->nb_rem_slots * CHAN_SLOT_DURATION_US);
            }

            
        } while (0);
    }
    else if (next_status == CHAN_NOT_PROG)
    {
    }

    // Update the status
    p_chan_entry->status = next_status;

    if (tmr_exp_time == 0)
    {
        // Do not clear timer if switch channel operation is not over
        if (next_status != CHAN_WAIT_NOA_CFM)
        {
            mm_timer_clear(&chan_env.tmr_ctxt_op);
        }
    }
    else
    {
        // Program operation timer
        chan_env.tmr_ctxt_op.env = p_chan_entry;
        // Program the timer
        //os_printf("tmr_ctxt_op:%x\r\n", &chan_env.tmr_ctxt_op.list_hdr);
        mm_timer_set(&chan_env.tmr_ctxt_op, tmr_exp_time);
    }
}

/**
 ****************************************************************************************
 * @brief Compute number of slots that have to be reserved for TBTT presence periods during
 *        the next CDE.
 *
 * @param[in] p_vif_entry    VIF entry
 * @param[in] p_chan_entry   Channel Context entry
 ****************************************************************************************
 */
static void chan_update_reserved_slots(struct vif_info_tag *p_vif_entry,
                                       struct chan_ctxt_tag *p_chan_entry)
{
    uint32_t bcn_int;
    uint8_t nb_tbtt;

    #if NX_BEACONING
    if (p_vif_entry->type == VIF_STA)
    #endif
    {
        // Get peer AP informations
        struct sta_info_tag *p_sta_entry = &sta_info_tab[p_vif_entry->u.sta.ap_id];

        bcn_int = p_sta_entry->bcn_int;
    }
    #if NX_BEACONING
    else
    {
        bcn_int = (uint32_t)p_vif_entry->u.ap.bcn_int << 10;
    }
    #endif

    // Deduce the number of TBTTs
    nb_tbtt = co_max(1, (chan_env.cde_dur_us / bcn_int));

    // Increase number of slots reserved for TBTT
    p_chan_entry->nb_res_slots += nb_tbtt * CHAN_MAX_TBTT_PRES_DUR;
}

/**
 ****************************************************************************************
 * @brief Distribute slots available during a Channel Distribution Event between all the
 *        scheduled channel contexts. This distribution is based on last status generated
 *        by the Traffic Detection module.
 *
 *        Current version of the algorithm is a very basic one.
 *              - A VIF on which no traffic has been detected over the last Traffic
 *        Detection interval will receive only CHAN_MAX_TBTT_PRES_DUR slots for its
 *        linked channel.
 *              - Else CHAN_VIF_NB_SLOTS will be allocated for its channel.
 *        Number of slots not distributed to VIFs with no traffic are then equally distributed
 *        between VIFs on which Traffic has been detected.
 *
 *        This algorithm can be replaced by a smarter one based on several statistics.
 *
 *        The number of slots allocated for a channel is stored in the nb_slots value of
 *        each scheduled Channel Context Entry.
 ****************************************************************************************
 */
static void chan_distribute_slots(void)
{
    // Number of VIFs with a scheduled channel context
    uint8_t nb_vifs = 0;
    // Number of VIFs om which traffic has been detected
    uint8_t nb_vifs_traffic = 0;
    // Number of slots for VIFs on which no traffic was detected
    uint16_t nb_slots_nt;
    // Number of slots that can be shared between slots with detected traffic
    uint16_t nb_slots_ava = 0;

    // VIF Entry
    struct vif_info_tag *p_vif_entry = vif_mgmt_first_used();

    CHAN_DEBUG_PRINT(4, "chan_distribute_slots\n");

    /*
     * ---------------------------------------------------------
     * Check Traffic Status
     * ---------------------------------------------------------
     */
    while (p_vif_entry)
    {
        // Channel Context linked with the VIF
        struct chan_ctxt_tag *p_chan_entry = p_vif_entry->chan_ctxt;

        if (p_chan_entry)
        {
            nb_vifs++;

            if (td_get_status(p_vif_entry->index) || !p_vif_entry->active)
            {
                nb_vifs_traffic++;
            }

            // Reset number of slots
            p_chan_entry->nb_slots = 0;
            p_chan_entry->nb_res_slots = 0;
        }

        // Get next VIF
        p_vif_entry = (struct vif_info_tag *)(p_vif_entry->list_hdr.next);
    }

    // Sanity check
    ASSERT_WARN(nb_vifs == (chan_env.cde_dur_us / (CHAN_VIF_NB_SLOTS * CHAN_SLOT_DURATION_US)));

    /*
     * ---------------------------------------------------------
     * Distribute slots accordingly with the traffic status
     * ---------------------------------------------------------
     */

    // Check if all VIFs have the same traffic status
    if ((nb_vifs_traffic == 0) || (nb_vifs == nb_vifs_traffic))
    {
        nb_slots_nt = CHAN_VIF_NB_SLOTS;
    }
    else
    {
        nb_slots_nt = CHAN_MAX_TBTT_PRES_DUR;

        // Deduce number of total additional slots available for VIFs with traffic
        nb_slots_ava = (nb_vifs - nb_vifs_traffic) * (CHAN_VIF_NB_SLOTS - CHAN_MAX_TBTT_PRES_DUR);
        // Get number of additional slots per VIF
        nb_slots_ava /= nb_vifs_traffic;
    }

    p_vif_entry = vif_mgmt_first_used();

    while (p_vif_entry)
    {
        // Channel Context linked with the VIF
        struct chan_ctxt_tag *p_chan_entry = p_vif_entry->chan_ctxt;

        if (p_chan_entry)
        {
            if (td_get_status(p_vif_entry->index) || !p_vif_entry->active)
            {
                p_chan_entry->nb_slots += (CHAN_VIF_NB_SLOTS + nb_slots_ava);
            }
            else
            {
                p_chan_entry->nb_slots += nb_slots_nt;
            }

            p_chan_entry->nb_rem_slots = p_chan_entry->nb_slots;

            // Update the number of reserved slots
            chan_update_reserved_slots(p_vif_entry, p_chan_entry);

            CHAN_DEBUG_PRINT(4, "    -> i=%d, s=%d r=%d\n", p_chan_entry->idx, p_chan_entry->nb_slots, p_chan_entry->nb_res_slots);
        }

        // Get next VIF
        p_vif_entry = (struct vif_info_tag *)(p_vif_entry->list_hdr.next);
    }
}

/**
 ****************************************************************************************
 * @brief Program the timer used for delaying Scan/RoC operations.
 *        chan_conn_less_delay_evt is called upon timer expiration
 ****************************************************************************************
 */
static void chan_conn_less_delay_prog(void)
{
    // Set status bit
    chan_env.status |= CO_BIT(CHAN_ENV_DELAY_PROG_BIT);
    // Set timer
    mm_timer_set(&chan_env.tmr_conn_less, ke_time() + CHAN_CONN_LESS_DELAY);
}

/**
 ****************************************************************************************
 * @brief Callback called upon Connection Less Delay timer expiration.
 *        If both Scan and RoC are scheduled, start RoC.
 *
 *        @param[in] env    Should be a NULL pointer, do not use it
 ****************************************************************************************
 */
static void chan_conn_less_delay_evt(void *env)
{
    struct chan_ctxt_tag *p_conn_less_chan = NULL;

    // In waiting, start RoC in priority
    if (chan_env.status & CO_BIT(CHAN_ENV_ROC_WAIT_BIT))
    {
        // Sanity check, no RoC should be in progress here
        ASSERT_ERR((chan_env.status & CO_BIT(CHAN_ENV_ROC_BIT)) == 0);

        // Clear status
        chan_env.status &= ~CO_BIT(CHAN_ENV_ROC_WAIT_BIT);
        // Set RoC bit status
        chan_env.status |= CO_BIT(CHAN_ENV_ROC_BIT);

        // Modify channel on which we will switch
        p_conn_less_chan = &chan_ctxt_pool[CHAN_ROC_CTXT_IDX];
    }
    // else start Scan
    else if (chan_env.status & CO_BIT(CHAN_ENV_SCAN_WAIT_BIT))
    {
        // Sanity check, no Scan should be in progress here
        ASSERT_ERR((chan_env.status & CO_BIT(CHAN_ENV_SCAN_BIT)) == 0);

        // Clear status
        chan_env.status &= ~CO_BIT(CHAN_ENV_SCAN_WAIT_BIT);
        // Set Scan bit status
        chan_env.status |= CO_BIT(CHAN_ENV_SCAN_BIT);

        // Modify channel on which we will switch
        p_conn_less_chan = &chan_ctxt_pool[CHAN_SCAN_CTXT_IDX];
    }

    if (p_conn_less_chan)
    {
        if (!chan_env.chan_switch)
        {
            // Trigger switch procedure
            chan_switch_start(p_conn_less_chan);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Callback called upon Channel Distribution Event timer expiration or when a new
 *        CDE has to be started.
 *        Its purpose is to update the bandwidth allocation status for each scheduled
 *        channel context based on last traffic detection information.
 *        Then it starts a new CDE.
 *
 *        Note: Start of new CDE is skipped in following conditions,
 *            - CDE is not needed anymore (less than two scheduled channels)
 *            - RoC or Scan is in progress
 *            - A timeout has been detected during current channel switch.
 *        In the last two situations, CDE will be restarted once pending procedures will be
 *        over.
 *
 *        @param[in] env    If env pointer is not NULL, it means that CDE has been restarted
 *                          at the end of switch procedure, can directly switch on first
 *                          CDE channel
 ****************************************************************************************
 */
static void chan_cde_evt(void *env)
{
    // Get Current Time
    uint32_t current_time = ke_time();
    // Next channel
    struct chan_ctxt_tag *p_chan_entry = NULL;

    CHAN_DEBUG_PRINT(2, "chan_cde_evt n=%d s=%d\n", chan_env.nb_sched_ctxt, chan_env.status);

    

    do
    {
        // Check that CDE is still needed
        if (chan_env.nb_sched_ctxt < 2)
        {
            break;
        }

        // Check if a RoC or a Scan procedure is in progress
        if (chan_env.status & (CO_BIT(CHAN_ENV_ROC_BIT) | CO_BIT(CHAN_ENV_SCAN_BIT)))
        {
            // CDE will be restart at the end of the scan/roc procedure
            break;
        }

        // Check if a switch timeout has occurred
        if (chan_env.status & CO_BIT(CHAN_ENV_TIMEOUT_BIT))
        {
            // CDE will be restarted once switch procedure will be over
            break;
        }

        // Program end of CDE - Has to be done before next operations
		os_printf("t3:%d\r\n", current_time + chan_env.cde_dur_us);
        mm_timer_set(&chan_env.tmr_cde, current_time + chan_env.cde_dur_us);

        // Update CDE time
        chan_env.cde_time = current_time;

        // Distribute bandwidth between the channel contexts
        chan_distribute_slots();

        if (chan_env.current_channel)
        {
            // Update current channel context status
            chan_upd_ctxt_status(chan_env.current_channel, CHAN_NOT_PROG);
        }

        // Get first channel context of the CDE
        p_chan_entry = chan_get_next_chan();

        if (env == NULL)
        {
            // Jump on channel
            chan_switch_start(p_chan_entry);
        }
        else
        {
            // Update chan_switch value
            chan_env.chan_switch = p_chan_entry;
        }
    } while (0);

    
}

#if (NX_HW_SCAN)
/**
 ****************************************************************************************
 * @brief Send a channel survey to the host by using the MM_CHANNEL_SURVEY_IND message.
 *
 ****************************************************************************************
 */
static void chan_send_survey_ind(void)
{
    // Get latest scanned channel
    struct scan_chan_tag const *p_scan_chan = scan_get_chan();
    // Allocate a MM_CHANNEL_SURVEY_IND message
    struct mm_channel_survey_ind *p_ind = KE_MSG_ALLOC(MM_CHANNEL_SURVEY_IND,
                                                       TASK_API, TASK_MM,
                                                       mm_channel_survey_ind);
    #if (NX_MDM_VER >= 20)
    uint8_t read_counter = 10;
    #endif //(NX_MDM_VER >= 20)

    // Fill the parameters
    p_ind->freq = p_scan_chan->freq;
    p_ind->chan_time_ms = ((p_scan_chan->flags & SCAN_PASSIVE_BIT) ? SCAN_PASSIVE_DURATION : SCAN_ACTIVE_DURATION) / 1000;
    // Fill the parameters - Time values read in register are in units of us (-> /1000)
    p_ind->chan_time_busy_ms = nxmac_edca_cca_busy_get() / 1000;
    p_ind->noise_dbm = 0;

    #if (NX_MDM_VER >= 20)
    /*
     * If read noise value is 0, value is not valid because of re-sync between AGC clock and
     * AHB clock. Register value is updated once AGC is in listen state.
     */
    while (read_counter--)
    {
        // During scan, config used is 1x1, noise has to be read to antenna 0
        int8_t reg_noise = (int8_t)riu_inbdpow20pnoisedbm0_getf();

        if (reg_noise != 0)
        {
            p_ind->noise_dbm = reg_noise;
            break;
        }
    }
    #endif //(NX_MDM_VER >= 20)

    // Send the indication
    ke_msg_send(p_ind);
}
#endif //(NX_HW_SCAN)

/**
 ****************************************************************************************
 * @brief Send MM_REMAIN_ON_CHANNEL_EXP_IND message to the upper layers.
 *
 * @param[in] Pointer to the RoC Channel Context
 ****************************************************************************************
 */
static void chan_send_roc_exp_ind(struct chan_ctxt_tag *roc_chan_ctxt)
{
    // Inform the host that the remain on channel has expired
    struct mm_remain_on_channel_exp_ind *ind = KE_MSG_ALLOC(MM_REMAIN_ON_CHANNEL_EXP_IND,
                                                            roc_chan_ctxt->taskid, TASK_MM,
                                                            mm_remain_on_channel_exp_ind);

    ind->chan_ctxt_index = roc_chan_ctxt->idx;
    ind->vif_index       = roc_chan_ctxt->vif_index;

    ke_msg_send(ind);
}

/**
 ****************************************************************************************
 * @brief Callback called upon Channel Operation Timer expiration.
 *        This timer is used for two main purposes:
 *            - Detect a timeout during a channel switch (too much time can be taken by HW when
 *              going to IDLE depending for instance of rate/length of current TXed packet)
 *              If timer expires while channel context state is CHAN_GOTO_IDLE or
 *              CHAN_WAIT_NOA_CFM, we keep in mind that the TO has happened.
 *              Will be processed once switch procedure is over.
 *            - Detect that the number of remaining slots for a channel has been drained.
 *
 * @param[in] env   Channel for which timer was programmed
 ****************************************************************************************
 */
static void chan_ctxt_op_evt(void *env)
{
    struct chan_ctxt_tag *p_chan_entry = (struct chan_ctxt_tag *)env;
	GLOBAL_INT_DECLARATION();

    CHAN_DEBUG_PRINT(2, "chan_ctxt_op_evt i=%d, s=%d nb=%d\n",
                       p_chan_entry->idx, p_chan_entry->status, chan_env.nb_sched_ctxt);

    switch (p_chan_entry->status)
    {
        case (CHAN_GOTO_IDLE):
        case (CHAN_WAIT_NOA_CFM):
        {
            // Set timeout bit
            chan_env.status |= CO_BIT(CHAN_ENV_TIMEOUT_BIT);

        } break;

        case (CHAN_WAITING_END):
        {
            if (p_chan_entry->idx < CHAN_TRAF_CTXT_CNT)
            {
                // Reset Channel Context status
                p_chan_entry->status = CHAN_NOT_PROG;

                if (chan_env.nb_sched_ctxt == 1)
                {
                    // Switch to default channel
                    p_chan_entry = (struct chan_ctxt_tag *)co_list_pick(&chan_env.list_sched_ctxt);
                }
                else if (chan_env.nb_sched_ctxt > 1)
                {
                    // Current time
                    uint32_t current_time = ke_time();

                    // Update number of remaining slots for current channel
                    chan_upd_nb_rem_slots(p_chan_entry, current_time);

                    // Update CDE Time
                    chan_env.cde_time = current_time;

                    // Get next channel context of the CDE
                    p_chan_entry = chan_get_next_chan();
                }

                // Jump on channel
                chan_switch_start(p_chan_entry);
            }
            else
            {
                #if (NX_POWERSAVE)
                // Power Save can be used
                GLOBAL_INT_DISABLE();
                ps_env.prevent_sleep &= ~PS_SCAN_ONGOING;
                GLOBAL_INT_RESTORE();

                nxmac_pwr_mgt_setf(chan_env.pm);
                #endif //(NX_POWERSAVE)

                chan_env.current_channel = NULL;

                if (p_chan_entry->idx == CHAN_SCAN_CTXT_IDX)
                {
                    #if (NX_HW_SCAN)
                    // Send a channel occupation report to the host
                    chan_send_survey_ind();
                    #endif //(NX_HW_SCAN)

                    // Clear Scan in progress status
                    chan_env.status &= ~CO_BIT(CHAN_ENV_SCAN_BIT);

                    // Indicate the end of scanning
                    ke_msg_send_basic(MM_SCAN_CHANNEL_END_IND, TASK_SCAN, TASK_NONE);
                }
                else if (p_chan_entry->idx == CHAN_ROC_CTXT_IDX)
                {
                    if (p_chan_entry->taskid != TASK_MM)
                    {
                        #if TDLS_ENABLE
                        if (p_chan_entry->taskid == TASK_TDLS)
                        {
                            tdls_send_chan_switch_base_ind(p_chan_entry);
                        }
                        else
                        #endif
                        {
                            chan_send_roc_exp_ind(p_chan_entry);
                        }
                    }
                    else
                    {
                        // Clear beacon detection status bit
                        chan_env.status &= ~CO_BIT(CHAN_ENV_BCN_DETECT_BIT);
                    }

                    // Clear ROC in progress status
                    chan_env.status &= ~CO_BIT(CHAN_ENV_ROC_BIT);
                }

                // Update the index
                p_chan_entry->idx = CHAN_CTXT_UNUSED;

                // Check if Delay timer has to be restarted
                if (chan_env.status & (CO_BIT(CHAN_ENV_ROC_WAIT_BIT) | CO_BIT(CHAN_ENV_SCAN_WAIT_BIT)))
                {
                    chan_conn_less_delay_prog();
                }
                else
                {
                    // Clear Delay timer status bit
                    chan_env.status &= ~CO_BIT(CHAN_ENV_DELAY_PROG_BIT);
                }

                if (chan_env.nb_sched_ctxt == 1)
                {
                    // Switch to default channel
                    p_chan_entry = (struct chan_ctxt_tag *)co_list_pick(&chan_env.list_sched_ctxt);

                    chan_switch_start(p_chan_entry);
                }
                else if (chan_env.nb_sched_ctxt > 1)
                {
                    // Restart CDE
                    chan_cde_evt(NULL);
                }
                else
                {
                    // Request to go to IDLE again
                    mm_force_idle_req();

                    // Host can now modify the IDLE state
                    mm_back_to_host_idle();
                }
            }
        } break;

        default:
        {
            // Nothing to do
        } break;
    }
}

/**
 ****************************************************************************************
 * @brief Function called once SW and HW are ready for a channel switch.
 *        It checks if a timeout has been detected during the procedure:
 *            - If yes and if CDE is used, we restart a CDE and we directly jump on the
 *        indicated next channel.
 ****************************************************************************************
 */
static void chan_pre_switch_channel(void)
{
    // Check if we have to change the next channel
    struct chan_ctxt_tag *p_new_chan_switch = NULL;

    // Stop switch timeout timer
    mm_timer_clear(&chan_env.tmr_ctxt_op);
	//os_printf("chan_pre_switch_channel\r\n");
    // Check if a Timeout has occurred during switch
    if (chan_env.status & CO_BIT(CHAN_ENV_TIMEOUT_BIT))
    {
        // Check if CDE is used
        if (chan_env.nb_sched_ctxt >= 2)
        {
            if (chan_env.chan_switch)
            {
                // Clear status of the current switch channel
                chan_env.chan_switch->status = CHAN_NOT_PROG;

                // CDE will be restarted soon, cancel switch
                chan_env.chan_switch = NULL;
            }
        }

        // Clear timeout bit
        chan_env.status &= ~CO_BIT(CHAN_ENV_TIMEOUT_BIT);
    }

    // If a Scan Request is pending, jump on Scan channel
    if (chan_env.status & CO_BIT(CHAN_ENV_SCAN_BIT))
    {
        // Modify channel on which we will switch
        p_new_chan_switch = &chan_ctxt_pool[CHAN_SCAN_CTXT_IDX];
    }
    // If a RoC Request is pending, jump on RoC Channel
    else if (chan_env.status & CO_BIT(CHAN_ENV_ROC_BIT))
    {
        // Modify channel on which we will switch
        p_new_chan_switch = &chan_ctxt_pool[CHAN_ROC_CTXT_IDX];
    }

    if (p_new_chan_switch)
    {
       if (chan_env.chan_switch)
       {
           // Clear status of the current switch channel
           chan_env.chan_switch->status = CHAN_NOT_PROG;
       }

       chan_env.chan_switch = p_new_chan_switch;
    }

    if (chan_env.chan_switch == NULL)
    {
        // Check if CDE is used
        if (chan_env.nb_sched_ctxt >= 2)
        {
            // Restart CDE and get first channel - Do not start switch on first CDE channel
            chan_cde_evt((void *)1);
        }
        else if (chan_env.nb_sched_ctxt == 1)
        {
            chan_env.chan_switch = (struct chan_ctxt_tag *)co_list_pick(&chan_env.list_sched_ctxt);
        }
    }

    if (chan_env.chan_switch != NULL)
    {
        // Finally go on new channel
        chan_switch_channel();
    }
}

/**
 ****************************************************************************************
 * @brief Function called during the Channel Switch procedure each time a NULL packet used
 *        for Absence indication is confirmed.
 *        The cfm_cnt counter value in the chan environment is decremented. When its value
 *        comes to 0, we can switch on a new channel.
 ****************************************************************************************
 */
static void chan_tx_cfm(void *dummy, uint32_t status)
{
    CHAN_DEBUG_PRINT(3, "chan_tx_cfm s=%d cnt=%d\n", (chan_env.chan_switch != NULL), chan_env.cfm_cnt);

    // Sanity check - We shall be waiting for at least 1 TX confirmation
    ASSERT_ERR(chan_env.cfm_cnt);

    // Decrease the number of confirmations still awaited
    chan_env.cfm_cnt--;

    // Check if all confirmations have been received
    if (chan_env.cfm_cnt == 0)
    {
        // Request to go to IDLE again, so that we can perform the channel switch
        mm_force_idle_req();

        // Switch on new channel
        chan_pre_switch_channel();
    }
}

/**
 ****************************************************************************************
 * @brief For each VIF linked with the channel context we are switching on, we send a NULL
 *        frame with the PM bit set to 0 in order to indicate our presence.
 ****************************************************************************************
 */
static void chan_notify_presence(void)
{
    struct vif_info_tag *p_vif_entry    = vif_mgmt_first_used();
    struct chan_ctxt_tag *p_cur_ctxt = chan_env.current_channel;

    #if (NX_POWERSAVE)
    // First check if STA VIFs on this channel are active or not
    if ((ps_env.ps_on) && !(ps_env.prevent_sleep & PS_PSM_PAUSED))
    {
        // STA VIFs are not active, so no need to indicate our presence
        return;
    }
    #endif //(NX_POWERSAVE)

    // Reset the PwrMgt bit in all frames
    nxmac_pwr_mgt_setf(0);

    // Go through all the active VIFs
    while (p_vif_entry != NULL)
    {
        // Check if VIF is attached to this channel context
        if (p_vif_entry->chan_ctxt == p_cur_ctxt)
        {
            // Check if the VIF is of STA type, and associated
            if ((p_vif_entry->type == VIF_STA) && p_vif_entry->active)
            {
                #if (NX_P2P)
                bool send = true;

                // If VIF is for P2P, check that GO is present
                if (p_vif_entry->p2p)
                {
                    send = p2p_info_tab[p_vif_entry->p2p_index].is_go_present;
                }

                if (send)
                #endif //(NX_P2P)
                {
                    // Send a NULL frame to indicate to the AP that we are leaving the channel
                    txl_frame_send_null_frame(p_vif_entry->u.sta.ap_id, NULL, NULL);
                }
            }
        }

        // Go to next VIF
        p_vif_entry = vif_mgmt_next(p_vif_entry);
    }
}

/**
 ****************************************************************************************
 * @brief For each VIF linked with the channel context we are leaving, we send a NULL
 *        frame with the PM bit set to 1 in order to indicate our absence until next TBTT.
 ****************************************************************************************
 */
static int chan_notify_absence(void)
{
    int cfm_cnt = 0;
    struct vif_info_tag *p_vif_entry = vif_mgmt_first_used();
    struct chan_ctxt_tag *cur;
    uint8_t prev_status;

    // First check if we are on a channel
    if (chan_env.current_channel == NULL)
    {
        return 0;
    }

    cur = chan_env.current_channel;

    #if (NX_POWERSAVE)
    // First check if STA VIFs on this channel are active or not
    if ((ps_env.ps_on) && !(ps_env.prevent_sleep & PS_PSM_PAUSED))
    {
        // STA VIFs are already in PS-mode, so no need to indicate our absence
        return 0;
    }
    #endif //(NX_POWERSAVE)


    // Set the PwrMgt bit in all frames
    nxmac_pwr_mgt_setf(1);

    // Go through all the active VIFs
    while (p_vif_entry != NULL)
    {
        // Check if VIF is attached to this channel context
        if (p_vif_entry->chan_ctxt == cur)
        {
            // Check if the VIF is of STA type, and associated
            if ((p_vif_entry->type == VIF_STA) && p_vif_entry->active &&
                (p_vif_entry->u.sta.ap_id != INVALID_STA_IDX))
            {
                #if (NX_P2P)
                bool send = true;

                // If VIF is for P2P, check that GO is present
                if (p_vif_entry->p2p)
                {
                    send = p2p_info_tab[p_vif_entry->p2p_index].is_go_present;
                }

                if (send)
                #endif //(NX_P2P)
                {
                    // Send a NULL frame to indicate to the AP that we are leaving the channel
                    // update status to allow frame transmission
                    prev_status = cur->status;
                    cur->status = CHAN_SENDING_NOA;
                    if (txl_frame_send_null_frame(p_vif_entry->u.sta.ap_id, chan_tx_cfm, NULL) == CO_OK)
                    {
                        // We expect one more confirmation
                        cfm_cnt++;
                    }
                    cur->status = prev_status;
                }
            }
        }

        // Go to next VIF
        p_vif_entry = vif_mgmt_next(p_vif_entry);
    }

    // Save the number of TX confirmation expected
    chan_env.cfm_cnt = cfm_cnt;

    // Check if packets have been programmed
    if (cfm_cnt)
    {
        // Update next_channel state
        chan_upd_ctxt_status(chan_env.chan_switch, CHAN_WAIT_NOA_CFM);

        // NULL frames programmed, we need to reactivate the HW
        mm_active();
    }

    return (cfm_cnt);
}

/**
 ****************************************************************************************
 * @brief This callback is called once HW has gone to the IDLE state during a channel
 *        switch procedure.
 ****************************************************************************************
 */
static void chan_goto_idle_cb(void)
{
    //os_printf("chan_goto_idle_cb s=%d\r\n", (chan_env.chan_switch != NULL));

    do
    {
        // Flush all the TX and RX queues
        mm_force_idle_req();

        /*
         * Check if a timeout has been raised during go to idle operation and if current channel
         * still exists.
         */
        if (chan_env.current_channel &&
            !(chan_env.status & CO_BIT(CHAN_ENV_TIMEOUT_BIT)))
        {
            // Check if we need to notify the absence of the local VIFs to the peers
            if (chan_notify_absence())
            {
                break;
            }
        }

        // Switch on a new channel
        chan_pre_switch_channel();
    } while (0);
}

/**
 ****************************************************************************************
 * @brief Send CHANNEL_PRE_SWITCH_IND message to the upper layers.
 *        No more packets should be pushed for transmission after reception of this message.
 *
 * @param[in] old_chan_ctxt     Channel context we are about to leave
 ****************************************************************************************
 */
static void chan_send_pre_switch_ind(struct chan_ctxt_tag *old_chan_ctxt)
{
    if (old_chan_ctxt->idx != CHAN_SCAN_CTXT_IDX)
    {
        // Allocate the indication structure
        struct mm_channel_pre_switch_ind *ind = KE_MSG_ALLOC(MM_CHANNEL_PRE_SWITCH_IND, TASK_API,
                                                             TASK_MM, mm_channel_pre_switch_ind);

        // Fill-in the parameters
        ind->chan_index = old_chan_ctxt->idx;

        // Send the message
        ke_msg_send(ind);
    }
}

/**
 ****************************************************************************************
 * @brief Send CHANNEL_SWITCH_IND message to the upper layers.
 *  *        Packets can be pushed for transmission after reception of this message.
 *
 * @param[in] new_chan_ctxt     Channel context on which we have switched
 ****************************************************************************************
 */
static void chan_send_switch_ind(struct chan_ctxt_tag *new_chan_ctxt)
{
    if (new_chan_ctxt->idx != CHAN_SCAN_CTXT_IDX)
    {
        // Allocate the indication structure
        struct mm_channel_switch_ind *ind = KE_MSG_ALLOC(MM_CHANNEL_SWITCH_IND, TASK_API,
                                                         TASK_MM, mm_channel_switch_ind);

        // Fill-in the parameters
        ind->chan_index = new_chan_ctxt->idx;
        // If ROC triggered internally, do not notify driver
        #if TDLS_ENABLE
        ind->roc        = ((new_chan_ctxt->idx == CHAN_ROC_CTXT_IDX) && (new_chan_ctxt->taskid != TASK_MM) && (new_chan_ctxt->taskid != TASK_TDLS));
        ind->roc_tdls   = new_chan_ctxt->roc_tdls;        
        #else
        ind->roc        = ((new_chan_ctxt->idx == CHAN_ROC_CTXT_IDX) && (new_chan_ctxt->taskid != TASK_MM));
        ind->roc_tdls   = 0;
        #endif
        ind->vif_index  = new_chan_ctxt->vif_index;

        // Send the message
        ke_msg_send(ind);
    }
}

#if (NX_P2P_GO)
/**
 ****************************************************************************************
 * @brief Check if NOA has to be started/updated in following configuration:
 *          - A GO VIF is active in parallel with a STA VIF
 *              -> NOA start time will be the next TBTT time for the STA connection
 *                  TODO [LT] - Start Time should move if TBTT moves
 *              -> NOA duration will depends on the bandwidth allocated for the STA link.
 *
 * @param[in] p_tbtt_entry   TBTT Information structure for the P2P GO VIF
 ****************************************************************************************
 */
static void chan_p2p_noa_manage(struct chan_tbtt_tag *p_tbtt_entry)
{
    // Get VIF interface
    struct vif_info_tag *p_vif_entry = &vif_info_tab[p_tbtt_entry->vif_index];
    // Get P2P information
    struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];

    // Get next STA TBTT entry
    struct chan_tbtt_tag *p_sta_tbtt_entry = (struct chan_tbtt_tag *)p_tbtt_entry->list_hdr.next;

    if (p_sta_tbtt_entry)
    {
        // Get next STA VIF entry
        struct vif_info_tag *p_sta_vif_entry = &vif_info_tab[p_sta_tbtt_entry->vif_index];

        // Check if at least one beacon has been received (would indicate that tbtt time is correct)

        if (p_sta_vif_entry->u.sta.bcn_rcved)
        {
            // Get STA Channel
            struct chan_ctxt_tag *p_sta_chan_entry = p_sta_vif_entry->chan_ctxt;
            // Absence Duration
            uint32_t duration_us = p_sta_chan_entry->nb_slots * CHAN_SLOT_DURATION_US;
            // Get beacon interval
            uint32_t go_bcn_int  = (uint32_t)p_vif_entry->u.ap.bcn_int << 10;

            if (chan_get_nb_slots(p_tbtt_entry->time + go_bcn_int, p_sta_tbtt_entry->time) < p_sta_chan_entry->nb_slots)
            {
                /*
                 * GO TBBT presence will occurs during the absence period. Due to NOA priority rules, we will have to
                 * be present for it. Increase absence duration in order to well to absent during the STA channel duration.
                 */

                duration_us += CHAN_MAX_TBTT_PRES_DUR * CHAN_SLOT_DURATION_US;
            }

            if (!p_p2p_entry->is_noa_bcn)
            {
                // Get channel context used by GO connection
                struct chan_ctxt_tag *p_chan_entry = p_vif_entry->chan_ctxt;
                // Get beacon interval used by peer AP
                uint32_t sta_bcn_int = sta_info_tab[p_sta_vif_entry->u.sta.ap_id].bcn_int;

                // Start NOA (Concurrent Mode)
                p_chan_entry->p2p_noa_idx =  p2p_go_noa_start(p_vif_entry, true, P2P_NOA_CONTINUOUS_COUNTER, false,
                                                              sta_bcn_int, duration_us,
                                                              p_sta_tbtt_entry->time);

                // Sanity check
                ASSERT_ERR(p_chan_entry->p2p_noa_idx < P2P_NOA_NB_MAX);
            }
            else
            {
                // Check if duration has been updated
                if (duration_us != p_p2p_entry->noa[0].noa_dur_us)
                {
                    p2p_go_noa_update_duration(p_vif_entry, 0, duration_us);
                }
            }
        }
    }
}
#endif //(NX_P2P_GO)

/**
 ****************************************************************************************
 * @brief Manually increase the next TBTT time after a collision with another TBTT has
 *        been detected. The priority of the TBTT is increased in order to avoid being
 *        delayed once again in the next collision check.
 *
 * @param[in] p_tbtt_entry      TBBT Entry that has to be delayed
 ****************************************************************************************
 */
static void chan_tbtt_delay(struct chan_tbtt_tag *p_tbtt_entry)
{
    // Get associated VIF entry
    struct vif_info_tag *p_vif_entry = &vif_info_tab[p_tbtt_entry->vif_index];
    // Beacon Interval in us
    uint32_t bcn_int_us;

    CHAN_DEBUG_PRINT(2, "chan_tbtt_delay i=%d\n", p_tbtt_entry->vif_index);

    #if NX_BEACONING
    if (p_vif_entry->type == VIF_STA)
    #endif
    {
        // Get peer AP information
        struct sta_info_tag *p_sta_entry = &sta_info_tab[p_vif_entry->u.sta.ap_id];

        bcn_int_us = p_sta_entry->bcn_int;
    }
    #if NX_BEACONING
    else
    {
        bcn_int_us = (uint32_t)p_vif_entry->u.ap.bcn_int << 10;
    }
    #endif

    // Update TBTT switch time
    p_tbtt_entry->time += bcn_int_us;

    if (p_tbtt_entry->priority < CHAN_TBTT_PRIORITY_MAX)
    {
        // Increase priority
        p_tbtt_entry->priority++;
    }
}

/**
 ****************************************************************************************
 * @brief Detect time conflict between two provided TBTT presence.
 *        It is considered that the TBTT presence duration is CHAN_MAX_TBTT_PRES_DUR.
 *
 * @param[in] p_tbtt_entry1      First TBTT entry to be checked
 * @param[in] p_tbtt_entry2      Second TBTT entry to be checked
 *
 * @return true if a collision has been detected, else false
 ****************************************************************************************
 */
static bool chan_tbtt_detect_conflict(struct chan_tbtt_tag *p_tbtt_entry1,
                                         struct chan_tbtt_tag *p_tbtt_entry2)
{
    // Indicate if a there is a conflict between the two contexts
    bool conflict;
    // Start/End times
    uint32_t start1, start2, end1, end2;

    start1 = p_tbtt_entry1->time;
    start2 = p_tbtt_entry2->time;
    end1   = (start1 + CHAN_MAX_TBTT_PRES_DUR * CHAN_SLOT_DURATION_US);
    end2   = (start2 + CHAN_MAX_TBTT_PRES_DUR * CHAN_SLOT_DURATION_US);

    /**
     * Conflict is detected in following cases:
     * (1)  -----------------------
     *      |   TBTT Presence 1   |
     *      ----------------------------------
     *                 |   TBTT Presence 2   |
     *                 -----------------------
     *
     * (2)              -----------------------
     *                  |   TBTT Presence 1   |
     *      -----------------------------------
     *      |   TBTT Presence 2   |
     *      -----------------------
     *
     * (3)        -----------------------
     *            |   TBTT Presence 1   |
     *      -----------------------------------
     *      |         TBTT Presence 2         |
     *      -----------------------------------
     *
     * (4)  -----------------------------------
     *      |         TBTT Presence 1         |
     *      -----------------------------------
     *            |   TBTT Presence 2   |
     *            -----------------------
     *
     * => if (((start1 >= start2) && (start1 <= end2)) ||
     *        ((start2 >= start1) && (start2 <= end1)))
     */
    if ((!hal_machw_time_cmp(start1, start2) && !hal_machw_time_cmp(end2, start1)) ||
        (!hal_machw_time_cmp(start2, start1) && !hal_machw_time_cmp(end1, start2)))
    {
        conflict = true;
    }
    else
    {
        conflict = false;
    }

    return (conflict);
}

/**
 ****************************************************************************************
 * @brief Try to insert the provided TBTT entry in the list of TBTTs. TBTT are sorted
 *        by chronological order. The function takes care of possible collisions between
 *        the different TBTT presence.
 *
 * @param[in] p_tbtt_entry      TBTT entry to be inserted
 ****************************************************************************************
 */
static void chan_tbtt_insert(struct chan_tbtt_tag *p_tbtt_entry)
{
    // Indicate if the element can be inserted
    bool insert = true;
    // Number of elements to delay
    uint8_t nb_delayed = 0;

    // Get the list of TBTT
    struct co_list *list_tbtt = &chan_env.list_tbtt;
    // First element to delay
    struct chan_tbtt_tag *p_delay_elem = NULL;
    // Element used to insert the TBTT structure at the good position in list_tbtt
    struct chan_tbtt_tag *p_prev_elem  = NULL;
    // Element extracted from the list
    struct chan_tbtt_tag *p_elem       = (struct chan_tbtt_tag *)co_list_pick(list_tbtt);

    /*---------------------------------------------------------------------
     * FIND TBTT POSITION
     *---------------------------------------------------------------------*/
    while (p_elem)
    {
        // Sanity checks, TBTT entry should not be already present in the list
        ASSERT_ERR(p_elem != p_tbtt_entry);

        // Check if the element is already in progress
        if (p_elem->status != CHAN_TBTT_PRESENCE)
        {
            if (chan_tbtt_detect_conflict(p_tbtt_entry, p_elem))
            {
                if (p_tbtt_entry->priority > p_elem->priority)
                {
                    if (!p_delay_elem)
                    {
                        p_delay_elem = p_elem;
                    }

                    // list_elem will be delayed
                    nb_delayed++;
                }
                else
                {
                    // Element cannot be inserted...
                    insert = false;
                    // ... And will be delayed
                    p_delay_elem = p_tbtt_entry;
                    nb_delayed = 1;

                    break;
                }
            }
            else
            {
                // If both value were the same, a conflict would have been detected
                if (p_tbtt_entry->time < p_elem->time)
                {
                    // p_tbtt_entry can be inserted before p_elem, so after p_prev_elem
                    break;
                }
                else
                {
                    // Loop again in order to check conflict with next element
                    p_prev_elem = p_elem;
                }
            }
        }
        else
        {
            if ((p_tbtt_entry->time < p_elem->time) ||
                 chan_tbtt_detect_conflict(p_tbtt_entry, p_elem))
            {
                // Element cannot be inserted...
                insert = false;
                // ... And will be delayed
                p_delay_elem = p_tbtt_entry;
                nb_delayed = 1;

                break;
            }
            else
            {
                p_prev_elem = p_elem;
            }
        }

        p_elem = (struct chan_tbtt_tag *)p_elem->list_hdr.next;
    }

    /*-----------------------------------------------------------------
     * PROCESS TBTT STRUCTURES THAT HAVE TO BE DELAYED
     *-----------------------------------------------------------------*/

    // Insert all delayed events
    while (nb_delayed--)
    {
        // Sanity checks
        ASSERT_ERR(p_delay_elem);

        if (p_delay_elem != p_tbtt_entry)
        {
            if (p_delay_elem->status == CHAN_TBTT_PROG)
            {
                // Stop TBTT Switch Timer
                mm_timer_clear(&chan_env.tmr_tbtt_switch);

                // Reset the status
                p_delay_elem->status = CHAN_TBTT_NOT_PROG;
            }

            // Try to extract element from the list of contexts
            co_list_extract(list_tbtt, &p_delay_elem->list_hdr);
        }

        // Add it in the list of delayed contexts
        co_list_push_back(&chan_env.list_tbtt_delay, &p_delay_elem->list_hdr);

        p_delay_elem = (struct chan_tbtt_tag *)p_delay_elem->list_hdr.next;
    }

    if (insert)
    {
        /*-----------------------------------------------------------------
         * INSERT THE TBTT STRUCTURE AT FOUND POSITION
         *-----------------------------------------------------------------*/
        co_list_insert_after(list_tbtt, &p_prev_elem->list_hdr, &p_tbtt_entry->list_hdr);
    }
}


/**
 ****************************************************************************************
 * @brief Callback called upon expiration of the TBTT switch timer.
 *        Jump on the appropriate channel.
 *
 * @param[in] env      TBTT entry linked with the channel on which we have to jump
 ****************************************************************************************
 */
static void chan_tbtt_switch_evt(void *env)
{
    // TBTT Switch Information structure
    struct chan_tbtt_tag *p_tbtt_entry = (struct chan_tbtt_tag *)env;
    // Get associated VIF Information Entry
    struct vif_info_tag *p_vif_entry = &vif_info_tab[p_tbtt_entry->vif_index];
    // Get associated channel context
    struct chan_ctxt_tag *p_chan_entry = p_vif_entry->chan_ctxt;
    // Get current time
    uint32_t current_time = ke_time();

    CHAN_DEBUG_PRINT(3, "chan_tbtt_switch_evt i=%d t=%d s=%d n=%d\n",
                           p_tbtt_entry->vif_index, p_tbtt_entry->time, chan_env.status, chan_env.nb_sched_ctxt);

    

    do
    {
        // Check if CDE is used
        if (chan_env.nb_sched_ctxt < 2)
        {
            break;
        }

        // Check if a RoC or a Scan procedure is in progress
        if (chan_env.status & (CO_BIT(CHAN_ENV_ROC_BIT) | CO_BIT(CHAN_ENV_SCAN_BIT)))
        {
            break;
        }

        // Check if a Channel Switch on another channel is already in progress
        if (chan_env.chan_switch && (chan_env.chan_switch != p_chan_entry))
        {
            break;
        }      

        #if (NX_P2P_GO)
        // Check if VIF is a P2P GO one
        if (p_vif_entry->p2p && (p_vif_entry->type == VIF_AP))
        {
            // Check if NOA has to be inserted/updated in the beacon
            chan_p2p_noa_manage(p_tbtt_entry);
        }
        #endif //(NX_P2P_GO)

        // Reset priority
        p_tbtt_entry->priority = 0;

        // Update number of remaining slots for current channel
        chan_upd_nb_rem_slots(chan_env.current_channel, current_time);

        // Update number of reserved slots
        p_chan_entry->nb_res_slots -= co_min(p_chan_entry->nb_res_slots, CHAN_MAX_TBTT_PRES_DUR);

        // Update CDE time
        chan_env.cde_time = current_time;

        // Update TBTT Switch status
        p_tbtt_entry->status = CHAN_TBTT_PRESENCE;

        // We are already switching on the channel
        if (!chan_env.chan_switch)
        {
            // Switch on TBTT channel
            chan_switch_start(p_chan_entry);
        }
    } while (0);
}

/**
 ****************************************************************************************
 * @brief Insert the provided TBTT entry in the list of TBTTs. TBTT are sorted
 *        by chronological order. The function takes care of possible collisions between
 *        the different TBTT presence.
 *
 * @param[in] p_tbtt_entry      TBTT entry to be inserted
 ****************************************************************************************
 */
static void chan_tbtt_schedule(struct chan_tbtt_tag *p_tbtt_entry)
{
    // First element of list_tbtt once the context has been inserted
    struct chan_tbtt_tag *first_elem;

    if (p_tbtt_entry)
    {
        #if (CHAN_DEBUG_TBTT_EN)
        first_elem = (struct chan_tbtt_tag *)co_list_pick(&chan_env.list_tbtt);

        while (first_elem)
        {
            first_elem = (struct chan_tbtt_tag *)(first_elem->list_hdr.next);
        }
        #endif //(CHAN_DEBUG_TBTT_EN)

        // Try to insert the element in the list of scheduled TBTTs
        chan_tbtt_insert(p_tbtt_entry);

        // Process delayed TBTT
        while (!co_list_is_empty(&chan_env.list_tbtt_delay))
        {
            struct chan_tbtt_tag *p_delay_elem
                = (struct chan_tbtt_tag *)co_list_pop_front(&chan_env.list_tbtt_delay);

            // Delay the TBTT
            chan_tbtt_delay(p_delay_elem);
            // Try to insert it back
            chan_tbtt_insert(p_delay_elem);
        }

        #if (CHAN_DEBUG_TBTT_EN)
        first_elem = (struct chan_tbtt_tag *)co_list_pick(&chan_env.list_tbtt);

        while (first_elem)
        {
            first_elem = (struct chan_tbtt_tag *)(first_elem->list_hdr.next);
        }
        #endif //(CHAN_DEBUG_TBTT_EN)
    }

    // Get first element of list of scheduled TBTTs
    first_elem = (struct chan_tbtt_tag *)co_list_pick(&chan_env.list_tbtt);

    // Check if a timer has/needs to be run
    if (first_elem && (first_elem->status == CHAN_TBTT_NOT_PROG))
    {
        // If switch is close enough in the future, start pre-switch
        if (hal_machw_time_cmp(first_elem->time, ke_time() + CHAN_MIN_TIMER_VALUE))
        {
            chan_tbtt_switch_evt(first_elem);
        }
        else
        {
            chan_env.tmr_tbtt_switch.env = first_elem;

            

            // Update status
            first_elem->status = CHAN_TBTT_PROG;

		    os_printf("t4:%d\r\n", first_elem->time);
            mm_timer_set(&chan_env.tmr_tbtt_switch, first_elem->time);
        }
    }
}

/**
 *****************************************************************************************
 * @brief Send a MM_FORCE_IDLE_REQ message to TASK_MM in order to require the HW to enter in
 *        IDLE mode
 ****************************************************************************************
 */
static void chan_send_force_idle(void)
{
    struct mm_force_idle_req *req = KE_MSG_ALLOC(MM_FORCE_IDLE_REQ,
                                                 TASK_MM, TASK_NONE,
                                                 mm_force_idle_req);

    // Set the request parameters
    req->cb = chan_goto_idle_cb;

    // Send the request
    ke_msg_send(req);
}

/**
 ****************************************************************************************
 * @brief Switch from chan_env.current_channel to chan_env.chan_switch.
 *        HW is supposed to be in IDLE mode when entering in this function.
 *        current_channel value is replaced by chan_switch value and chan_switch is set to
 *        NULL.
 ****************************************************************************************
 */
static void chan_switch_channel(void)
{
    struct chan_ctxt_tag *p_chan_entry = chan_env.chan_switch;
    struct mm_chan_ctxt_add_req *param = &p_chan_entry->channel;    

    //os_printf("chan_switch_channel i=%d\n", p_chan_entry->idx);

    // Program the RF with the new channel
    phy_set_channel(param->band, param->type, param->prim20_freq, param->center1_freq,
                    param->center2_freq, PHY_PRIM);
    tpc_update_tx_power(param->tx_power);

    // Set the basic rates in the HW according to the band
    nxmac_rates_set(mm_env.basic_rates[param->band]);

    // Indicate to the upper layers that a channel switch has occurred
    chan_send_switch_ind(p_chan_entry);

    // Store the current channel to the environment
    chan_env.current_channel = p_chan_entry;
    // Channel switch is now complete
    chan_env.chan_switch = NULL;

    // Update channel status
    chan_upd_ctxt_status(p_chan_entry, CHAN_WAITING_END);

    // Check if we switch to an operating channel (Traffic or RoC), or a scanning one
    if (p_chan_entry->idx != CHAN_SCAN_CTXT_IDX)
    {
        if (p_chan_entry->idx != CHAN_ROC_CTXT_IDX)
        {
            // Notify to the peer devices that we are back
            chan_notify_presence();
        }

        if (p_chan_entry->idx < CHAN_TRAF_CTXT_CNT)
        {
            // Go through list of used VIFs
            struct vif_info_tag *p_vif_entry = vif_mgmt_first_used();

            while (p_vif_entry)
            {
                // Check if VIF is on new channel
                if (p_vif_entry->chan_ctxt == p_chan_entry)
                {
                    #if (NX_TD)
                    // Keep in mind that we have been present on the traffic channel
                    td_env[p_vif_entry->index].has_active_chan = true;
                    #endif //(NX_TD)

                    // Try to send pending frames
                    vif_mgmt_send_postponed_frame(p_vif_entry);
                }

                // Get next VIF
                p_vif_entry = (struct vif_info_tag *)co_list_next(&p_vif_entry->list_hdr);
            }
        }
    }
    else
    {
        // Clear CCA busy register (see MM_CHANNEL_SURVEY_IND use)
        nxmac_edca_cca_busy_set(0);
		
		//os_printf("MM_SCAN_CHANNEL_START_IND\r\n");

        // Confirm the channel switch to the scanning module
        ke_msg_send_basic(MM_SCAN_CHANNEL_START_IND, TASK_SCAN, TASK_NONE);
    }

    #if (NX_POWERSAVE)
    if (p_chan_entry->idx >= CHAN_SCAN_CTXT_IDX)
    {
        GLOBAL_INT_DECLARATION();
			
        // Disable PS while we are scanning
        GLOBAL_INT_DISABLE();
        ps_env.prevent_sleep |= PS_SCAN_ONGOING;
        GLOBAL_INT_RESTORE();

        // No PM bit set in ProbeReq
        chan_env.pm = nxmac_pwr_mgt_getf();
        nxmac_pwr_mgt_setf(0);
    }
    #endif //(NX_POWERSAVE)

    // Go to active state
    mm_active();
}

/**
 ****************************************************************************************
 * @brief Initiate channel switch procedure in order to jump to indicated channel.
 *        If we already are present on provided channel or if a switch procedure is
 *        already in progress, nothing is done.
 *        Else requests the HW to go in IDLE state.
 *
 * @param[in] p_chan_entry  Channel on which switch is required
 ****************************************************************************************
 */
static void chan_switch_start(struct chan_ctxt_tag *p_chan_entry)
{
    //os_printf("chan_switch_start%d\r\n", p_chan_entry->idx);
    do
    {
        // Check if we already are on the specified channel
        if (chan_env.current_channel == p_chan_entry)
        {
            if ((chan_env.nb_sched_ctxt > 1) && (p_chan_entry->idx < CHAN_TRAF_CTXT_CNT))
            {
                // If CDE is used, update channel status
                chan_upd_ctxt_status(p_chan_entry, CHAN_WAITING_END);
            }

            break;
        }

        // Check if we are currently switching
        if (chan_env.chan_switch)
        {
            break;
        }        

        if (chan_env.current_channel)
        {
            // Indicate to the upper layers that a channel switch will occur soon
            chan_send_pre_switch_ind(chan_env.current_channel);
        }

        // Save the pointer to the ongoing channel switch
        chan_env.chan_switch = p_chan_entry;

        // Set Channel Context status
        chan_upd_ctxt_status(p_chan_entry, CHAN_GOTO_IDLE);

        // Send force_idle_req message
        chan_send_force_idle();
    } while(0);
}

/**
 ****************************************************************************************
 * @brief Go through the list of channel contexts in order to check if a channel contexts
 *        using exactly the same parameters than those provided by the host is
 *        currently used.
 *
 * @param[in] p_add_req  Parameters of the channel to be added
 * @param[in|out] idx    Pointer to the found channel context index
 *
 * @return true if a channel context with the same parameters has been found, else false
 ****************************************************************************************
 */
static bool chan_check_chan(struct mm_chan_ctxt_add_req const *p_add_req, uint8_t *p_idx)
{
	int chan_idx;
    bool found = false;

    // Go through the list of channel contexts
    for (chan_idx = 0; chan_idx < CHAN_TRAF_CTXT_CNT; chan_idx++)
    {
        struct chan_ctxt_tag *p_chan_entry = &chan_ctxt_pool[chan_idx];

        // Only consider added channel contexts
        if (p_chan_entry->idx != CHAN_CTXT_UNUSED)
        {
            // Compare received parameters with channel ones (exclude tx_power)
            if (!memcmp(p_add_req, &p_chan_entry->channel,
                        offsetof(struct mm_chan_ctxt_add_req, tx_power)))
            {
                // Return the found channel index
                *p_idx = chan_idx;
                // And escape the loop
                found = true;
                break;
            }
        }
    }

    return (found);
}

/**
 ****************************************************************************************
 * @brief Initialize all parameters stored in a channel context entry.
 *
 * @param[in] p_chan_entry  Channel context entry to be initialazed
 ****************************************************************************************
 */
static void chan_ctxt_init(struct chan_ctxt_tag *p_chan_entry)
{
    // Reset the channel information
    memset(p_chan_entry, 0, sizeof(struct chan_ctxt_tag));

    p_chan_entry->taskid = TASK_NONE;
    p_chan_entry->idx    = CHAN_CTXT_UNUSED;
}

/**
 * PUBLIC FUNCTIONS DEFINITION
 ****************************************************************************************
 */

void chan_init(void)
{
		int i;
	
    CHAN_DEBUG_PRINT(1, "chan_init\n");

    // Initialize the CHAN environment
    memset(&chan_env, 0, sizeof(chan_env));

    // Initialize the free channel context list
    for (i = 0; i < CHAN_CHAN_CTXT_CNT; i++)
    {
        struct chan_ctxt_tag *p_chan_entry = &chan_ctxt_pool[i];

        chan_ctxt_init(p_chan_entry);

        if (i < NX_CHAN_CTXT_CNT)
        {
            // Add it to the free channel context list
            co_list_push_back(&chan_env.list_free_ctxt, &p_chan_entry->list_hdr);
        }
        else if (i == CHAN_SCAN_CTXT_IDX)
        {
            p_chan_entry->channel.center2_freq = 0;
            p_chan_entry->channel.type         = PHY_CHNL_BW_20;
        }
    }

    // Set TBTT Switch Timer callback
    chan_env.tmr_tbtt_switch.cb = chan_tbtt_switch_evt;
    // Set Channel Distribution Event Timer callback and environment
    chan_env.tmr_cde.cb         = chan_cde_evt;
    chan_env.tmr_cde.env        = NULL;
    // Set Channel Switch Timer callback
    chan_env.tmr_ctxt_op.cb     = chan_ctxt_op_evt;
    // Set Connection Less Delay Time callback
    chan_env.tmr_conn_less.cb   = chan_conn_less_delay_evt;
}

#if (NX_HW_SCAN)
void chan_scan_req(uint8_t band, uint16_t freq, int8_t pwr, uint32_t duration_us,
                   uint8_t vif_index)
{
    // Get channel context used for Scan
    struct chan_ctxt_tag *p_scan_chan = &chan_ctxt_pool[CHAN_SCAN_CTXT_IDX];

    ASSERT_ERR(p_scan_chan->idx == CHAN_CTXT_UNUSED);

    p_scan_chan->idx                  = CHAN_SCAN_CTXT_IDX;
    p_scan_chan->taskid               = TASK_NONE;
    p_scan_chan->channel.band         = band;
    p_scan_chan->channel.center1_freq = freq;
    p_scan_chan->channel.prim20_freq  = freq;
    p_scan_chan->channel.tx_power     = pwr;
    p_scan_chan->vif_index            = vif_index;
    p_scan_chan->status               = CHAN_NOT_PROG;
    p_scan_chan->nb_rem_slots         = duration_us / CHAN_SLOT_DURATION_US;

    // Update status
    chan_env.status |= CO_BIT(CHAN_ENV_SCAN_WAIT_BIT);

    // Check if we can start the Connection Less Delay Timer
    if (!(chan_env.status & CO_BIT(CHAN_ENV_DELAY_PROG_BIT)))
    {
        chan_conn_less_delay_prog();
    }
}
#endif //(NX_HW_SCAN)

uint8_t chan_roc_req(struct mm_remain_on_channel_req const *req, ke_task_id_t taskid)
{
    // Returned status
    uint8_t status = CO_FAIL;
    // Get context allocated for the remain of channel operation
    struct chan_ctxt_tag *p_roc_chan = &chan_ctxt_pool[CHAN_ROC_CTXT_IDX];

    CHAN_DEBUG_PRINT(1, "chan_roc_req o=%d f=%d d=%d\n",
                            req->op_code, req->prim20_freq, req->duration_ms);

    // Check the operation code
    switch (req->op_code)
    {
        case (MM_ROC_OP_START):
        {
            // Check if a remain on channel is in progress
            if (p_roc_chan->idx != CHAN_CTXT_UNUSED)
            {
                break;
            }

            p_roc_chan->idx                  = CHAN_ROC_CTXT_IDX;
            p_roc_chan->channel.band         = req->band;
            p_roc_chan->channel.type         = req->type;
            p_roc_chan->channel.prim20_freq  = req->prim20_freq;
            p_roc_chan->channel.center1_freq = req->center1_freq;
            p_roc_chan->channel.center2_freq = req->center2_freq;
            p_roc_chan->taskid               = taskid;
            p_roc_chan->status               = CHAN_NOT_PROG;
            p_roc_chan->nb_rem_slots         = (req->duration_ms * 1000) / CHAN_SLOT_DURATION_US;
            p_roc_chan->vif_index            = req->vif_index;
            p_roc_chan->channel.tx_power     = req->tx_power;

            // If internal RoC (use for beacon detection), do not delay
            #if TDLS_ENABLE
            if ((taskid == TASK_MM) || (taskid == TASK_TDLS))
            #else
            if (taskid == TASK_MM)
            #endif
            {
                chan_env.status |= CO_BIT(CHAN_ENV_ROC_BIT);
                #if TDLS_ENABLE
                if (taskid == TASK_TDLS)
                {
                    p_roc_chan->roc_tdls = true;
                }
                #endif

                if (!chan_env.chan_switch)
                {
                    // Trigger switch procedure
                    chan_switch_start(p_roc_chan);
                }
            }
            else
            {
                // Update status
                chan_env.status |= CO_BIT(CHAN_ENV_ROC_WAIT_BIT);

                // Check if we can start the Connection Less Delay Timer
                if (!(chan_env.status & CO_BIT(CHAN_ENV_DELAY_PROG_BIT)))
                {
                    chan_conn_less_delay_prog();
                }
            }

            status = CO_OK;
        } break;

        case (MM_ROC_OP_CANCEL):
        {
            // Check if a remain on channel context is used
            if (p_roc_chan->idx == CHAN_CTXT_UNUSED)
            {
                break;
            }

            #if TDLS_ENABLE
            if (taskid == TASK_TDLS)
            {
                p_roc_chan->roc_tdls = false;
            }
            #endif

            // React depending on the current context state
            switch (p_roc_chan->status)
            {
                case (CHAN_NOT_PROG):
                {
                    // Clear wait RoC status bit
                    chan_env.status &= ~CO_BIT(CHAN_ENV_ROC_WAIT_BIT);
                    
                    #if TDLS_ENABLE
                    if (taskid == TASK_TDLS)
                    {
                        tdls_send_chan_switch_base_ind(p_roc_chan);
                    }
                    else
                    #endif
                    {
                        // Send RoC Expiration Indication
                        chan_send_roc_exp_ind(p_roc_chan);
                    }
                } break;

                case (CHAN_GOTO_IDLE):
                case (CHAN_WAIT_NOA_CFM):
                {
                    // Clear RoC in progress status bit
                    chan_env.status &= ~CO_BIT(CHAN_ENV_ROC_BIT);

                    chan_env.chan_switch = NULL;
                } break;

                case (CHAN_WAITING_END):
                {
                    // Clear channel context operation timer and...
                    mm_timer_clear(&chan_env.tmr_ctxt_op);
                    // ...Act as if the End of Operation timer had expired
                    chan_ctxt_op_evt((void *)p_roc_chan);
                } break;

                default:
                {
                } break;
            }

            // Reset the index
            p_roc_chan->idx = CHAN_CTXT_UNUSED;

            // Check if we have to stop the delay timer
            if (((chan_env.status & CO_BIT(CHAN_ENV_DELAY_PROG_BIT)) != 0) &&
                ((chan_env.status & CO_BIT(CHAN_ENV_SCAN_WAIT_BIT)) == 0))
            {
                chan_env.status &= ~CO_BIT(CHAN_ENV_DELAY_PROG_BIT);

                mm_timer_clear(&chan_env.tmr_conn_less);

                // Clear Delay timer status bit
                chan_env.status &= ~CO_BIT(CHAN_ENV_DELAY_PROG_BIT);
            }

            status = CO_OK;
        } break;

        default:
            break;
    }

    return (status);
}

uint8_t chan_ctxt_add(struct mm_chan_ctxt_add_req const *p_add_req, uint8_t *idx)
{
    // Returned status
    uint8_t status = CO_FAIL;

    CHAN_DEBUG_PRINT(1, "chan_ctxt_add\n");

    do
    {
        // Check if a channel with the provided parameters as already been added
        if (chan_check_chan(p_add_req, idx))
        {
            // Channel already exists, allocation is successful
            status = CO_OK;
            break;
        }
        else
        {
            // Allocate a channel context from the list
            struct chan_ctxt_tag *p_chan_entry
                        = (struct chan_ctxt_tag *)co_list_pop_front(&chan_env.list_free_ctxt);

            if (p_chan_entry == NULL)
            {
                break;
            }

            // Compute the index
            *idx = p_chan_entry->idx = CO_GET_INDEX(p_chan_entry, chan_ctxt_pool);

            // Initialize the operating channel context structure
            p_chan_entry->channel.band         = p_add_req->band;
            p_chan_entry->channel.type         = p_add_req->type;
            p_chan_entry->channel.center1_freq = p_add_req->center1_freq;
            p_chan_entry->channel.center2_freq = p_add_req->center2_freq;
            p_chan_entry->channel.prim20_freq  = p_add_req->prim20_freq;
            p_chan_entry->channel.tx_power     = p_add_req->tx_power;

            // Allocation is successful
            status = CO_OK;
        }
    } while (0);

    return (status);
}

void chan_ctxt_del(uint8_t chan_idx)
{
    // Retrieve the channel context
    struct chan_ctxt_tag *p_chan_entry = &chan_ctxt_pool[chan_idx];

    CHAN_DEBUG_PRINT(1, "chan_ctxt_del chan_idx=%d\n", chan_idx);

    // Sanity checks - An unused channel should not be freed
    ASSERT_ERR(p_chan_entry->idx != CHAN_CTXT_UNUSED);
    // Sanity checks - No more VIFs should be linked with the channel
    ASSERT_ERR(p_chan_entry->nb_linked_vif == 0);

    // Push back the channel context in the free list
    co_list_push_back(&chan_env.list_free_ctxt, &p_chan_entry->list_hdr);

    // Reset channel context information
    chan_ctxt_init(p_chan_entry);
}

void chan_ctxt_link(uint8_t vif_idx, uint8_t chan_idx)
{
    struct chan_ctxt_tag *p_chan_entry = &chan_ctxt_pool[chan_idx];
    struct vif_info_tag *p_vif_entry      = &vif_info_tab[vif_idx];

    CHAN_DEBUG_PRINT(1, "chan_ctxt_link vif_idx=%d, chan_idx=%d\n", vif_idx, chan_idx);

    // Sanity checks
    ASSERT_ERR(p_vif_entry->chan_ctxt == NULL);
    ASSERT_ERR(p_chan_entry->idx != CHAN_CTXT_UNUSED);

    // Channel context will now be linked to VIF
    p_vif_entry->chan_ctxt = p_chan_entry;
    // Increase number of VIFs linked with the channel context
    p_chan_entry->nb_linked_vif++;

    // Update the CDE duration
    chan_env.cde_dur_us += (CHAN_VIF_NB_SLOTS * CHAN_SLOT_DURATION_US);

    // Schedule the channel if first link
    if (p_chan_entry->nb_linked_vif == 1)
    {
        // Update channel status
        p_chan_entry->status = CHAN_NOT_PROG;

        // Increase number of scheduled contexts
        chan_env.nb_sched_ctxt++;

        // Insert the context in the list of scheduled contexts
        co_list_push_back(&chan_env.list_sched_ctxt, &p_chan_entry->list_hdr);

        // Check that no switch is pending
        if (!chan_env.chan_switch)
        {
            // Check that a RoC or a Scan is not in progress
            if ((chan_env.status & CHAN_ROC_SCAN_PENDING_MASK) == 0)
            {
                if (chan_env.nb_sched_ctxt == 1)
                {
                    // Start switching on channel
                    chan_switch_start(p_chan_entry);
                }
                else
                {
                    // Start a new Channel Distribution Event
                    chan_cde_evt(NULL);
                }
            }
        }
        else
        {
            // Cancel switch, a new channel will be chosen once pre-switch will be over
            chan_env.chan_switch->status = CHAN_NOT_PROG;
            // CDE will be restarted soon, cancel switch
            chan_env.chan_switch = NULL;
        }
    }

    #if (NX_P2P)
    // If VIF is a P2P VIF, schedule the channel
    if (p_vif_entry->p2p)
    {
        // Increase number of P2P VIF and number of P2P presences
        p_chan_entry->nb_p2p_presence++;
        p_chan_entry->nb_linked_p2p_vif++;
    }
    #endif //(NX_P2P)

    chan_update_tx_power(p_chan_entry);
}

void chan_ctxt_unlink(uint8_t vif_idx)
{
    struct vif_info_tag *p_vif_entry   = &vif_info_tab[vif_idx];
    struct chan_ctxt_tag *p_chan_entry = p_vif_entry->chan_ctxt;

    CHAN_DEBUG_PRINT(1, "chan_ctxt_unlink i=%d ci=%d s=%d nb=%d\n", vif_idx,  p_chan_entry->idx,
                           p_chan_entry->status, p_chan_entry->nb_linked_vif);

    // Sanity checks
    ASSERT_ERR(p_chan_entry != NULL);

    // Remove TBTT Switch element
    co_list_extract(&chan_env.list_tbtt, &p_vif_entry->tbtt_switch.list_hdr);
    // ... and reset status
    p_vif_entry->tbtt_switch.status = CHAN_TBTT_NOT_PROG;

    // Channel context will now be linked to VIF
    p_vif_entry->chan_ctxt = NULL;
    // Decrease number of VIFs linked with the channel context
    p_chan_entry->nb_linked_vif--;

    if (p_chan_entry->status != CHAN_NOT_SCHEDULED)
    {
        #if (NX_P2P)
        if (p_vif_entry->p2p)
        {
            // Get associated P2P entry
            struct p2p_info_tag *p_p2p_entry = &p2p_info_tab[p_vif_entry->p2p_index];

            if (p_p2p_entry->is_go_present)
            {
                // Decrease number of presence
                p_chan_entry->nb_p2p_presence--;
            }

            // Decrease number of P2P VIFs linked with the channel context
            p_chan_entry->nb_linked_p2p_vif--;
        }
        #endif //(NX_P2P)

        chan_env.cde_dur_us -= (CHAN_VIF_NB_SLOTS * CHAN_SLOT_DURATION_US);

        // If no more VIFs are linked with the channel, unschedule it
        if (!p_chan_entry->nb_linked_vif)
        {
            // Indicate if a switch is in progress
            bool switch_in_prog = (chan_env.chan_switch != NULL);
            // Indicate if we are switch on the channel to be unscheduled
            bool switch_unlk = (chan_env.chan_switch == p_chan_entry);

            // Remove the context from the list of channel contexts
            co_list_extract(&chan_env.list_sched_ctxt, &p_chan_entry->list_hdr);

            // Reset the status
            p_chan_entry->status = CHAN_NOT_SCHEDULED;

            // Decrease number of scheduled channels
            chan_env.nb_sched_ctxt--;

            #if (NX_P2P_GO)
            if (chan_env.nb_sched_ctxt == 1)
            {
                // CDE will be disabled stop all used NOA
                struct vif_info_tag *p_vif_noa_entry = vif_mgmt_first_used();

                while (p_vif_noa_entry)
                {
                    if (p_vif_noa_entry->p2p && (p_vif_noa_entry->type == VIF_AP))
                    {
                        p2p_go_noa_stop(p_vif_noa_entry, p_chan_entry->p2p_noa_idx, false);
                    }

                    p_vif_noa_entry = (struct vif_info_tag *)p_vif_noa_entry->list_hdr.next;
                }
            }
            #endif //(NX_P2P_GO)

            // Check if we are on the channel
            if (chan_env.current_channel == p_chan_entry)
            {
                chan_env.current_channel = NULL;
            }
            // Check if we are switching on the channel to be unscheduled
            else if (switch_unlk)
            {
                chan_env.chan_switch = NULL;
            }

            if (switch_in_prog)
            {
                /*
                 * If a switch procedure is in progress, act as if a switch timeout has occurred.
                 * Upon end of switch procedure, we will jump on another channel either through beginning of
                 * a new CDE or simply because only one channel will be available
                 */
                chan_env.status |= CO_BIT(CHAN_ENV_TIMEOUT_BIT);
            }
            else
            {
                // If CDE is used, restart CDE
                if (chan_env.nb_sched_ctxt >= 2)
                {
                    // Start a new channel distribution event
                    chan_cde_evt(NULL);
                }
                else if (chan_env.nb_sched_ctxt == 1)
                {
                    chan_switch_start((struct chan_ctxt_tag *)co_list_pick(&chan_env.list_sched_ctxt));
                }
            }
        }
    }

    // If no more VIF linked with the channel, delete the channel
    if (p_chan_entry->nb_linked_vif == 0)
    {
        chan_ctxt_del(p_chan_entry->idx);
    }

    // Schedule next TBTT Switch element
    chan_tbtt_schedule(NULL);

    chan_update_tx_power(p_chan_entry);
}

void chan_ctxt_update(struct mm_chan_ctxt_update_req const *p_upd_req)
{
    // Get channel context
    struct chan_ctxt_tag *p_chan_entry = &chan_ctxt_pool[p_upd_req->chan_index];
	
		GLOBAL_INT_DECLARATION();

    CHAN_DEBUG_PRINT(1, "chan_ctxt_update chan_idx=%d\n", p_upd_req->chan_index);

    // Update the channel context
    p_chan_entry->channel.band         = p_upd_req->band;
    p_chan_entry->channel.type         = p_upd_req->type;
    p_chan_entry->channel.center1_freq = p_upd_req->center1_freq;
    p_chan_entry->channel.center2_freq = p_upd_req->center2_freq;
    p_chan_entry->channel.prim20_freq  = p_upd_req->prim20_freq;
    p_chan_entry->channel.tx_power     = p_upd_req->tx_power;

    // Check if we are currently on this channel
    if (chan_env.current_channel == p_chan_entry)
    {
        // Handle the packets already in the RX queue to ensure that the
        // channel information indicated to the upper MAC is correct. This has to be done with
        // interrupts disabled, as the normal handling of the packets is done under interrupt
        GLOBAL_INT_DISABLE();
        rxl_timer_int_handler();
        rxl_cntrl_evt(0);
        GLOBAL_INT_RESTORE();
        
        // Program the RF with the new channel
        phy_set_channel(p_upd_req->band, p_upd_req->type, p_upd_req->prim20_freq,
                        p_upd_req->center1_freq, p_upd_req->center2_freq, PHY_PRIM);
        tpc_update_tx_power(p_chan_entry->channel.tx_power);
    }
}

void chan_tbtt_switch_update(struct vif_info_tag *p_vif_entry, uint32_t tbtt_time)
{
    // Get VIF's channel context
    struct chan_ctxt_tag *p_chan_entry = p_vif_entry->chan_ctxt;
    // TBTT Switch element
    struct chan_tbtt_tag *p_tbtt_entry = &p_vif_entry->tbtt_switch;

    do
    {
        // Verify that the Channel Context has been scheduled
        if (!p_chan_entry || (p_chan_entry->status == CHAN_NOT_SCHEDULED))
        {
            break;
        }

        // Verify that tbtt time has been modified
        if (p_tbtt_entry->time == (tbtt_time - CHAN_SWITCH_DELAY))
        {
            break;
        }

        // Update TBTT switch time based on new TBTT time
        p_tbtt_entry->time = tbtt_time - CHAN_SWITCH_DELAY;

        // Check that more than 1 channel contexts are currently used
        if (chan_env.nb_sched_ctxt < 2)
        {
            break;
        }

        CHAN_DEBUG_PRINT(3, "chan_tbtt_switch_update i=%d, s=%d, tbtt=%d, ct=%d\n",
                               p_vif_entry->index, p_tbtt_entry->status, tbtt_time, ke_time());

        // If TBTT Presence is in progress, TBTT Switch will be rescheduled at end of presence
        if (p_tbtt_entry->status != CHAN_TBTT_PRESENCE)
        {
            // Reset the status
            p_tbtt_entry->status = CHAN_TBTT_NOT_PROG;

            // Remove TBTT information from list of TBTTs
            co_list_extract(&chan_env.list_tbtt, &p_vif_entry->tbtt_switch.list_hdr);

            // Schedule the TBTT
            chan_tbtt_schedule(p_tbtt_entry);
        }
    } while (0);
}

void chan_bcn_to_evt(struct vif_info_tag *p_vif_entry)
{
    // TBTT Switch Information structure
    struct chan_tbtt_tag *p_tbtt_entry = &p_vif_entry->tbtt_switch;
    // Get current channel context
    struct chan_ctxt_tag *p_chan_entry = chan_env.current_channel;
    // Current time
    uint32_t current_time = ke_time();

    CHAN_DEBUG_PRINT(3, "chan_bcn_to_evt idx=%d s=%d\n", p_tbtt_entry->vif_index, chan_env.status);

    

    do
    {
        if (p_tbtt_entry->status != CHAN_TBTT_PRESENCE)
        {
            break;
        }

        // Update status
        p_tbtt_entry->status = CHAN_TBTT_NOT_PROG;

        // Remove TBTT information from list of TBTTs
        co_list_extract(&chan_env.list_tbtt, &p_tbtt_entry->list_hdr);

        // Check if CDE is used
        if (chan_env.nb_sched_ctxt < 2)
        {
            break;
        }

        // Reschedule the TBTT Switch event
        chan_tbtt_schedule(p_tbtt_entry);

        // Check if a RoC or a Scan procedure is in progress
        if (chan_env.status & (CO_BIT(CHAN_ENV_ROC_BIT) | CO_BIT(CHAN_ENV_SCAN_BIT)))
        {
            break;
        }

        // Check if a Channel Switch is still in progress
        if (chan_env.chan_switch)
        {
            break;
        }

        // Update number of remaining slots for current channel
        chan_upd_nb_rem_slots(p_chan_entry, current_time);

        // If number of reserved slots is not zero, it means than TBTT presence has started in previous NOA
        if (p_chan_entry->nb_res_slots)
        {
            p_chan_entry->nb_res_slots -= co_min(chan_get_nb_slots(current_time, chan_env.cde_time), p_chan_entry->nb_res_slots);
        }

        // Update CDE time
        chan_env.cde_time = current_time;

        // Get first channel context of the CDE
        p_chan_entry = chan_get_next_chan();

        if (chan_env.current_channel != p_chan_entry)
        {
            // Jump on channel if different than current chan
            chan_switch_start(p_chan_entry);
        }
    } while (0);
}

void chan_bcn_detect_start(struct vif_info_tag *p_vif_entry)
{
    // Get linked channel context
    struct chan_ctxt_tag *p_chan_entry = p_vif_entry->chan_ctxt;

    // Sanity check
    ASSERT_ERR(p_chan_entry);

    CHAN_DEBUG_PRINT(1, "chan_bcn_detect_start idx=%d\n", p_chan_entry->idx);

    // TODO [LT] - Beacon Detection status should be kept by VIF
    // TODO [LT] - Once beacon is received we could use Cancel Remain on Channel
    if (!(chan_env.status & CO_BIT(CHAN_ENV_BCN_DETECT_BIT)) &&
        (chan_env.nb_sched_ctxt > 1))
    {
        // Get peer AP information
        struct sta_info_tag *p_sta_entry = &sta_info_tab[p_vif_entry->u.sta.ap_id];

        // Start a remain on channel procedure in order to received all the beacons
        struct mm_remain_on_channel_req *p_req = KE_MSG_ALLOC(MM_REMAIN_ON_CHANNEL_REQ,
                                                              TASK_MM, TASK_MM,
                                                              mm_remain_on_channel_req);

        // Fill request parameters
        p_req->op_code   = MM_ROC_OP_START;
        p_req->vif_index = p_vif_entry->index;
        p_req->band      = p_chan_entry->channel.band;
        p_req->type      = p_chan_entry->channel.type;
        p_req->prim20_freq  = p_chan_entry->channel.prim20_freq;
        p_req->center1_freq = p_chan_entry->channel.center1_freq;
        p_req->center2_freq = p_chan_entry->channel.center2_freq;
        p_req->duration_ms  = (p_sta_entry->bcn_int - 5000) / 1000;
        p_req->tx_power  = p_chan_entry->channel.tx_power;

        ke_msg_send(p_req);

        // Set Beacon Detection bit in environment status
        chan_env.status |= CO_BIT(CHAN_ENV_BCN_DETECT_BIT);
    }
}


#if (NX_P2P)
void chan_p2p_absence_update(struct chan_ctxt_tag *p_chan_entry, bool absence)
{
    // Next channel context to be used
    struct chan_ctxt_tag *p_next_chan_entry;
    // Get current time
    uint32_t current_time = ke_time();

    CHAN_DEBUG_PRINT(3, "chan_p2p_absence_update i=%d a=%d\n", p_chan_entry->idx, absence);

    // Update number of presences on P2P channel
    if (absence)
    {
        p_chan_entry->nb_p2p_presence--;
    }
    else
    {
        p_chan_entry->nb_p2p_presence++;
    }

    do
    {
        // Check if CDE is used
        if (chan_env.nb_sched_ctxt < 2)
        {
            break;
        }

        // Check if a RoC or a Scan procedure is in progress
        if (chan_env.status & (CO_BIT(CHAN_ENV_ROC_BIT) | CO_BIT(CHAN_ENV_SCAN_BIT)))
        {
            break;
        }

        // Check if a Channel Switch is still in progress
        if (chan_env.chan_switch)
        {
            break;
        }

        // Check if current channel has P2P presences
        if (chan_env.current_channel->nb_p2p_presence)
        {
            // We can stay on the current channel
            break;
        }

        if (absence)
        {
            if (chan_env.current_channel != p_chan_entry)
            {
                // No need to leave the current channel
                break;
            }
            else
            {
                p_chan_entry->status = CHAN_PRESENT;
            }
        }

        // Get a new channel
        p_next_chan_entry = chan_get_next_chan();

        if (p_next_chan_entry != chan_env.current_channel)
        {
            // Update number of remaining slots for current channel
            chan_upd_nb_rem_slots(chan_env.current_channel, current_time);

            // Update CDE time
            chan_env.cde_time = current_time;

            // Jump on channel
            chan_switch_start(p_next_chan_entry);
        }
    } while (0);
}
#endif //(NX_P2P)

bool chan_is_on_channel(struct vif_info_tag *p_vif_entry)
{
    bool is_on_channel = false;

    if (chan_env.current_channel)
    {
        if (chan_env.current_channel->idx < CHAN_SCAN_CTXT_IDX)
        {
            is_on_channel = (p_vif_entry->chan_ctxt == chan_env.current_channel);
        }
        else
        {
            is_on_channel = (chan_env.current_channel->vif_index == p_vif_entry->index);
        }
    }

    return (is_on_channel);
}

bool chan_is_tx_allowed(struct vif_info_tag *p_vif_entry)
{
    if ((!chan_is_on_channel(p_vif_entry)) ||
        (chan_env.chan_switch &&
         (chan_env.current_channel->status != CHAN_SENDING_NOA)))
    {
        return false;
    }

    return true;
}

void chan_update_tx_power(struct chan_ctxt_tag *p_chan_entry)
{
    int8_t i, min_pwr = VIF_UNDEF_POWER;

    if (p_chan_entry->nb_linked_vif == 0)
        return;

    for (i = 0 ; i < NX_VIRT_DEV_MAX; i++) {
        struct vif_info_tag *p_vif_entry = &vif_info_tab[i];

        if (p_vif_entry->chan_ctxt == p_chan_entry)
        {
            if (p_vif_entry->user_tx_power < min_pwr)
                min_pwr = p_vif_entry->user_tx_power;

            if (p_vif_entry->tx_power < min_pwr)
                min_pwr = p_vif_entry->tx_power;
        }
    }

    if (min_pwr != VIF_UNDEF_POWER)
    {
        p_chan_entry->channel.tx_power = min_pwr;
    }
}

#endif //(NX_CHNL_CTXT)

/// @} end of group
