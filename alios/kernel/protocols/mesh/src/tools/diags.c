/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>

#include "umesh_utils.h"
#include "core/mesh_mgmt_tlvs.h"
#include "core/mesh_mgmt.h"
#include "core/address_mgmt.h"
#include "hal/interfaces.h"
#include "tools/diags.h"

extern void response_append(const char *format, ...);

static ur_error_t send_trace_route_response(network_context_t *network,
                                            ur_addr_t *dest,
                                            uint32_t timestamp);

static ur_error_t handle_trace_route_request(message_t *message)
{
    ur_error_t error = UR_ERROR_NONE;
    uint8_t *tlvs;
    uint16_t tlvs_length;
    mm_timestamp_tv_t *timestamp;
    message_info_t *info;
    network_context_t *network;

    if (umesh_mm_get_device_state() < DEVICE_STATE_DETACHED) {
        return UR_ERROR_NONE;
    }

    MESH_LOG_DEBUG("handle trace route request");

    info = message->info;
    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t) -
                  info->payload_offset;
    tlvs = ur_mem_alloc(tlvs_length);
    if (tlvs == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, sizeof(mm_header_t) + info->payload_offset,
                    tlvs, tlvs_length);

    timestamp = (mm_timestamp_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length,
                                                     TYPE_TIMESTAMP);
    network = get_network_context_by_meshnetid(info->src.netid, true);
    error = send_trace_route_response(network, &info->src, timestamp->timestamp);
    ur_mem_free(tlvs, tlvs_length);
    return error;
}

static ur_error_t handle_trace_route_response(message_t *message)
{
    uint8_t *tlvs;
    uint16_t tlvs_length;
    mm_timestamp_tv_t *timestamp;
    uint32_t time;
    message_info_t *info;

    if (umesh_mm_get_device_state() < DEVICE_STATE_DETACHED) {
        return UR_ERROR_NONE;
    }

    MESH_LOG_DEBUG("handle trace route response");

    info = message->info;
    tlvs_length = message_get_msglen(message) - sizeof(mm_header_t) -
                  info->payload_offset;
    tlvs = ur_mem_alloc(tlvs_length);
    if (tlvs == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, sizeof(mm_header_t) + info->payload_offset,
                    tlvs, tlvs_length);

    timestamp = (mm_timestamp_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length,
                                                     TYPE_TIMESTAMP);
    time = umesh_now_ms() - timestamp->timestamp;
    info = message->info;

    ur_mem_free(tlvs, tlvs_length);
    response_append("%04x:%04x, time %d ms\r\n",
                    info->src.netid, info->src.addr.short_addr, time);
    return UR_ERROR_NONE;
}

ur_error_t send_trace_route_request(network_context_t *network,
                                    ur_addr_t *dest)
{
    ur_error_t error = UR_ERROR_NONE;
    message_t *message;
    mm_timestamp_tv_t *timestamp;
    message_info_t *info;
    uint8_t *data;
    uint8_t *data_orig;
    uint16_t length;

    length = sizeof(mm_header_t) + sizeof(mm_timestamp_tv_t);
    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);

    timestamp = (mm_timestamp_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)timestamp, TYPE_TIMESTAMP);
    timestamp->timestamp = umesh_now_ms();
    data += sizeof(mm_timestamp_tv_t);

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_TRACE_ROUTE_REQUEST,
                               data_orig, length, DIAGS_1);
    if (message == NULL) {
        ur_mem_free(data_orig, length);
        return UR_ERROR_MEM;
    }

    info = message->info;
    info->network = network;
    memcpy(&info->dest, dest, sizeof(info->dest));

    error = address_resolve(message);
    if (error == UR_ERROR_NONE) {
        error = mf_send_message(message);
    } else if (error == UR_ERROR_DROP) {
        message_free(message);
    }
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send trace route request, len %d", length);
    return error;
}

static ur_error_t send_trace_route_response(network_context_t *network,
                                            ur_addr_t *dest,
                                            uint32_t src_timestamp)
{
    ur_error_t error = UR_ERROR_NONE;
    message_t *message;
    mm_timestamp_tv_t *timestamp;
    message_info_t *info;
    uint8_t *data;
    uint8_t *data_orig;
    uint16_t length;

    length = sizeof(mm_header_t) + sizeof(mm_timestamp_tv_t);
    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);

    timestamp = (mm_timestamp_tv_t *)data;
    umesh_mm_init_tv_base((mm_tv_t *)timestamp, TYPE_TIMESTAMP);
    timestamp->timestamp = src_timestamp;
    data += sizeof(mm_timestamp_tv_t);

    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_TRACE_ROUTE_RESPONSE,
                               data_orig, length, DIAGS_2);
    if (message == NULL) {
        ur_mem_free(data_orig, length);
        return UR_ERROR_MEM;
    }

    info = message->info;
    info->network = network;
    memcpy(&info->dest, dest, sizeof(info->dest));

    error = address_resolve(message);
    if (error == UR_ERROR_NONE) {
        error = mf_send_message(message);
    } else if (error == UR_ERROR_DROP) {
        message_free(message);
    }
    ur_mem_free(data_orig, length);

    MESH_LOG_DEBUG("send trace route response, len %d", length);
    return error;
}

ur_error_t handle_diags_command(message_t *message, bool dest_reached)
{
    ur_error_t error = UR_ERROR_NONE;
    mm_header_t *mm_header;
    message_info_t *info;
    uint8_t *data;

    info = message->info;
    data = ur_mem_alloc(sizeof(mm_header_t));
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    message_copy_to(message, info->payload_offset, data, sizeof(mm_header_t));
    mm_header = (mm_header_t *)data;
    switch (mm_header->command & COMMAND_COMMAND_MASK) {
        case COMMAND_TRACE_ROUTE_REQUEST:
            error = handle_trace_route_request(message);
            break;
        case COMMAND_TRACE_ROUTE_RESPONSE:
            if (dest_reached) {
                error = handle_trace_route_response(message);
            }
            break;
        default:
            error = UR_ERROR_FAIL;
            break;
    }
    ur_mem_free(data, sizeof(mm_header_t));
    return error;
}
