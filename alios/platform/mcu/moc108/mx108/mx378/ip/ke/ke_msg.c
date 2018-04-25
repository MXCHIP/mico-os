/**
 ****************************************************************************************
 *
 * @file ke_msg.c
 *
 * @brief This file contains the scheduler primitives called to create or delete
 * a task. It contains also the scheduler itself.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup KE_MSG
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include <stddef.h>
#include "co_int.h"
#include "co_bool.h"
#include "arch.h"

#include "rwnx_config.h"

#include "ke_config.h"
#include "ke_queue.h"
#include "ke_msg.h"
#include "ke_task.h"
#include "mem_pub.h"
#include "ke_event.h"
#include "ke_env.h"

#include "include.h"
#include "arm_arch.h"
#include "uart_pub.h"
#include "rwnx.h"

/**
 ****************************************************************************************
 * @brief Allocate memory for a message
 *
 * This primitive allocates memory for a message that has to be sent. The memory
 * is allocated dynamically on the heap and the length of the variable parameter
 * structure has to be provided in order to allocate the correct size.
 *
 * Several additional parameters are provided which will be preset in the message
 * and which may be used internally to choose the kind of memory to allocate.
 *
 * The memory allocated will be automatically freed by the kernel, after the
 * pointer has been sent to ke_msg_send(). If the message is not sent, it must
 * be freed explicitly with ke_msg_free().
 *
 * Allocation failure is considered critical and should not happen.
 *
 * @param[in] id        Message identifier
 * @param[in] dest_id   Destination Task Identifier
 * @param[in] src_id    Source Task Identifier
 * @param[in] param_len Size of the message parameters to be allocated
 *
 * @return Pointer to the parameter member of the ke_msg. If the parameter
 *         structure is empty, the pointer will point to the end of the message
 *         and should not be used (except to retrieve the message pointer or to
 *         send the message)
 ****************************************************************************************
 */
void *ke_msg_alloc(ke_msg_id_t const id,
                   ke_task_id_t const dest_id,
                   ke_task_id_t const src_id,
                   uint16_t const param_len)
{
    struct ke_msg *msg = (struct ke_msg*) os_malloc(sizeof(struct ke_msg) +
                                                    param_len - sizeof (uint32_t));
    void *param_ptr = NULL;

    ASSERT_ERR(msg != NULL);
    msg->hdr.next = NULL;
    msg->id = id;
    msg->dest_id = dest_id;
    msg->src_id = src_id;
    msg->param_len = param_len;

    param_ptr = ke_msg2param(msg);

    memset(param_ptr, 0, param_len);

    return param_ptr;
}


/**
 ****************************************************************************************
 * @brief Message sending.
 *
 * Send a message previously allocated with any ke_msg_alloc()-like functions.
 *
 * The kernel will take care of freeing the message memory.
 *
 * Once the function have been called, it is not possible to access its data
 * anymore as the kernel may have copied the message and freed the original
 * memory.
 *
 * @param[in] param_ptr  Pointer to the parameter member of the message that
 *                       should be sent.
 ****************************************************************************************
 */
void ke_msg_send(void const *param_ptr)
{
    struct ke_msg * msg = ke_param2msg(param_ptr);
    int type = KE_TYPE_GET(msg->dest_id);

    if (ke_task_local(type))
    {
        ke_queue_push(&ke_env.queue_sent, &msg->hdr);

        ke_evt_set(KE_EVT_KE_MESSAGE_BIT);
    }
    else
    {
        if(g_rwnx_connector.msg_outbound_func)
        {
            (*g_rwnx_connector.msg_outbound_func)(msg);
        }
    }
}

/**
 ****************************************************************************************
 * @brief Basic message sending.
 *
 * Send a message that has a zero length parameter member. No allocation is
 * required as it will be done internally.
 *
 * @param[in] id        Message identifier
 * @param[in] dest_id   Destination Identifier
 * @param[in] src_id    Source Identifier
 ****************************************************************************************
 */
void ke_msg_send_basic(ke_msg_id_t const id,
                       ke_task_id_t const dest_id,
                       ke_task_id_t const src_id)
{
    void *no_param = ke_msg_alloc(id, dest_id, src_id, 0);
    ke_msg_send(no_param);
}

/**
 ****************************************************************************************
 * @brief Message forwarding.
 *
 * Forward a message to another task by changing its destination and source tasks IDs.
 *
 * @param[in] param_ptr  Pointer to the parameter member of the message that
 *                       should be sent.
 * @param[in] dest_id New destination task of the message.
 * @param[in] src_id New source task of the message.
 ****************************************************************************************
 */
void ke_msg_forward(void const *param_ptr,
                    ke_task_id_t const dest_id,
                    ke_task_id_t const src_id)

{
    struct ke_msg * msg = ke_param2msg(param_ptr);

    msg->dest_id = dest_id;
    msg->src_id = src_id;

    ke_msg_send(param_ptr);
}

/**
 ****************************************************************************************
 * @brief Message forwarding.
 *
 * Forward a message to another task by changing its message, destination and source tasks IDs.
 *
 * @param[in] param_ptr  Pointer to the parameter member of the message that
 *                       should be sent.
 * @param[in] msg_id New Id of the message.
 * @param[in] dest_id New destination task of the message.
 * @param[in] src_id New source task of the message.
 ****************************************************************************************
 */
void ke_msg_forward_and_change_id(void const *param_ptr,
                                  ke_msg_id_t const msg_id,
                                  ke_task_id_t const dest_id,
                                  ke_task_id_t const src_id)
{
    struct ke_msg * msg;

	msg = ke_param2msg(param_ptr);
    msg->id = msg_id;
    msg->dest_id = dest_id;
    msg->src_id = src_id;

    ke_msg_send(param_ptr);
}

/**
 ****************************************************************************************
 * @brief Free allocated message
 *
 * @param[in] msg   Pointer to the message to be freed (not the parameter member!)
 ****************************************************************************************
 */
void ke_msg_free(struct ke_msg const *msg)
{
    if(msg)
    {
    	os_free( (void*) msg);
    }
}

/// @} KE_MSG
