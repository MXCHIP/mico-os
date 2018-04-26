/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "umesh_utils.h"

void ur_start_timer(ur_timer_t *timer, uint32_t dt, timer_handler_t handler, void *args)
{
    int ret;

    ur_stop_timer(timer, NULL);
    ret = umesh_pal_post_delayed_action(dt, handler, args);
    *timer = (ret == 0? handler: NULL);
}

void ur_stop_timer(ur_timer_t *timer, void *args)
{
    timer_handler_t handler;

    if (*timer != NULL) {
        handler = (timer_handler_t)(*timer);
        umesh_pal_cancel_delayed_action(-1, handler, args);
        *timer = NULL;
    }
}
