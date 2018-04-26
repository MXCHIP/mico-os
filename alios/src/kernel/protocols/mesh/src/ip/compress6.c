/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "umesh.h"
#include "umesh_utils.h"
#include "core/network_data.h"
#include "core/mesh_forwarder.h"
#include "ip/ip.h"
#include "ip/compress6.h"

static bool is_mcast_addr(ur_ip6_addr_t *ip6_addr)
{
    return (ip6_addr->m8[0] == 0xff);
}

static void get_addr_prefix(ur_ip6_prefix_t *prefix)
{
    uint16_t meshnetid = get_main_netid(umesh_get_meshnetid());

    memset(prefix, 0, sizeof(*prefix));
    prefix->length = 64;
    prefix->prefix.m8[0] = 0xfc;
    prefix->prefix.m8[6] = (uint8_t)(meshnetid >> 8);
    prefix->prefix.m8[7] = (uint8_t)meshnetid;
}

static uint8_t ucast_addr_compress(ur_ip6_addr_t *ip6_addr, uint8_t *buffer,
                                   uint8_t *len)
{
    ur_ip6_prefix_t prefix;

    get_addr_prefix(&prefix);
    if (memcmp(prefix.prefix.m8, ip6_addr->m8, 8) != 0) {
        memcpy(buffer, &ip6_addr->m8[0], 16);
        *len = *len + 16;
        return UCAST_ADDR_128BIT;
    }

    if (memcmp(&prefix.prefix.m8[8], &ip6_addr->m8[8], 6) == 0) {
        return UCAST_ADDR_ELIDED;
    }

    if (memcmp(&ip6_addr->m8[8], FIXED_IID, 6) == 0) {
        memcpy(buffer, &ip6_addr->m8[14], 2);
        *len = *len + 2;
        return UCAST_ADDR_16BIT;
    }

    memcpy(buffer, &ip6_addr->m8[8], 8);
    *len = *len + 8;
    return UCAST_ADDR_64BIT;
}

static uint8_t ucast_addr_decompress(uint8_t mode, ur_ip6_prefix_t *prefix,
                                     const uint8_t *iphc_buffer,
                                     ur_ip6_addr_t *ip6_addr, uint16_t shortid)
{
    switch (mode) {
        case UCAST_ADDR_ELIDED:
            memcpy(&ip6_addr->m8[0], prefix->prefix.m8, 14);
            ip6_addr->m8[14] = (shortid >> 8) & 0xff;
            ip6_addr->m8[15] = shortid & 0xff;
            return 0;
            break;
        case UCAST_ADDR_16BIT:
            memcpy(&ip6_addr->m8[0], prefix->prefix.m8, 8);
            memcpy(&ip6_addr->m8[8], FIXED_IID, 6);
            memcpy(&ip6_addr->m8[14], iphc_buffer, 2);
            return 2;
            break;
        case UCAST_ADDR_64BIT:
            memcpy(&ip6_addr->m8[0], prefix->prefix.m8, 8);
            memcpy(&ip6_addr->m8[8], iphc_buffer, 8);
            return 8;
            break;
        case UCAST_ADDR_128BIT:
            memcpy(&ip6_addr->m8[0], iphc_buffer, 16);
            return 16;
            break;
        default:
            break;
    }
    return 0;
}

static uint8_t mcast_addr_compress(ur_ip6_addr_t *ip6_addr,
                                   uint8_t *buffer, uint8_t *len)
{
    ur_ip6_addr_t tmp;
    memset(tmp.m8, 0x00, sizeof(tmp.m8));

    tmp.m8[0] = 0xff;
    tmp.m8[1] = 0x02;

    if (memcmp(&tmp.m8[0], ip6_addr->m8, 15) == 0) {
        buffer[0] = ip6_addr->m8[15];
        *len = *len + 1;
        return MCAST_ADDR_8BIT;
    }

    if (memcmp(&tmp.m8[2], &ip6_addr->m8[2], 11) == 0) {
        buffer[0] = ip6_addr->m8[1];
        memcpy(&buffer[1], &ip6_addr->m8[13], 3);
        *len = *len + 4;
        return MCAST_ADDR_32BIT;
    }

    if (memcmp(&tmp.m8[2], &ip6_addr->m8[2], 9) == 0) {
        buffer[0] = ip6_addr->m8[1];
        memcpy(&buffer[1], &ip6_addr->m8[11], 5);
        *len = *len + 6;
        return MCAST_ADDR_48BIT;
    }

    memcpy(buffer, &ip6_addr->m8[0], 16);
    *len = *len + 16;
    return MCAST_ADDR_128BIT;
}

