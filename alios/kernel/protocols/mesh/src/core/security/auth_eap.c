/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include "core/auth_dot1x.h"
#include "core/auth_eap.h"
#include "core/auth_mgmt.h"
#include "core/mesh_mgmt.h"
#include "core/mesh_forwarder.h"
#include "hal/interfaces.h"

static ur_error_t send_eap_identity(dot1x_context_t *context);
static ur_error_t handle_eap_id2(dot1x_context_t *context, const uint8_t *buf,
                                 uint16_t len);

ur_error_t handle_eap_request(dot1x_context_t *context, message_t *message)
{
    eap_header_t *eap_hdr;
    uint16_t offset;
    uint8_t *cur;
    uint8_t id;
    uint8_t buf[256];

    MESH_LOG_INFO("handle eap request");

    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    message_copy_to(message, offset, buf, message_get_msglen(message) - offset);
    
    eap_hdr = (eap_header_t *)buf;
    if (eap_hdr->code != EAP_CODE_REQUEST) {
        return UR_ERROR_FAIL;
    }

    // check identifier
    id = eap_hdr->id;
    if (id - context->identifier < 1) {
        return UR_ERROR_FAIL;
    }
    context->identifier = id;

    cur = (uint8_t *)eap_hdr + sizeof(eap_header_t);
    MESH_LOG_INFO("eap auth type: %d", *cur);

    // process EAP request
    switch (*cur++) {
        case EAP_TYPE_IDENTITY:
            return send_eap_identity(context);
        case EAP_TYPE_EXPANDED_TYPES: {
            // EAP-ID2
            uint16_t len;
            offset += sizeof(eap_header_t) + 1;
            len = message_get_msglen(message) - offset; 
            message_copy_to(message, offset, cur, len);
            return handle_eap_id2(context, cur, len);
        }
        case EAP_TYPE_NOTIFICATION:
        case EAP_TYPE_NAK:
            break; 
        // add new auth method from here
        case EAP_TYPE_MD5_CHALLENGE:
        case EAP_TYPE_ONE_TIME_PASSWORD:
        case EAP_TYPE_GENERIC_TOKEN_CARD:
        case EAP_TYPE_TLS:
        case EAP_TYPE_EXPERIMENTAL:
        default:
            break;
    }
    return UR_ERROR_NONE;
}

ur_error_t send_eap_response(dot1x_context_t *context)
{
    ur_error_t error = UR_ERROR_NONE;
    eapol_header_t *eapol_hdr;
    eap_header_t *eap_hdr;
    uint16_t length;
    message_t *message;
    message_info_t *info;
    
    eapol_hdr = (eapol_header_t *)(context->response + sizeof(mm_header_t));
    eap_hdr = (eap_header_t *)((uint8_t *)eapol_hdr + sizeof(eapol_header_t));
    
    length = eap_hdr->length;
    eapol_hdr->version = EAPOL_VERSION;
    eapol_hdr->type = EAPOL_EAP;
    eapol_hdr->length = htons(length);
    
    length += sizeof(mm_header_t) + sizeof(eapol_header_t);
    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_AUTH_DOT1X,
                               context->response, length, NETWORK_MGMT_2);
    if (message == NULL) {
        return UR_ERROR_MEM;
    }
    
    info = message->info;
    set_mesh_short_addr(&info->dest, context->auth_context.auth_candidate->netid,
                        context->auth_context.auth_candidate->sid);
    error = mf_send_message(message);
    return error;
}

ur_error_t send_eap_request(dot1x_context_t *context)
{
    ur_error_t error = UR_ERROR_NONE;
    eapol_header_t *eapol_hdr;
    eap_header_t *eap_hdr;
    uint16_t length;
    message_t *message;
    message_info_t *info;
    
    eapol_hdr = (eapol_header_t *)(context->request + sizeof(mm_header_t));
    eap_hdr = (eap_header_t *)((uint8_t *)eapol_hdr + sizeof(eapol_header_t));
    
    length = eap_hdr->length;
    eapol_hdr->version = EAPOL_VERSION;
    eapol_hdr->type = EAPOL_EAP;
    eapol_hdr->length = htons(length);

    length += sizeof(mm_header_t) + sizeof(eapol_header_t);
    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_AUTH_DOT1X,
                               context->request, length, NETWORK_MGMT_2);
    if (message) {
        info = message->info;
        memcpy(&info->dest, &context->auth_context.peer, sizeof(info->dest));
        error = mf_send_message(message);
    }
    return error;
}

