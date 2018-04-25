/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdbool.h>

#include "ip/ip.h"
#include "umesh_utils.h"

bool is_valid_digit(uint8_t ch)
{
    if (ch >= '0' && ch <= '9') {
        return true;
    }

    return false;
}

bool is_valid_upper(uint8_t ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return true;
    }

    return false;
}

bool is_valid_lower(uint8_t ch)
{
    if (ch >= 'a' && ch <= 'z') {
        return true;
    }

    return false;
}

ur_error_t string_to_ip6_addr(const char *buf, ur_ip6_addr_t *target)
{

    uint32_t addr_index;
    uint32_t zero_blocks;
    uint32_t current_block_index;
    uint32_t current_block_value;
    const char *s;

    zero_blocks = 8;
    for (s = buf; *s != 0; s++) {
        if (*s == ':') {
            zero_blocks--;
        } else if (!(is_valid_digit(*s) || is_valid_lower(*s) || is_valid_upper(*s))) {
            break;
        }
    }

    addr_index = 0;
    current_block_index = 0;
    current_block_value = 0;
    for (s = buf; *s != 0; s++) {
        if (*s == ':') {
            if (target) {
                if (current_block_index & 0x1) {
                    target->m32[addr_index++] |= current_block_value;
                } else {
                    target->m32[addr_index] = current_block_value << 16;
                }
            }
            current_block_index++;
            current_block_value = 0;
            if (current_block_index > 7) {
                return UR_ERROR_PARSE;
            }
            if (s[1] == ':') {
                if (s[2] == ':') {
                    return 0;
                }
                s++;
                while (zero_blocks > 0) {
                    zero_blocks--;
                    if (current_block_index & 0x1) {
                        addr_index++;
                    } else {
                        if (target) {
                            target->m32[addr_index] = 0;
                        }
                    }
                    current_block_index++;
                    if (current_block_index > 7) {
                        return UR_ERROR_PARSE;
                    }
                }
            }
        } else if (is_valid_digit(*s) || is_valid_lower(*s) || is_valid_upper(*s)) {
            current_block_value = (current_block_value << 4) +
                                  (is_valid_digit(*s) ? *s - '0' :
                                   10 + (is_valid_lower(*s) ? *s - 'a' : *s - 'A'));
        } else {
            break;
        }
    }

    if (target) {
        if (current_block_index & 0x1) {
            target->m32[addr_index++] |= current_block_value;
        } else {
            target->m32[addr_index] = current_block_value << 16;
        }
    }

    if (target) {
        for (addr_index = 0; addr_index < 4; addr_index++) {
            target->m32[addr_index] = htonl(target->m32[addr_index]);
        }
    }

    if (current_block_index != 7) {
        return UR_ERROR_PARSE;
    }

    return UR_ERROR_NONE;
}
