/*-----------------------------------------------------------------------
/  Low level disk interface modlue include file 
/-----------------------------------------------------------------------*/
#ifndef _DISKIO_DEFINED
#define _DISKIO_DEFINED

#ifdef __cplusplus
extern "C" {
#endif

#include "ffconf.h"

typedef uint8	DSTATUS;

typedef enum {
	RES_OK = 0,		/* 0: Successful */
	RES_ERROR,		/* 1: R/W Error */
	RES_WRPRT,		/* 2: Write Protected */
	RES_NOTRDY,		/* 3: Not Ready */
	RES_PARERR		/* 4: Invalid Parameter */
} DRESULT;

typedef enum
{
	DISK_TYPE_SD =0,
	DISK_TYPE_UDISK
} DISKTPYE;

/* Disk Status Bits (DSTATUS) */
#define STA_NOINIT		0x01	/* Drive not initialized */
#define STA_NODISK		0x02	/* No medium in the drive */
#define STA_PROTECT		0x04	/* Write protected */


#include "include.h"
#include "arm_arch.h"
#include "uart_pub.h"
#define FAT_INTF_DEBUG

#ifdef FAT_INTF_DEBUG
    #define FAT_PRT      os_printf
    #define FAT_WARN     warning_prf
    #define FAT_FATAL    fatal_prf
#else
    #define FAT_PRT      null_prf
    #define FAT_WARN     null_prf
    #define FAT_FATAL    null_prf
#endif

DSTATUS disk_initialize (uint8 pdrv);
DSTATUS disk_status (uint8 pdrv);
DRESULT disk_read (uint8 pdrv, uint8* buff, uint32 start_sector, uint32 sector_cnt);

#ifdef __cplusplus
}
#endif

#endif
