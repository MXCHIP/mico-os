#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "iot_import.h"
#include "iot_export.h"

#include "mqtt_instance.h"
#include "queue.h"
#include "mempool.h"
#include "linkkit.h"
#include "lite-utils.h"
#include "json_parser.h"

#define TOPIC_MAXLEN    (128)

#define NELEMS(x)   (sizeof(x) / sizeof((x)[0]))

#ifdef LINKIT_DEBUG
#define DPRINT(...)                                      \
do {                                                     \
    printf("\033[1;31;40m%s.%d: ", __func__, __LINE__);  \
    printf(__VA_ARGS__);                                 \
    printf("\033[0m");                                   \
} while (0)
#else
#define DPRINT(...)
#endif
typedef struct lk_watcher_t {
    int request_id;
    void *sem;

    int code;
    struct lk_watcher_t *next;
} lk_watcher_t;

typedef struct mqtt_service_s {
    char *topic;
    char identifer[48];

    int (*service_cb)(char *in, char *out, int out_len, void *ctx);
    void *ctx;

    struct mqtt_service_s *next;
} lk_service_t;

typedef struct {
    char productKey[16];
    char deviceName[32];
    char deviceSecret[48];

    queue_t  *linkkit_msgq;
    AMemPool *k1mblks_pool;             /* 1KB memory block pool */
    AMemPool *watcher_pool;
    AMemPool *msgqblk_pool;

    int max_msg_size;
    int max_msgq_size;

    linkkit_ops_t lk_ops;
    void         *lk_ctx;

    lk_service_t *first_service;        /* service list head */
    lk_watcher_t *first_watcher;        /* watcher list head */

    void *watcher_lock;

    int connected;

    int (*event_handler)(int event_type, void *ctx);
    int (*delete_subdev)(char *productKey, char *deviceName, void *ctx);
    void *ctx;
} MqttContext;

static MqttContext mqtt;

enum {
    MSG_TYPE_SET_PROPERTY,
    MSG_TYPE_GET_PROPERTY,
    MSG_TYPE_CALL_SERVICE,

    MSG_TYPE_CLOUD_CONNECTED,
    MSG_TYPE_CLOUD_DISCONNECTED,
};

typedef struct {
    int type;   /* See MSG_TYPE_* */
    int id;

    char method[48];
    char topic[TOPIC_MAXLEN];

    char *params;
    int params_len;
} lk_msg_t;

typedef struct {
    char *topic;
    char *uri;
    void (*handler)(char *topic, int topic_len, void *payload, int payload_len, void *ctx);
} topic_map_t;

static void linkkit_call_service(char *topic, int topic_len, void *payload, int payload_len, void *ctx);
static void rawdata_reply_handler(char *topic, int topic_len, void *payload, int payload_len, void *ctx);
static void event_reply_handler(char *topic, int topic_len, void *payload, int payload_len, void *ctx);
static void down_rawdata_handler(char *topic, int topic_len, void *payload, int payload_len, void *ctx);

static int handle_message(lk_msg_t *m);

static topic_map_t alltopics[] = {
    { NULL, "/thing/service/property/set", linkkit_call_service  },
    { NULL, "/thing/service/property/get", linkkit_call_service  },
    { NULL, "/thing/model/up_raw_reply",   rawdata_reply_handler },
    { NULL, "/thing/event/+/post_reply",   event_reply_handler   },
    { NULL, "/thing/model/down_raw",       down_rawdata_handler  },
};

static lk_service_t *find_service(char *identifer)
{
    lk_service_t *s;
    for (s = mqtt.first_service; s; s = s->next) {
        if (strcmp(s->identifer, identifer) == 0)
            return s;
    }

    return NULL;
}

static void event_handle(int event, void *ctx)
{
    int type = 0;

    switch (event) {
    case MQTT_INSTANCE_EVENT_CONNECTED:
    case MQTT_INSTANCE_EVENT_DISCONNECTED:
        if (event == MQTT_INSTANCE_EVENT_CONNECTED) {
            type = MSG_TYPE_CLOUD_CONNECTED;
            mqtt.connected = 1;
        } else {
            type = MSG_TYPE_CLOUD_DISCONNECTED;
            mqtt.connected = 0;
        }

        lk_msg_t *m = AMemPool_Get(mqtt.msgqblk_pool);
        if (!m)
            return;
        memset(m, 0, sizeof(lk_msg_t));
        m->type = type;
        // TODO:[guangyong.lgy]Call handle_message directly
        //queue_put(mqtt.linkkit_msgq, m);
        handle_message(m);
        break;
    default:
        break;
    }
}

