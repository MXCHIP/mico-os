/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "kvro.h"

#define KV_BLOCK_SIZE_BITS 12
#define KV_TOTAL_SIZE (16 * 1024)
#define KV_MAX_KEY_LEN 128
#define KV_MAX_VAL_LEN 512

/* Key-value function return code description */
#define KV_OK 0                     /* Successed */
#define KV_LOOP_CONTINUE 10000      /* Loop Continue */
#define KV_ERR_NO_SPACE -10001      /* The space is out of range */
#define KV_ERR_INVALID_PARAM -10002 /* The parameter is invalid */
#define KV_ERR_MALLOC_FAILED -10003 /* The os memory malloc error */
#define KV_ERR_NOT_FOUND -10004     /* Could not found the item */
#define KV_ERR_FLASH_READ -10005    /* The flash read operation error */
#define KV_ERR_FLASH_WRITE -10006   /* The flash write operation error */
#define KV_ERR_FLASH_ERASE -10007   /* The flash erase operation error */
#define KV_ERR_OS_LOCK -10008       /* The error related to os lock */
#define KV_ERR_OS_SEM -10009        /* The error related to os semaphose */

#define KV_ERR_ENCRYPT -10010     /* Data encryption error */
#define KV_ERR_DECRYPT -10011     /* Data decryption error */
#define KV_ERR_NOT_SUPPORT -10012 /* The function is not support yet */

#if (KV_BLOCK_SIZE_BITS >= 16) || (KV_TOTAL_SIZE >= 0x10000)
typedef uint32_t kvro_size_t; /* If more than 64KB */
#else
typedef uint16_t kvro_size_t;
#endif

#ifdef _WIN32
#define KVMAGR_PKD
#else
#define KVMAGR_PKD __attribute__((packed))
#endif

#ifdef _WIN32
#pragma pack(push, 1)
#endif

typedef struct _block_header_t
{
    uint8_t magic;
    uint8_t state;
    uint8_t reserved[2];
} KVMAGR_PKD block_hdr_t;

typedef struct _item_header_t
{
    uint8_t magic;
    uint8_t state;
    uint8_t crc;
    uint8_t key_len;
    uint16_t val_len;
    kvro_size_t origin_off;
} KVMAGR_PKD item_hdr_t;

#ifdef _WIN32
#pragma pack(pop)
#endif

#define KV_BLOCK_SIZE (1 << KV_BLOCK_SIZE_BITS)
#define KV_BLOCK_NUMS (KV_TOTAL_SIZE >> KV_BLOCK_SIZE_BITS)
#define KV_BLOCK_OFF_MASK ~(KV_BLOCK_SIZE - 1)
#define KV_BLOCK_HDR_SIZE sizeof(block_hdr_t)

#define KV_BLOCK_STATE_USED 0xCC
#define KV_BLOCK_STATE_CLEAN 0xEE
#define KV_BLOCK_STATE_DIRTY 0x44
#define INVALID_BLK_STATE(state) (((state) != KV_BLOCK_STATE_USED) &&  \
                                  ((state) != KV_BLOCK_STATE_CLEAN) && \
                                  ((state) != KV_BLOCK_STATE_DIRTY))

#define KV_ITEM_HDR_SIZE sizeof(item_hdr_t)
#define KV_ITEM_STATE_NORMAL 0xEE
#define KV_ALIGN_MASK ~(4 - 1)
#define KV_RESERVED_BLOCKS 1

#define KV_ALIGN(x) ((x + ~KV_ALIGN_MASK) & KV_ALIGN_MASK)

typedef struct _kvro_item_t
{
    item_hdr_t hdr;
    char *store;
    uint16_t len;
    kvro_size_t pos;
} kvro_item_t;

typedef struct _block_info_t
{
    kvro_size_t space;
    uint8_t state;
} block_info_t;

typedef struct _kvro_mgr_t
{
    uint8_t inited;
    uint8_t gc_trigger;
    uint8_t gc_waiter;
    uint8_t clean_blk_nums;
    kvro_size_t write_pos;
    void *gc_sem;
    void *lock;
    block_info_t block_info[KV_BLOCK_NUMS];
} kvro_mgr_t;

typedef struct _kvro_store_t
{
    char *p;
    int res;
    uint16_t len;
} kvro_store_t;

static const uint8_t KV_BLOCK_MAGIC_NUM = 'K';
static const uint8_t KV_ITEM_MAGIC_NUM = 'I';

typedef int (*item_func)(kvro_item_t *item, const char *key);

static kvro_mgr_t g_kvro_mgr;

kvro_item_t *kvro_item_traverse(item_func func, uint8_t blk_idx, const char *key);

