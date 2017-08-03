# Copyright (C) 2008-2015, Marvell International Ltd.
# All Rights Reserved.

libs-y += libmdns

libmdns-objs-y := mdns_main.c mdns.c mdns_respond.c
libmdns-objs-y += mdns_respond_thread.c mdns_query.c
libmdns-objs-y += mdns_cli.c debug.c dname.c

libmdns-objs-$(CONFIG_ENABLE_TESTS)      += test.c
libmdns-cflags-$(CONFIG_ENABLE_TESTS)    += -DMDNS_TESTS

libmdns-cflags-$(CONFIG_MDNS_QUERY)      += -DMDNS_QUERY_API
libmdns-cflags-$(CONFIG_MDNS_DEBUG)      += -DMDNS_LOG -DMDNS_DBG
libmdns-cflags-$(CONFIG_MDNS_CHECK_ARGS) += -DMDNS_CHECK_ARGS