static int topic_printf(char *topic, int topic_len, const char *fmt, ...)
{
    char *ptr = topic;
    char *end = topic + topic_len;

    snprintf(ptr, topic_len, "/sys/%s/%s", mqtt.productKey, mqtt.deviceName);
    ptr += strlen(ptr);

    va_list ap;
    va_start(ap, fmt);
    ptr += vsnprintf(ptr, end - ptr, fmt, ap);
    va_end(ap);

    return ptr - topic;
}

#define RESPONSE_FMT    "{\"id\": \"%d\", \"code\": %d, \"data\": %s}"

static int linkkit_send_response(char *topic, int id, int code, char *data)
{
    if (data == NULL || strlen(data) == 0)
        data = "{}";

    char *msg_pub = AMemPool_Get(mqtt.k1mblks_pool);
    if (!msg_pub)
        return -1;
    snprintf(msg_pub, mqtt.max_msg_size, RESPONSE_FMT, id, code, data);

    int ret = mqtt_publish(topic, 1, msg_pub, strlen(msg_pub));

    AMemPool_Put(mqtt.k1mblks_pool, msg_pub);

    return ret;
}

static int linkkit_parse_request(char *in, int in_len, char *id, int id_len, char *ver, int ver_len,
                                 char **method, int *method_len, char **params, int *params_len)
{
    int len;
    char *id_str = json_get_value_by_name(in, in_len, "id", &len, NULL);
    if (id_str && len < id_len)
        strncpy(id, id_str, len);

    char *ver_str = json_get_value_by_name(in, in_len, "version", &len, NULL);
    if (ver_str && len < ver_len)
        strncpy(ver, ver_str, len);

    *method = json_get_value_by_name(in, in_len, "method", method_len, NULL);
    *params = json_get_value_by_name(in, in_len, "params", params_len, NULL);

    DPRINT("     id: %s\n", id);
    DPRINT("version: %s\n", ver);
    DPRINT(" method: %.*s\n", *method_len, *method);
    DPRINT(" params: %.*s\n", *params_len, *params);

    return 0;
}

static int linkkit_parse_reply(char *in, int in_len, char *id, int id_len, int *code, char **msg, int *msg_len)
{
    int len;
    char *id_str = json_get_value_by_name(in, in_len, "id", &len, NULL);
    if (id_str && len < id_len)
        strncpy(id, id_str, len);

    char *code_str = json_get_value_by_name(in, in_len, "code", &len, NULL);
    if (code_str)
        *code = atoi(code_str);

    *msg = json_get_value_by_name(in, in_len, "message", msg_len, NULL);

    DPRINT("     id: %s\n",   id);
    DPRINT("   code: %d\n",  *code);
    DPRINT("message: %.*s\n", *msg_len, *msg);

    return 0;
}

static void linkkit_call_service(char *topic, int topic_len, void *payload, int payload_len, void *ctx)
{
    /* print topic name and topic message */
    DPRINT("----\n");
    DPRINT("  Topic: '%.*s' (Length: %d)\n", topic_len, topic, topic_len);
    DPRINT("Payload: '%.*s' (Length: %d)\n", payload_len, (char *)payload, payload_len);
    DPRINT("----\n");

    char reqid[16]   = {0};
    char version[16] = {0};

    char *method = NULL;
    int   method_len = 0;

    char *params = NULL;
    int   params_len = 0;

    linkkit_parse_request(payload, payload_len,
                          reqid,   sizeof(reqid),
                          version, sizeof(version),
                          &method, &method_len,
                          &params, &params_len);

    if (strlen(reqid) == 0 /*|| strlen(version) == 0*/ || strlen(method) == 0 || params_len <= 0) {
        DPRINT("parse input failed\n");
        return;
    }

    lk_msg_t *m = AMemPool_Get(mqtt.msgqblk_pool);
    if (!m)
        return;
    memset(m, 0, sizeof(lk_msg_t));

    if (strstr(topic, "property/get") != NULL) {
        m->type = MSG_TYPE_GET_PROPERTY;
    } else if (strstr(topic, "property/set") != NULL) {
        m->type = MSG_TYPE_SET_PROPERTY;
    } else {
        m->type = MSG_TYPE_CALL_SERVICE;
    }

    m->id = atoi(reqid);

    if (method_len > sizeof(m->method) - 1) {
        AMemPool_Put(mqtt.msgqblk_pool, m);
        DPRINT("method too long: %.*s\n", method_len, method);
        return;
    }

    strncpy(m->method, method, method_len);

    if (topic_len > sizeof(m->topic) - 1) {
        AMemPool_Put(mqtt.msgqblk_pool, m);
        DPRINT("topic too long: %.*s\n", method_len, method);
        return;
    }

    strncpy(m->topic, topic, topic_len);

    if (params_len > 0) {
        if (params_len > mqtt.max_msg_size - 1) {
            AMemPool_Put(mqtt.msgqblk_pool, m);
            DPRINT("params too long: %.*s\n", params_len, params);
            return;
        }

        m->params = AMemPool_Get(mqtt.k1mblks_pool);  /* 1 for '\0' */
        if (!m->params) {
            AMemPool_Put(mqtt.msgqblk_pool, m);
            DPRINT("AMemPool_Get failed\n");
            return;
        }

        strncpy(m->params, params, params_len);
        m->params[params_len] = '\0';
    }
    // TODO:[guangyong.lgy]Call handle_message directly
    //queue_put(mqtt.linkkit_msgq, m);
    handle_message(m);
}

