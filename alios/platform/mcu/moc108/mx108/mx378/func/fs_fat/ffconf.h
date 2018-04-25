#ifndef _FFCONF
#define _FFCONF         7211	/* Revision ID */

#define	_USE_LFN	    0		/* 0 to 3 */
#define	_MAX_LFN	    255		/* Maximum LFN length to handle (12 to 255) */
#define	_LFN_UNICODE	0	    /* 0:ANSI/OEM or 1:Unicode */
#define _VOLUMES	    1
#define	_MIN_SS		    512
#define	_MAX_SS		    512
#define _FS_REENTRANT	0		/* 0:Disable or 1:Enable */

typedef unsigned char  uint8;                   /* 无符号8位整型变量                        */
typedef signed   char  int8;                    /* 有符号8位整型变量                        */
typedef unsigned short uint16;                  /* 无符号16位整型变量                       */
typedef signed   short int16;                   /* 有符号16位整型变量                       */
typedef unsigned int   uint32;                  /* 无符号32位整型变量                       */
typedef signed   int   int32;                   /* 有符号32位整型变量                       */
typedef float          fp32;                    /* 单精度浮点数（32位长度）                 */
typedef double         fp64;                    /* 双精度浮点数（64位长度）                 */
typedef unsigned long long uint64;
typedef long long   int64;

typedef unsigned char  u_int8;                   /* 无符号8位整型变量                        */
typedef unsigned short u_int16;                  /* 无符号16位整型变量                       */
typedef unsigned int   u_int32;                  /* 无符号32位整型变量 */


#endif 
