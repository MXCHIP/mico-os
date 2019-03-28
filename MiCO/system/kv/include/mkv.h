/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef MKV_H
#define MKV_H

#include <stdint.h>

#include "kv_api.h"
#include "mkv_adapt.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the kv module
 *
 * @retrun 0 on success, otherwise will be failed.
 *
 */
int mkv_init(void);

/**
 * @brief De-initialize the kv module
 *
 * @retrun none
 */
void mkv_deinit(void);

/**
 * Add a new KV pair.
 *
 * @param[in]  key    the key of the KV pair.
 * @param[in]  val    the value of the KV pair.
 * @param[in]  len    the length of the value.
 *
 * @return  0 on success, negative error on failure.
 */
int mkv_item_set(const char *key, const void *val, int len);

/**
 * Get the KV pair's value stored in buffer by its key.
 *
 * @note: the buffer_len should be larger than the real length of the value,
 *        otherwise buffer would be NULL.
 *
 * @param[in]      key         the key of the KV pair to get.
 * @param[out]     val         the memory to store the value.
 * @param[in-out]  len         in: the length of the input buffer.
 *                             out: the real length of the value.
 *
 * @return  0 on success, negative error on failure.
 */
int mkv_item_get(const char *key, void *val, int *len);

/**
 * Delete the KV pair by its key.
 *
 * @param[in]  key  the key of the KV pair to delete.
 *
 * @return  0 on success, negative error on failure.
 */
int mkv_item_delete(const char *key);

/**
 * Delete the KV pair by its prefix.
 *
 * @param[in]  prefix  the prefix of the kv pair which is need to delete.
 *
 * @return  0 on success, negative error on failure.
 */
int mkv_item_delete_by_prefix(const char *prefix);

#ifdef __cplusplus
}
#endif

#endif  /* MKV_H */

