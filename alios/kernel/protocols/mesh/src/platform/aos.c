/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <aos/aos.h>
#ifdef CONFIG_ALICRYPTO
#include <ali_crypto.h>
#endif

#include <umesh.h>
#include <umesh_hal.h>
#include <umesh_pal.h>

/*
 * symbols to export
 */
AOS_EXPORT(ur_error_t, umesh_init, node_mode_t);
AOS_EXPORT(ur_error_t, umesh_start, void);
AOS_EXPORT(ur_error_t, umesh_stop, void);
AOS_EXPORT(uint8_t, umesh_get_device_state, void);
AOS_EXPORT(uint8_t, umesh_get_mode, void);
AOS_EXPORT(ur_error_t, umesh_set_mode, uint8_t);
AOS_EXPORT(const mac_address_t *, umesh_get_mac_address, media_type_t);
AOS_EXPORT(const void *, ur_adapter_get_default_ipaddr, void);
AOS_EXPORT(const void *, ur_adapter_get_mcast_ipaddr, void);

static uint32_t g_time_offset = 0;
void *umesh_pal_malloc(int sz)
{
    return aos_malloc(sz);
}

void umesh_pal_free(void *ptr)
{
    aos_free(ptr);
}

uint32_t umesh_pal_now_ms(void)
{
    return aos_now_ms();
}

uint32_t umesh_pal_get_timestamp(void)
{
    return aos_now_ms() + g_time_offset;
}

void umesh_pal_set_timestamp(uint32_t timestamp)
{
    g_time_offset = timestamp - umesh_pal_now_ms();
}

int umesh_pal_kv_get(const char *key, void *buf, int *len)
{
#ifdef AOS_KV
    return aos_kv_get(key, buf, len);
#else
    return -1;
#endif
}

int umesh_pal_kv_set(const char *key, void *buf, int len, int sync)
{
#ifdef AOS_KV
    return aos_kv_set(key, buf, len, sync);
#else
    return -1;
#endif
}

void umesh_pal_post_event(int code, unsigned long value)
{
    aos_post_event(EV_MESH, code, value);
}

void umesh_pal_log(const char *fmt, ...)
{
    va_list args;

    printf("[mesh][%06d] ", (unsigned)aos_now_ms());
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\r\n");
}

int umesh_pal_sem_new(pal_sem_hdl_t *hdl, int count)
{
    return aos_sem_new((aos_sem_t *)hdl, 1);
}

int umesh_pal_sem_wait(pal_sem_hdl_t *hdl, int ms)
{
    return aos_sem_wait((aos_sem_t *)hdl, ms < 0 ? AOS_WAIT_FOREVER : ms);
}

void umesh_pal_sem_signal(pal_sem_hdl_t *hdl)
{
    aos_sem_signal((aos_sem_t *)hdl);
}

void umesh_pal_sem_free(pal_sem_hdl_t *hdl)
{
    aos_sem_free((aos_sem_t *)hdl);
}

int umesh_pal_schedule_call(void (*task)(void *), void *arg)
{
    int ret;

    ret = aos_schedule_call(task, arg);

    return ret < 0 ? -1 : 0;
}

int umesh_pal_post_delayed_action(int ms, void (*handler)(void *arg), void *arg)
{
    return aos_post_delayed_action(ms, handler, arg);
}

void umesh_pal_cancel_delayed_action(int ms, void (*handler)(void *arg), void *arg)
{
    aos_cancel_delayed_action(ms, handler, arg);
}

/*
 * security
 */
typedef void *umesh_aes_ctx_t;

