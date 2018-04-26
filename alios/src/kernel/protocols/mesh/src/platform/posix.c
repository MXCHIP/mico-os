/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>

#include <semaphore.h>

#include <umesh.h>
#include <umesh_pal.h>

void *umesh_pal_malloc(int sz)
{
    return malloc(sz);
}

void umesh_pal_free(void *ptr)
{
    free(ptr);
}

static struct timeval sys_start_time;
static int32_t g_time_offset;
uint32_t umesh_pal_now_ms(void)
{
    struct timeval tv;
    long long ms;

    gettimeofday(&tv, NULL);
    timersub(&tv, &sys_start_time, &tv);
    ms = tv.tv_sec * 1000LL + tv.tv_usec / 1000;
    return ms;
}

uint32_t umesh_pal_get_timestamp(void)
{
    return umesh_pal_now_ms() + g_time_offset;
}

void umesh_pal_set_timestamp(uint32_t timestamp)
{
    g_time_offset = timestamp - umesh_pal_now_ms();
}

int umesh_pal_kv_get(const char *key, void *buf, int *len)
{
    return -1;
}

int umesh_pal_kv_set(const char *key, void *buf, int len, int sync)
{
    return -1;
}

void umesh_pal_log(const char *fmt, ...)
{
    va_list args;

    printf("[mesh][%06d] ", (unsigned)umesh_pal_now_ms());
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\r\n");
}

int umesh_pal_sem_new(pal_sem_hdl_t *hdl, int count)
{
    sem_t *s = malloc(sizeof(*s));
    sem_init(s, 0, count);
    *hdl = (long)s;
    return 0;
}

int umesh_pal_sem_wait(pal_sem_hdl_t *hdl, int ms)
{
    sem_t *sem;

    if (hdl == NULL) {
        return -EINVAL;
    }

    sem = (sem_t *)*hdl;
    if (ms < 0) {
        return sem_wait(sem);
    } else if (ms == 0) {
        return sem_trywait(sem);
    }

    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_nsec += ms * 1000 * 1000;
    ts.tv_sec += ts.tv_nsec / 1000000000;
    ts.tv_nsec %= 1000000000;

    return sem_timedwait(sem, &ts);
}

void umesh_pal_sem_signal(pal_sem_hdl_t *hdl)
{
    if (hdl == NULL) {
        return;
    }

    sem_post((sem_t *)*hdl);
}

void umesh_pal_sem_free(pal_sem_hdl_t *hdl)
{
    if (hdl == NULL) {
        return;
    }

    sem_destroy((sem_t *)*hdl);
    free((void *)*hdl);
}

void umesh_pal_post_event(int code, unsigned long value)
{
}

/*
 * security
 */
typedef void *umesh_aes_ctx_t;

ur_error_t umesh_pal_aes_encrypt(const uint8_t *key, uint8_t key_size,
                             const void *src,
                             uint16_t size, void *dst)
{
    ur_error_t error = UR_ERROR_NONE;
    return error;
}

ur_error_t umesh_pal_aes_decrypt(const uint8_t *key, uint8_t key_size,
                             const void *src,
                             uint16_t size, void *dst)
{
    ur_error_t error = UR_ERROR_NONE;
    return error;
}

/* init */
typedef struct {
    pal_sem_hdl_t sem;
    char *buf;
} cli_cookie_t;

static void handle_cli_response(char *buf, int len, void *priv)
{
    if (!buf && !len) {
        cli_cookie_t *cookie = priv;
        umesh_pal_sem_signal(&cookie->sem);
        return;
    }

    if (!len)
        return;

    printf("%s", (char *)buf);
}

static void umesh_command(char *buf)
{
    cli_cookie_t cookie = {};

    umesh_pal_sem_new(&cookie.sem, 0);
    cookie.buf = buf;
    umesh_cli_cmd(cookie.buf, strlen(cookie.buf), handle_cli_response, &cookie);

    umesh_pal_sem_wait(&cookie.sem, -1);
    umesh_pal_sem_free(&cookie.sem);
}

#include <readline/readline.h>
#include <readline/history.h>
#include <pthread.h>
static void *start_cli(void *st)
{
    while(1) {
        char *line = readline("umesh> ");
        if (!line) {
            break;
        }

        if (strlen(line) > 0)
            add_history(line);

        umesh_command(line);
    }

    return 0;
}

void umesh_pal_ready(void)
{
    pthread_t th;
    pthread_create(&th, NULL, start_cli, NULL);
}

void umesh_pal_init(void)
{
    gettimeofday(&sys_start_time, NULL);
}

