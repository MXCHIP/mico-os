/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <yunit.h>
#include <aos/aos.h>
#include <aos/kernel.h>

#include "umesh_hal.h"
#include "umesh_utils.h"

void test_hal_mesh_case(void)
{
    int32_t num;
    const ur_mem_stats_t *mem_stats = ur_mem_get_stats();

    num = mem_stats->num;
    hal_umesh_get_bcast_mtu(NULL);
    hal_umesh_get_ucast_mtu(NULL);
    hal_umesh_get_channel(NULL);
    hal_umesh_get_chnlist(NULL, NULL);
    hal_umesh_set_txpower(NULL, 2);
    hal_umesh_get_txpower(NULL);
    hal_umesh_get_extnetid(NULL, NULL);
    hal_umesh_get_stats(NULL);
    mem_stats = ur_mem_get_stats();
    YUNIT_ASSERT(num == mem_stats->num);
}