static lk_watcher_t *create_watcher(int request_id)
{
    lk_watcher_t *watcher = AMemPool_Get(mqtt.watcher_pool);
    if (!watcher)
        return NULL;
    memset(watcher, 0, sizeof(lk_watcher_t));

    watcher->request_id = request_id;

    watcher->sem = HAL_SemaphoreCreate();
    if (!watcher->sem) {
        AMemPool_Put(mqtt.watcher_pool, watcher);
        return NULL;
    }

    HAL_MutexLock(mqtt.watcher_lock);
    watcher->next = mqtt.first_watcher;
    mqtt.first_watcher = watcher;
    HAL_MutexUnlock(mqtt.watcher_lock);

    return watcher;
}

static int destroy_watcher(lk_watcher_t *w)
{
    lk_watcher_t **wp, *w1;

    HAL_MutexLock(mqtt.watcher_lock);
    wp = &mqtt.first_watcher;
    while ((*wp) != NULL) {
        w1 = *wp;
        if (w1 == w)
            *wp = w->next;
        else
            wp = &w1->next;
    }
    HAL_MutexUnlock(mqtt.watcher_lock);

    HAL_SemaphoreDestroy(w->sem);

    AMemPool_Put(mqtt.watcher_pool, w);

    return 0;
}

static int wait_watcher(lk_watcher_t *watcher, int *code)
{
    if (HAL_SemaphoreWait(watcher->sem, 5000) < 0) {
        DPRINT("sem_wait_ms timeout\n");
        return -1;
    }

    *code = watcher->code;

    return 0;
}

static int wakeup_watcher(int request_id, int code)
{
    lk_watcher_t *w, *w_next;

    HAL_MutexLock(mqtt.watcher_lock);
    for (w = mqtt.first_watcher; w; w = w_next) {
        w_next = w->next;

        if (w->request_id == request_id) {
            w->code = code;
            HAL_SemaphorePost(w->sem);
            break;
        }
    }
    HAL_MutexUnlock(mqtt.watcher_lock);

    return 0;
}

static void event_reply_handler(char *topic, int topic_len, void *payload, int payload_len, void *ctx)
{
    DPRINT("event_reply: payload: %.*s\n", payload_len, (char *)payload);

    char id[16] = {0};
    int code = 0;
    char *message = NULL;
    int message_len = 0;

    linkkit_parse_reply((char *)payload, payload_len,
                         id, sizeof(id), &code, &message, &message_len);

    //wakeup_watcher(atoi(id), code);
}

static void rawdata_reply_handler(char *topic, int topic_len, void *payload, int payload_len, void *ctx)
{
    uint8_t *ptr = payload;

    DPRINT("%d: ", payload_len);

    int i;
    for (i = 0; i < payload_len; i++) {
        printf("%02x ", ptr[i]);
    }

    printf("\n");
}

static void down_rawdata_handler(char *topic, int topic_len, void *payload, int payload_len, void *ctx)
{
    if (mqtt.lk_ops.down_rawdata)
        mqtt.lk_ops.down_rawdata(payload, payload_len, mqtt.lk_ctx);

    char reply_topic[TOPIC_MAXLEN];
    topic_printf(reply_topic, sizeof(reply_topic), "/thing/model/down_raw_reply");

    linkkit_send_response(reply_topic, 123, 200, NULL);
}

