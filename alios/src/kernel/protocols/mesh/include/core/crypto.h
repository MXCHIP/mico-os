/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef UMESH_CRYPTO_H
#define UMESH_CRYPTO_H

#define umesh_aes_encrypt umesh_pal_aes_encrypt
#define umesh_aes_decrypt umesh_pal_aes_decrypt

ur_error_t umesh_aes_encrypt(const uint8_t *key, uint8_t key_size,
                             const void *src,
                             uint16_t size, void *dst);
ur_error_t umesh_aes_decrypt(const uint8_t *key, uint8_t key_size,
                             const void *src,
                             uint16_t size, void *dst);

#endif  /* UMESH_CRYPTO_H */
