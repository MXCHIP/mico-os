/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#ifndef _SMART_UTILS_H_
#define _SMART_UTILS_H_

#include "os.h"

#include "zconfig_config.h"


#ifndef DEBUG
#define __zc_loglevel_printf(log_level, format, ...)    do { } while (0)
#else

#ifdef _PLATFORM_QCOM_
extern int cmnos_printf(const char *format, ...);
#define __zc_loglevel_printf(log_level, format, ...)    \
        do {                                                    \
            cmnos_printf(format, ##__VA_ARGS__);                    \
        } while (0)
#elif defined (_PLATFORM_ESPRESSIF_)
extern int printf(const char *format, ...);
#define __zc_loglevel_printf(log_level, format, ...)    \
        do {                                                    \
            printf(format, ##__VA_ARGS__);                  \
        } while (0)
#elif defined (_PLATFORM_REALTEK_) //realtek 8711
extern int printf(const char *format, ...);
#define __zc_loglevel_printf(log_level, format, ...)    \
        do {                                                    \
            printf(format, ##__VA_ARGS__);                  \
        } while (0)
#else
#define __zc_loglevel_printf(log_level, format, ...)    \
    do {                                                    \
            os_printf(format, ##__VA_ARGS__);       \
    } while (0)
#endif //end of _PLATFORM_QCOM_

#endif //end of ifndef DEBUG

//for library safety, close the log output
#define log(format, ...)                        __zc_loglevel_printf(LOGLEVEL_DEBUG, format, ##__VA_ARGS__)
#define info(format, ...)                       __zc_loglevel_printf(LOGLEVEL_INFO, format, ##__VA_ARGS__)
#define warn(format, ...)                       __zc_loglevel_printf(LOGLEVEL_WARN, format, ##__VA_ARGS__)
#define error(format, ...)                      __zc_loglevel_printf(LOGLEVEL_ERROR, format, ##__VA_ARGS__)

#define bug_on(condition, format, ...)                          \
do {                                            \
        if (condition) {                            \
            error("########BUG ON %d!!!\r\n", __LINE__);            \
            error(format, ##__VA_ARGS__);                   \
            while (1);                          \
        }                                       \
} while (0)

#define warn_on(condition, format, ...)                         \
do {                                            \
        if (condition) {                            \
            warn("########WARNING ON %d!!!\r\n", __LINE__);         \
            warn(format, ##__VA_ARGS__);                    \
        }                                   \
} while (0)

void dump_hex(unsigned char *data, int len, int tab_num);
void dump_ascii(unsigned char *data, int len, int tab_num);
void dump_mac(u8 *src, u8 *dst);



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
          int (*cmp_func)(const void *, const void *));

u16 zconfig_checksum(u8 *data, u8 len);
u16 zconfig_checksum_v2(u8 *data, u8 len);

char *zconfig_calc_tpsk(char *model, char *secret, char *tpsk, int tpsk_len);
#endif  // _UTILS_H_
