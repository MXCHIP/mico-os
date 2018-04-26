#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <aos/aos.h>

#include "iot_import.h"
#include "iot_export.h"
#include "json.h"
#include "linkkit.h"
#include "thread.h"
#include "linkkit_app.h"
#include "iot_import_product.h"

static void ota_init();
#if 0
#if 0
#define PRODUCT_KEY    "a1FQ6WqQqpS"
#define DEVICE_NAME    "eLYtwXSpTvlQ5dFJh92y"
#define DEVICE_SECRET  "bEwXvV5fsRfVDvf7Y2kZxTKR6CDEc4l8"
#else

#if 0
#define PRODUCT_KEY    "a1K6DIcooTl"
#define DEVICE_NAME    "ialFl3LEb5UtNCxWvoTV"
#define DEVICE_SECRET  "07OvBD3g4dLCzbzeeWO3yOmFYMa3xLZH"
#else
#define PRODUCT_KEY    "a1YuzBzBNte"
#define DEVICE_NAME    "68fS8slG2kHbhP6lpnME"
#define DEVICE_SECRET  "tx4miXUxdxzcdKhSY4nyRz7SZYXwNZeH"
#endif
#endif
#endif
#define DPRINT(...)                                      \
do {                                                     \
    printf("\033[1;31;40m%s.%d: ", __func__, __LINE__);  \
    printf(__VA_ARGS__);                                 \
    printf("\033[0m");                                   \
} while (0)

typedef struct {
    int WorkMode;
    int LightSwitch;

    int connected;

    linkkit_dev_t *lk_dev;
} sample_context_t;

typedef struct ota_device_info {
    const char *product_key;
    const char *device_name;
    void *pclient;
} OTA_device_info_t;

OTA_device_info_t ota_device_info;

static int smartled_get_property(char *in, char *out, int out_len, void *ctx)
{
    sample_context_t *sample = ctx;

    DPRINT("in: %s\n", in);

    AJsonObject root;
    if (AJson_Parse(&root, in, strlen(in)) < 0)
        return -1;

    char *prop = NULL;
    int   len = 0;

    char *ptr = out;
    char *end = out + out_len;

    strncat(ptr, "{", end - ptr);
    ptr += strlen(ptr);

    int count = 0;
    AJson_ForEachObjectInArray(&root, obj) {
        if (AJson_GetString(obj, &prop, &len) < 0)
            break;

        if (strncmp(prop, "WorkMode", len) == 0) {
            if (count > 0) {
                strncat(ptr, ", ", end - ptr);
                ptr += strlen(ptr);
            }

            ptr += snprintf(ptr, end - ptr, "\"WorkMode\": %d", sample->WorkMode);
            count++;
        } else if (strncmp(prop, "LightSwitch", len) == 0) {
            if (count > 0) {
                strncat(ptr, ", ", end - ptr);
                ptr += strlen(ptr);
            }

            ptr += snprintf(ptr, end - ptr, "\"LightSwitch\": %d", sample->WorkMode);
            count++;
        }
    }

    strncat(ptr, "}", end - ptr);
    ptr += strlen(ptr);

    if (count == 0)
        out[0] = '\0';

    DPRINT("out: %s\n", out);

    return 0;
}

static int smartled_set_property(char *in, char *out, int out_len, void *ctx)
{
    sample_context_t *sample = ctx;

    DPRINT("in: %s\n", in);

    AJsonObject root;
    if (AJson_Parse(&root, in, strlen(in)) < 0)
        return -1;

    AJson_ForEachObject(&root, k, k_len, obj) {
        if (strncmp(k, "WorkMode", k_len) == 0) {
            if (AJson_GetInt(obj, &sample->WorkMode) < 0)
                break;
        } else if (strncmp(k, "LightSwitch", k_len) == 0) {
            if (AJson_GetInt(obj, &sample->LightSwitch) < 0)
                break;
        }
    }

    DPRINT("sample->WorkMode = %d\n", sample->WorkMode);
    DPRINT("sample->LightSwitch = %d\n", sample->LightSwitch);

    linkkit_post_property(sample->lk_dev, in);

    return 0;
}

static void dump_hex(const void *data, int len)
{
    const uint8_t *d = data;

    printf("%d: ", len);
    int i;
    for (i = 0; i < len; i++) {
        printf("%02x ", d[i]);
    }
    printf("\n");
}

