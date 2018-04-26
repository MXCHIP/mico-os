/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef _UTILS_H_
#define _UTILS_H_

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#include "os.h"

/**
 * @brief string to hex
 *
 * @param[in] str: input hex string
 * @param[in] str_len: length of input hex string
 * @param[out] out: output hex byte stream
 * @param[in/out] output_len: [in] for output buffer size, [out] for
 *                 output hex byte len
 * @Note None.
 *
 * @retval return num of hex bytes converted, 0 means error.
 */
int utils_str_to_hex(const char *str, int str_len, uint8_t *out, int out_buf_len);

/**
 * @brief hex to string
 *
 * @param[in] buf: input hex byte stream
 * @param[in] buf_len: input stream length in byte
 * @param[out] str: encoded hex string
 * @param[in/out] str_len: [in] for str buffer size, [out] for
 *                  encoded string length
 * @Note output str buffer is NULL-terminated(if str_buf_len is longer enough)
 *
 * @retval return length of str converted, 0 means error.
 */
int utils_hex_to_str(const uint8_t *buf, int buf_len, char *str, int str_buf_len);

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif

#endif
