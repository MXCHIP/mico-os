/**
 ****************************************************************************************
 * @file td.c
 *
 * @brief Traffic Detection (TD) Module
 *
 * Copyright (C) RivieraWaves 2015-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup TD
 * @{
 ****************************************************************************************
 */

/**
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "td.h"

#if (NX_TD)

#include "ke_timer.h"

#if (NX_DPSM)
#include "ps.h"
#endif //(NX_DPSM)
#if (NX_CHNL_CTXT)
#include "chan.h"
#include "vif_mgmt.h"
#endif //(NX_CHNL_CTXT)
#if (NX_P2P_GO)
#include "p2p.h"
#endif //(NX_P2P_GO)

/**
 * DEBUG (DEFINES, MACROS, ...)
 ****************************************************************************************
 */

/**
 * Debug Configuration for TD Module
 *      0 - Traces are disabled
 *      1.. - Level of verbosity
 */
#define TD_DEBUG_TRACES_EN    (0)

#if (TD_DEBUG_TRACES_EN)
/// Function used to print module information
#define TD_DEBUG_PRINT(lvl, format, ...)                            \
    do {                                                            \
        if (lvl <= TD_DEBUG_TRACES_EN)                              \
        {                                                           \
        }                                                           \
    } while (0);
#else
#define TD_DEBUG_PRINT(lvl, format, ...)
#endif //(TD_DEBUG_TRACES_EN)

/**
 * MACROS
 ****************************************************************************************
 */

/// Return the VIF index of a Traffic Detection environment
#define TD_GET_VIF_INDEX(p_td_env)                                  \
    ((p_td_env - &td_env[0]) / sizeof(struct td_env_tag))

/**
 * PRIVATE VARIABLES DECLARATION
 ****************************************************************************************
 */

struct td_env_tag td_env[NX_VIRT_DEV_MAX];
#if (NX_TD_STA)
/// List of TD Environment Entries used for per-STA traffic detection
struct td_sta_env_tag td_sta_env[NX_REMOTE_STA_MAX];
#endif //(NX_TD_STA)

/**
 * PRIVATE FUNCTIONS DECLARATION
 ****************************************************************************************
 */

#if (NX_TD_STA)
/**
 ****************************************************************************************
 * @brief
 ****************************************************************************************
 */
static void td_update_sta_status(uint8_t sta_idx)
{
    // Get TD STA environment
    struct td_sta_env_tag *p_tds_env = &td_sta_env[sta_idx];
    // New status
    uint8_t new_status = 0;

    if (p_tds_env->pck_cnt_tx >= TD_DEFAULT_PCK_NB_THRES)
    {
        new_status |= CO_BIT(TD_STATUS_TX);
    }

    if (p_tds_env->pck_cnt_rx >= TD_DEFAULT_PCK_NB_THRES)
    {
        new_status |= CO_BIT(TD_STATUS_RX);
    }

    // Update the status
    p_tds_env->status = new_status;

    // Reset counters
    p_tds_env->pck_cnt_tx = 0;
    p_tds_env->pck_cnt_rx = 0;
}
#endif //(NX_TD_STA)

/**
 ****************************************************************************************
 * @brief
 ****************************************************************************************
 */
static void td_timer_end(void *env)
{
    struct td_env_tag *p_td_env = (struct td_env_tag *)env;
    uint32_t current_time = ke_time();
    uint8_t new_status = 0;

    #if (NX_CHNL_CTXT)
    // Check if VIF's channel has been scheduled during last TD interval
    if (p_td_env->has_active_chan)
    #endif //(NX_CHNL_CTXT)
    {
        #if (NX_TD_STA)
        // Get VIF Entry
        struct vif_info_tag *p_vif_entry = &vif_info_tab[p_td_env->vif_index];
        // STA Entry
        struct sta_info_tag *p_sta_entry;
        #endif //(NX_TD_STA)

        // Check if traffic has been detected on TX path
        if (p_td_env->pck_cnt_tx >= TD_DEFAULT_PCK_NB_THRES)
        {
            new_status |= CO_BIT(TD_STATUS_TX);
        }

        // Check if traffic has been detected on RX path
        if (p_td_env->pck_cnt_rx >= TD_DEFAULT_PCK_NB_THRES)
        {
            new_status |= CO_BIT(TD_STATUS_RX);
        }

        #if (NX_DPSM)
        // Check if traffic has been detected on TX path after PS filtering
        if (p_td_env->pck_cnt_tx_ps >= TD_DEFAULT_PCK_NB_THRES)
        {
            new_status |= CO_BIT(TD_STATUS_TX_PS);
        }

        // Check if traffic has been detected on RX path after PS filtering
        if (p_td_env->pck_cnt_rx_ps >= TD_DEFAULT_PCK_NB_THRES)
        {
            new_status |= CO_BIT(TD_STATUS_RX_PS);
        }

        // If PS traffic status has been modified, notify PS module
        if ((p_td_env->status ^ new_status) & (CO_BIT(TD_STATUS_TX_PS) | CO_BIT(TD_STATUS_RX_PS)))
        {
            ps_traffic_status_update(p_td_env->vif_index,
                                     (new_status & (CO_BIT(TD_STATUS_TX_PS) | CO_BIT(TD_STATUS_RX_PS))));
        }
        #endif //(NX_DPSM)

        #if (NX_P2P_GO)
        // If traffic status has been modified, notify P2P module
        if ((p_td_env->status ^ new_status) & (CO_BIT(TD_STATUS_TX) | CO_BIT(TD_STATUS_RX)))
        {
            // Notify P2P module
            p2p_go_td_evt(p_td_env->vif_index, new_status & (CO_BIT(TD_STATUS_TX) | CO_BIT(TD_STATUS_RX)));
        }
        #endif //(NX_P2P_GO)

        // Store status
        p_td_env->status = new_status;

        #if (NX_TD_STA)
        p_sta_entry = (struct sta_info_tag *)co_list_pick(&p_vif_entry->sta_list);

        while (p_sta_entry)
        {
            // Update STA traffic status
            td_update_sta_status(p_sta_entry->staid);

            // Get next STA entry
            p_sta_entry = (struct sta_info_tag *)p_sta_entry->list_hdr.next;
        }
        #endif //(NX_TD_STA)
    }

    // Reset packets counters
    p_td_env->pck_cnt_tx = 0;
    p_td_env->pck_cnt_rx = 0;
    #if (NX_DPSM)
    p_td_env->pck_cnt_tx_ps = 0;
    p_td_env->pck_cnt_rx_ps = 0;
    #endif //(NX_DPSM)

    #if (NX_CHNL_CTXT)
    if (chan_env.current_channel == vif_info_tab[p_td_env->vif_index].chan_ctxt)
    {
        p_td_env->has_active_chan = true;
    }
    else
    {
        p_td_env->has_active_chan = false;
    }
    #endif //(NX_CHNL_CTXT)

    // Reprogram timer
    mm_timer_set(&p_td_env->td_timer, current_time + TD_DEFAULT_INTV_US);
}