static uint8_t mcast_addr_decompress(uint8_t mode, const uint8_t *iphc_buffer,
                                     ur_ip6_addr_t *ip6_addr)
{
    memset(ip6_addr->m8, 0x00, 16);
    ip6_addr->m8[0] = 0xff;
    switch (mode) {
        case MCAST_ADDR_8BIT:
            ip6_addr->m8[1] = 0x02;
            memcpy(&ip6_addr->m8[15], &iphc_buffer[0], 1);
            return 1;
            break;
        case MCAST_ADDR_32BIT:
            ip6_addr->m8[1] = iphc_buffer[0];
            memcpy(&ip6_addr->m8[13], &iphc_buffer[1], 3);
            return 4;
            break;
        case MCAST_ADDR_48BIT:
            ip6_addr->m8[1] = iphc_buffer[0];
            memcpy(&ip6_addr->m8[11], &iphc_buffer[1], 5);
            return 6;
            break;
        case MCAST_ADDR_128BIT:
            memcpy(&ip6_addr->m8[0], &iphc_buffer[0], 16);
            return 16;
            break;
    }
    return 0;
}

static uint8_t ipv6_header_compress(ur_ip6_header_t *ip6_header,
                                    uint8_t *buffer)
{
    uint8_t iphc_len = 0;
    iphc_header_t *iphc_header;

    iphc_len = 2;

    iphc_header = (iphc_header_t *)buffer;
    iphc_header->DP = IPHC_DISPATCH;

    /* Determine TF field: Traffic Class, Flow Label */
    uint32_t v_tc_fl = ntohl(ip6_header->v_tc_fl);
    if ((v_tc_fl & FLOW_LABEL_MASK) == 0) {
        if ((v_tc_fl & TRAFFIC_CLASS_MASK) == 0) {
            iphc_header->TF = TC_FL_BOTH_ELIDED;
        } else {
            iphc_header->TF = TC_APENDED_FL_ELIDED;
            buffer[iphc_len++] = (v_tc_fl) >> 20;
        }
    } else {
        if ((v_tc_fl & TC_DSCP_MASK) == 0) {
            iphc_header->TF = DCSP_ELEDED_ECN_FL_APPENDED;
            buffer[iphc_len] = (v_tc_fl >> 20) & 0xc0;
            buffer[iphc_len++] |= (v_tc_fl >> 16) & 0x0f;
            buffer[iphc_len++] = (v_tc_fl >> 8) & 0xff;
            buffer[iphc_len++] = v_tc_fl & 0xff;
        } else {
            iphc_header->TF = TC_FL_BOTH_APEENDED;
            buffer[iphc_len++] = (v_tc_fl >> 20) & 0xff;
            buffer[iphc_len++] = (v_tc_fl >> 16) & 0x0f;
            buffer[iphc_len++] = (v_tc_fl >> 8) & 0xff;
            buffer[iphc_len++] = v_tc_fl & 0xff;
        }
    }

    /* Compress NH? Only if UDP for now. */
    if (ip6_header->next_header == UR_IPPROTO_UDP) {
        iphc_header->NH = NEXT_HEADER_ELIDED;
    } else {
        iphc_header->NH = NEXT_HEADER_APPENDED;
        buffer[iphc_len++] = ip6_header->next_header;
    }

    /* Compress hop limit? */
    if (ip6_header->hop_lim == 255) {
        iphc_header->HLIM = HOP_LIM_255;
    } else if (ip6_header->hop_lim == 64) {
        iphc_header->HLIM = HOP_LIM_64;
    } else if (ip6_header->hop_lim == 1) {
        iphc_header->HLIM = HOP_LIM_1;
    } else {
        iphc_header->HLIM = HOP_LIM_APPENDED;
        buffer[iphc_len++] = ip6_header->hop_lim;
    }

    /* stateless address compressing: CID=0 SAC=0 DAC=0 M=0*/
    /* TODO: support context based compression */
    iphc_header->CID = STATELESS_COMPRESS;
    iphc_header->SAC = STATELESS_COMPRESS;
    iphc_header->DAC = STATELESS_COMPRESS;

    /* Compress source address */
    iphc_header->SAM = ucast_addr_compress(&ip6_header->src, &buffer[iphc_len],
                                           &iphc_len);

    /* Compress destination address */
    if (is_mcast_addr(&ip6_header->dest) == true) {
        iphc_header->M = MULTICAST_DESTINATION;
        iphc_header->DAM = mcast_addr_compress(&ip6_header->dest, &buffer[iphc_len],
                                               &iphc_len);
    } else {
        iphc_header->M = UNICAST_DESTINATION;
        iphc_header->DAM = ucast_addr_compress(&ip6_header->dest, &buffer[iphc_len],
                                               &iphc_len);
    }

    *(uint16_t *)iphc_header = htons(*(uint16_t *)iphc_header);
    return iphc_len;
}

