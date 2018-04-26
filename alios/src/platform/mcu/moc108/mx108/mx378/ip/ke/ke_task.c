/**
 ****************************************************************************************
 *
 * @file ke_task.c
 *
 * @brief This file contains the implementation of the kernel task management.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */
/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "include.h"
#include "co_int.h"
#include "co_bool.h"
#include "arch.h"

#include "ke_config.h"
#include "ke_task.h"
#include "ke_env.h"
#include "ke_queue.h"
#include "ke_event.h"
#include "mem_pub.h"
#include "mm_task.h"
#include "scan_task.h"

#if (TDLS_ENABLE)
#include "tdls_task.h"
#endif

#include "scanu_task.h"
#include "me_task.h"
#include "sm_task.h"
#include "bam_task.h"
#include "apm_task.h"
#include "mesh_task.h"

/// Table grouping the task descriptors
static const struct ke_task_desc TASK_DESC[TASK_MAX] =
{
    // ** LMAC tasks  **
    /// MM task
    {mm_state_handler, &mm_default_handler, mm_state, MM_STATE_MAX, MM_IDX_MAX},                     //[                     TASK_MM] =                                                    

#if NX_HW_SCAN
    /// SCAN task
    {NULL, &scan_default_handler, scan_state, SCAN_STATE_MAX, SCAN_IDX_MAX},                         //[                     TASK_SCAN] =                                                  
#endif

#if (TDLS_ENABLE)
    {NULL, &tdls_default_handler, tdls_state, TDLS_STATE_MAX, TDLS_IDX_MAX}, 
#endif

    /// SCAN task
    {scanu_state_handler, &scanu_default_handler, scanu_state, SCANU_STATE_MAX, SCANU_IDX_MAX},      //[                     TASK_SCANU] =                                                 
    /// ME task
    {NULL, &me_default_handler, me_state, ME_STATE_MAX, ME_IDX_MAX},                                 //[                     TASK_ME]  =                                                   
    /// SM task
    {NULL, &sm_default_handler, sm_state, SM_STATE_MAX, SM_IDX_MAX},                                 //[                     TASK_SM]  =                                                   

#if NX_BEACONING
    {NULL, &apm_default_handler, apm_state, APM_STATE_MAX, APM_IDX_MAX},
#endif
	
	/// BAM task
    {NULL, &bam_default_handler, bam_state, BAM_STATE_MAX, BAM_IDX_MAX},                             //[                     TASK_BAM] =                                                   

#if (RW_MESH_EN)
    /// MESH task
    {NULL, &mesh_default_handler, mesh_state, MESH_STATE_MAX, MESH_IDX_MAX},
#endif //(RW_MESH_EN)
};

/**
 * @brief Compare destination task callback
 */
static bool cmp_dest_id(struct co_list_hdr const * msg, uint32_t dest_id)
{
    return ((struct ke_msg*)msg)->dest_id == dest_id;
}

/**
 ****************************************************************************************
 * @brief Reactivation of saved messages.
 *
 * This primitive looks for all the messages destined to the task ke_task_id that
 * have been saved and inserts them into the sent priority queue. These
 * messages will be scheduled at the next scheduler pass.
 *
 * @param[in] ke_task_id    Destination Identifier
 ****************************************************************************************
 */
static void ke_task_saved_update(ke_task_id_t const ke_task_id)
{
    struct ke_msg * msg;
    
    GLOBAL_INT_DECLARATION();

    for(;;)
    {
        // if the state has changed look in the Save queue if a message
        // need to be handled
        msg = (struct ke_msg*) ke_queue_extract(&ke_env.queue_saved,
                                                &cmp_dest_id,
                                                (uint32_t) ke_task_id);

        if (msg == NULL) break;

        //printf ("-- saved found %x %x\n", ke_task_id, msg->id);

        // Insert it back in the sent queue
        GLOBAL_INT_DISABLE();
        ke_queue_push(&ke_env.queue_sent, (struct co_list_hdr*)msg);
        GLOBAL_INT_RESTORE();

        // trigger the event
        ke_evt_set(KE_EVT_KE_MESSAGE_BIT);
    }

    return;
}


/**
 ****************************************************************************************
 * @brief Set the state of the task identified by its Task Id.
 *
 * In this function we also handle the SAVE service: when a task state changes we
 * try to activate all the messages currently saved in the save queue for the given
 * task identifier.
 *
 * @param[in]  id       Identifier of the task instance whose state is going to be modified
 * @param[in]  state_id New State
 ****************************************************************************************
 */
void ke_state_set(ke_task_id_t const id,
                  ke_state_t const state_id)
{
    ke_state_t *ke_stateid_ptr;
    int idx = KE_IDX_GET(id);
    int type = KE_TYPE_GET(id);

    ASSERT_ERR(type < TASK_MAX);
    ASSERT_ERR(ke_task_local(type));
    ASSERT_ERR(idx < TASK_DESC[type].idx_max);

    ke_stateid_ptr = &TASK_DESC[type].state[idx];

    ASSERT_ERR(ke_stateid_ptr);

    if (*ke_stateid_ptr != state_id)
    {
        *ke_stateid_ptr = state_id;

        // if the state has changed update the SAVE queue
        ke_task_saved_update(id);
    }
}


/**
 ****************************************************************************************
 * @brief Retrieve the state of a task.
 *
 * @param[in]  id   Task id.
 *
 * @return          Current state of the task
 ****************************************************************************************
 */
