#ifndef __GENER_DMA_H__
#define __GENER_DMA_H__

#if CFG_GENERAL_DMA

//#define GENER_DMA_DEBUG

#include "uart_pub.h"
#ifdef GENER_DMA_DEBUG
#define GENER_DMA_PRT                   os_printf
#define GENER_DMA_WPRT                  os_printf
#else
#define GENER_DMA_PRT                   null_prf
#define GENER_DMA_WPRT                  null_prf
#endif

#define GENER_DMA_BASE                      (0x00809000)

// DMA 0
#define GENER_DMA0_REG0_CONF                (GENER_DMA_BASE + 0x00*4)
#define GDMA_X_DMA_EN                     (1 << 0)
#define GDMA_X_FIN_INTEN                  (1 << 1)
#define GDMA_X_HFIN_INTEN                 (1 << 2)
#define GDMA_X_SRCDATA_WIDTH_POSI         (4)
#define GDMA_X_SRCDATA_WIDTH_MASK         (0x3)
#define GDMA_X_DSTDATA_WIDTH_POSI         (6)
#define GDMA_X_DSTDATA_WIDTH_MASK         (0x3)
#define GDMA_DATA_WIDTH_8BIT              (0x0)
#define GDMA_DATA_WIDTH_16BIT             (0x1)
#define GDMA_DATA_WIDTH_32BIT             (0x2)
#define GDMA_X_SRCADDR_INC                (1 << 8)
#define GDMA_X_DSTADDR_INC                (1 << 9)
#define GDMA_X_SRCADDR_LOOP               (1 << 10)
#define GDMA_X_DSTADDR_LOOP               (1 << 11)
#define GDMA_X_CHNL_PRIO_POSI             (12)
#define GDMA_X_CHNL_PRIO_MASK             (0x3)
#define GDMA_X_TRANS_LEN_POSI             (16)
#define GDMA_X_TRANS_LEN_MASK             (0xffffU)

#define GENER_DMA0_REG1_DST_START_ADDR      (GENER_DMA_BASE + 0x01*4)

#define GENER_DMA0_REG2_SRC_START_ADDR      (GENER_DMA_BASE + 0x02*4)

#define GENER_DMA0_REG3_DSTLOOP_END_ADDR    (GENER_DMA_BASE + 0x03*4)

#define GENER_DMA0_REG4_DSTLOOP_START_ADDR  (GENER_DMA_BASE + 0x04*4)

#define GENER_DMA0_REG5_SRCLOOP_END_ADDR    (GENER_DMA_BASE + 0x05*4)

#define GENER_DMA0_REG6_SRCLOOP_START_ADDR  (GENER_DMA_BASE + 0x06*4)

#define GENER_DMA0_REG7_REMAIN_LEN          (GENER_DMA_BASE + 0x07*4)
#define GDMA_REMAIN_LEN_MASK            (0x1ffff)

// DMA1
#define GENER_DMA1_REG8_CONF                (GENER_DMA_BASE + 0x08*4)

#define GENER_DMA1_REG9_DST_START_ADDR      (GENER_DMA_BASE + 0x09*4)

#define GENER_DMA1_REGA_SRC_START_ADDR      (GENER_DMA_BASE + 0x0A*4)

#define GENER_DMA1_REGB_DSTLOOP_END_ADDR    (GENER_DMA_BASE + 0x0B*4)

#define GENER_DMA1_REGC_DSTLOOP_START_ADDR  (GENER_DMA_BASE + 0x0C*4)

#define GENER_DMA1_REGD_SRCLOOP_END_ADDR    (GENER_DMA_BASE + 0x0D*4)

#define GENER_DMA1_REGE_SRCLOOP_START_ADDR  (GENER_DMA_BASE + 0x0E*4)

#define GENER_DMA1_REGF_REMAIN_LEN          (GENER_DMA_BASE + 0x0F*4)

// DMA2
#define GENER_DMA2_REG10_CONF               (GENER_DMA_BASE + 0x10*4)

#define GENER_DMA2_REG11_DST_START_ADDR     (GENER_DMA_BASE + 0x11*4)

#define GENER_DMA2_REG12_SRC_START_ADDR     (GENER_DMA_BASE + 0x12*4)

#define GENER_DMA2_REG13_DSTLOOP_END_ADDR   (GENER_DMA_BASE + 0x13*4)

#define GENER_DMA2_REG14_DSTLOOP_START_ADDR (GENER_DMA_BASE + 0x14*4)

#define GENER_DMA2_REG15_SRCLOOP_END_ADDR   (GENER_DMA_BASE + 0x15*4)

#define GENER_DMA2_REG16_SRCLOOP_START_ADDR (GENER_DMA_BASE + 0x16*4)

#define GENER_DMA2_REG17_REMAIN_LEN         (GENER_DMA_BASE + 0x17*4)

// DMA3
#define GENER_DMA3_REG18_CONF               (GENER_DMA_BASE + 0x18*4)

#define GENER_DMA3_REG19_DST_START_ADDR     (GENER_DMA_BASE + 0x19*4)

#define GENER_DMA3_REG1A_SRC_START_ADDR     (GENER_DMA_BASE + 0x1A*4)

#define GENER_DMA3_REG1B_DSTLOOP_END_ADDR   (GENER_DMA_BASE + 0x1B*4)

#define GENER_DMA3_REG1C_DSTLOOP_START_ADDR (GENER_DMA_BASE + 0x1C*4)

#define GENER_DMA3_REG1D_SRCLOOP_END_ADDR   (GENER_DMA_BASE + 0x1D*4)

#define GENER_DMA3_REG1E_SRCLOOP_START_ADDR (GENER_DMA_BASE + 0x1E*4)

#define GENER_DMA3_REG1F_REMAIN_LEN         (GENER_DMA_BASE + 0x1F*4)

#define GENER_DMA_REG20_DMA_INT_STATUS      (GENER_DMA_BASE + 0x20*4)
#define GENER_DMA_FIN_INT_STATUS_POSI       (0)
#define GENER_DMA_FIN_INT_STATUS_MASK       (0xf)
#define GENER_DMA_HFIN_INT_STATUS_POSI      (8)
#define GENER_DMA_HFIN_INT_STATUS_MASK      (0xf)

UINT32 gdma_ctrl(UINT32 cmd, void *param);

#endif // CFG_GENERAL_DMA

#endif // __GENER_DMA_H__
