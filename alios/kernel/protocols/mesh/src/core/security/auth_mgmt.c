/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>

#include "aos/aos.h"
#include "core/auth_mgmt.h"
#include "core/auth_relay.h"
#include "core/link_mgmt.h"
#include "core/mesh_mgmt.h"
#include "core/mesh_forwarder.h"
#include "core/topology.h"
#include "hal/interfaces.h"
#include "umesh_config.h"
#include "umesh_utils.h"
#include "umesh_types.h"

auth_context_t g_auth_context = {
    .is_auth_enable   = false,
    .is_auth_busy     = false,
    .is_auth_success  = false,
    .local_id2_len    = TFS_ID2_LEN,
    .auth_code_len    = TFS_AUTH_CODE_LEN,
    .peer_auth_result = false,
    .auth_candidate   = NULL,
    .auth_retry_times = 0,
};
socket_t udp_sock;

static uint8_t vendor_oui[3] = { 0xd8, 0x96, 0xe0 }; // Alibaba Cloud OUI
static uint8_t vendor_type[4] = { 0x00, 0x00, 0x00, 0x01 }; // EAP-ID2

static void handle_auth_timer(void *args);
static ur_error_t send_auth_challenge(dot1x_context_t *context);
static ur_error_t send_auth_code(dot1x_context_t *context);

static void handle_udp_socket(const uint8_t *payload, uint16_t length)
{
    uint8_t cmd;
    uint8_t *cur = (uint8_t *)payload;
    uint8_t src[EXT_ADDR_SIZE];
    const mac_address_t *mac = umesh_mm_get_mac_address();
    network_context_t *network = get_default_network_context();
    dot1x_context_t *dot1x_context = get_dot1x_context();

    if (length == 0) {
        return;
    }

    MESH_LOG_DEBUG("recv socket from sp server");
    MESH_LOG_DEBUG("src mac: %02x:%02x:%02x:%02x:%02x:%02x\n",
                  mac->addr[0], mac->addr[1], mac->addr[2],
                  mac->addr[3], mac->addr[4], mac->addr[5]);

    // src
    memcpy(src, cur, EXT_ADDR_SIZE);
    cur += EXT_ADDR_SIZE;
    length -= EXT_ADDR_SIZE;

    // dst
    cur += EXT_ADDR_SIZE;
    length -= EXT_ADDR_SIZE;

    // cmd
    cmd = *cur++;
    length--;

    switch (cmd) {
        case ID2_AUTH_CHALLENGE:
            if (umesh_mm_get_device_state() == DEVICE_STATE_LEADER ||
                g_auth_context.is_auth_success) {
                MESH_LOG_INFO("sp server -> joiner router: challenge");

                // challenge
                memcpy(dot1x_context->auth_context.id2_challenge, cur, length);
 
                // send challenge to joiner
                ur_stop_timer(&g_auth_context.auth_timer, NULL);
                g_auth_context.auth_state = AUTH_RELAY_CHALLENGE;
                send_auth_challenge(dot1x_context);
                ur_start_timer(&g_auth_context.auth_timer, network->hal->auth_relay_interval,
                               handle_auth_timer, NULL);
            }
            break;
        case ID2_AUTH_RESULT:
            if (umesh_mm_get_device_state() == DEVICE_STATE_LEADER ||
                g_auth_context.is_auth_success) {
                MESH_LOG_INFO("sp server -> joiner router: joiner's auth result");

                g_auth_context.peer_auth_result = *cur;
                ur_stop_timer(&g_auth_context.auth_timer, NULL);
                if (g_auth_context.peer_auth_result) {
                    send_eap_success(dot1x_context);
                } else {
                    send_eap_failure(dot1x_context);
                }
            }
            break;
         default:
            MESH_LOG_DEBUG("unknown cmd\n");
            break;
    }
}

