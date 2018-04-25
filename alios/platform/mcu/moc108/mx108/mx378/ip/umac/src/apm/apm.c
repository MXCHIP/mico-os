/**
****************************************************************************************
*
* @file apm.c
*
* Copyright (C) RivieraWaves 2011-2016
*
* @brief Definition of the APM module environment.
*
****************************************************************************************
*/

#include "apm.h"
#include "apm_task.h"
#include "mac_frame.h"
#include "vif_mgmt.h"
#include "me_utils.h"
#include "me_task.h"

#if NX_BEACONING

/** @addtogroup APM
* @{
*/

/// Definition of the global environment.
struct apm apm_env;

/*
* FUNCTION IMPLEMENTATIONS
****************************************************************************************
*/

/**
 ****************************************************************************************
 * @brief Push a BSS configuration message to the list.
 *
 * @param[in] param  Pointer to the message parameters
 ****************************************************************************************
 */
static void apm_bss_config_push(void *param)
{
    struct ke_msg *msg = ke_param2msg(param);

    co_list_push_back(&apm_env.bss_config, &msg->hdr);
}

void apm_init(void)
{
    // Reset the environment
    memset(&apm_env, 0, sizeof(apm_env));
    apm_env.aging_sta_idx = 0;

    // Set state as Idle
    ke_state_set(TASK_APM, APM_IDLE);
}

void apm_start_cfm(uint8_t status)
{
    struct apm_start_req const *param = apm_env.param;
    struct apm_start_cfm *cfm = KE_MSG_ALLOC(APM_START_CFM, TASK_API, TASK_APM, apm_start_cfm);

    // Check if the status is OK
    if (status == CO_OK)
    {
        struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];
        struct sta_info_tag *sta = &sta_info_tab[VIF_TO_BCMC_IDX(param->vif_idx)];
        struct mm_set_vif_state_req *req = KE_MSG_ALLOC(MM_SET_VIF_STATE_REQ, TASK_MM,
                                                        TASK_APM, mm_set_vif_state_req);

        // Fill the message parameters
        req->active = true;
        req->inst_nbr = vif->index;

        // Send the message to the task
        ke_msg_send(req);

        vif->flags = param->flags;
        vif->u.ap.ctrl_port_ethertype = param->ctrl_port_ethertype;
        vif->u.ap.ps_sta_cnt = 0;
        cfm->ch_idx = vif->chan_ctxt->idx;
        cfm->bcmc_idx = VIF_TO_BCMC_IDX(param->vif_idx);

        // Copy the rate set to the station entry
        sta->info.rate_set = param->basic_rates;
        // Open the control port for this station
        sta->ctrl_port_state = PORT_OPEN;

        // Initialize the policy table
        me_init_bcmc_rate(sta);

        // Initialize tx power on first transmit
        sta->pol_tbl.upd_field |= CO_BIT(STA_MGMT_POL_UPD_TX_POWER);

        // Initialize MAC address with muticast bit set
        sta->mac_addr.array[0] = 0x1;
    }

    // Fill-in the message parameters
    cfm->status = status;
    cfm->vif_idx = param->vif_idx;

    // Send the message
    ke_msg_send(cfm);

    // Free the parameters
    ke_msg_free(ke_param2msg(param));
    apm_env.param = NULL;

    // We are now waiting for the channel addition confirmation
    ke_state_set(TASK_APM, APM_IDLE);
}


void apm_set_bss_param(void)
{
    struct apm_start_req const *param = apm_env.param;
    struct vif_info_tag *vif = &vif_info_tab[param->vif_idx];
    struct me_set_ps_disable_req *ps = KE_MSG_ALLOC(ME_SET_PS_DISABLE_REQ, TASK_ME, TASK_APM,
                                                    me_set_ps_disable_req);
    struct mm_set_bssid_req *bssid = KE_MSG_ALLOC(MM_SET_BSSID_REQ, TASK_MM, TASK_APM,
                                                  mm_set_bssid_req);
    struct mm_set_beacon_int_req *bint = KE_MSG_ALLOC(MM_SET_BEACON_INT_REQ, TASK_MM,
                                                      TASK_APM, mm_set_beacon_int_req);
    struct me_set_active_req *active =  KE_MSG_ALLOC(ME_SET_ACTIVE_REQ, TASK_ME,
                                                   TASK_APM, me_set_active_req);

    // Disable PS mode when an AP is started
    ps->ps_disable = true;
    ps->vif_idx = vif->index;
    apm_bss_config_push(ps);

    // BSSID
    bssid->bssid = vif->mac_addr;
    bssid->inst_nbr = param->vif_idx;
    apm_bss_config_push(bssid);

    // Beacon interval
    bint->beacon_int = param->bcn_int;
    bint->inst_nbr = param->vif_idx;
    apm_bss_config_push(bint);

    // Go back to ACTIVE after setting the BSS parameters
    active->active = true;
    active->vif_idx = param->vif_idx;
    apm_bss_config_push(active);

    // Send the first BSS configuration message
    apm_send_next_bss_param();

    // We are now waiting for the channel addition confirmation
    ke_state_set(TASK_APM, APM_BSS_PARAM_SETTING);
}

void apm_send_next_bss_param(void)
{
    struct ke_msg *msg = (struct ke_msg *)co_list_pop_front(&apm_env.bss_config);

    // Sanity check - We shall have a message available
    ASSERT_ERR(msg != NULL);

    // Send the message
    ke_msg_send(ke_msg2param(msg));
}

void apm_bcn_set(void)
{
    struct apm_start_req const *param = apm_env.param;
    struct mm_bcn_change_req *bcn =  KE_MSG_ALLOC(MM_BCN_CHANGE_REQ, TASK_MM,
                                                  TASK_APM, mm_bcn_change_req);
    // Fill-in the parameters
    bcn->bcn_ptr = param->bcn_addr;
    bcn->bcn_len = param->bcn_len;
    bcn->tim_oft = param->tim_oft;
    bcn->tim_len = param->tim_len;
    bcn->inst_nbr = param->vif_idx;

    // Send the beacon information to the LMAC
    ke_msg_send(bcn);

    ke_state_set(TASK_APM, APM_BCN_SETTING);
}

void apm_stop(struct vif_info_tag *vif)
{
    struct me_set_ps_disable_req *ps = KE_MSG_ALLOC(ME_SET_PS_DISABLE_REQ, TASK_ME, TASK_APM,
                                                    me_set_ps_disable_req);
    struct me_set_active_req *idle =  KE_MSG_ALLOC(ME_SET_ACTIVE_REQ, TASK_ME,
                                                    TASK_APM, me_set_active_req);

    // Re-allow PS mode in case it was disallowed
    ps->ps_disable = false;
    ps->vif_idx = vif->index;
    ke_msg_send(ps);

    // VIF state
    if (vif->active)
    {
        struct mm_set_vif_state_req *state = KE_MSG_ALLOC(MM_SET_VIF_STATE_REQ, TASK_MM,
                                                          TASK_APM, mm_set_vif_state_req);
        state->active = false;
        state->inst_nbr = vif->index;
        ke_msg_send(state);
    }

    // Unlink channel context
    if (vif->chan_ctxt != NULL)
    {
        // Unlink the VIF from the channel context
        chan_ctxt_unlink(vif->index);
    }

    idle->active = false;
    idle->vif_idx = vif->index;
    ke_msg_send(idle);
}
/// @}

#endif

