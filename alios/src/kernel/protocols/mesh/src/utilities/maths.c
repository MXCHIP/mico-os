/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <string.h>

#include "umesh.h"
#include "umesh_utils.h"

uint32_t umesh_get_random(void)
{
    uint32_t result = result;
#ifndef CONFIG_AOS_MESH_RRN
    uint32_t seed = umesh_now_ms();
    uint8_t byte[4 + 1];
    const mac_address_t *mac_addr = umesh_get_mac_address(MEDIA_TYPE_DFL);

    seed += result;
    seed += (uint32_t)mac_addr->value;
    seed = seed % 9999;
    snprintf((char *)byte, sizeof(byte), "%04d", seed);
    memcpy(&result, byte, 4);
#endif
    return result;
}

#define rotation(value_a, value_b) ((value_a << value_b) | (value_a >> (32 - value_b)))

#define mix(value_a, value_b, value_c) \
{ \
    value_a -= value_c; \
    value_a ^= rotation(value_c, 4); \
    value_c += value_b; \
    value_b -= value_a; \
    value_b ^= rotation(value_a, 6); \
    value_a += value_c; \
    value_c -= value_b; \
    value_c ^= rotation(value_b, 8); \
    value_b += value_a; \
    value_a -= value_c; \
    value_a ^= rotation(value_c, 16); \
    value_c += value_b; \
    value_b -= value_a; \
    value_b ^= rotation(value_a, 19); \
    value_a += value_c; \
    value_c -= value_b; \
    value_c ^= rotation(value_b, 4);  \
    value_b += value_a; \
}

#define final(value_a, value_b, value_c) \
{ \
    value_c ^= value_b; \
    value_c -= rotation(value_b, 14); \
    value_a ^= value_c; \
    value_a -= rotation(value_c, 11); \
    value_b ^= value_a; \
    value_b -= rotation(value_a, 25); \
    value_c ^= value_b; \
    value_c -= rotation(value_b, 16); \
    value_a ^= value_c; \
    value_a -= rotation(value_c, 4);  \
    value_b ^= value_a; \
    value_b -= rotation(value_a, 14); \
    value_c ^= value_b; \
    value_c -= rotation(value_b,24); \
}

// Jenkin's hashword
uint32_t umesh_get_hashword(const uint32_t *key, uint16_t length, uint32_t init)
{
    uint32_t value_a = 0xdeadbeef + (((uint32_t)length)<<2) + init;
    uint32_t value_b = 0xdeadbeef + (((uint32_t)length)<<2) + init;
    uint32_t value_c = 0xdeadbeef + (((uint32_t)length)<<2) + init;

    while (length > 3) {
        value_a += key[0];
        value_b += key[1];
        value_c += key[2];
        mix(value_a, value_b, value_c);
        length -= 3;
        key += 3;
    }

    switch(length) {
        case 3:
            value_c += key[2];
        case 2:
            value_b += key[1];
        case 1:
            value_a += key[0];
            final(value_a, value_b, value_c);
        case 0:
          break;
    }

    return value_c;
}
