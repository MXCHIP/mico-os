/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include <cpu_event.h>

#include "umesh.h"
#include "umesh_utils.h"

#define DEVTAP "/dev/net/tun"
#define DEVTAP_DEFAULT_IF "tun0"

static struct {
    pthread_t th;
    int fd;
} tapif_stat = {
    .fd = -1,
};

struct databuf {
    int len;
    char buf[0];
};

void ur_adapter_input_buf(void *buf, int len);
static void pass_to_urmesh(const void *arg)
{
    struct databuf *dbuf = (struct databuf *)arg;

    ur_adapter_input_buf(dbuf->buf, dbuf->len);

    cpu_event_free(dbuf);
}

static void *tapif_recv_entry(void *arg)
{
    int fd = tapif_stat.fd;
    int f = fcntl(fd, F_GETFL) | O_NONBLOCK;
    if (fcntl(fd, F_SETFL, f) < 0) {
        perror("setting non-blocking");
    }

    while (tapif_stat.fd >= 0) {
        char buf[2048];

        fd_set rdset;
        struct timeval tmo = { .tv_usec = 500000 };
        FD_ZERO(&rdset);
        FD_SET(fd, &rdset);
        int ret = select(fd + 1, &rdset, NULL, NULL, &tmo);
        if (ret == 0) {
            continue;
        } else if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        int len = read(fd, buf, sizeof buf);
        if (len < 0) {
            continue;
        }

        struct databuf *databuf = cpu_event_malloc(sizeof(struct databuf) + len);
        databuf->len = len;
        memcpy(databuf->buf, buf, len);
        cpu_call_handler(pass_to_urmesh, databuf);
    }
    return 0;
}

int umesh_tapif_init(const char *ifname)
{
    int fd = open(DEVTAP, O_RDWR);

    struct ifreq ifr;
    memset(&ifr, 0, sizeof(ifr));
    strncpy(ifr.ifr_name, DEVTAP_DEFAULT_IF, sizeof(ifr.ifr_name));
    ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = 0;
    ifr.ifr_flags = IFF_TUN | IFF_NO_PI;

    if (ioctl(fd, TUNSETIFF, (void *) &ifr) < 0) {
        perror("tapif_init: "DEVTAP" ioctl TUNSETIFF");
        exit(1);
    }

    if (ioctl(fd, TUNSETOFFLOAD, 0) < 0) {
        perror("tapif_init: "DEVTAP" ioctl TUNSETOFFLOAD");
    }

    tapif_stat.fd = fd;
    pthread_create(&tapif_stat.th, NULL, tapif_recv_entry, NULL);

    return fd;
}

void umesh_tapif_deinit(void)
{
    if (tapif_stat.fd < 0) {
        return;
    }
    close(tapif_stat.fd);
    tapif_stat.fd = -1;
    pthread_join(tapif_stat.th, NULL);
}

void umesh_tapif_send(void *buf, int len)
{
    write(tapif_stat.fd, buf, len);
}

struct tbuf {
    struct tun_pi pi;
    char data[2048];
};

void umesh_tapif_send_pbuf(struct pbuf *pbuf)
{
    struct tbuf buf;
    pbuf_copy_partial(pbuf, buf.data, sizeof(buf.data), 0);
    write(tapif_stat.fd, buf.data, pbuf->tot_len);
}

#ifndef CONFIG_NET_LWIP
#include "core/mesh_mgmt.h"

static ur_adapter_callback_t g_tapif_callback;

static uint16_t calc_csum(ur_ip4_header_t *ip_hdr)
{
    uint16_t *src = (uint16_t *)ip_hdr;
    uint32_t cksum = 0;
    int i;
    for (i = 0; i < sizeof(*ip_hdr) / 2; i++) {
        cksum += src[i];
    }
    cksum -= ip_hdr->chksum;
    return ~(uint16_t)((cksum & 0xffff) + (cksum >> 16));
}

static void retarget_ip4(ur_ip4_header_t *ip_hdr, int len)
{
    ip_hdr->dest.m32 = 0x0100000a;
    if (ip_hdr->src.m32 == ip_hdr->dest.m32) {
        ip_hdr->src.m32 = 0x0200000a;
    }

    ip_hdr->chksum = calc_csum(ip_hdr);
    switch (ip_hdr->proto) {
        case UR_IPPROTO_UDP: {
            ur_udp_header_t *uhdr = (ur_udp_header_t *)(ip_hdr + 1);
            uhdr->chksum = 0;
        }
        break;
    }
}

static void retarget_ip4_src(ur_ip4_header_t *ip_hdr, int len)
{
    if (ip_hdr->src.m32 != 0x0100000a) {
        return;
    }

    ip_hdr->src.m32 = 0x0200000a;
    ip_hdr->chksum = calc_csum(ip_hdr);
    switch (ip_hdr->proto) {
        case UR_IPPROTO_UDP: {
            ur_udp_header_t *uhdr = (ur_udp_header_t *)(ip_hdr + 1);
            uhdr->chksum = 0;
        }
        break;
    }
}

/* recv from tapif network */
void ur_adapter_input_buf(void *buf, int len)
{
    ur_ip4_header_t *ip_hdr = (ur_ip4_header_t *)buf;
    uint16_t sid = ntohl(ip_hdr->dest.m32) - 2;

    /* if it is our address, redirect to tun if ip 10.0.0.1 */
    if (sid == umesh_get_sid()) {
        retarget_ip4(ip_hdr, len);
        umesh_tapif_send(buf, len);
        MESH_LOG_DEBUG("redirect to tun if\n");
        return;
    }

    struct pbuf *pbuf = pbuf_alloc(PBUF_RAW, len, PBUF_POOL);
    if (!pbuf) {
        return;
    }

    retarget_ip4_src(ip_hdr, len);
    pbuf_take(pbuf, buf, len);
    umesh_output_sid(pbuf, umesh_get_meshnetid(), sid);
    pbuf_free(pbuf);
}

/* recv from mesh network */
ur_error_t ur_adapter_input(struct pbuf *buf)
{
    ur_error_t error = UR_ERROR_NONE;
    char ip_payload[2048];
    ur_ip4_header_t *ip_hdr = (ur_ip4_header_t *)ip_payload;
    uint16_t sid;

    pbuf_copy_partial(buf, ip_payload, buf->tot_len, 0);

    sid = ntohl(ip_hdr->dest.m32) - 2;
    if (sid == umesh_get_sid()) {
        retarget_ip4(ip_hdr, buf->tot_len);
    }

    umesh_tapif_send(ip_payload, buf->tot_len);

    pbuf_free(buf);
    return error;
}

ur_error_t ur_adapter_interface_up(void)
{
    if (umesh_mm_get_device_state() == DEVICE_STATE_LEADER) {
        umesh_tapif_init("tun0");
    }

    return UR_ERROR_NONE;
}

ur_error_t ur_adapter_interface_down(void)
{
    umesh_tapif_deinit();

    return UR_ERROR_NONE;
}

ur_error_t ur_adapter_interface_update(void)
{
    return UR_ERROR_NONE;
}

ur_error_t ur_adapter_interface_init(void)
{
    g_tapif_callback.input = ur_adapter_input;
    g_tapif_callback.interface_up = ur_adapter_interface_up;
    g_tapif_callback.interface_down = ur_adapter_interface_down;
    g_tapif_callback.interface_update = ur_adapter_interface_update;
    umesh_register_callback(&g_tapif_callback);
    return UR_ERROR_NONE;
}
#endif
