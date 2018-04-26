/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include "core/auth_dot1x.h"
#include "core/auth_eap.h"
#include "core/mesh_mgmt.h"

static ur_error_t handle_eapol_start(message_t *message);
static ur_error_t handle_eapol_eap(message_t *message);

dot1x_context_t g_dot1x_context;

dot1x_context_t *get_dot1x_context(void)
{
    return &g_dot1x_context;
}

ur_error_t send_eapol_start(dot1x_context_t *context)
{
    ur_error_t error = UR_ERROR_NONE;
    eapol_header_t *eapol_hdr;
    uint16_t length;
    message_t *message = NULL;
    message_info_t *info;

    eapol_hdr = (eapol_header_t *)(context->request + sizeof(mm_header_t));
    eapol_hdr->version = EAPOL_VERSION;
    eapol_hdr->type = EAPOL_START;
    eapol_hdr->length = 0; // eapol_start has no packet body

    length = sizeof(mm_header_t) + sizeof(eapol_header_t);
    message = mf_build_message(MESH_FRAME_TYPE_CMD, COMMAND_AUTH_DOT1X,
                               context->request, length, NETWORK_MGMT_2);
    if (message == NULL) {
        return UR_ERROR_MEM;
    }

    info = message->info;
    set_mesh_short_addr(&info->dest, context->auth_context.auth_candidate->netid,
                        context->auth_context.auth_candidate->sid);
    error = mf_send_message(message);
    MESH_LOG_INFO("send eapol start");
    return error;
}

ur_error_t handle_eapol(message_t *message)
{
    eapol_header_t eapol_hdr;

    MESH_LOG_INFO("handle eapol message");

    message_copy_to(message, sizeof(mm_header_t), (uint8_t *)&eapol_hdr,
                    sizeof(eapol_header_t));
    MESH_LOG_DEBUG("version: %d, type: %d, length: %d",
                   eapol_hdr.version, eapol_hdr.type, ntohs(eapol_hdr.length));

    switch (eapol_hdr.type) {
        case EAPOL_START:
            return handle_eapol_start(message);
        case EAPOL_EAP:
            return handle_eapol_eap(message);
        case EAPOL_KEY:
        default:
            return UR_ERROR_FAIL;
    }
    return UR_ERROR_NONE;
}

static ur_error_t handle_eapol_start(message_t *message)
{
    uint8_t *cur;
    uint16_t offset;
    eap_header_t *eap_hdr;
    eapol_header_t eapol_hdr;
    message_info_t *info;
    
    MESH_LOG_INFO("handle eapol start message");

    info = message->info;
    memcpy(&g_dot1x_context.auth_context.peer, &info->src_mac, sizeof(ur_addr_t));
    message_copy_to(message, sizeof(mm_header_t), (uint8_t *)&eapol_hdr,
                    sizeof(eapol_header_t));
    if (eapol_hdr.type != EAPOL_START)
        return UR_ERROR_FAIL;

    // eap header
    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    eap_hdr = (eap_header_t *)(g_dot1x_context.request + offset);
    eap_hdr->code = EAP_CODE_REQUEST;
    eap_hdr->id = ++g_dot1x_context.identifier;
    cur = (uint8_t *)eap_hdr + sizeof(eap_header_t);
    
    *cur++ = EAP_TYPE_IDENTITY;
    eap_hdr->length = cur - (uint8_t *)eap_hdr;

    MESH_LOG_INFO("send EAP Request/Identity");
    return send_eap_request(&g_dot1x_context);
}

static ur_error_t handle_eapol_eap(message_t *message)
{
    eap_header_t eap_hdr;
    uint16_t offset = sizeof(mm_header_t) + sizeof(eapol_header_t);

    MESH_LOG_INFO("handle eap message");

    message_copy_to(message, offset, (uint8_t *)&eap_hdr, sizeof(eap_header_t));
    MESH_LOG_DEBUG("code: %d, id: %d, length: %d",
                   eap_hdr.code, eap_hdr.id, ntohs(eap_hdr.length));

    switch (eap_hdr.code) {
        case EAP_CODE_REQUEST:
            return handle_eap_request(&g_dot1x_context, message);
        case EAP_CODE_RESPONSE:
            return handle_eap_response(&g_dot1x_context, message);
        case EAP_CODE_SUCCESS:
            return handle_eap_success(&g_dot1x_context, message);
        case EAP_CODE_FAILURE:
            return handle_eap_failure(&g_dot1x_context, message);
        default:
            return UR_ERROR_FAIL;
    }
    return UR_ERROR_NONE;
}
