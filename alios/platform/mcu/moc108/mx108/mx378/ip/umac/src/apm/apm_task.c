/**
****************************************************************************************
*
* @file apm_task.c
*
* @brief The UMAC's AP Manager (APM) module implementation.
*
* Copyright (C) RivieraWaves 2011-2016
*
****************************************************************************************
*/

#include "apm.h"
#include "apm_task.h"
#include "me_utils.h"
#include "bam.h"
#include "ke_timer.h"
#include "vif_mgmt.h"
#include "chan.h"
#include "me_task.h"
#include "me.h"
#include "ps.h"
#include "tpc.h"

#if NX_BEACONING
/**
 * @addtogroup TASK_APM
 * @{
 */

/**
****************************************************************************************
* @brief APM module message handler.
* This function handles the message APM_AP_START_REQ from SME.
* This message requests the UMAC to start an AP in a BSS network upon issue of
* NXAPI_AP_START_REQ API.
* @param[in] msgid Id of the message received (probably unused)
* @param[in] param Pointer to the parameters of the message.
* @param[in] dest_id TaskId of the receiving task.
* @param[in] src_id TaskId of the sending task.
* @return Whether the message was consumed or not.
****************************************************************************************
*/
static int
apm_start_req_handler(ke_msg_id_t const msgid,
                         struct apm_start_req *param,
                         ke_task_id_t const dest_id,
                         ke_task_id_t const src_id)
{

    struct apm_start_cfm *cfm;
    uint8_t status;
    // Channel context index
    uint8_t chan_idx;

    do
    {
        struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];

        // Check if the VIF is configured as AP
        if (vif->type != VIF_AP)
        {
            status = CO_BAD_PARAM;
            break;
        }

        // Check if we are busy or not
        if (ke_state_get(TASK_APM) != APM_IDLE)
        {
            status = CO_BUSY;
            break;
        }

        // Check if the AP is not already started
        if (vif->active)
        {
            status = CO_OP_IN_PROGRESS;
            break;
        }

        // Sanity check - We should not have a channel context already registered
        ASSERT_ERR(vif->chan_ctxt == NULL);

        // Save the parameters
        apm_env.param = param;

        if (me_add_chan_ctx(&chan_idx, &param->chan, param->center_freq1,
                            param->center_freq2, param->ch_width) == CO_OK)
        {
            struct mac_bss_info *bss = &vif->bss_info;
            int8_t pwr;
            uint8_t idx;

            // Save the BW and channel information in the VIF
            bss->chan = me_freq_to_chan_ptr(param->chan.band, param->chan.freq);
            bss->chan->tx_power = param->chan.tx_power;
            bss->center_freq1 = param->center_freq1;
            bss->center_freq2 = param->center_freq2;
            bss->bw = (param->ch_width == PHY_CHNL_BW_80P80)?BW_160MHZ:param->ch_width;
            bss->phy_bw = param->ch_width;
            bss->power_constraint = 0;

            // Link the VIF to the channel context
            chan_ctxt_link(param->vif_idx, chan_idx);

            // Set the BSS parameters to LMAC
            apm_set_bss_param();

            pwr = bss->chan->tx_power;
            tpc_update_vif_tx_power(vif, &pwr, &idx);
        }
        else
        {
            status = CO_FAIL;
            break;
        }

        // We will now proceed to the AP starting
        return (KE_MSG_NO_FREE);
    } while(0);

    cfm = KE_MSG_ALLOC(APM_START_CFM, src_id, dest_id, apm_start_cfm);
    cfm->status = status;
    cfm->vif_idx = param->vif_idx;

    // Send the message
    ke_msg_send(cfm);

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles scan confirm from the scan module.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_set_ps_disable_cfm_handler(ke_msg_id_t const msgid,
                              void const *param,
                              ke_task_id_t const dest_id,
                              ke_task_id_t const src_id)
{
    // Sanity check - This message can be received only in the CHAN CTXT ADDING state
    ASSERT_ERR((ke_state_get(TASK_APM) == APM_BSS_PARAM_SETTING) ||
               (ke_state_get(TASK_APM) == APM_IDLE));

    // Check the state
    if (ke_state_get(TASK_APM) == APM_BSS_PARAM_SETTING)
    {
        // Send the next BSS parameter configuration message
        apm_send_next_bss_param();
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles scan confirm from the scan module.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_bss_param_setting_handler(ke_msg_id_t const msgid,
                             void const *param,
                             ke_task_id_t const dest_id,
                             ke_task_id_t const src_id)
{
    // Sanity check - This message can be received only in the CHAN CTXT ADDING state
    ASSERT_ERR(ke_state_get(TASK_APM) == APM_BSS_PARAM_SETTING);

    // Send the next BSS parameter configuration message
    apm_send_next_bss_param();

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles scan confirm from the scan module.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
me_set_active_cfm_handler(ke_msg_id_t const msgid,
                          void const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    // Sanity check - This message can be received only in the STA ADDING state
    ASSERT_ERR((ke_state_get(TASK_APM) == APM_BSS_PARAM_SETTING) ||
               (ke_state_get(TASK_APM) == APM_IDLE));

    // Check the state
    if (ke_state_get(TASK_APM) == APM_BSS_PARAM_SETTING)
    {
        // Sanity check - All the BSS configuration parameters shall be set
        ASSERT_ERR(co_list_is_empty(&apm_env.bss_config));

        // Set the beacon information to the LMAC
        apm_bcn_set();
    }

    return (KE_MSG_CONSUMED);
}

/**
 ****************************************************************************************
 * @brief SM module message handler.
 * This function handles scan confirm from the scan module.
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return Whether the message was consumed or not.
 ****************************************************************************************
 */
static int
mm_bcn_change_cfm_handler(ke_msg_id_t const msgid,
                          void const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    // Sanity check - This message can be received only in the APM_SCHEDULING_CHAN_CTX state
    ASSERT_ERR(ke_state_get(TASK_APM) == APM_BCN_SETTING);

    // Set the beacon information to the LMAC
    apm_start_cfm(CO_OK);

    return (KE_MSG_CONSUMED);
}

/**
****************************************************************************************
* @brief APM module message handler.
* This function handles the message APM_AP_START_REQ from SME.
* This message requests the UMAC to start an AP in a BSS network upon issue of
* NXAPI_AP_START_REQ API.
* @param[in] msgid Id of the message received (probably unused)
* @param[in] param Pointer to the parameters of the message.
* @param[in] dest_id TaskId of the receiving task.
* @param[in] src_id TaskId of the sending task.
* @return Whether the message was consumed or not.
****************************************************************************************
*/
static int
apm_stop_req_handler(ke_msg_id_t const msgid,
                     struct apm_stop_req const *param,
                     ke_task_id_t const dest_id,
                     ke_task_id_t const src_id)
{

    do
    {
        struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];

        // Check if the VIF is configured as AP
        if (vif->type != VIF_AP)
            break;

        // Check if the AP is not already started
        if (!vif->active)
            break;

        // Check if we are busy or not
        if (ke_state_get(TASK_APM) != APM_IDLE)
            return KE_MSG_SAVED;

        // Stop the AP
        apm_stop(vif);

        // Send the confirmation
        ke_msg_send_basic(APM_STOP_CFM, src_id, dest_id);

        // We will now proceed to the AP starting
        return (KE_MSG_CONSUMED);
    } while(0);

    // Send the confirmation
    ke_msg_send_basic(APM_STOP_CFM, src_id, dest_id);

    return (KE_MSG_CONSUMED);
}

/**
****************************************************************************************
* @brief APM module message handler.
* This function handles the message APM_AP_START_CAC_REQ from SME.
* This message requests the UMAC to start listening on the specified channel
* until APM_AP_STOP_CAC_REQ is received.
* CAC (Channel Availability Check) is done to check for radar before starting
* an AP on a DFS channel
*
* @param[in] msgid Id of the message received (probably unused)
* @param[in] param Pointer to the parameters of the message.
* @param[in] dest_id TaskId of the receiving task.
* @param[in] src_id TaskId of the sending task.
* @return Whether the message was consumed or not.
****************************************************************************************
*/
static int
apm_start_cac_req_handler(ke_msg_id_t const msgid,
                          struct apm_start_cac_req const *param,
                          ke_task_id_t const dest_id,
                          ke_task_id_t const src_id)
{
    struct apm_start_cac_cfm *cfm;
    uint8_t status = CO_OK;
    uint8_t chan_idx;

    GLOBAL_INT_DECLARATION();

    do
    {
        struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];
        struct mm_chan_ctxt_add_req req;

        // Check if the VIF is configured as AP
        if (vif->type != VIF_AP) {
            status = CO_BAD_PARAM;
            break;
        }

        // Check if the AP is not already started
        if (vif->active) {
            status = CO_BUSY;
            break;
        }

        // Check if we are busy or not
        if (ke_state_get(TASK_APM) != APM_IDLE) {
            status = CO_BUSY;
            break;
        }

        // create new channel context
        req.band         = param->chan.band;
        req.type         = param->ch_width;
        req.prim20_freq  = param->chan.freq;
        req.center1_freq = param->center_freq1;
        req.center2_freq = param->center_freq2;

        if (chan_ctxt_add(&req, &chan_idx) == CO_OK) {
            // and Link the VIF to the channel context
            chan_ctxt_link(param->vif_idx, chan_idx);
        } else {
            status = CO_FAIL;
            break;
        }

        #if (NX_POWERSAVE)
        // Disable PS during the whole CAC period
        GLOBAL_INT_DISABLE();
        ps_env.prevent_sleep |= PS_CAC_STARTED;
        GLOBAL_INT_RESTORE();
        #endif


    } while(0);

    // Send the confirmation
    cfm = KE_MSG_ALLOC(APM_START_CAC_CFM, src_id, dest_id, apm_start_cac_cfm);
    cfm->status = status;
    cfm->ch_idx = chan_idx;

    // Send the message
    ke_msg_send(cfm);

    return (KE_MSG_CONSUMED);
}

/**
****************************************************************************************
* @brief APM module message handler.
* This function handles the message APM_AP_STOP_CAC_REQ from SME.
* This message requests the UMAC to stop listening of the specified channel.
* CAC (Channel Availability Check) ends if a radar is detected or after a defined period
*
* @param[in] msgid Id of the message received (probably unused)
* @param[in] param Pointer to the parameters of the message.
* @param[in] dest_id TaskId of the receiving task.
* @param[in] src_id TaskId of the sending task.
* @return Whether the message was consumed or not.
****************************************************************************************
*/
static int
apm_stop_cac_req_handler(ke_msg_id_t const msgid,
                         struct apm_stop_cac_req const *param,
                         ke_task_id_t const dest_id,
                         ke_task_id_t const src_id)
{
		GLOBAL_INT_DECLARATION();
	
    do
    {
        struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];

        // Check if the VIF is configured as AP
        if (vif->type != VIF_AP) {
            break;
        }

        // Check if the AP is not already started
        if (vif->active) {
            break;
        }

        // Check if we are busy or not
        if (ke_state_get(TASK_APM) != APM_IDLE) {
            break;
        }

        chan_ctxt_unlink(param->vif_idx);

        #if (NX_POWERSAVE)
        //  Power Save can be used
        GLOBAL_INT_DISABLE();
        ps_env.prevent_sleep &= ~PS_CAC_STARTED;
        GLOBAL_INT_RESTORE();
        #endif

    } while(0);

    // Send the confirmation
    ke_msg_send_basic(APM_STOP_CAC_CFM, src_id, dest_id);

    return (KE_MSG_CONSUMED);
}


