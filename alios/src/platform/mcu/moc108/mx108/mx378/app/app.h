#ifndef _APP_H_
#define _APP_H_

#pragma once

#include "rtos_pub.h"

//#define APP_DEBUG

#ifdef APP_DEBUG
#define APP_PRT       os_printf
#define APP_WPRT      warning_prf
#else
#define APP_PRT       os_null_printf
#define APP_WPRT      warning_prf
#endif

enum
{
	BMSG_NULL_TYPE = 0,
	BMSG_RX_TYPE   = 1,
	BMSG_TX_TYPE   = 2,
	BMSG_IOCTL_TYPE = 3,
	BMSG_SKT_TX_TYPE   = 4,
    BMSG_TX_RAW_TYPE = 5,
    BMSG_RX_LSIG = 6, /* phy receive 802.11 LSIG*/
};

typedef struct bus_message 
{
	uint32_t type;
	uint32_t arg;
	uint32_t len;
	mico_semaphore_t sema;
}BUS_MSG_T;

#define CORE_QITEM_COUNT          (64)
#define CORE_STACK_SIZE           (4 * 1024)

typedef struct _wifi_core_
{
	uint32_t queue_item_count;
	mico_queue_t io_queue;
	
	mico_thread_t handle;
	uint32_t stack_size;
}WIFI_CORE_T;

void app_start(void);
void core_thread_uninit(void);

#endif // _APP_H_
// eof

