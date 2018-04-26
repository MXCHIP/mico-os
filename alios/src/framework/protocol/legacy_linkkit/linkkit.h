#ifndef __LINKKIT_H__
#define __LINKKIT_H__

#include <stdint.h>

enum {
    LINKKIT_EVENT_CLOUD_DISCONNECTED = 0,
    LINKKIT_EVENT_CLOUD_CONNECTED    = 1,
};

typedef struct {
    char *productKey;       /* produce key            */
    char *deviceName;       /* device name            */
    char *deviceSecret;     /* device secret          */

    int maxMsgSize;         /* max message size       */
    int maxMsgQueueSize;    /* max message queue size */

    int (*event_handler)(int event_type, void *ctx);
    int (*delete_subdev)(char *productKey, char *deviceName, void *ctx);
    void *ctx;
} linkkit_params_t;

int linkkit_init(linkkit_params_t *initParams);
int linkkit_exit(void);

typedef struct linkkit_dev_s
{
    void*      ctx;
} linkkit_dev_t;

typedef struct {
    int (*get_property)(char *in, char *out, int out_len, void *ctx);
    int (*set_property)(char *in, char *out, int out_len, void *ctx);

    int (*down_rawdata)(const void *data, int len, void *ctx);
} linkkit_ops_t;

linkkit_dev_t *linkkit_start(linkkit_ops_t *ops, void *ctx);
int linkkit_stop(linkkit_dev_t *dev);

int linkkit_register_service(linkkit_dev_t *dev, char *identifer, int (*service_cb)(char *in, char *out, int out_len, void *ctx), void *ctx);
int linkkit_unregister_service(linkkit_dev_t *dev, char *identifer);

int linkkit_dispatch(uint32_t timeout_ms);

int linkkit_post_event(linkkit_dev_t *dev, char *identifer, char *json);
int linkkit_post_property(linkkit_dev_t *dev, char *json);

int linkkit_upload_rawdata(linkkit_dev_t *dev, void *data, int len);

void linkkit_dumpinfo(void);

#endif