#ifdef CONFIG_ALICRYPTO
uint8_t g_umesh_iv[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static ur_error_t umesh_aes_encrypt_decrypt(umesh_aes_ctx_t *aes,
                                            const void *src,
                                            uint16_t size,
                                            void *dst)
{
    ali_crypto_result result;
    uint32_t dlen = 1024;

    if (aes == NULL) {
        return UR_ERROR_FAIL;
    }

    result = ali_aes_finish(src, size, dst, &dlen, SYM_NOPAD, aes);

    if (result != ALI_CRYPTO_SUCCESS) {
        return UR_ERROR_FAIL;
    }

    return UR_ERROR_NONE;
}
#endif

ur_error_t umesh_pal_aes_encrypt(const uint8_t *key, uint8_t key_size,
                             const void *src,
                             uint16_t size, void *dst)
{
    ur_error_t error = UR_ERROR_NONE;
#ifdef CONFIG_ALICRYPTO
    umesh_aes_ctx_t *aes;
    uint32_t aes_ctx_size;
    ali_crypto_result result;

    if (key == NULL || src == NULL || dst == NULL) {
        return UR_ERROR_FAIL;
    }

    result = ali_aes_get_ctx_size(AES_CTR, &aes_ctx_size);
    if (result != ALI_CRYPTO_SUCCESS) {
        return UR_ERROR_FAIL;
    }

    aes = aos_malloc(aes_ctx_size);
    if (aes == NULL) {
        return UR_ERROR_FAIL;
    }

    result = ali_aes_init(AES_CTR, true,
                          key, NULL, key_size, g_umesh_iv, aes);
    if (result != ALI_CRYPTO_SUCCESS) {
        aos_free(aes);
        return UR_ERROR_FAIL;
    }

    error = umesh_aes_encrypt_decrypt(aes, src, size, dst);
    aos_free(aes);
#endif

    return error;
}

ur_error_t umesh_pal_aes_decrypt(const uint8_t *key, uint8_t key_size,
                             const void *src,
                             uint16_t size, void *dst)
{
    ur_error_t error = UR_ERROR_NONE;
#ifdef CONFIG_ALICRYPTO
    umesh_aes_ctx_t *aes;
    uint32_t aes_ctx_size;
    ali_crypto_result result;

    if (key == NULL || src == NULL || dst == NULL) {
        return UR_ERROR_FAIL;
    }

    result = ali_aes_get_ctx_size(AES_CTR, &aes_ctx_size);
    if (result != ALI_CRYPTO_SUCCESS) {
        return UR_ERROR_FAIL;
    }

    aes = aos_malloc(aes_ctx_size);
    if (aes == NULL) {
        return UR_ERROR_FAIL;
    }

    result = ali_aes_init(AES_CTR, false,
                          key, NULL, key_size, g_umesh_iv, aes);
    if (result != ALI_CRYPTO_SUCCESS) {
        aos_free(aes);
        return UR_ERROR_FAIL;
    }

    error = umesh_aes_encrypt_decrypt(aes, src, size, dst);
    aos_free(aes);
#endif

    return error;
}

/* init */
typedef struct {
    aos_sem_t sem;
    char buf[128];
} cli_cookie_t;

static void handle_cli_response(char *buf, int len, void *priv)
{
    if (!buf && !len) {
        cli_cookie_t *cookie = priv;
        aos_sem_signal(&cookie->sem);
        return;
    }

    if (!len)
        return;

    aos_cli_printf("%s", (char *)buf);
}

static void umesh_command(char *pcWriteBuffer, int xWriteBufferLen, int argc,
                          char **argv)
{
    cli_cookie_t *cookie = aos_malloc(sizeof(*cookie));
    int i;

    aos_sem_new(&cookie->sem, 0);
    cookie->buf[0] = 0;
    for (i=1;i<argc;i++) {
        int left;

        left = sizeof(cookie->buf) - strlen(cookie->buf) - 1;
        strncat(cookie->buf, argv[i], left);
        left = sizeof(cookie->buf) - strlen(cookie->buf) - 1;
        strncat(cookie->buf, " ", left);
    }
    umesh_cli_cmd(cookie->buf, strlen(cookie->buf), handle_cli_response, cookie);

    aos_sem_wait(&cookie->sem, AOS_WAIT_FOREVER);
    aos_sem_free(&cookie->sem);
    aos_free(cookie);
}

static struct cli_command ncmd = {
    .name = "umesh",
    .help = "umesh [cmd]",
    .function = umesh_command,
};

void umesh_pal_ready(void)
{
    aos_cli_register_command(&ncmd);
}

void umesh_pal_init(void)
{
}

void umesh_pal_radio_sleep(void)
{
    hal_umesh_radio_sleep(NULL);
}

void umesh_pal_radio_wakeup(void)
{
    hal_umesh_radio_wakeup(NULL);
}
