#ifndef _UART_H_
#define _UART_H_

#include "include.h"
#include "fifo.h"

//#define UART_DEBUG

#ifdef UART_DEBUG
#define UART_PRT                 os_printf
#define UART_WPRT                warning_prf
#define STATIC
#else
#define UART_PRT                 os_null_printf
#define UART_WPRT                os_null_printf
#define STATIC                   static
#endif

#define UART_PORT_1 (0)
#define UART_PORT_2 (1)

#define IRQ_UART(n)                   (n == UART_PORT_1 ? IRQ_UART1 : IRQ_UART2)
#define PRI_IRQ_UART(n)               (n == UART_PORT_1 ? PRI_IRQ_UART1 : PRI_IRQ_UART2)
#define PWD_UART_CLK_BIT(n)           (n == UART_PORT_1 ? PWD_UART1_CLK_BIT : PWD_UART2_CLK_BIT)
#define GFUNC_MODE_UART(n)            (n == UART_PORT_1 ? GFUNC_MODE_UART1 : GFUNC_MODE_UART2)
#define IRQ_UART_BIT(n)               (n == UART_PORT_1 ? IRQ_UART1_BIT : IRQ_UART2_BIT)

#define DEBUG_PRT_MAX_CNT          (16)

#if CFG_UART_DEBUG_COMMAND_LINE
#define RX_RB_LENGTH               (128)  // 64
#define TX_RB_LENGTH               (64)
#else
#define RX_RB_LENGTH               (64)
#define TX_RB_LENGTH               (64)
#endif

#define CARRIAGE_RETURN(buf, count) \
    do { \
        buf[count - 1] = '\r'; \
        buf[count] = '\n'; \
        buf[count + 1] = 0; \
        rc += 1; \
    } while (0)

/* uart parameter config----start*/
#define UART_BAUDRATE_3250000        3250000
#define UART_BAUDRATE_2000000        2000000
#define UART_BAUDRATE_921600         921600
#define UART_BAUDRATE_460800         460800
#define UART_BAUDRATE_230400         230400
#define UART_BAUDRATE_115200         115200  //default
#define UART_BAUDRATE_3000           3250
#define UART_BAUDRATE_19200          19200

#define UART_BAUD_RATE               UART_BAUDRATE_115200

#define UART_CLOCK_FREQ_10M          10000000
#define UART_CLOCK_FREQ_48M          48000000
#define UART_CLOCK_FREQ_24M          24000000
#define UART_CLOCK_FREQ_26M          26000000
#define UART_CLOCK_FREQ_52M          52000000
#define UART_CLOCK_FREQ_120M         120000000

#if (CFG_RUNNING_PLATFORM == FPGA_PLATFORM)
#define UART_CLOCK                   UART_CLOCK_FREQ_24M
#else
#define UART_CLOCK                   UART_CLOCK_FREQ_26M
#endif // CFG_RUNNING_PLATFORM == FPGA_PLATFORM

#define TX_FIFO_THRD               (0x01)
#define RX_FIFO_THRD                RX_RB_LENGTH // (0x20)

#define DEF_TX_EN                   0x1
#define DEF_RX_EN                   0x1
#define DEF_IRDA_MODE               0x0    // 0:uart mode  1:IRDA MODE
#define DEF_DATA_LEN                0x3    // 0=5bit, 1=6bit, 2=7bit, 3=8bit
#define DEF_PARITY_EN               0x0    // 0=no parity  1: enable parity
#define DEF_PARITY_MODE             0x0    // 0:odd  1: even
#define DEF_STOP_BIT                0x0    // 1bit

#define FLOW_CTRL_HIGH_CNT          (96)
#define FLOW_CTRL_LOW_CNT           (32)

#define DEBUG_TX_FIFO_MAX_COUNT     16

/* uart parameter config----end*/

typedef struct _uart_
{
    UINT16 status;

    KFIFO_PTR rx;

    KFIFO_PTR tx;
} UART_S, *UART_PTR;


#if (0 == CFG_RELEASE_FIRMWARE)
#define DEAD_WHILE()   do{           \
                            while(1);\
                         }while(0)
#else
#define DEAD_WHILE()   do{           \
                            os_printf("dead\r\n");\
                         }while(0)
#endif

#define UART_BASE_ADDR(n)                      (n == UART_PORT_1 ? 0x0802100 : 0x0802200)

#define REG_UART_CONFIG(n)                     (UART_BASE_ADDR(n) + 4 * 0)
#define UART_TX_ENABLE                         (1 << 0)
#define UART_RX_ENABLE                         (1 << 1)
#define UART_IRDA                              (1 << 2)
#define UART_DATA_LEN_POSI                     (3)
#define UART_DATA_LEN_MASK                     (0x03)
#define UART_PAR_EN                            (1 << 5)
#define UART_PAR_ODD_MODE                      (1 << 6)
#define UART_STOP_LEN_2                        (1 << 7)
#define UART_CLK_DIVID_POSI                    (8)
#define UART_CLK_DIVID_MASK                    (0x1FFF)

