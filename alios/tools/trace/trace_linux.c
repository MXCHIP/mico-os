/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define DEFAULT_PORT    8000
#define TIMEOUT_SECONDS 200

#define ROUND_POINT(sz) (((sz) + (4 - 1)) & ~(4 - 1))

uint32_t trace_buf[1025];

uint32_t len_remain;
uint32_t flag_trace;

uint32_t max_fifo_len;

void task_trace_proc(void *buf , uint32_t len)
{
	uint32_t para1;
	uint32_t para2;
	char *s;
	char *s2;
	char *s3;
	uint32_t *s4;

	uint32_t *tmp;

	uint32_t para_num = 0;
	uint8_t *buf_proc = (uint8_t *)buf;
	uint32_t func_len;
	uint32_t str_len;
	uint32_t str_len2;
	uint32_t str_len3;

	if (len_remain > 0) {
		uint32_t fun_code = *((uint32_t *)buf_proc);
		switch (fun_code) {
			case 0x101:
				printf("trace init\n");
				memmove(buf_proc, (buf_proc + 12), len - 12);
				len_remain = len_remain - 12;
				break;

			case 0x102:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s switch to %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x103:

				s = (char *)buf + 8;
				printf("create task %s\n", s);
				str_len = strlen(s) + 1;
				str_len = ROUND_POINT(str_len);
				memmove(buf_proc, (buf_proc + 8 + str_len + 4), len - 12 - str_len);
				len_remain = len_remain - 12 - str_len;
				break;

			case 0x104:
				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("interrupt task %s switch to %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x105:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s change task %s pri to 0x%x pri\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			case 0x106:
				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s suspend to %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x107:
				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s resume %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x108:
				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s delete %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x109:
				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s abort %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x10a:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;
				str_len = ROUND_POINT(str_len);
				para2 = *((uint32_t *)(s + str_len));
				printf("task name is %s sleep %d ticks\n", s, para2);
				memmove(buf_proc, (buf_proc + 8 + str_len + 4 + 4), len - 16 - str_len);
				len_remain = len_remain - 16 - str_len;
				break;
			default:
				break;
		}

	}
}


void sema_trace_proc(void *buf , uint32_t len)
{
	uint32_t para1;
	uint32_t para2;

	char *s;
	char *s2;
	char *s3;
	uint32_t *s4;

	uint32_t *tmp;

	uint32_t para_num = 0;
	uint8_t *buf_proc = (uint8_t *)buf;
	uint32_t func_len;
	uint32_t str_len;
	uint32_t str_len2;
	uint32_t str_len3;
	uint32_t str_len4;

	if (len_remain > 0) {
		uint32_t fun_code = *((uint32_t *)buf_proc);
		switch (fun_code) {

			case 0x201:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s create sem %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x202:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s exec sem %s overflowed\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x203:

				s = (char *)buf + 8;
				
				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s sem %s increase count\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x204:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s get sem %s success\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x205:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s wait sem %s 0x%x ticks\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			case 0x206:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				s3 = s2 +str_len2;
				str_len3 = strlen(s3) + 1;

				str_len4 = ROUND_POINT((str_len + str_len2 + str_len3));
				s4 = (uint32_t *)(s + str_len4);

				printf("task %s wake %s by sem %s, opt %d\n", s, s2, s3, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len4 + 8), len - 16 - str_len4);
				len_remain = len_remain - 16 - str_len4;
				break;

			case 0x207:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s del sem %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			default:
				break;
		}

	}
}


void mutex_trace_proc(void *buf , uint32_t len)
{
	uint32_t para1;
	uint32_t para2;

	char *s;
	char *s2;
	char *s3;
	uint32_t *s4;

	uint32_t *tmp;

	uint32_t para_num = 0;
	uint8_t *buf_proc = (uint8_t *)buf;
	uint32_t func_len;
	uint32_t str_len;
	uint32_t str_len2;
	uint32_t str_len3;
	uint32_t str_len4;

	if (len_remain > 0) {
		uint32_t fun_code = *((uint32_t *)buf_proc);
		switch (fun_code) {

			case 0x301:

				s = (char *)buf + 8;
				
				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s create mutex %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x302:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s release mutex task %s 0x%x pri\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			case 0x303:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s get mutex task %s wait 0x%x ticks\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			case 0x304:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s pri inv mutex %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x305:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s get mutex %s wait 0x%x ticks\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			case 0x306:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s release mutex %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x307:

				s = (char *)buf + 8;
				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				s3 = s2 +str_len2;
				str_len3 = strlen(s3) + 1;

				str_len4 = ROUND_POINT((str_len + str_len2 + str_len3));

				printf("task %s wake %s by mutex %s\n", s, s2, s3);
				memmove(buf_proc, (buf_proc + 8 + str_len4 + 4), len - 12 - str_len4);
				len_remain = len_remain - 12 - str_len4;
				break;

			case 0x308:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s delete mutex %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			default:
				break;
		}

	}
}


