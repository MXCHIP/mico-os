/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>

#ifdef CONFIG_NET_LWIP
#include "lwip/pbuf.h"
#else
#include "utilities/mem/pbuf.h"
#endif

#include "umesh_utils.h"

static ur_message_stats_t g_message_stats;

message_t *message_alloc(uint16_t length, uint16_t debug_info)
{
    message_t *message;
    uint16_t offset = 0;
    data_t *data;

    if (g_message_stats.num >= MESSAGE_BUF_SIZE) {
        g_message_stats.queue_fulls++;
        return NULL;
    }
    message = ur_mem_alloc(sizeof(message_t));
    if (message == NULL) {
        g_message_stats.mem_fails++;
        return NULL;
    }
    message->data = (data_t *)pbuf_alloc(PBUF_RAW, length, PBUF_POOL);
    if (message->data == NULL) {
        ur_mem_free(message, sizeof(message_t));
        g_message_stats.pbuf_fails++;
        return NULL;
    }
    data = message->data;
    while (data) {
        offset += data->len;
        memset(data->payload, 0, data->len);
        if (offset < length) {
            data = data->next;
        } else {
            break;
        }
    }

    message->info = ur_mem_alloc(sizeof(message_info_t));
    if (message->info == NULL) {
        pbuf_free((struct pbuf *)message->data);
        ur_mem_free(message, sizeof(message_t));
        g_message_stats.mem_fails++;
        return NULL;
    }
    memset(message->info, 0, sizeof(message_info_t));
    message->frag_offset = 0;
    message->retries = 0;
    message->tot_len = length;

    if (debug_info >= MSG_DEBUG_INFO_SIZE) {
        debug_info = UT_MSG;
    }
    message->debug_info = debug_info;
    g_message_stats.debug_info[debug_info]++;

    g_message_stats.num++;
    g_message_stats.size += length;
    return message;
}

ur_error_t message_free(message_t *message)
{
    if (message != NULL) {
        g_message_stats.num--;
        g_message_stats.size -= message->tot_len;
        g_message_stats.debug_info[message->debug_info]--;

        pbuf_free((struct pbuf *)message->data);

        if (message->info) {
            ur_mem_free(message->info, sizeof(message_info_t));
        }
        ur_mem_free(message, sizeof(message_t));
    }
    return UR_ERROR_NONE;
}

ur_error_t message_copy_to(const message_t *src, uint16_t src_offset,
                           uint8_t *dest, uint16_t dest_length)
{
    uint16_t length;

    if (src == NULL || dest == NULL) {
        return UR_ERROR_FAIL;
    }

    length = pbuf_copy_partial((struct pbuf *)src->data, dest, dest_length,
                               src_offset);

    if (length != dest_length) {
        return UR_ERROR_FAIL;
    }

    return UR_ERROR_NONE;
}

ur_error_t message_copy_from(const message_t *dest,
                             uint8_t *src, uint16_t src_length)
{
    pbuf_take((struct pbuf *)dest->data, src, src_length);
    return UR_ERROR_NONE;
}

ur_error_t message_copy(message_t *dest, const message_t *src)
{
    pbuf_copy(dest->data, (data_t *)src->data);
    if (src->info && dest->info) {
        memcpy(dest->info, src->info, sizeof(message_info_t));
    }
    dest->frag_offset = src->frag_offset;
    return UR_ERROR_NONE;
}

ur_error_t message_set_payload_offset(const message_t *message, int16_t size)
{
    if (message == NULL) {
        return UR_ERROR_FAIL;
    }

    if (pbuf_header((struct pbuf *)message->data, size)) {
        return UR_ERROR_FAIL;
    }
    return UR_ERROR_NONE;
}

uint8_t *message_get_payload(const message_t *message)
{
    if (message == NULL) {
        return NULL;
    }

    return (uint8_t *)(((struct pbuf *)message->data)->payload);
}

uint16_t message_get_msglen(const message_t *message)
{
    if (message == NULL) {
        return 0;
    }

    return ((struct pbuf *)message->data)->tot_len;
}

ur_error_t message_set_msglen(const message_t *message, uint16_t length)
{
    if (message == NULL) {
        return UR_ERROR_FAIL;
    }

    ((struct pbuf *)message->data)->tot_len = length;
    return UR_ERROR_NONE;
}

uint16_t message_get_buflen(const message_t *message)
{
    if (message == NULL) {
        return 0;
    }

    return ((struct pbuf *)message->data)->len;
}

ur_error_t message_set_buflen(const message_t *message, uint16_t length)
{
    if (message == NULL) {
        return UR_ERROR_FAIL;
    }

    ((struct pbuf *)message->data)->len = length;
    return UR_ERROR_NONE;
}

ur_error_t message_concatenate(message_t *dest, message_t *message,
                               bool reference)
{
    if (dest == NULL || message == NULL) {
        return UR_ERROR_FAIL;
    }

    pbuf_cat((struct pbuf *)dest->data, (struct pbuf *)message->data);

    if (reference) {
        pbuf_ref(message->data);
    }
    dest->tot_len += message->tot_len;

    g_message_stats.debug_info[message->debug_info]--;

    ur_mem_free(message->info, sizeof(message_info_t));
    ur_mem_free(message, sizeof(message_t));
    g_message_stats.num--;
    return UR_ERROR_NONE;
}

message_t *message_queue_get_head(message_queue_t *queue)
{
    if (dlist_empty(queue)) {
        return NULL;
    }
    return dlist_first_entry(queue, message_t, next);
}

ur_error_t message_queue_enqueue(message_queue_t *queue, message_t *message)
{
    dlist_add_tail(&message->next, queue);
    return UR_ERROR_NONE;
}

ur_error_t message_queue_dequeue(message_t *message)
{
    dlist_del(&message->next);
    return UR_ERROR_NONE;
}

uint16_t message_queue_get_size(message_queue_t *queue)
{
    return dlist_entry_number(queue);
}

void umesh_message_init(void)
{
    memset(&g_message_stats, 0, sizeof(g_message_stats));
}

const ur_message_stats_t *message_get_stats(void)
{
    return &g_message_stats;
}
