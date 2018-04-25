/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>
#include "zconfig_lib.h"
#include "zconfig_utils.h"
#include "base64.h"

//isprint is missing in mico and qcom platform
#define isprint(data)   1

void dump_hex(unsigned char *data, int len, int tab_num)
{
    int i;
    for (i = 0; i < len; i++) {
        info("%02x ", data[i]);

        if (!((i + 1) % tab_num)) {
            info("  ");
        }
    }

    info("\r\n");
}

void dump_ascii(unsigned char *data, int len, int tab_num)
{
    int i;
    for (i = 0; i < len; i++) {
        if (isprint(data[i])) {
            info("%-2c ", data[i]);
        } else {
            info("-- ");
        }

        if (!((i + 1) % tab_num)) {
            info("  ");
        }
    }

    info("\r\n");
}

void dump_mac(u8 *src, u8 *dst)
{
    unsigned char *mac;

    mac = src;
    info("%02x:%02x:%02x:%02x:%02x:%02x > ",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    mac = dst;
    info("%02x:%02x:%02x:%02x:%02x:%02x\r\n",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    /* elimite compiler warning */
    mac = mac;
}
#if 0
static void generic_swap(char *a, char *b, int size)
{
    char t;

    do {
        t = *(char *)a;
        *(char *)a++ = *(char *)b;
        *(char *)b++ = t;
    } while (--size > 0);
}

/**
 * sort - sort an array of elements
 * @base: pointer to data to sort
 * @num: number of elements
 * @size: size of each element
 * @cmp_func: pointer to comparison function
 * @swap_func: pointer to swap function or NULL
 *
 * This function does a heapsort on the given array. You may provide a
 * swap_func function optimized to your element type.
 *
 * Sorting time is O(n log n) both on average and worst-case. While
 * qsort is about 20% faster on average, it suffers from exploitable
 * O(n*n) worst-case behavior and extra memory requirements that make
 * it less suitable for kernel use.
 */
void sort(char *base, int num, int size,
          int (*cmp_func)(const void *, const void *))
{
    /* pre-scale counters for performance */
    int i = (num / 2 - 1) * size, n = num * size, c, r;

    /* heapify */
    for ( ; i >= 0; i -= size) {
        for (r = i; r * 2 + size < n; r  = c) {
            c = r * 2 + size;
            if (c < n - size &&
                cmp_func(base + c, base + c + size) < 0) {
                c += size;
            }
            if (cmp_func(base + r, base + c) >= 0) {
                break;
            }
            generic_swap(base + r, base + c, size);
        }
    }

    /* sort */
    for (i = n - size; i > 0; i -= size) {
        generic_swap(base, base + i, size);
        for (r = 0; r * 2 + size < i; r = c) {
            c = r * 2 + size;
            if (c < i - size &&
                cmp_func(base + c, base + c + size) < 0) {
                c += size;
            }
            if (cmp_func(base + r, base + c) >= 0) {
                break;
            }
            generic_swap(base + r, base + c, size);
        }
    }
}
#endif

u16 zconfig_checksum(u8 *data, u8 len)
{
    u8 i;
    u16 sum = 0, res;

    for (i = 0; i < len; i++) {
        sum += data[i];
    }

    res = sum & (0x7F << 0);
    res |= (sum & (0x7F << 7)) << 1;

    return res;
}


/* for wps */
u16 zconfig_checksum_v2(u8 *data, u8 len)
{
    u8 i;
    u16 sum = 0, res;

    for (i = 0; i < len; i++) {
        sum += data[i];
    }

    res = sum & (0x7F << 0);
    res |= (sum & (0x7F << 7)) << 1;

    if (!(res & 0x00FF)) {
        res |= 0x0001;
    }
    if (!(res & 0xFF00)) {
        res |= 0x0100;
    }

    return res;
}

static const char *zc_auth_str[] = {
    "open",
    "shared",
    "wpa-psk",
    "wpa-8021x",
    "wpa2-psk",
    "wpa2-8021x",
    "wpa-psk - wpa2-psk",
};

const char *zconfig_auth_str(u8 auth)
{
    if (auth <= ZC_AUTH_TYPE_MAX) {
        return zc_auth_str[auth];
    } else {
        return "invalid auth";
    }
}

static const char *zc_encry_str[] = {
    "none",
    "wep",
    "tkip",
    "aes",
    "tkip-aes"
};

const char *zconfig_encry_str(u8 encry)
{
    if (encry <= ZC_ENC_TYPE_MAX) {
        return zc_encry_str[encry];
    } else {
        return "invalid encry";
    }
}

#ifndef WITHOUT_SHA256

/*
 * use sha256.c instead of sha2.c because of code size
 * details see the following
   zmjun@ubuntu:~/work/zconfig$ ls -l build-shared/sha2*
   -rw-rw-r-- 1 zmjun zmjun 3960 Mar 21 02:22 build-shared/sha256.o
   -rw-rw-r-- 1 zmjun zmjun 5164 Mar 21 02:22 build-shared/sha2.o
   zmjun@ubuntu:~/work/zconfig$ ls -l build-static/sha2*
   -rw-rw-r-- 1 zmjun zmjun 3820 Mar 21 02:22 build-static/sha256.o
   -rw-rw-r-- 1 zmjun zmjun 4904 Mar 21 02:22 build-static/sha2.o
 */

#include "sha256.h"
char *zconfig_calc_tpsk(char *model, char *secret, char *tpsk, int tpsk_len)
{
    u8 digest[SHA256_DIGEST_SIZE];
    int len = strlen(model) + strlen(secret);
    char *buffer = os_malloc(len);
    warn_on(!buffer, "os_malloc failed!\r\n");

    memcpy(buffer, model, strlen(model));
    memcpy(buffer + strlen(model), secret, strlen(secret));

    SHA256_hash(buffer, len, digest);
    os_free(buffer);
#if 0
    int i;
    for (i = 0; i < SHA256_DIGEST_SIZE; i++) {
        printf("%02x", digest[i]);
    }
    printf("\n");
#endif

    base64_encode((const unsigned char *)digest, SHA256_DIGEST_SIZE,
                  (unsigned char *)tpsk, &tpsk_len);

    return tpsk;
}
#endif  //ZCONFIG_SHA256
