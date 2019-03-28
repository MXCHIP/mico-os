#include <stdio.h>
#include "mkv.h"

enum
{
    MKV_CMD_SET,
    MKV_CMD_GET,
    MKV_CMD_DEL,
};

typedef struct
{
    uint8_t cmd;
    const char *key;
    void *val;
    int len;
    void *sem;
    int rc;
} mkv_msg_t;

static void *mkv_msg_queue;

static void mkv_daemon(void)
{
    mkv_msg_t *msg;

    while (1)
    {
        kv_gc();
        mkv_queue_pop(mkv_msg_queue, &msg);
        switch (msg->cmd)
        {
        case MKV_CMD_SET:
            msg->rc = kv_item_set(msg->key, (const void *)msg->val, msg->len);
            break;
        case MKV_CMD_GET:
            msg->rc = kv_item_get(msg->key, msg->val, (int32_t *)&msg->len);
            break;
        case MKV_CMD_DEL:
            msg->rc = kv_item_delete(msg->key);
            break;
        default:
            msg->rc = -1;
        }
        mkv_sem_release(msg->sem);
    }
}

int mkv_init(void)
{
    int rc;
    if ((rc = kv_init()) != 0)
        return rc;

    if ((mkv_msg_queue = mkv_queue_new(10)) == NULL)
        return -1;

    return mkv_thread_new(mkv_daemon);
}

int mkv_item_set(const char *key, const void *val, int len)
{
    int rc;
    mkv_msg_t *msg, msg_storage;

    msg = &msg_storage;
    msg->cmd = MKV_CMD_SET;
    msg->key = key;
    msg->val = (void *)val;
    msg->len = len;
    if ((msg->sem = mkv_sem_new()) == NULL)
        return -1;

    mkv_queue_push(mkv_msg_queue, &msg);

    mkv_sem_acquire(msg->sem);

    rc = msg->rc;

    mkv_sem_delete(msg->sem);

    return rc;
}

int mkv_item_get(const char *key, void *val, int *len)
{
    int rc;
    mkv_msg_t *msg, msg_storage;

    msg = &msg_storage;
    msg->cmd = MKV_CMD_GET;
    msg->key = key;
    msg->val = val;
    msg->len = *len;
    if ((msg->sem = mkv_sem_new()) == NULL)
        return -1;

    mkv_queue_push(mkv_msg_queue, &msg);

    mkv_sem_acquire(msg->sem);

    rc = msg->rc;
    *len = msg->len;

    mkv_sem_delete(msg->sem);

    return rc;
}

int mkv_item_delete(const char *key)
{
    int rc;
    mkv_msg_t *msg, msg_storage;

    msg = &msg_storage;
    msg->cmd = MKV_CMD_DEL;
    msg->key = key;
    if ((msg->sem = mkv_sem_new()) == NULL)
        return -1;

    mkv_queue_push(mkv_msg_queue, &msg);

    mkv_sem_acquire(msg->sem);

    rc = msg->rc;

    mkv_sem_delete(msg->sem);

    return rc;
}

void *kv_lock_create(void)
{
    return (void *)0xDEADBEEF;
}

int32_t kv_lock_free(void *lock)
{
    return 0;
}

int32_t kv_lock(void *lock)
{
    return 0;
}

int32_t kv_unlock(void *lock)
{
    return 0;
}

void *kv_sem_create(void)
{
    return (void *)0xDEADBEEF;
}

int32_t kv_sem_free(void *sem)
{
    return 0;
}

int32_t kv_sem_wait(void *sem)
{
    return 0;
}

int32_t kv_sem_post_all(void *sem)
{
    return 0;
}

int32_t kv_start_task(const char *name, void (*fn)(void *), void *arg,
                      uint32_t stack)
{
    return 0;
}

void kv_delete_task(void)
{
}