static int smartled_down_rawdata(const void *data, int len, void *ctx)
{
    if (len > 0)
        dump_hex(data, len);
    return 0;
}

static int post_all_properties(sample_context_t *sample)
{
    char prop[64] = {0};
    snprintf(prop, sizeof(prop),
            "{\"WorkMode\": %d, \"LightSwitch\": %d}",
            sample->WorkMode, sample->LightSwitch);
    return linkkit_post_property(sample->lk_dev, prop);
}

static int smartled_event_handler(int event_type, void *ctx)
{
    sample_context_t *sample = ctx;

    switch (event_type) {
    case LINKKIT_EVENT_CLOUD_CONNECTED:
        sample->connected = 1;
        ota_init();
        DPRINT("cloud connected\n");
        aos_post_event(EV_YUNIO, CODE_YUNIO_ON_CONNECTED, 0);
        post_all_properties(sample);
        break;
    case LINKKIT_EVENT_CLOUD_DISCONNECTED:
        sample->connected = 0;
        DPRINT("cloud disconnected\n");
        aos_post_event(EV_YUNIO, CODE_YUNIO_ON_DISCONNECTED, 0);
        break;
    default:
        break;
    }

    return 0;
}

static linkkit_ops_t alinkops = {
    .get_property = smartled_get_property,
    .set_property = smartled_set_property,
    .down_rawdata = smartled_down_rawdata,
};

static int SetLightSwitchTimer(char *in, char *out, int out_len, void *ctx)
{
    DPRINT("in: %s\n", in);
    return 0;
}

static void *dispatch_thread(void *params);

sample_context_t sample;

void *linkkit_thread(void *arg)
{
    memset(&sample, 0, sizeof(sample_context_t));

    sample.WorkMode = 1;
    sample.LightSwitch = 1;

    linkkit_params_t initParams = {
        .productKey = PRODUCT_KEY,
        .deviceName = DEVICE_NAME,
        .deviceSecret = DEVICE_SECRET,

        .maxMsgSize = 1200,
        .maxMsgQueueSize = 8,

        .event_handler = smartled_event_handler,
        .ctx = &sample,
    };

    if (linkkit_init(&initParams) < 0) {
        DPRINT("linkkit_init failed\n");
        return NULL;
    }

    sample.lk_dev = linkkit_start(&alinkops, &sample);
    if (!sample.lk_dev) {
        DPRINT("linkkit_start failed\n");
        return NULL;
    }

    linkkit_register_service(sample.lk_dev, "SetLightSwitchTimer", SetLightSwitchTimer, &sample);

    // TODO:[guangyong.lgy]Don't create linkkit thread
    //lk_thread_create("linkkit", 2048, dispatch_thread, &sample);
#if 0
    uint64_t prev_time = HAL_UptimeMs();

    while (1) {
        linkkit_dispatch(100);

        uint64_t now = HAL_UptimeMs();
        uint64_t itv = now - prev_time;
        if (itv > 10000) { /* post event every 10 seconds */
            linkkit_post_event(sample.lk_dev, "Error", "{\"errorCode\": 0}");
            prev_time = now;
        }
    }

    linkkit_stop(sample.lk_dev);
    linkkit_exit();
#endif
    return NULL;
}

static void *dispatch_thread(void *params)
{
    uint64_t prev_time = HAL_UptimeMs();

    sample_context_t *sample = params;

    while (1) {
        linkkit_dispatch(100);

        uint64_t now = HAL_UptimeMs();
        uint64_t itv = now - prev_time;
        if (itv > 10000) { /* post event every 10 seconds */
            //linkkit_post_event("Error", "{\"errorCode\": 0}");
            prev_time = now;
        }
    }

    return NULL;
}

int linkkit_app(void)
{
    //main_thread = lk_thread_create("linkkit", 8192, linkkit_thread, NULL);
    linkkit_thread(NULL);
    return 0;
}

static void ota_init()
{
    static int init=0;
    if(init){
        return;
    }
    init=1;
    ota_device_info.product_key=PRODUCT_KEY;
    ota_device_info.device_name=DEVICE_NAME;
    ota_device_info.pclient=NULL;
    aos_post_event(EV_SYS, CODE_SYS_ON_START_FOTA, (long unsigned int)&ota_device_info);
}