static int mqtt_subscribe_all(void)
{
    int i;
    for (i = 0; i < NELEMS(alltopics); i++) {
        topic_map_t *map = &alltopics[i];
        int ret = mqtt_subscribe(map->topic, map->handler, NULL);
        if (ret < 0) {
            DPRINT("mqtt_subscribe(%s) failed, ret = %d\n", map->topic, ret);
        }
    }

    return 0;
}

static int mqtt_unsubscribe_all(void)
{
    int i;
    for (i = 0; i < NELEMS(alltopics); i++) {
        mqtt_unsubscribe(alltopics[i].topic);
    }

    return 0;
}

#define TOPIC_PREFIX  "/sys/%s/%s"

static int calculate_topic_size(char *uri)
{
    int size = 0;

    size += strlen(TOPIC_PREFIX);
    size += strlen(mqtt.productKey);
    size += strlen(mqtt.deviceName);
    size += strlen(uri);

    return size;
}

static char *topic_strcpy(char *uri)
{
    int len = calculate_topic_size(uri);

    char *topic = LITE_malloc(len);
    if (!topic)
        return NULL;

    topic_printf(topic, len, uri);

    return topic;
}

int linkkit_init(linkkit_params_t *initParams)
{
    memset(&mqtt, 0, sizeof(MqttContext));

    strncpy(mqtt.productKey,   initParams->productKey,   sizeof(mqtt.productKey)   - 1);
    strncpy(mqtt.deviceName,   initParams->deviceName,   sizeof(mqtt.deviceName)   - 1);
    strncpy(mqtt.deviceSecret, initParams->deviceSecret, sizeof(mqtt.deviceSecret) - 1);

    mqtt.max_msg_size = initParams->maxMsgSize;
    mqtt.max_msgq_size = initParams->maxMsgQueueSize;

    mqtt.event_handler = initParams->event_handler;
    mqtt.delete_subdev = initParams->delete_subdev;
    mqtt.ctx           = initParams->ctx;

    mqtt.watcher_lock = HAL_MutexCreate();
    if (!mqtt.watcher_lock)
        goto fail;

    mqtt.linkkit_msgq = queue_new(mqtt.max_msgq_size);
    if (!mqtt.linkkit_msgq)
        goto fail;

    mqtt.msgqblk_pool = AMemPool_New(sizeof(lk_msg_t), 4, NULL, NULL);
    if (!mqtt.msgqblk_pool)
        goto fail;

    mqtt.k1mblks_pool = AMemPool_New(mqtt.max_msg_size, 2, NULL, NULL);
    if (!mqtt.k1mblks_pool)
        goto fail;

    mqtt.watcher_pool = AMemPool_New(sizeof(lk_watcher_t), 4, NULL, NULL);
    if (!mqtt.watcher_pool)
        goto fail;

    /*
     * init topic table
     */
    int i;
    for (i = 0; i < NELEMS(alltopics); i++) {
        topic_map_t *map = &alltopics[i];
        map->topic = topic_strcpy(map->uri);
        if (!map->topic)
            goto fail;
    }

    return 0;

fail:
    linkkit_exit();
    return -1;
}

int linkkit_exit(void)
{
    if (mqtt.msgqblk_pool) {
        AMemPool_Free(mqtt.msgqblk_pool);
        mqtt.msgqblk_pool = NULL;
    }

    if (mqtt.k1mblks_pool) {
        AMemPool_Free(mqtt.k1mblks_pool);
        mqtt.k1mblks_pool = NULL;
    }

    if (mqtt.watcher_pool) {
        AMemPool_Free(mqtt.watcher_pool);
        mqtt.watcher_pool = NULL;
    }

    if (mqtt.linkkit_msgq) {
        queue_free(mqtt.linkkit_msgq);
        mqtt.linkkit_msgq = NULL;
    }

    if (mqtt.watcher_lock) {
        HAL_MutexDestroy(mqtt.watcher_lock);
        mqtt.watcher_lock = NULL;
    }

    int i;
    for (i = 0; i < NELEMS(alltopics); i++) {
        topic_map_t *map = &alltopics[i];
        if (map->topic)
            LITE_free(map->topic);
    }

    memset(&mqtt, 0, sizeof(MqttContext));

    return 0;
}