void *kvro_malloc(uint32_t size);
void kvro_free(void *ptr);
int kvro_flash_read(uint32_t offset, void *buf, uint32_t nbytes);
/******************************************************/
/****************** Internal Interface ****************/
/******************************************************/

static void kvro_item_free(kvro_item_t *item)
{
    if (item)
    {
        if (item->store)
        {
            kvro_free(item->store);
        }
        kvro_free(item);
    }
}

static int __item_find_cb(kvro_item_t *item, const char *key)
{
    int res;

    if (item->hdr.key_len != strlen(key))
    {
        return KV_LOOP_CONTINUE;
    }

    item->store = (char *)kvro_malloc(item->hdr.key_len + item->hdr.val_len);
    if (!item->store)
    {
        return KV_ERR_MALLOC_FAILED;
    }

    res = kvro_flash_read(item->pos + KV_ITEM_HDR_SIZE, item->store,
                          item->len);
    if (res != KV_OK)
    {
        return KV_ERR_FLASH_READ;
    }

    if (memcmp(item->store, key, strlen(key)) == 0)
    {
        return KV_OK;
    }

    return KV_LOOP_CONTINUE;
}

static kvro_item_t *kvro_item_search(const char *key)
{
    uint8_t i;

    kvro_item_t *item = NULL;

    for (i = 0; i < KV_BLOCK_NUMS; i++)
    {
        if (g_kvro_mgr.block_info[i].state != KV_BLOCK_STATE_CLEAN)
        {
            item = kvro_item_traverse(__item_find_cb, i, key);
            if (item != NULL)
            {
                return item;
            }
        }
    }

    return NULL;
}

static int kvro_init_internal(void)
{
    uint8_t i;
    block_hdr_t hdr;

    for (i = 0; i < KV_BLOCK_NUMS; i++)
    {
        memset(&hdr, 0, KV_BLOCK_HDR_SIZE);
        kvro_flash_read((i << KV_BLOCK_SIZE_BITS), &hdr, KV_BLOCK_HDR_SIZE);
        if (hdr.magic == KV_BLOCK_MAGIC_NUM)
        {
            if (INVALID_BLK_STATE(hdr.state))
            {
                continue;
            }
            g_kvro_mgr.block_info[i].state = hdr.state;
        }
    }

    return KV_OK;
}

kvro_item_t *kvro_item_traverse(item_func func, uint8_t blk_idx, const char *key)
{
    int res;
    uint16_t len;
    kvro_size_t pos;
    kvro_size_t end;
    kvro_item_t *item;
    item_hdr_t *hdr;

    pos = (blk_idx << KV_BLOCK_SIZE_BITS) + KV_BLOCK_HDR_SIZE;
    end = (blk_idx << KV_BLOCK_SIZE_BITS) + KV_BLOCK_SIZE;
    len = 0;

    do
    {
        item = (kvro_item_t *)kvro_malloc(sizeof(kvro_item_t));
        if (!item)
        {
            return NULL;
        }

        memset(item, 0, sizeof(kvro_item_t));
        hdr = &(item->hdr);

        if (kvro_flash_read(pos, hdr, KV_ITEM_HDR_SIZE) != KV_OK)
        {
            kvro_item_free(item);
            return NULL;
        }

        if (hdr->magic != KV_ITEM_MAGIC_NUM)
        {
            if ((hdr->magic == 0xFF) && (hdr->state == 0xFF))
            {
                kvro_item_free(item);
                break;
            }
            hdr->val_len = 0xFFFF;
        }

        if ((hdr->val_len > KV_MAX_VAL_LEN) ||
            (hdr->key_len > KV_MAX_KEY_LEN) ||
            (hdr->val_len == 0) || (hdr->key_len == 0))
        {
            pos += KV_ITEM_HDR_SIZE;
            kvro_item_free(item);

            if (g_kvro_mgr.block_info[blk_idx].state == KV_BLOCK_STATE_USED)
            {
                g_kvro_mgr.block_info[blk_idx].state = KV_BLOCK_STATE_DIRTY;
            }
            continue;
        }

        len = KV_ALIGN(KV_ITEM_HDR_SIZE + hdr->key_len + hdr->val_len);

        if (hdr->state == KV_ITEM_STATE_NORMAL)
        {
            item->pos = pos;
            item->len = hdr->key_len + hdr->val_len;

            res = func(item, key);
            if (res == KV_OK)
            {
                return item;
            }
            else if (res != KV_LOOP_CONTINUE)
            {
                kvro_item_free(item);
                return NULL;
            }
        }
        else
        {
            if (g_kvro_mgr.block_info[blk_idx].state == KV_BLOCK_STATE_USED)
            {
                g_kvro_mgr.block_info[blk_idx].state = KV_BLOCK_STATE_DIRTY;
            }
        }

        kvro_item_free(item);
        pos += len;
    } while (end > (pos + KV_ITEM_HDR_SIZE));

    if (end > pos)
    {
        g_kvro_mgr.block_info[blk_idx].space = end - pos;
    }
    else
    {
        g_kvro_mgr.block_info[blk_idx].space = KV_ITEM_HDR_SIZE;
    }

    return NULL;
}

