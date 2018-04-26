/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <string.h>

#include "umesh_utils.h"

enum {
    CONFIGS_MAGIC_NUMBER = 0xbe5cc5ec,
    CONFIGS_VERSION      = 1,
};

ur_error_t ur_configs_read(ur_configs_t *config)
{
    int ret = -1;
    int len = sizeof(*config);

    if (config == NULL) {
        return UR_ERROR_FAIL;
    }

    ret = umesh_kv_get("umesh", config, &len);
    if (ret < 0) {
        return UR_ERROR_FAIL;
    }
    if (config->magic_number == CONFIGS_MAGIC_NUMBER &&
        config->version == CONFIGS_VERSION) {
        return UR_ERROR_NONE;
    }
    memset(config, 0xff, sizeof(ur_configs_t));
    return UR_ERROR_FAIL;
}

ur_error_t ur_configs_write(ur_configs_t *config)
{
    int ret = -1;

    if (config == NULL) {
        return UR_ERROR_FAIL;
    }

    config->magic_number = CONFIGS_MAGIC_NUMBER;
    config->version = CONFIGS_VERSION;

    ret = umesh_kv_set("umesh", config, sizeof(*config), 1);
    if (ret < 0) {
        return UR_ERROR_FAIL;
    }

    return UR_ERROR_NONE;
}
