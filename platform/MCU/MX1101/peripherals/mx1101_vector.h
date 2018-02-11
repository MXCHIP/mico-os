/**
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 *
 */

#ifndef _MX1290_VECTOR_TABLE_H_
#define _MX1290_VECTOR_TABLE_H_


/* Exported types ------------------------------------------------------------*/

/** @defgroup IRQ_Exported_Types IRQ Exported Types
  * @{
  */
typedef void (*HAL_VECTOR_FUN) (void);
typedef unsigned int (*IRQ_FUN)(void *Data);
/**
  * @}
  */

/* Exported constants --------------------------------------------------------*/
/** @defgroup IRQ_Exported_Constants IRQ Exported Constants
  * @{
  */

/** @defgroup IRQn_enum 
  * @{
  */ 

typedef enum IRQn
{
    /*
     * CM3 internal interrupt(Exception/Fault)
     */
    RST_IRQn		= -15,
    NMI_IRQn		= -14,
    HARDFLT_IRQn	= -13,
    MMFLT_IRQn		= -12,//programmable as below
    BUSFLT_IRQn		= -11,
    USGFLT_IRQn		= -10,
    SVCALL_IRQn		= -5,
    DBGMON_IRQn		= -4,
    PENDSV_IRQn		= -2,
    SysTick_IRQn	= -1,
    /*
     * SOC interrupt(External Interrupt)
     */
    GPIO_IRQn		= 0,
    RTC_IRQn		= 1,
    IR_IRQn			= 2,
    FUART_IRQn		= 3,
    BUART_IRQn		= 4,
    PWC_IRQn		= 5,
    TMR0_IRQn		= 6,
    USB_IRQn		= 7,
    DMACH0_IRQn		= 8,
    DMACH1_IRQn		= 9,
    DECODER_IRQn	= 10,
    SPIS_IRQn		= 11,
    SD_IRQn			= 12,
    SPIM_IRQn		= 13,
    TMR1_IRQn		= 14,
    WDG_IRQn		= 15,
} IRQn_Type;

#endif //_MX1290_VECTOR_TABLE_H_

