/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef _key_value_h_
#define _key_value_h_

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
extern "C"
{
#endif

/* The totally storage size for key-value store */
#ifndef CONFIG_AOS_KV_BUFFER_SIZE
#define KV_TOTAL_SIZE   (16 * 1024)
#else
#define KV_TOTAL_SIZE   CONFIG_AOS_KV_BUFFER_SIZE
#endif

/* The physical parition for key-value store */
#ifndef CONFIG_AOS_KV_PTN
#define KV_PTN    HAL_PARTITION_PARAMETER_2
#else
#define KV_PTN    CONFIG_AOS_KV_PTN
#endif

/**
 * @brief init the kv module.
 *
 * @param[in] none.
 *
 * @note: the default KV size is @HASH_TABLE_MAX_SIZE, the path to store
 *        the kv file is @KVFILE_PATH.
 * @retval  0 on success, otherwise -1 will be returned
 */
int aos_kv_init(void);

/**
 * @brief deinit the kv module.
 *
 * @param[in] none.
 *
 * @note: all the KV in RAM will be released.
 * @retval none.
 */
void aos_kv_deinit(void);

/**
 * Add a new KV pair.
 *
 * @param[in]  key    the key of the KV pair.
 * @param[in]  value  the value of the KV pair.
 * @param[in]  len    the length of the value.
 * @param[in]  sync   save the KV pair to flash right now (should always be 1).
 *
 * @return  0 on success, negative error on failure.
 */
int aos_kv_set(const char *key, const void *value, int len, int sync);

/**
 * Get the KV pair's value stored in buffer by its key.
 *
 * @note: the buffer_len should be larger than the real length of the value,
 *        otherwise buffer would be NULL.
 *
 * @param[in]      key         the key of the KV pair to get.
 * @param[out]     buffer      the memory to store the value.
 * @param[in-out]  buffer_len  in: the length of the input buffer.
 *                             out: the real length of the value.
 *
 * @return  0 on success, negative error on failure.
 */
int aos_kv_get(const char *key, void *buffer, int *buffer_len);

/**
 * Delete the KV pair by its key.
 *
 * @param[in]  key  the key of the KV pair to delete.
 *
 * @return  0 on success, negative error on failure.
 */
int aos_kv_del(const char *key);

#if defined(__cplusplus) /* If this is a C++ compiler, use C linkage */
}
#endif

#endif