static void handle_auth_timer(void *args)
{
    dot1x_context_t *dot1x_context = get_dot1x_context();
    network_context_t *network = get_default_network_context();

    MESH_LOG_INFO("handle auth timer");

    switch (g_auth_context.auth_state) {
        case AUTH_REQUEST_START:
            if (g_auth_context.auth_retry_times < AUTH_REQUEST_RETRY_TIMES) {
                ++g_auth_context.auth_retry_times;
                if (!g_auth_context.is_auth_busy) {
                    MESH_LOG_INFO("joiner -> joiner router: EAPoL Start");
                    send_eapol_start(dot1x_context);
                    g_auth_context.is_auth_busy = true;
                }
            }
            break;
        case AUTH_RELAY_CHALLENGE:
            if (g_auth_context.auth_retry_times < AUTH_RELAY_RETRY_TIMES) {
                ++g_auth_context.auth_retry_times;
                send_auth_challenge(dot1x_context);
                ur_start_timer(&g_auth_context.auth_timer, network->hal->auth_relay_interval,
                               handle_auth_timer, NULL);
            }
            break;
        case AUTH_RELAY_AUTH_CODE:
            if (g_auth_context.auth_retry_times < AUTH_RELAY_RETRY_TIMES) {
                ++g_auth_context.auth_retry_times;
                send_auth_code(dot1x_context);
                ur_start_timer(&g_auth_context.auth_timer, network->hal->auth_relay_interval,
                               handle_auth_timer, NULL);
            }
            break;
        case AUTH_DONE:
        default:
            break;
    }
}

ur_error_t auth_init(void)
{
    memset(g_auth_context.local_id2, 0, sizeof(g_auth_context.local_id2));
    memset(g_auth_context.peer_id2, 0, sizeof(g_auth_context.peer_id2));
    memset(g_auth_context.auth_code, 0, sizeof(g_auth_context.auth_code));

    // read ID2
    tfs_get_ID2(g_auth_context.local_id2, &g_auth_context.local_id2_len);

    MESH_LOG_DEBUG("id2: %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x\
                         %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                   g_auth_context.local_id2[0], g_auth_context.local_id2[1], g_auth_context.local_id2[2],
                   g_auth_context.local_id2[3], g_auth_context.local_id2[4], g_auth_context.local_id2[5],
                   g_auth_context.local_id2[6], g_auth_context.local_id2[7], g_auth_context.local_id2[8],
                   g_auth_context.local_id2[9], g_auth_context.local_id2[10], g_auth_context.local_id2[11],
                   g_auth_context.local_id2[12], g_auth_context.local_id2[13], g_auth_context.local_id2[14],
                   g_auth_context.local_id2[15], g_auth_context.local_id2[16], g_auth_context.local_id2[17],
                   g_auth_context.local_id2[18], g_auth_context.local_id2[19], g_auth_context.local_id2[20],
                   g_auth_context.local_id2[21], g_auth_context.local_id2[22], g_auth_context.local_id2[23]);
 
    udp_sock.socket = socket_init(handle_udp_socket);
    return UR_ERROR_NONE; 
}

void auth_enable(void)
{
    if (!g_auth_context.is_auth_enable) {
        g_auth_context.is_auth_enable = true;
    }
}

void auth_disable(void)
{
    if (g_auth_context.is_auth_enable) {
        g_auth_context.is_auth_enable = false;
    }
}

auth_state_t get_auth_state(void)
{
    return g_auth_context.auth_state;
}

void set_auth_state(auth_state_t state)
{
    g_auth_context.auth_state = state;
}

bool get_auth_result(void)
{
    return g_auth_context.is_auth_success;
}

neighbor_t *get_auth_candidate(void)
{
    return g_auth_context.auth_candidate;
}

bool is_auth_enabled(void)
{
    return g_auth_context.is_auth_enable;
}

bool is_auth_busy(void)
{
    return g_auth_context.is_auth_busy;
}

ur_error_t start_auth(neighbor_t *nbr, auth_handler_t handler)
{
    uint32_t random;
    network_context_t *network = get_default_network_context();
    dot1x_context_t *dot1x_context = get_dot1x_context();

    if (g_auth_context.is_auth_busy ||
        g_auth_context.auth_candidate ||
        (g_auth_context.auth_state != AUTH_IDLE &&
         g_auth_context.auth_state != AUTH_DONE)) {
         return UR_ERROR_BUSY;
    }

    if (nbr) {
        if (umesh_mm_get_channel(network->hal) != nbr->channel) {
            umesh_mm_set_prev_channel();
            umesh_mm_set_channel(network->hal, nbr->channel);
        }

        network->meshnetid = nbr->netid;
    }

    if (!g_auth_context.is_auth_enable) {
        return UR_ERROR_FAIL;
    }

    if (g_auth_context.auth_timer) {
        ur_stop_timer(&g_auth_context.auth_timer, NULL);
    }

    g_auth_context.auth_candidate = nbr;
    g_auth_context.auth_state = AUTH_REQUEST_START;
    g_auth_context.auth_handler = handler;
    dot1x_context->auth_context = g_auth_context;

    random = umesh_get_random();
    ur_start_timer(&g_auth_context.auth_timer,
                   (random % network->hal->auth_request_interval + 1),
                   handle_auth_timer, NULL);
    return UR_ERROR_NONE;
}

