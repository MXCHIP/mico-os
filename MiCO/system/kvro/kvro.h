#ifndef KVRO_H
#define KVRO_H

int kvro_init(void);
int kvro_item_get(const char *key, void *buffer, int *buffer_len);
int kvro_item_delete(const char *key);

#endif