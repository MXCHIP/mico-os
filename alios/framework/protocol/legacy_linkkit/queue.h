#ifndef __LITE_TMP_QUEUE_H__
#define __LITE_TMP_QUEUE_H__

typedef struct queue_s queue_t;

queue_t *queue_new(int max_nodes);
int queue_free(queue_t *q);

void *queue_get(queue_t *q, uint32_t timeout_ms);
int queue_put(queue_t *q, void *data);

#endif /* __LITE_TMP_QUEUE_H__ */