ke_state_t ke_state_get(ke_task_id_t const id)
{
    int idx = KE_IDX_GET(id);
    int type = KE_TYPE_GET(id);

    ASSERT_ERR(type < TASK_MAX);
    ASSERT_ERR(ke_task_local(type));
    ASSERT_ERR(idx < TASK_DESC[type].idx_max);

    // Get the state
    return TASK_DESC[type].state[idx];
}


/**
 ****************************************************************************************
 * @brief Search message handler function matching the msg id
 *
 * @param[in] msg_id        Message identifier
 * @param[in] state_handler Pointer to the state handler
 *
 * @return                  Pointer to the message handler (NULL if not found)
 *
 ****************************************************************************************
 */
static ke_msg_func_t ke_handler_search(ke_msg_id_t const msg_id,
                                       struct ke_state_handler const *state_handler)
{
    int i;
    
    // Get the message handler function by parsing the message table
    for (i = (state_handler->msg_cnt-1); 0 <= i; i--)
    {
        if (state_handler->msg_table[i].id == msg_id)
        {
            // If handler is NULL, message should not have been received in this state
            ASSERT_ERR(state_handler->msg_table[i].func);

            return state_handler->msg_table[i].func;
        }
    }

    // If we execute this line of code, it means that we did not find the handler
    return NULL;
}

/**
 ****************************************************************************************
 * @brief Retrieve appropriate message handler function of a task
 *
 * @param[in]  msg_id   Message identifier
 * @param[in]  task_id  Task instance identifier
 *
 * @return              Pointer to the message handler (NULL if not found)
 *
 ****************************************************************************************
 */
static ke_msg_func_t ke_task_handler_get(ke_msg_id_t const msg_id,
                                         ke_task_id_t const task_id)
{
	const struct ke_task_desc *desc;
	ke_msg_func_t func = NULL;
	int type = KE_TYPE_GET(task_id);
	int idx = KE_IDX_GET(task_id);

	ASSERT_ERR(type < TASK_MAX);
	ASSERT_ERR(ke_task_local(type));
	ASSERT_ERR(idx < TASK_DESC[type].idx_max);

    desc = TASK_DESC + type;

    ASSERT_ERR(desc);

    // Retrieve a pointer to the task instance data
    if (desc->state_handler)
    {
        func = ke_handler_search(msg_id, desc->state_handler + desc->state[idx]);
    }

    // No handler... need to retrieve the default one
    if (func == NULL && desc->default_handler)
    {
        func = ke_handler_search(msg_id, desc->default_handler);
    }

    return func;
}


/**
 ****************************************************************************************
 * @brief Task scheduler entry point.
 *
 * This function is the scheduler of messages. It tries to get a message
 * from the sent queue, then try to get the appropriate message handler
 * function (from the current state, or the default one). This function
 * is called, then the message is saved or freed.
 *
 * @param[in] dummy Parameter not used but required to follow the kernel event callback
 * format
 ****************************************************************************************
 */
void ke_task_schedule(int dummy)
{    
    ke_msg_func_t func;
    GLOBAL_INT_DECLARATION();
    
    // Process one message at a time to ensure that events having higher priority are
    // handled in time
    do
    {
        int msg_status;
        struct ke_msg *msg;
    
        // Get a message from the queue
        GLOBAL_INT_DISABLE();
        msg = (struct ke_msg*) ke_queue_pop(&ke_env.queue_sent);
        GLOBAL_INT_RESTORE();
        
        if (msg == NULL) 
        {
            break;
        }

        // Retrieve a pointer to the task instance data
        func = ke_task_handler_get(msg->id, msg->dest_id);

        // sanity check
        ASSERT_WARN(func != NULL);

        // Call the message handler
        if (func != NULL)
        {
            msg_status = func(msg->id, ke_msg2param(msg), msg->dest_id, msg->src_id);
        }
        else
        {
            msg_status = KE_MSG_CONSUMED;
        }

        switch (msg_status)
        {
        case KE_MSG_CONSUMED:
            ke_msg_free(msg);

        case KE_MSG_NO_FREE:
            break;

        case KE_MSG_SAVED:
            // The message has been saved
            // Insert it at the end of the save queue
            ke_queue_push(&ke_env.queue_saved, (struct co_list_hdr*) msg);
            break;

        default:
            ASSERT_ERR(0);
        } // switch case
    } while(0);

    // Verify if we can clear the event bit
    GLOBAL_INT_DISABLE();
    if (co_list_is_empty(&ke_env.queue_sent))
        ke_evt_clear(KE_EVT_KE_MESSAGE_BIT);
    GLOBAL_INT_RESTORE();
}

/**
 ****************************************************************************************
 * @brief Generic message handler to consume message without handling it in the task.
 *
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return KE_MSG_CONSUMED
 ****************************************************************************************
 */
int ke_msg_discard(ke_msg_id_t const msgid,
                   void const *param,
                   ke_task_id_t const dest_id,
                   ke_task_id_t const src_id)
{
    return KE_MSG_CONSUMED;
}

/**
 ****************************************************************************************
 * @brief Generic message handler to consume message without handling it in the task.
 *
 * @param[in] msgid Id of the message received (probably unused)
 * @param[in] param Pointer to the parameters of the message.
 * @param[in] dest_id TaskId of the receiving task.
 * @param[in] src_id TaskId of the sending task.
 * @return KE_MSG_CONSUMED
 ****************************************************************************************
 */
int ke_msg_save(ke_msg_id_t const msgid,
                void const *param,
                ke_task_id_t const dest_id,
                ke_task_id_t const src_id)
{
    return KE_MSG_SAVED;
}

/// @} TASK