void event_trace_proc(void *buf, uint32_t len)
{
	uint32_t para1;
	uint32_t para2;

	char *s;
	char *s2;
	char *s3;
	uint32_t *s4;

	uint32_t *tmp;

	uint32_t para_num = 0;
	uint8_t *buf_proc = (uint8_t *)buf;
	uint32_t func_len;
	uint32_t str_len;
	uint32_t str_len2;
	uint32_t str_len3;
	uint32_t str_len4;

	if (len_remain > 0) {
		uint32_t fun_code = *((uint32_t *)buf_proc);
		switch (fun_code) {

			case 0x401:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s create event %s flags_init 0x%x\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			case 0x402:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s get event %s success\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x403:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s wait event %s 0x%x ticks\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			case 0x404:

				s = (char *)buf + 8;
				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				s3 = s2 +str_len2;
				str_len3 = strlen(s3) + 1;

				str_len4 = ROUND_POINT((str_len + str_len2 + str_len3));

				printf("task %s wake %s by event %s\n", s, s2, s3);
				memmove(buf_proc, (buf_proc + 8 + str_len4 + 4), len - 12 - str_len4);
				len_remain = len_remain - 12 - str_len4;
				break;

			case 0x405:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s delete event %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			default:
				break;
		}

	}
}


void buf_queue_trace_proc(void *buf, uint32_t len)
{
	uint32_t para1;
	uint32_t para2;

	char *s;
	char *s2;
	char *s3;
	uint32_t *s4;

	uint32_t *tmp;

	uint32_t para_num = 0;
	uint8_t *buf_proc = (uint8_t *)buf;
	uint32_t func_len;
	uint32_t str_len;
	uint32_t str_len2;
	uint32_t str_len3;
	uint32_t str_len4;

	if (len_remain > 0) {
		uint32_t fun_code = *((uint32_t *)buf_proc);
		switch (fun_code) {

			case 0x501:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s create event %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x502:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s get mutex task %s wait 0x%x ticks\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			case 0x503:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s post msg to buf_queue %s size 0x%x\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			case 0x504:

				s = (char *)buf + 8;
				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				s3 = s2 +str_len2;
				str_len3 = strlen(s3) + 1;

				str_len4 = ROUND_POINT((str_len + str_len2 + str_len3));

				printf("task %s wake %s by event %s\n", s, s2, s3);
				memmove(buf_proc, (buf_proc + 8 + str_len4 + 4), len - 12 - str_len4);
				len_remain = len_remain - 12 - str_len4;
				break;

			case 0x505:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				s4 = (uint32_t *)(s + str_len3);

				printf("task %s get buf_queue %s wait 0x%x ticks\n", s, s2, *s4);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 8), len - 16 - str_len3);
				len_remain = len_remain - 16 - str_len3;
				break;

			default:
				break;
		}

	}
}


void timer_trace_proc(void *buf, uint32_t len)
{
	uint32_t para1;
	uint32_t para2;

	char *s;
	char *s2;
	char *s3;
	uint32_t *s4;

	uint32_t *tmp;

	uint32_t para_num = 0;
	uint8_t *buf_proc = (uint8_t *)buf;
	uint32_t func_len;
	uint32_t str_len;
	uint32_t str_len2;
	uint32_t str_len3;
	uint32_t str_len4;

	if (len_remain > 0) {
		uint32_t fun_code = *((uint32_t *)buf_proc);
		switch (fun_code) {

			case 0x601:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s create timer %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x602:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s delete timer %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			default:
				break;
		}

	}
}

void misc_trace_proc(void *buf, uint32_t len)
{
	uint32_t para1;
	uint32_t para2;

	char *s;
	char *s2;
	char *s3;
	uint32_t *s4;

	uint32_t *tmp;

	uint32_t para_num = 0;
	uint8_t *buf_proc = (uint8_t *)buf;
	uint32_t func_len;
	uint32_t str_len;
	uint32_t str_len2;
	uint32_t str_len3;
	uint32_t str_len4;

	if (len_remain > 0) {
		uint32_t fun_code = *((uint32_t *)buf_proc);
		switch (fun_code) {

			case 0x701:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s create timer %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x801:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s create mm pool %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0x901:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s create mm region %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0xa01:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s create work %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0xa02:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s create workqueue %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			case 0xa03:

				s = (char *)buf + 8;

				str_len = strlen(s) + 1;

				s2 = s +str_len;
				str_len2 = strlen(s2) + 1;

				str_len3 = ROUND_POINT((str_len + str_len2));
				printf("task %s delete workqueue %s\n", s, s2);
				memmove(buf_proc, (buf_proc + 8 + str_len3 + 4), len - 12 - str_len3);
				len_remain = len_remain - 12 - str_len3;
				break;

			default:
				break;
		}

	}
}