uint8_t udp_header_compress(ur_udp_header_t *udp_header, uint8_t *buffer)
{
    int nhc_len = 1;
    nhc_header_t *nhc_header = (nhc_header_t *)buffer;

    //set UPD LOWPAN_NHC header
    nhc_header->DP = NHC_UDP_DISPATCH;

    /* TODO: support optional checksum compression */
    nhc_header->C = 0x0;

    uint16_t src_port = ntohs(udp_header->src_port);
    uint16_t dst_port = ntohs(udp_header->dst_port);
    uint16_t chksum = ntohs(udp_header->chksum);
    /* port compression */
    if ((src_port & 0xfff0) == 0xf0b0 &&
        (dst_port & 0xfff0) == 0xf0b0) {
        nhc_header->P = BOTH_PORT_COMPRESSED;
        buffer[nhc_len] = (src_port << 4) & 0xf0;
        buffer[nhc_len++] |= dst_port & 0x0f;
    } else if ((src_port & 0xff00) == 0xf000) {
        nhc_header->P = SRC_PORT_COMPRESSED;
        buffer[nhc_len++] = src_port;
        buffer[nhc_len++] = dst_port >> 8;
        buffer[nhc_len++] = dst_port;
    } else if ((dst_port & 0xff00) == 0xf000) {
        nhc_header->P = DST_PORT_COMPRESSED;
        buffer[nhc_len++] = src_port >> 8;
        buffer[nhc_len++] = src_port;
        buffer[nhc_len++] = dst_port;
    } else {
        nhc_header->P = NO_PORT_COMPRESSED;
        buffer[nhc_len++] = src_port >> 8;
        buffer[nhc_len++] = src_port;
        buffer[nhc_len++] = dst_port >> 8;
        buffer[nhc_len++] = dst_port;
    }

    buffer[nhc_len++] = chksum >> 8;
    buffer[nhc_len++] = chksum;

    return nhc_len;
}

ur_error_t lp_header_compress(const uint8_t *header, uint8_t *buffer,
                              uint16_t *ip_header_len, uint16_t *hc_header_len)
{
    uint16_t iphc_len, nhc_len;
    ur_ip6_header_t *ip6_header = (ur_ip6_header_t *)header;

    uint32_t v_tc_fl = ntohl(ip6_header->v_tc_fl);
    if ((v_tc_fl & VERSION_MASK) != IP_VERSION_6) {
        return UR_ERROR_FAIL;
    }

    /* Compress IPv6 header */
    iphc_len = ipv6_header_compress(ip6_header, buffer);
    *ip_header_len = UR_IP6_HLEN;
    *hc_header_len = iphc_len;
    MESH_LOG_DEBUG("lowpan6: compressed 40 bytes IPv6 header to"
                   " %u bytes 6LowPAN IPHC header", iphc_len);

    /* Compress UDP header? */
    if (ip6_header->next_header == UR_IPPROTO_UDP) {
        ur_udp_header_t *udp_header;
        udp_header = (ur_udp_header_t *)(header + UR_IP6_HLEN);
        nhc_len = udp_header_compress(udp_header, buffer + iphc_len);
        MESH_LOG_DEBUG("lowpan6: compressed 8 bytes UDP header to"
                       " %d bytes 6LowPAN NHC header", nhc_len);
        *ip_header_len = UR_IP6_HLEN + UR_UDP_HLEN;
        *hc_header_len = *hc_header_len + nhc_len;
    }

    return UR_ERROR_NONE;
}