#define SERVICE_TOPIC "/thing/service/%s"

inline static int calculate_service_topic_size(char *identifer)
{
    int size = 0;

    size += strlen(TOPIC_PREFIX);
    size += strlen(SERVICE_TOPIC);
    size += strlen(mqtt.productKey);
    size += strlen(mqtt.deviceName);
    size += strlen(identifer);

    return size;
}

int linkkit_register_service(linkkit_dev_t *dev, char *identifer, int (*service_cb)(char *in, char *out, int out_len, void *ctx), void *ctx)
{
    if (find_service(identifer) != NULL)
        return -1;  /* service allready exist */

    lk_service_t *srv = LITE_malloc(sizeof(lk_service_t));
    if (!srv)
        return -1;
    memset(srv, 0, sizeof(lk_service_t));

    int topic_size = calculate_service_topic_size(identifer);

    srv->topic = LITE_malloc(topic_size);
    if (!srv->topic) {
        LITE_free(srv);
        return -1;
    }

    if (strlen(identifer) >= sizeof(srv->identifer)) {
        LITE_free(srv->topic);
        LITE_free(srv);
        assert(0);
        return -1;
    }

    strncpy(srv->identifer, identifer, sizeof(srv->identifer) - 1);
    srv->service_cb = service_cb;

    topic_printf(srv->topic, topic_size, SERVICE_TOPIC, identifer);

    /* Subscribe the specific topic */
    int ret = mqtt_subscribe(srv->topic, linkkit_call_service, NULL);
    if (ret < 0) {
        DPRINT("mqtt_subscribe(%s) failed, ret = %d\n", srv->topic, ret);
        LITE_free(srv->topic);
        LITE_free(srv);
        return -1;
    }

    srv->next = mqtt.first_service;
    mqtt.first_service = srv;

    return 0;
}

int linkkit_unregister_service(linkkit_dev_t *dev, char *identifer)
{
    lk_service_t *s = find_service(identifer);
    if (!s)
        return -1;

    lk_service_t **sp, *s1;

    sp = &mqtt.first_service;
    while ((*sp) != NULL) {
        s1 = *sp;
        if (s1 == s)
            *sp = s->next;
        else
            sp = &s1->next;
    }

    mqtt_unsubscribe(s->topic);

    LITE_free(s->topic);
    LITE_free(s);

    return 0;
}

linkkit_dev_t *linkkit_start(linkkit_ops_t *ops, void *ctx)
{
    if (!ops)
        return NULL;

    linkkit_dev_t *dev = LITE_malloc(sizeof(linkkit_dev_t));
    if (!dev)
        return NULL;
    memset(dev, 0, sizeof(linkkit_dev_t));

    mqtt.lk_ops = *ops;
    mqtt.lk_ctx = ctx;

    if (mqtt_init_instance(mqtt.productKey, mqtt.deviceName, mqtt.deviceSecret, mqtt.max_msg_size) < 0) {
        DPRINT("mqtt_init_instance failed\n");
        LITE_free(dev);
        return NULL;
    }

    mqtt_set_event_cb(event_handle, ctx);

    if (mqtt_subscribe_all() < 0) {
        LITE_free(dev);
        return NULL;
    }

    if (mqtt.event_handler) {
        mqtt.connected = 1;
        mqtt.event_handler(LINKKIT_EVENT_CLOUD_CONNECTED, mqtt.ctx);
    }

    return dev;
}

int linkkit_stop(linkkit_dev_t *dev)
{
    mqtt_unsubscribe_all();

    mqtt_deinit_instance();

    LITE_free(dev);

    return 0;
}

#define REQUEST_FMT "{\"id\": \"%d\", \"version\": \"1.0\", \"params\": %s, \"method\": \"thing.event.%s.post\"}"

static int gbl_id = 0;

int linkkit_post_event(linkkit_dev_t *dev, char *identifer, char *json)
{
    if (!mqtt.connected)
        return -1;

    int ret = -1;

    char *request = AMemPool_Get(mqtt.k1mblks_pool);
    if (!request)
        return -1;

    int id = gbl_id++;

    lk_watcher_t *watcher = create_watcher(id);
    if (!watcher) {
        AMemPool_Put(mqtt.k1mblks_pool, request);
        return -1;
    }

    snprintf(request, mqtt.max_msg_size, REQUEST_FMT, id, json, identifer);

    DPRINT("request: %s\n", request);

    char topic[TOPIC_MAXLEN] = {0};
    topic_printf(topic, sizeof(topic), "/thing/event/%s/post", identifer);

    if (mqtt_publish(topic, 1, request, strlen(request)) < 0) {
        DPRINT("mqtt_publish failed\n");
        goto fail;
    }

//    int code;
//    if (wait_watcher(watcher, &code) < 0)
//        goto fail;

//    ret = code == 200 ? 0 : -1;
      ret = 0;

fail:
    destroy_watcher(watcher);
    AMemPool_Put(mqtt.k1mblks_pool, request);

    return ret;
}