#if 0
int main(int argc, char* argv[])
{
	int fh1;
	int len;

   fh1 = open("trace_test", O_RDONLY);

   if( fh1 == -1 )   {
      perror( "Open failed on input file" );
    }

    while (1) {
	    len = read(fh1, trace_buf, 4);

		if (len == 0) {
			break;
    	}
	
		len_remain = trace_buf[0];

		len = read(fh1, trace_buf, len_remain);

        if (len_remain != len) {
            printf("trace analyse finsihed\n");
            close(fh1);
            exit(0);
        }

		while (1)  {

			if (len_remain > 0) {
				task_trace_proc(trace_buf, len_remain);
				sema_trace_proc(trace_buf, len_remain);
				mutex_trace_proc(trace_buf, len_remain);
				event_trace_proc(trace_buf, len_remain);
				buf_queue_trace_proc(trace_buf, len_remain);
				timer_trace_proc(trace_buf, len_remain);
				misc_trace_proc(trace_buf, len_remain);
			}
			else {
				break;
			}

		}
    }

    printf("trace analyse finsihed\n");

	return 0;
}
#else

int main(int argc, char* argv[])
{
	int    len;
    int    socket_fd;
    int    connect_fd;
    int    len_recv_total;
    fd_set fdset;
    int    ret;
    int    on;

    struct timeval tv;
    struct sockaddr_in servaddr; 

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1 ) {
        printf("create socket error: %s(errno: %d)\n",strerror(errno),errno); 
        exit(0); 
    }

    on = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
 
    memset(&servaddr, 0, sizeof(servaddr)); 
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(DEFAULT_PORT);

    if (bind(socket_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) == -1) {
        printf("bind socket error: %s(errno: %d)\n",strerror(errno),errno); 
        exit(0); 
    }

    if (listen(socket_fd, 10) == -1){
        printf("listen socket error: %s(errno: %d)\n",strerror(errno),errno);
        exit(0);
    } 

    printf("======waiting for client's request======\n");

    if ((connect_fd = accept(socket_fd, (struct sockaddr*)NULL, NULL)) == -1) {
        printf("accept socket error: %s(errno: %d)",strerror(errno),errno);
        exit(0); 
    }

    while (1) {

        len_recv_total = 0;

        while (1) {
            FD_ZERO(&fdset);
            FD_SET(connect_fd, &fdset);

            tv.tv_sec = TIMEOUT_SECONDS;
            tv.tv_usec = 0;

            ret = select(connect_fd + 1, &fdset, NULL, NULL, &tv);

            if (ret == 0) {
                goto finish;
            }

            len = recv(connect_fd, (trace_buf + len_recv_total), 4 - len_recv_total, 0);

            if ((ret == 1) && (len == 0)) {
                goto finish;
            }

            len_recv_total += len;

            if (len_recv_total ==  4) {
               break;
            }
        }

		len_remain = trace_buf[0];

        if (len_remain > max_fifo_len) {
            max_fifo_len = len_remain;
        }

        len_recv_total = 0;

        while (1) {
            FD_ZERO(&fdset);
            FD_SET(connect_fd, &fdset);

            tv.tv_sec = TIMEOUT_SECONDS;
            tv.tv_usec = 0;
            ret = select(connect_fd + 1, &fdset, NULL, NULL, &tv);

            if (ret == 0) {
                goto finish;
            }

            len = recv(connect_fd, (trace_buf + len_recv_total), len_remain - len_recv_total, 0);

            if ((ret == 1) && (len == 0)) {
                goto finish;
            }

            len_recv_total += len;

            if (len_recv_total ==  len_remain) {
                break;
            }
        }

		while (1)  {

			if (len_remain > 0) {
				task_trace_proc(trace_buf, len_remain);
				sema_trace_proc(trace_buf, len_remain);
				mutex_trace_proc(trace_buf, len_remain);
				event_trace_proc(trace_buf, len_remain);
				buf_queue_trace_proc(trace_buf, len_remain);
				timer_trace_proc(trace_buf, len_remain);
				misc_trace_proc(trace_buf, len_remain);
			}
			else {
				break;
			}

		}
    }

finish:
    close(connect_fd);
    close(socket_fd);
    printf("max_fifo_len is %d\n", max_fifo_len);
    printf("trace analyse finsihed\n");

	return 0;
}

#endif