ur_error_t stop_auth(void)
{
    if (!g_auth_context.is_auth_enable) {
        return UR_ERROR_FAIL;
    }

    if (g_auth_context.auth_timer) {
        ur_stop_timer(&g_auth_context.auth_timer, NULL);
    }

    g_auth_context.is_auth_success = false;
    g_auth_context.is_auth_busy = false;
    return UR_ERROR_NONE;
}

ur_error_t send_auth_start(dot1x_context_t *context)
{
    uint8_t *cur;
    uint16_t offset;
    eap_header_t *eap_hdr;
    eap_id2_header_t *id2_hdr;

    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    eap_hdr = (eap_header_t *)(context->request + offset);
    eap_hdr->code = EAP_CODE_REQUEST;
    eap_hdr->id = ++context->identifier;
    cur = (uint8_t *)eap_hdr + sizeof(eap_header_t);

    *cur++ = EAP_TYPE_EXPANDED_TYPES;
    id2_hdr = (eap_id2_header_t *)cur;
    memcpy(id2_hdr->oui, vendor_oui, 3);
    memcpy(id2_hdr->type, vendor_type, 4);
    id2_hdr->cmd = EAP_ID2_START;
    cur += sizeof(eap_id2_header_t);

    eap_hdr->length = cur - (uint8_t *)eap_hdr;
    MESH_LOG_INFO("send EAP-ID2 start");
    return send_eap_request(context);
}

static ur_error_t send_auth_identity(dot1x_context_t *context)
{
    uint8_t *cur;
    uint16_t offset;
    mm_node_id2_tv_t *id2;
    eap_header_t *eap_hdr;
    eap_id2_header_t *id2_hdr;

    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    eap_hdr = (eap_header_t *)(context->response + offset);
    eap_hdr->code = EAP_CODE_RESPONSE;
    eap_hdr->id = context->identifier;
    cur = (uint8_t *)eap_hdr + sizeof(eap_header_t);

    // type
    *cur++ = EAP_TYPE_EXPANDED_TYPES;

    // vendor data
    id2_hdr = (eap_id2_header_t *)cur;
    memcpy(id2_hdr->oui, vendor_oui, 3);
    memcpy(id2_hdr->type, vendor_type, 4);
    id2_hdr->cmd = EAP_ID2_IDENTITY;
    cur += sizeof(eap_id2_header_t);

    // ID2
    id2 = (mm_node_id2_tv_t *)cur;
    umesh_mm_init_tv_base((mm_tv_t *)id2, TYPE_NODE_ID2);
    memcpy(id2->device_id, context->auth_context.local_id2, TFS_ID2_LEN);
    cur += sizeof(mm_node_id2_tv_t);

    eap_hdr->length = cur - (uint8_t *)eap_hdr;
    MESH_LOG_INFO("send EAP-ID2 identity");
    return send_eap_response(context);
}