static ur_error_t ipv6_header_decompress(uint8_t *header, uint16_t *header_size,
                                         uint16_t *lowpan_header_size,
                                         ur_addr_t *src, ur_addr_t *dest)
{
    ur_ip6_header_t ip6_header;
    iphc_header_t *iphc_header;
    uint8_t offset = sizeof(iphc_header_t);
    ur_ip6_prefix_t prefix;
    uint16_t tmp;

    tmp = (uint16_t)((header[0] << 8) | header[1]);
    iphc_header = (iphc_header_t *)&tmp;

    if (iphc_header->CID == STATEFULL_COMPRESS ||
        iphc_header->SAC == STATEFULL_COMPRESS ||
        iphc_header->DAC == STATEFULL_COMPRESS) {
        /* does not support statefull comporess yet */
        return UR_ERROR_FAIL;
    }

    /* Set IPv6 version, traffic class and flow label. */
    ip6_header.v_tc_fl = IP_VERSION_6;
    if (iphc_header->TF == TC_FL_BOTH_APEENDED) {
        ip6_header.v_tc_fl |= ((uint32_t)header[offset++]) << 20;
        ip6_header.v_tc_fl |= ((uint32_t)(header[offset++] & 0x0F)) << 16;
        ip6_header.v_tc_fl |= ((uint32_t)header[offset++]) << 8;
        ip6_header.v_tc_fl |= ((uint32_t)header[offset++]);
    } else if (iphc_header->TF == DCSP_ELEDED_ECN_FL_APPENDED) {
        ip6_header.v_tc_fl |= (uint32_t)(header[offset] & 0xc0) << 20;
        ip6_header.v_tc_fl |= (uint32_t)(header[offset++] & 0x0f) << 16;
        ip6_header.v_tc_fl |= ((uint32_t)header[offset++]) << 8;
        ip6_header.v_tc_fl |= ((uint32_t)header[offset++]);
    } else if (iphc_header->TF == TC_APENDED_FL_ELIDED) {
        ip6_header.v_tc_fl |= ((uint32_t)header[offset++]) << 20;
    } else if (iphc_header->TF == TC_FL_BOTH_ELIDED) {
        ip6_header.v_tc_fl |= 0;
    }
    ip6_header.v_tc_fl = htonl(ip6_header.v_tc_fl);

    /* Set Next Header */
    if (iphc_header->NH == NEXT_HEADER_APPENDED) {
        ip6_header.next_header = header[offset++];
    } else {
        /* We should fill this later with NHC decoding */
        ip6_header.next_header = 0x00;
    }

    /* Set Hop Limit */
    if (iphc_header->HLIM == HOP_LIM_APPENDED) {
        ip6_header.hop_lim = header[offset++];
    } else if (iphc_header->HLIM == HOP_LIM_1) {
        ip6_header.hop_lim = 1;
    } else if (iphc_header->HLIM == HOP_LIM_64) {
        ip6_header.hop_lim = 64;
    } else if (iphc_header->HLIM == HOP_LIM_255) {
        ip6_header.hop_lim = 255;
    }

    get_addr_prefix(&prefix);
    /* Source address decoding. */
    offset += ucast_addr_decompress(iphc_header->SAM, &prefix, &header[offset],
                                    &ip6_header.src, src->addr.short_addr);

    /* Destination address decoding. */
    if (iphc_header->M == MULTICAST_DESTINATION) {
        offset += mcast_addr_decompress(iphc_header->DAM, &header[offset],
                                        &ip6_header.dest);
    } else {
        offset += ucast_addr_decompress(iphc_header->DAM, &prefix, &header[offset],
                                        &ip6_header.dest, dest->addr.short_addr);
    }

    *lowpan_header_size = offset;
    *header_size = UR_IP6_HLEN;
    memmove(header + UR_IP6_HLEN, header + offset, UR_UDP_HLEN);
    memcpy(header, &ip6_header, UR_IP6_HLEN);

    MESH_LOG_DEBUG("lowpan6: decompressed %d bytes 6LowPAN IPHC header"
                   " to 40 bytes IPv6 header", offset);

    return UR_ERROR_NONE;
}