#define REG_UART_FIFO_CONFIG(n)                 (UART_BASE_ADDR(n) + 4 * 1)
#define TX_FIFO_THRESHOLD_MASK                 (0xFF)
#define TX_FIFO_THRESHOLD_POSI                 (0)
#define RX_FIFO_THRESHOLD_MASK                 (0xFF)
#define RX_FIFO_THRESHOLD_POSI                 (8)
#define RX_STOP_DETECT_TIME_MASK               (0x03)
#define RX_STOP_DETECT_TIME_POSI               (16)
#define RX_STOP_DETECT_TIME32                  (0)
#define RX_STOP_DETECT_TIME64                  (1)
#define RX_STOP_DETECT_TIME128                 (2)
#define RX_STOP_DETECT_TIME256                 (3)

#define REG_UART_FIFO_STATUS(n)                 (UART_BASE_ADDR(n) + 4 * 2)
#define TX_FIFO_COUNT_MASK                     (0xFF)
#define TX_FIFO_COUNT_POSI                     (0)
#define RX_FIFO_COUNT_MASK                     (0xFF)
#define RX_FIFO_COUNT_POSI                     (8)
#define TX_FIFO_FULL                           (1 << 16)
#define TX_FIFO_EMPTY                          (1 << 17)
#define RX_FIFO_FULL                           (1 << 18)
#define RX_FIFO_EMPTY                          (1 << 19)
#define FIFO_WR_READY                          (1 << 20)
#define FIFO_RD_READY                          (1 << 21)

#define REG_UART_FIFO_PORT(n)                   (UART_BASE_ADDR(n) + 4 * 3)
#define UART_TX_FIFO_DIN_MASK                  (0xFF)
#define UART_TX_FIFO_DIN_POSI                  (0)
#define UART_RX_FIFO_DOUT_MASK                 (0xFF)
#define UART_RX_FIFO_DOUT_POSI                 (8)

#define REG_UART_INTR_ENABLE(n)                 (UART_BASE_ADDR(n) + 4 * 4)
#define TX_FIFO_NEED_WRITE_EN                  (1 << 0)
#define RX_FIFO_NEED_READ_EN                   (1 << 1)
#define RX_FIFO_OVER_FLOW_EN                   (1 << 2)
#define UART_RX_PARITY_ERR_EN                  (1 << 3)
#define UART_RX_STOP_ERR_EN                    (1 << 4)
#define UART_TX_STOP_END_EN                    (1 << 5)
#define UART_RX_STOP_END_EN                    (1 << 6)
#define UART_RXD_WAKEUP_EN                     (1 << 7)

#define REG_UART_INTR_STATUS(n)                 (UART_BASE_ADDR(n) + 4 * 5)
#define TX_FIFO_NEED_WRITE_STA                  (1 << 0)
#define RX_FIFO_NEED_READ_STA                   (1 << 1)
#define RX_FIFO_OVER_FLOW_STA                   (1 << 2)
#define UART_RX_PARITY_ERR_STA                  (1 << 3)
#define UART_RX_STOP_ERR_STA                    (1 << 4)
#define UART_TX_STOP_END_STA                    (1 << 5)
#define UART_RX_STOP_END_STA                    (1 << 6)
#define UART_RXD_WAKEUP_STA                     (1 << 7)

#define REG_UART_FLOW_CONFIG(n)                 (UART_BASE_ADDR(n) + 4 * 6)
#define FLOW_CTRL_LOW_CNT_MASK                   (0xFF)
#define FLOW_CTRL_LOW_CNT_POSI                   (0)
#define FLOW_CTRL_HIGH_CNT_MASK                  (0xFF)
#define FLOW_CTRL_HIGH_CNT_POSI                  (8)
#define FLOW_CONTROL_EN                          (1 << 16)

#define REG_UART_WAKE_CONFIG(n)                 (UART_BASE_ADDR(n) + 4 * 7)
#define UART_WAKE_COUNT_MASK                   (0x3FF)
#define UART_WAKE_COUNT_POSI                   (0)
#define UART_TXD_WAIT_CNT_MASK                 (0x3FF)
#define UART_TXD_WAIT_CNT_POSI                 (10)
#define UART_RXD_WAKE_EN                       (1 << 20)
#define UART_TXD_WAKE_EN                       (1 << 21)
#define RXD_NEGEDGE_WAKE_EN                    (1 << 22)

#define UART_TX_WRITE_READY(n)             (REG_READ(REG_UART_FIFO_STATUS(n)) & FIFO_WR_READY)

#define UART_WRITE_BYTE(n,v)               do                                   \
										{                                     \
											v = (v & UART_TX_FIFO_DIN_MASK)   \
													<< UART_TX_FIFO_DIN_POSI; \
											REG_WRITE(REG_UART_FIFO_PORT(n), v); \
										}while(0)
#define UART_READ_BYTE(n,v)               do                                    \
										{                                     \
											v = (REG_READ(REG_UART_FIFO_PORT(n)) \
													>> UART_RX_FIFO_DOUT_POSI) \
													& UART_RX_FIFO_DOUT_MASK;\
										}while(0)

#define UART_READ_BYTE_DISCARD(n)       do                                    \
										{                                     \
											REG_READ(REG_UART_FIFO_PORT(n));\
										}while(0)

/*******************************************************************************
* Function Declarations
*******************************************************************************/

#endif // _UART_H_