static ur_error_t send_auth_challenge(dot1x_context_t *context)
{
    uint8_t *cur;
    uint16_t offset;
    eap_header_t *eap_hdr;
    eap_id2_header_t *id2_hdr;
    mm_node_id2_tv_t *id2;
    mm_id2_challenge_tv_t *id2_challenge;

    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    eap_hdr = (eap_header_t *)(context->request + offset);
    eap_hdr->code = EAP_CODE_REQUEST;
    eap_hdr->id = ++context->identifier;
    cur = (uint8_t *)eap_hdr + sizeof(eap_header_t);

    // type
    *cur++ = EAP_TYPE_EXPANDED_TYPES;

    // vendor data
    id2_hdr = (eap_id2_header_t *)cur;
    memcpy(id2_hdr->oui, vendor_oui, 3);
    memcpy(id2_hdr->type, vendor_type, 4);
    id2_hdr->cmd = EAP_ID2_CHALLENGE;
    cur += sizeof(eap_id2_header_t);

    // challenge
    id2_challenge = (mm_id2_challenge_tv_t *)cur;
    umesh_mm_init_tv_base((mm_tv_t *)id2_challenge, TYPE_ID2_CHALLENGE);
    memcpy(id2_challenge->challenge, context->auth_context.id2_challenge, TFS_CHALLENGE_LEN);
    cur += sizeof(mm_id2_challenge_tv_t);

    // ID2
    id2 = (mm_node_id2_tv_t *)cur;
    umesh_mm_init_tv_base((mm_tv_t *)id2, TYPE_NODE_ID2);
    memcpy(id2->device_id, context->auth_context.peer_id2, TFS_ID2_LEN);
    cur += sizeof(mm_node_id2_tv_t);

    eap_hdr->length = cur - (uint8_t *)eap_hdr;
    MESH_LOG_INFO("send EAP-ID2 challenge");
    return send_eap_request(context);
}

static ur_error_t send_auth_code(dot1x_context_t *context)
{
    uint8_t *cur;
    uint16_t offset;
    eap_header_t *eap_hdr;
    eap_id2_header_t *id2_hdr;
    mm_id2_auth_code_tlv_t *id2_auth_code;

    offset = sizeof(mm_header_t) + sizeof(eapol_header_t);
    eap_hdr = (eap_header_t *)(context->response + offset);
    eap_hdr->code = EAP_CODE_RESPONSE;
    eap_hdr->id = context->identifier;
    cur = (uint8_t *)eap_hdr + sizeof(eap_header_t);

    // type
    *cur++ = EAP_TYPE_EXPANDED_TYPES;

    // vendor data
    id2_hdr = (eap_id2_header_t *)cur;
    memcpy(id2_hdr->oui, vendor_oui, 3);
    memcpy(id2_hdr->type, vendor_type, 4);
    id2_hdr->cmd = EAP_ID2_AUTH_CODE;
    cur += sizeof(eap_id2_header_t);

    // auth code
    id2_auth_code = (mm_id2_auth_code_tlv_t *)cur;
    umesh_mm_init_tlv_base((mm_tlv_t *)id2_auth_code, TYPE_ID2_AUTH_CODE,
                           context->auth_context.auth_code_len);
    cur += sizeof(mm_id2_auth_code_tlv_t);
    memcpy(cur, context->auth_context.auth_code, context->auth_context.auth_code_len);
    cur += context->auth_context.auth_code_len;

    eap_hdr->length = cur - (uint8_t *)eap_hdr;
    MESH_LOG_INFO("send EAP-ID2 auth code");
    return send_eap_response(context);
}

ur_error_t handle_auth_start(dot1x_context_t *context, const uint8_t *buf)
{
    eap_id2_header_t *id2_hdr = (eap_id2_header_t *)buf;

    MESH_LOG_INFO("handle EAP-ID2 Start");

    if (id2_hdr->cmd != EAP_ID2_START) {
        return UR_ERROR_FAIL;
    }
    
    if (memcmp(id2_hdr->oui, vendor_oui, 3) != 0) {
        return UR_ERROR_FAIL;
    }

    if (memcmp(id2_hdr->type, vendor_type, 4) != 0) {
        return UR_ERROR_FAIL;
    }

    return send_auth_identity(context);
}

