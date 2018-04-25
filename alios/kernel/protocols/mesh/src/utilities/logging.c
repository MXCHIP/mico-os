/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "umesh_utils.h"

static ur_log_level_t g_log_level = UR_LOG_LEVEL_INFO;

ur_log_level_t ur_log_get_level(void)
{
    return g_log_level;
}

void ur_log_set_level(ur_log_level_t level)
{
    g_log_level = level;
}
