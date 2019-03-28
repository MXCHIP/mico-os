/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include "mkv.h"
#include "kv_api.h"
#include "kv_adapt.h"
#include "kv_types.h"

extern kv_item_t *kv_item_traverse(item_func func, uint8_t blk_idx, const char *key);

static int __item_print_cb(kv_item_t *item, const char *key)
{
    kv_size_t off;

    char *p_key = NULL;
    char *p_val = NULL;

    p_key = (char *)kv_malloc(item->hdr.key_len + 1);
    if (!p_key) {
        return KV_ERR_MALLOC_FAILED;
    }

    memset(p_key, 0, item->hdr.key_len + 1);
    off = item->pos + KV_ITEM_HDR_SIZE;
    kv_flash_read(off, p_key, item->hdr.key_len);

    p_val = (char *)kv_malloc(item->hdr.val_len + 1);
    if (!p_val) {
        kv_free(p_key);
        return KV_ERR_MALLOC_FAILED;
    }

    memset(p_val, 0, item->hdr.val_len + 1);
    off = item->pos + KV_ITEM_HDR_SIZE + item->hdr.key_len;
    kv_flash_read(off, p_val, item->hdr.val_len);

    printf("%s = %s\r\n", p_key, p_val);
    kv_free(p_key);
    kv_free(p_val);

    return KV_LOOP_CONTINUE;
}

void handle_kv_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    int i   = 0;
    int num = 0;
    int res = KV_OK;
    int len = KV_MAX_VAL_LEN;

    char *buffer = NULL;

    const char *rtype = argc > 1 ? argv[1] : "";

    if (strcmp(rtype, "set") == KV_OK) {
        if (argc != 4) {
            return;
        }

        res = mkv_item_set(argv[2], argv[3], strlen(argv[3]));
        if (res != KV_OK) {
            printf("cli set kv failed %d.\r\n", res);
        }
    } else if (strcmp(rtype, "get") == KV_OK) {
        if (argc != 3) {
            return;
        }

        buffer = (char *)kv_malloc(KV_MAX_VAL_LEN);
        if (!buffer) {
            printf("there is no space\r\n");
            return;
        }

        memset(buffer, 0, KV_MAX_VAL_LEN);
        res = mkv_item_get(argv[2], buffer, &len);
        if (res != 0) {
            printf("cli: no paired kv\r\n");
        } else {
            printf("value is %s\r\n", buffer);
        }

        if (buffer) {
            kv_free(buffer);
        }
    } else if (strcmp(rtype, "del") == KV_OK) {
        if (argc != 3) {
            return;
        }

        res = mkv_item_delete(argv[2]);
        if (res != KV_OK) {
            printf("cli kv del failed %d\r\n", res);
        }
    } else if (strcmp(rtype, "list") == KV_OK) {
        for (i = 0; i < KV_BLOCK_NUMS; i++) {
            kv_item_traverse((item_func)__item_print_cb, i, NULL);
        }
    } else if (strcmp(rtype, "seti") == KV_OK) {
        if (argc != 4) {
            return;
        }

        num = atoi(argv[3]);
        res = mkv_item_set(argv[2], (void *)(&num), sizeof(int));
        if (res != KV_OK) {
            printf("cli set integer kv failed %d.\r\n", res);
        }
    } else if (strcmp(rtype, "geti") == KV_OK) {
        num = 0;
        len = sizeof(int);

        if (argc != 3) {
            return;
        }

        res = mkv_item_get(argv[2], &num, &len);
        if (res != 0) {
            printf("cli: no paired kv\r\n");
        } else {
            printf("value is %d\r\n", num);
        }
    }

    return;
}