ur_error_t send_eap_success(dot1x_context_t *context)
{
    ur_error_t error = UR_ERROR_NONE;
    eapol_header_t *eapol_hdr;
    eap_header_t *eap_hdr;
    uint16_t length;
    uint8_t *data;
    uint8_t *data_orig;
    message_t *message;
    message_info_t *info;
    
    length = sizeof(mm_header_t) + sizeof(eapol_header_t) +
             sizeof(eap_header_t);
    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);

    eapol_hdr = (eapol_header_t *)data;
    eap_hdr = (eap_header_t *)((uint8_t *)eapol_hdr + sizeof(eapol_header_t));
    
    eap_hdr->code = EAP_CODE_SUCCESS;
    eap_hdr->id = ++context->identifier;
    eap_hdr->length = 4;
 
    eapol_hdr->version = EAPOL_VERSION;
    eapol_hdr->type = EAPOL_EAP;
    eapol_hdr->length = 4;
    
    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_AUTH_DOT1X,
                               data_orig, length, NETWORK_MGMT_2);
    if (message) {
        info = message->info;
        memcpy(&info->dest, &context->auth_context.peer, sizeof(info->dest));
        error = mf_send_message(message);
    }
    
    MESH_LOG_INFO("send EAP-Success, len %d", sizeof(eapol_header_t) + 4);
    return error;
}

ur_error_t send_eap_failure(dot1x_context_t *context)
{
    ur_error_t error = UR_ERROR_NONE;
    eapol_header_t *eapol_hdr;
    eap_header_t *eap_hdr;
    uint16_t length;
    uint8_t *data;
    uint8_t *data_orig;
    message_t *message;
    message_info_t *info;
    
    length = sizeof(mm_header_t) + sizeof(eapol_header_t) +
             sizeof(eap_header_t);
    data = ur_mem_alloc(length);
    if (data == NULL) {
        return UR_ERROR_MEM;
    }
    data_orig = data;
    data += sizeof(mm_header_t);

    eapol_hdr = (eapol_header_t *)data;
    eap_hdr = (eap_header_t *)((uint8_t *)eapol_hdr + sizeof(eapol_header_t));
    
    eap_hdr->code = EAP_CODE_FAILURE;
    eap_hdr->id = ++context->identifier;
    eap_hdr->length = 4;
 
    eapol_hdr->version = EAPOL_VERSION;
    eapol_hdr->type = EAPOL_EAP;
    eapol_hdr->length = 4;
    
    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_AUTH_DOT1X,
                               data_orig, length, NETWORK_MGMT_2);
    if (message) {
        info = message->info;
        memcpy(&info->dest, &context->auth_context.peer, sizeof(info->dest));
        error = mf_send_message(message);
    }
    
    MESH_LOG_INFO("send EAP-Failure, len %d", sizeof(eapol_header_t) + 4);
    return error;
}

ur_error_t handle_eap_response(dot1x_context_t *context, message_t *message)
{
    eap_header_t *eap_hdr;
    uint16_t offset;
    uint8_t *cur;
    uint8_t id;
    uint8_t buf[256];

    MESH_LOG_INFO("handle eap response");

    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    message_copy_to(message, offset, buf, (message_get_msglen(message) - offset));

    eap_hdr = (eap_header_t *)buf;
    if (eap_hdr->code != EAP_CODE_RESPONSE) {
        return UR_ERROR_FAIL;
    }

    // check identifier
    id = eap_hdr->id;
    if (id - context->identifier > 0) {
        return UR_ERROR_FAIL;
    }

    cur = (uint8_t *)eap_hdr + sizeof(eap_header_t);
    MESH_LOG_INFO("eap auth type: %d", *cur);

    // process EAP response
    switch (*cur++) {
        case EAP_TYPE_IDENTITY:
            return handle_eap_identity(context, message);
        case EAP_TYPE_EXPANDED_TYPES: {
            // EAP-ID2
            uint16_t len;
            offset += sizeof(eap_header_t) + 1;
            len = message_get_msglen(message) - offset; 
            message_copy_to(message, offset, cur, len);
            return handle_eap_id2(context, cur, len);
        }
        case EAP_TYPE_NOTIFICATION:
        case EAP_TYPE_NAK:
            break;
        // add new auth method from here
        case EAP_TYPE_MD5_CHALLENGE:
        case EAP_TYPE_ONE_TIME_PASSWORD:
        case EAP_TYPE_GENERIC_TOKEN_CARD:
        case EAP_TYPE_TLS:
        case EAP_TYPE_EXPERIMENTAL:
        default:
            break;
    }
    return UR_ERROR_NONE;
}