/******************************************************/
/****************** Public Interface ******************/
/******************************************************/

int kvro_init(void)
{
    int res;

    if (g_kvro_mgr.inited)
    {
        return KV_OK;
    }

    if (KV_BLOCK_NUMS <= KV_RESERVED_BLOCKS)
    {
        return KV_ERR_INVALID_PARAM;
    }

    memset(&g_kvro_mgr, 0, sizeof(kvro_mgr_t));

    if ((res = kvro_init_internal()) != KV_OK)
    {
        return res;
    }

    g_kvro_mgr.inited = 1;

    return KV_OK;
}

int kvro_item_get(const char *key, void *buffer, int *buffer_len)
{
    kvro_item_t *item = NULL;

    if (!key || !buffer || !buffer_len || (*buffer_len <= 0))
    {
        return KV_ERR_INVALID_PARAM;
    }

    item = kvro_item_search(key);

    if (!item)
    {
        return KV_ERR_NOT_FOUND;
    }

    if (*buffer_len < item->hdr.val_len)
    {
        *buffer_len = item->hdr.val_len;
        kvro_item_free(item);
        return KV_ERR_NO_SPACE;
    }
    else
    {
        memcpy(buffer, (item->store + item->hdr.key_len), item->hdr.val_len);
        *buffer_len = item->hdr.val_len;
    }

    kvro_item_free(item);

    return KV_OK;
}

static void print_val(char *val, int n)
{
    int i;
    for (i = 0; i < n && isascii(val[i]); i++)
        ;
    if (i == n)
    {
        printf("%s\r\n", val);
    }
    else
    {
        printf("(HEX) ");
        for (i = 0; i < n; i++)
        {
            printf("%02X ", val[i]);
        }
        printf("\r\n");
    }
}

static int __item_print_cb(kvro_item_t *item, const char *key)
{
    kvro_size_t off;

    char *p_key = NULL;
    char *p_val = NULL;

    p_key = (char *)kvro_malloc(item->hdr.key_len + 1);
    if (!p_key)
    {
        return KV_ERR_MALLOC_FAILED;
    }

    memset(p_key, 0, item->hdr.key_len + 1);
    off = item->pos + KV_ITEM_HDR_SIZE;
    kvro_flash_read(off, p_key, item->hdr.key_len);

    p_val = (char *)kvro_malloc(item->hdr.val_len + 1);
    if (!p_val)
    {
        kvro_free(p_key);
        return KV_ERR_MALLOC_FAILED;
    }

    memset(p_val, 0, item->hdr.val_len + 1);
    off = item->pos + KV_ITEM_HDR_SIZE + item->hdr.key_len;
    kvro_flash_read(off, p_val, item->hdr.val_len);

    printf("%s = ", p_key);
    print_val(p_val, item->hdr.val_len);

    kvro_free(p_key);
    kvro_free(p_val);

    return KV_LOOP_CONTINUE;
}

void handle_kvro_cmd(char *pwbuf, int blen, int argc, char **argv)
{
    int i = 0;
    int res = KV_OK;
    int len = KV_MAX_VAL_LEN;

    char *buffer = NULL;

    const char *rtype = argc > 1 ? argv[1] : "";

    if (strcmp(rtype, "get") == KV_OK)
    {
        if (argc != 3)
        {
            return;
        }

        buffer = (char *)kvro_malloc(KV_MAX_VAL_LEN);
        if (!buffer)
        {
            printf("there is no space\r\n");
            return;
        }

        memset(buffer, 0, KV_MAX_VAL_LEN);
        res = kvro_item_get(argv[2], buffer, &len);
        if (res != 0)
        {
            printf("cli: no paired kvro\r\n");
        }
        else
        {
            printf("value is ");
            print_val(buffer, len);
        }

        if (buffer)
        {
            kvro_free(buffer);
        }
    }
    else if (strcmp(rtype, "list") == KV_OK)
    {
        for (i = 0; i < KV_BLOCK_NUMS; i++)
        {
            kvro_item_traverse((item_func)__item_print_cb, i, NULL);
        }
    }

    return;
}