/// DEFAULT handler definition.
const struct ke_msg_handler apm_default_state[] =
{
    {APM_START_REQ, (ke_msg_func_t)apm_start_req_handler},
    {APM_STOP_REQ, (ke_msg_func_t)apm_stop_req_handler},
    {ME_SET_ACTIVE_CFM, (ke_msg_func_t)me_set_active_cfm_handler},
    {MM_BCN_CHANGE_CFM, (ke_msg_func_t)mm_bcn_change_cfm_handler},
    {MM_SET_BSSID_CFM, mm_bss_param_setting_handler},
    {MM_SET_BEACON_INT_CFM, mm_bss_param_setting_handler},
    {ME_SET_PS_DISABLE_CFM, (ke_msg_func_t)me_set_ps_disable_cfm_handler},
    {MM_SET_VIF_STATE_CFM, ke_msg_discard},
    {APM_START_CAC_REQ, (ke_msg_func_t)apm_start_cac_req_handler},
    {APM_STOP_CAC_REQ, (ke_msg_func_t)apm_stop_cac_req_handler},
};

/// Specifies the message handlers that are common to all states.
const struct ke_state_handler apm_default_handler =
    KE_STATE_HANDLER(apm_default_state);

/// Defines the placeholder for the states of all the task instances.
ke_state_t apm_state[APM_IDX_MAX];

/// @} end of addtogroup

#endif