ur_error_t handle_eap_identity(dot1x_context_t *context, message_t *message)
{
    uint16_t offset;
    uint8_t *cur;
    uint8_t buf[24];

    MESH_LOG_INFO("handle eap identity");

    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    message_copy_to(message, offset, buf, (message_get_msglen(message) - offset));

    cur = buf + sizeof(eap_header_t);
    if (*cur++ != EAP_TYPE_IDENTITY) {
        return UR_ERROR_FAIL;
    }

    // identity
    if (memcmp(context->auth_context.peer.addr.addr, cur, EXT_ADDR_SIZE) != 0) {
        return UR_ERROR_FAIL;
    }
    MESH_LOG_DEBUG("peer src mac: %02x:%02x:%02x:%02x:%02x:%02x",
            context->auth_context.peer.addr.addr[0], context->auth_context.peer.addr.addr[1],
            context->auth_context.peer.addr.addr[2], context->auth_context.peer.addr.addr[3],
            context->auth_context.peer.addr.addr[4], context->auth_context.peer.addr.addr[5]);

    return send_auth_start(context);
}

ur_error_t handle_eap_success(dot1x_context_t *context, message_t *message)
{
    uint16_t offset;
    eap_header_t eap_hdr;

    MESH_LOG_INFO("handle eap success");

    ur_stop_timer(&context->auth_context.auth_timer, NULL);
    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    message_copy_to(message, offset, (uint8_t *)&eap_hdr, sizeof(eap_header_t));
    if (eap_hdr.code != EAP_CODE_SUCCESS) {
        return UR_ERROR_FAIL;
    }

    context->auth_context.auth_handler(context->auth_context.auth_candidate,
                                       true);
    return UR_ERROR_NONE;
}

ur_error_t handle_eap_failure(dot1x_context_t *context, message_t *message)
{
    uint16_t offset;
    eap_header_t eap_hdr;

    MESH_LOG_INFO("handle eap failure");

    ur_stop_timer(&context->auth_context.auth_timer, NULL);
    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    message_copy_to(message, offset, (uint8_t *)&eap_hdr, sizeof(eap_header_t));
    if (eap_hdr.code != EAP_CODE_FAILURE) {
        return UR_ERROR_FAIL;
    }

    context->auth_context.auth_handler(context->auth_context.auth_candidate,
                                       false);
    return UR_ERROR_NONE;
}

static ur_error_t handle_eap_id2(dot1x_context_t *context, const uint8_t *buf,
                                 uint16_t len)
{
    eap_id2_header_t *id2_hdr = (eap_id2_header_t *)buf;

    switch (id2_hdr->cmd) {
        case EAP_ID2_START:
            return handle_auth_start(context, buf);
        case EAP_ID2_IDENTITY:
        case EAP_ID2_CHALLENGE:
        case EAP_ID2_AUTH_CODE:
            return handle_auth_relay(context, buf, len);
        default:
            return UR_ERROR_FAIL;
    }
}

static ur_error_t send_eap_identity(dot1x_context_t *context)
{
    uint8_t *cur;
    uint16_t offset;
    eap_header_t *eap_hdr;
    const mac_address_t *mac = umesh_mm_get_mac_address();
    
    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    eap_hdr = (eap_header_t *)(context->response + offset);
    eap_hdr->code = EAP_CODE_RESPONSE;
    eap_hdr->id = context->identifier;
    cur = (uint8_t *)eap_hdr + sizeof(eap_header_t);
    
    *cur++ = EAP_TYPE_IDENTITY;
    memcpy(cur, mac->addr, EXT_ADDR_SIZE);
    cur += EXT_ADDR_SIZE;

    eap_hdr->length = cur - (uint8_t *)eap_hdr;
    MESH_LOG_INFO("send EAP-Response/Identity");
    return send_eap_response(context);
}