static ur_error_t next_header_decompress(uint8_t *nhc_data, uint8_t *nhc_len)
{
    ur_udp_header_t udp_header_storage;
    ur_udp_header_t *udp_header = &udp_header_storage;
    nhc_header_t *nhc_header = (nhc_header_t *)nhc_data;

    if (nhc_header->DP == NHC_UDP_DISPATCH) {
        /* UDP decompress */
        uint8_t offset = sizeof(nhc_header_t);

        if (nhc_header->C == CHKSUM_ELIDED) {
            /* @todo support checksum decompress */
            return UR_ERROR_FAIL; /* not supported yet */
        }

        /* Decompress ports */
        if (nhc_header->P == NO_PORT_COMPRESSED) {
            udp_header->src_port = (uint16_t)nhc_data[offset++] << 8;
            udp_header->src_port |= (uint16_t)nhc_data[offset++];
            udp_header->dst_port = (uint16_t)nhc_data[offset++] << 8;
            udp_header->dst_port |= (uint16_t)nhc_data[offset++];
        } else if (nhc_header->P == DST_PORT_COMPRESSED) {
            udp_header->src_port = (uint16_t)nhc_data[offset++] << 8;
            udp_header->src_port |= (uint16_t)nhc_data[offset++];
            udp_header->dst_port = 0xf000 | nhc_data[offset++];
        } else if (nhc_header->P == SRC_PORT_COMPRESSED) {
            udp_header->src_port = 0xf000 | nhc_data[offset++];
            udp_header->dst_port = (uint16_t)nhc_data[offset++] << 8;
            udp_header->dst_port |= (uint16_t)nhc_data[offset++];
        } else if (nhc_header->P == BOTH_PORT_COMPRESSED) {
            udp_header->src_port = 0xf0b0 | ((nhc_data[offset] & 0xF0) >> 4);
            udp_header->dst_port = 0xf0b0 | (nhc_data[offset] & 0x0F);
            offset += 1;
        }
        udp_header->src_port = htons(udp_header->src_port);
        udp_header->dst_port = htons(udp_header->dst_port);

        /* fill in udp header CHECKSUM field */
        udp_header->chksum = (uint16_t)nhc_data[offset++] << 8;
        udp_header->chksum |= (uint16_t)nhc_data[offset++];
        udp_header->chksum = htons(udp_header->chksum);
        *nhc_len = offset;

        memcpy(nhc_data, udp_header, UR_UDP_HLEN);

        MESH_LOG_DEBUG("lowpan6: decompressed %d bytes 6LowPAN NHC header"
                       " to 8 bytes UDP header", offset);

        return UR_ERROR_NONE;
    } else {
        MESH_LOG_DEBUG("lowpan6: unsupported 6LowPAN NHC header, decompress failed");
        /* does not support other NHC yet */
        return UR_ERROR_FAIL;
    }
}

ur_error_t lp_header_decompress(uint8_t *header, uint16_t *header_size,
                                uint16_t *lowpan_header_size,
                                ur_addr_t *src, ur_addr_t *dest)
{
    ur_error_t error = UR_ERROR_FAIL;
    ur_ip6_header_t *ip6_header;
    iphc_header_t *iphc_header;
    uint16_t payload_size;
    uint16_t tmp;

    *lowpan_header_size = 0;
    payload_size = *header_size;
    if (payload_size < MIN_LOWPAN_FRM_SIZE) {
        goto exit;
    }
    tmp = (uint16_t)((header[0] << 8) | header[1]);
    iphc_header = (iphc_header_t *)&tmp;

    error = ipv6_header_decompress(header, header_size, lowpan_header_size,
                                   src, dest);
    if (error != UR_ERROR_NONE) {
        goto exit;
    }

    ip6_header = (ur_ip6_header_t *)header;
    payload_size -= (*lowpan_header_size);

    if (iphc_header->NH == 0x01) {
        uint8_t nhc_len = 0;
        uint8_t *nhc_data = header + (*header_size);
        ur_udp_header_t *udp_header;

        error = next_header_decompress(nhc_data, &nhc_len);
        if (error != UR_ERROR_NONE) {
            goto exit;
        }

        udp_header = (ur_udp_header_t *)nhc_data;
        (*lowpan_header_size) += nhc_len;
        (*header_size) += UR_UDP_HLEN;
        payload_size = payload_size - nhc_len + UR_UDP_HLEN;
        udp_header->length = htons(payload_size);
        ip6_header->next_header = UR_IPPROTO_UDP;
    }
    ip6_header->len = htons(payload_size);

exit:
    return error;
}