int linkkit_post_property(linkkit_dev_t *dev, char *json)
{
    return linkkit_post_event(dev, "property", json);
}

int linkkit_upload_rawdata(linkkit_dev_t *dev, void *data, int len)
{
    if (!mqtt.connected)
        return -1;

    char topic[TOPIC_MAXLEN] = {0};
    topic_printf(topic, sizeof(topic), "/thing/model/up_raw");

    return mqtt_publish(topic, 1, data, len);
}

static int handle_message(lk_msg_t *m)
{
    if (m->type == MSG_TYPE_CLOUD_CONNECTED) {
        if (mqtt.event_handler)
            mqtt.event_handler(LINKKIT_EVENT_CLOUD_CONNECTED, mqtt.ctx);
        return 0;
    } else if (m->type == MSG_TYPE_CLOUD_DISCONNECTED) {
        if (mqtt.event_handler)
            mqtt.event_handler(LINKKIT_EVENT_CLOUD_DISCONNECTED, mqtt.ctx);
        return 0;
    }

    char *identifer = NULL;

    char *out_json = AMemPool_Get(mqtt.k1mblks_pool);  /* 1 for '\0' */
    if (!out_json)
        return -1;
    out_json[0] = '\0';
    out_json[mqtt.max_msg_size - 1] = '\0';

    char topic[TOPIC_MAXLEN] = {0};
    snprintf(topic, sizeof(topic), "%s_reply", m->topic);

    switch (m->type) {
    case MSG_TYPE_GET_PROPERTY:
        if (mqtt.lk_ops.get_property)
            mqtt.lk_ops.get_property(m->params, out_json, mqtt.max_msg_size - 1, mqtt.lk_ctx);
        break;
    case MSG_TYPE_SET_PROPERTY:
        if (mqtt.lk_ops.set_property)
            mqtt.lk_ops.set_property(m->params, out_json, mqtt.max_msg_size - 1, mqtt.lk_ctx);
        break;
    default:
        identifer = m->method + strlen("thing.service.");
        lk_service_t *srv = find_service(identifer);
        if (!srv) {
            linkkit_send_response(topic, m->id, 202, out_json);
            AMemPool_Put(mqtt.k1mblks_pool, out_json);
            return -1;
        }
        srv->service_cb(m->params, out_json, mqtt.max_msg_size - 1, mqtt.lk_ctx);
    }

    linkkit_send_response(topic, m->id, 200, out_json);

    AMemPool_Put(mqtt.k1mblks_pool, out_json);

    if (m->params)
        AMemPool_Put(mqtt.k1mblks_pool, m->params);
    AMemPool_Put(mqtt.msgqblk_pool, m);

    return 0;
}

int linkkit_dispatch(uint32_t timeout_ms)
{
    while (1) {
        lk_msg_t *m = queue_get(mqtt.linkkit_msgq, timeout_ms);
        if (!m)
            break;

        handle_message(m);

        if (m->params)
            AMemPool_Put(mqtt.k1mblks_pool, m->params);
        AMemPool_Put(mqtt.msgqblk_pool, m);
    }

    return 0;
}

void linkkit_dumpinfo(void)
{
    int k1_used = AMemPool_NumUsed(mqtt.k1mblks_pool);
    int wp_used = AMemPool_NumUsed(mqtt.watcher_pool);
    int mq_used = AMemPool_NumUsed(mqtt.msgqblk_pool);

    int k1_free = AMemPool_NumFree(mqtt.k1mblks_pool);
    int wp_free = AMemPool_NumFree(mqtt.watcher_pool);
    int mq_free = AMemPool_NumFree(mqtt.msgqblk_pool);

    DPRINT("k1: %dused %dfree\n", k1_used, k1_free);
    DPRINT("wp: %dused %dfree\n", wp_used, wp_free);
    DPRINT("mq: %dused %dfree\n", mq_used, mq_free);
}