/**
 * PUBLIC FUNCTIONS
 ****************************************************************************************
 */
void td_init(void)
{
    uint8_t counter;

    TD_DEBUG_PRINT(1, D_CRT "td_init\n");

    // Reset all TD environments
    for (counter = 0; counter < NX_VIRT_DEV_MAX; counter++)
    {
        td_reset(counter);
    }

    #if (NX_TD_STA)
    // Reset all TD environments used for per-STA Traffic Detection
    for (counter = 0; counter < NX_REMOTE_STA_MAX; counter++)
    {
        td_sta_reset(counter);
    }
    #endif //(NX_TD_STA)
}

void td_reset(uint8_t vif_index)
{
    // Get TD environment
    struct td_env_tag *p_td_env = &td_env[vif_index];

    TD_DEBUG_PRINT(1, D_CRT "td_reset idx=%d\n", vif_index);

    if (p_td_env->is_on)
    {
        // Stop TD timer
        mm_timer_clear(&p_td_env->td_timer);
    }

    // Initialize memory
    memset(p_td_env, 0, sizeof(struct td_env_tag));

    // Set timer information
    p_td_env->td_timer.cb  = td_timer_end;
    p_td_env->td_timer.env = p_td_env;

    // Store VIF Index
    p_td_env->vif_index = vif_index;
}

#if (NX_TD_STA)
void td_sta_reset(uint8_t sta_index)
{
    // Get TD environment for provided STA
    struct td_sta_env_tag *p_tds_env = &td_sta_env[sta_index];

    TD_DEBUG_PRINT(1, D_CRT "td_sta_reset idx=%d\n", sta_index);

    // Initialize content of environment
    p_tds_env->pck_cnt_tx = 0;
    p_tds_env->pck_cnt_rx = 0;
    p_tds_env->status = 0;
}
#endif //(NX_TD_STA)

void td_start(uint8_t vif_index)
{
    // Get TD environment
    struct td_env_tag *p_td_env = &td_env[vif_index];

    // Check if Traffic Detected is not already on
    if (!p_td_env->is_on)
    {
        // Get current time
        uint32_t current_time = ke_time();

        TD_DEBUG_PRINT(1, D_CRT "td_start idx=%d\n", vif_index);

        // Keep in mind that the TD timer is on
        p_td_env->is_on = true;

        // Program end of TD interval
        mm_timer_set(&p_td_env->td_timer, current_time + TD_DEFAULT_INTV_US);
    }
}

void td_pck_ind(uint8_t vif_index, uint8_t sta_index, bool rx)
{
    // Get TD environment
    struct td_env_tag *p_td_env = &td_env[vif_index];
    #if (NX_TD_STA)
    // Get TD STA environment
    struct td_sta_env_tag *p_tds_env = (sta_index < NX_REMOTE_STA_MAX) ? &td_sta_env[sta_index] : NULL;
    #endif //(NX_TD_STA)

    if (rx)
    {       
        // Increase number of RX packets
        p_td_env->pck_cnt_rx++;

        #if (NX_TD_STA)
        // Sanity check
        ASSERT_ERR(p_tds_env);

        if (p_tds_env->pck_cnt_rx < TD_DEFAULT_PCK_NB_THRES)
        {
            p_tds_env->pck_cnt_rx++;
        }
        #endif //(NX_TD_STA)
    }
    else
    {       
        // Increase number of TX packets
        p_td_env->pck_cnt_tx++;

        #if (NX_TD_STA)
        if (p_tds_env && (p_tds_env->pck_cnt_tx < TD_DEFAULT_PCK_NB_THRES))
        {
            p_tds_env->pck_cnt_tx++;
        }
        #endif //(NX_TD_STA)
    }
}

#if (NX_DPSM)
void td_pck_ps_ind(uint8_t vif_index, bool rx)
{
    TD_DEBUG_PRINT(3, D_CRT "td_pck_ps_ind idx=%d, rx=%d\n", vif_index, rx);

    if (rx)
    {     
        // Increase number of RX packets
        td_env[vif_index].pck_cnt_rx_ps++;
    }
    else
    {
        // Increase number of RX packets
        td_env[vif_index].pck_cnt_tx_ps++;
    }
}
#endif //(NX_DPSM)

#endif //(NX_TD)

/// @} end of group