ur_error_t handle_auth_relay(dot1x_context_t *context,
                             const uint8_t *buf, uint16_t len)
{
    ur_error_t error = UR_ERROR_NONE;
    eap_id2_header_t *id2_hdr;
    mm_id2_challenge_tv_t *id2_challenge;
    mm_id2_auth_code_tlv_t *id2_auth_code;
    mm_node_id2_tv_t *id2;
    const uint8_t *tlvs;
    uint16_t tlvs_length;
    bool joiner = false;
    
    MESH_LOG_INFO("handle EAP-ID2 Relay");
 
    id2_hdr = (eap_id2_header_t *)buf;
    if (id2_hdr->cmd != EAP_ID2_IDENTITY &&
        id2_hdr->cmd != EAP_ID2_CHALLENGE &&
        id2_hdr->cmd != EAP_ID2_AUTH_CODE) {
        return UR_ERROR_FAIL;
    }
    
    if (memcmp(id2_hdr->oui, vendor_oui, 3) != 0) {
        return UR_ERROR_FAIL;
    }

    if (memcmp(id2_hdr->type, vendor_type, 4) != 0) {
        return UR_ERROR_FAIL;
    }

    if (!get_auth_result() &&
        umesh_mm_get_device_state() != DEVICE_STATE_LEADER) {
        joiner = true;
    }

    tlvs = buf + sizeof(eap_id2_header_t);
    tlvs_length = len - sizeof(eap_id2_header_t);

    switch (id2_hdr->cmd) {
        case EAP_ID2_IDENTITY: {
            MESH_LOG_INFO("handle EAP-ID2 Identity");
            mm_node_id2_tv_t *id2;
            const mac_address_t *mac;
            mac = umesh_mm_get_mac_address();
            id2 = (mm_node_id2_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length,
                                                      TYPE_NODE_ID2);
            if (id2) {
                memcpy(context->auth_context.peer_id2, id2->device_id, TFS_ID2_LEN);
            } else {
                error = UR_ERROR_FAIL;
                goto exit;
            }

            // send id2 to sp server
            socket_sendmsg(udp_sock.socket, context->auth_context.peer.addr.addr, mac->addr,
                           ID2_AUTH_REQUEST, id2->device_id, TFS_ID2_LEN,
                           ID2_SERVER_ADDR, ID2_SERVER_PORT);
            MESH_LOG_INFO("joiner router -> sp server: joiner's ID2");
            break;
        }
        case EAP_ID2_CHALLENGE: {
            MESH_LOG_INFO("handle EAP-ID2 Challenge");
            network_context_t *network = get_default_network_context();
            id2 = (mm_node_id2_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length,
                                                     TYPE_NODE_ID2);
            // check ID2
            if (joiner && (memcmp(context->auth_context.local_id2, id2->device_id, TFS_ID2_LEN) == 0)) {
                id2_challenge = (mm_id2_challenge_tv_t *)umesh_mm_get_tv(tlvs, tlvs_length,
                                                                         TYPE_ID2_CHALLENGE);
                if (id2_challenge) {
                    // the first parameter "challenge" is a pointer of string type
                    // here we pass the ascii hex of each character array instead
                    // and also need to supplement the last '\0' as the ending, hence
                    // the total length of "challenge" array should be 33 bytes.
                    uint8_t challenge[TFS_CHALLENGE_LEN+1];
                    memset(challenge, 0, sizeof(challenge));
                    memcpy(challenge, id2_challenge->challenge, TFS_CHALLENGE_LEN);
                    tfs_id2_get_challenge_auth_code(challenge, NULL, 0,
                                                    context->auth_context.auth_code,
                                                    &context->auth_context.auth_code_len);
                    MESH_LOG_INFO("auth code len: %d", context->auth_context.auth_code_len);
                } else {
                    error = UR_ERROR_FAIL;
                    goto exit;
                }

                ur_stop_timer(&context->auth_context.auth_timer, NULL);
                set_auth_state(AUTH_RELAY_AUTH_CODE);
                send_auth_code(context);
                ur_start_timer(&context->auth_context.auth_timer, network->hal->auth_relay_interval,
                               handle_auth_timer, NULL);
                MESH_LOG_INFO("joiner -> joiner router: auth code");
                break;
            }
        }
        case EAP_ID2_AUTH_CODE: {
            MESH_LOG_INFO("handle EAP-ID2 auth code");
            if (!joiner &&
                (id2_auth_code = (mm_id2_auth_code_tlv_t *)umesh_mm_get_tv(tlvs, tlvs_length,
                                                                   TYPE_ID2_AUTH_CODE))) {
                const mac_address_t *mac;
                mac = umesh_mm_get_mac_address();
                uint8_t *data = (uint8_t *)id2_auth_code + sizeof(mm_tlv_t);
        
                // send joiner's auth code to sp server
                socket_sendmsg(udp_sock.socket, context->auth_context.peer.addr.addr, mac->addr,
                               ID2_AUTH_CODE, data, id2_auth_code->base.length,
                               ID2_SERVER_ADDR, ID2_SERVER_PORT);
                MESH_LOG_INFO("joiner router -> sp server: joiner's auth code");
            }
            break;
        }
        default:
            break;
    }

exit:
    return error;
}
