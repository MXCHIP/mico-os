#include "include.h"
#include "arm_arch.h"

#include "uart_pub.h"
#include "uart.h"

#include "drv_model_pub.h"
#include "sys_ctrl_pub.h"
#include "mem_pub.h"
#include "icu_pub.h"
#include "gpio_pub.h"

#include <stdio.h>

#include "ll.h"
#include "mem_pub.h"
#include "intc_pub.h"


////just for uart init
/**
 * UART data width
 */
typedef enum
{
    DATA_WIDTH_5BIT,
    DATA_WIDTH_6BIT,
    DATA_WIDTH_7BIT,
    DATA_WIDTH_8BIT
} uart_data_width_t;

/**
 * UART stop bits
 */
typedef enum
{
    STOP_BITS_1,
    STOP_BITS_2,
} uart_stop_bits_t;

/**
 * UART flow control
 */
typedef enum
{
    FLOW_CONTROL_DISABLED,
    FLOW_CONTROL_CTS,
    FLOW_CONTROL_RTS,
    FLOW_CONTROL_CTS_RTS
} uart_flow_control_t;

/**
 * UART parity
 */
typedef enum
{
    NO_PARITY,
    ODD_PARITY,
    EVEN_PARITY,
} uart_parity_t;

typedef struct
{
    uint32_t					  baud_rate;
    uart_data_width_t    data_width;
    uart_parity_t 	      parity;
    uart_stop_bits_t	  stop_bits;
    uart_flow_control_t  flow_control;
    uint8_t					      flags;	 /**< if set, UART can wake up MCU from stop mode, reference: @ref UART_WAKEUP_DISABLE and @ref UART_WAKEUP_ENABLE*/
} uart_config_t;

void bk_printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
}

/***********************************************/
/**************** add by swyang ****************/
/***********************************************/

extern void uart_rx_cb(UINT8 port);
extern void uart_tx_cb(UINT8 port);

void uart_fifo_flush(UINT8 port)
{
    UINT32 val;
    UINT32 reg;

    val = REG_READ(REG_UART_CONFIG(port));
    reg = val & (~(UART_TX_ENABLE | UART_RX_ENABLE));

    REG_WRITE(REG_UART_CONFIG(port), reg);
    REG_WRITE(REG_UART_CONFIG(port), val);
}

void uart1_isr(void)
{
    UINT8 port;
    UINT32 status;
    UINT32 intr_en;
    UINT32 intr_status;

    port = UART_PORT_1;

    intr_en = REG_READ(REG_UART_INTR_ENABLE(port));
    intr_status = REG_READ(REG_UART_INTR_STATUS(port));
    REG_WRITE(REG_UART_INTR_STATUS(port), intr_status);
    status = intr_status & intr_en;

    if(status & (RX_FIFO_NEED_READ_STA | UART_RX_STOP_END_STA))
    {
        uart_rx_cb(port);
    }
    else if(status & TX_FIFO_NEED_WRITE_STA)
    {
    }
    else if(status & RX_FIFO_OVER_FLOW_STA)
    {
    }
    else if(status & UART_RX_PARITY_ERR_STA)
    {
        uart_fifo_flush(port);
    }
    else if(status & UART_RX_STOP_ERR_STA)
    {
    }
    else if(status & UART_TX_STOP_END_STA)
    {
        uart_tx_cb(port);
    }
    else if(status & UART_RXD_WAKEUP_STA)
    {
    }
    else
    {
    }
}

void uart2_isr(void)
{
    UINT8 port;
    UINT32 status;
    UINT32 intr_en;
    UINT32 intr_status;

    port = UART_PORT_2;

    intr_en = REG_READ(REG_UART_INTR_ENABLE(port));
    intr_status = REG_READ(REG_UART_INTR_STATUS(port));
    REG_WRITE(REG_UART_INTR_STATUS(port), intr_status);
    status = intr_status & intr_en;

    if(status & (RX_FIFO_NEED_READ_STA | UART_RX_STOP_END_STA))
    {
        uart_rx_cb(port);
    }
    else if(status & TX_FIFO_NEED_WRITE_STA)
    {
    }
    else if(status & RX_FIFO_OVER_FLOW_STA)
    {
    }
    else if(status & UART_RX_PARITY_ERR_STA)
    {
        uart_fifo_flush(port);
    }
    else if(status & UART_RX_STOP_ERR_STA)
    {
    }
    else if(status & UART_TX_STOP_END_STA)
    {
        uart_tx_cb(port);
    }
    else if(status & UART_RXD_WAKEUP_STA)
    {
    }
    else
    {
    }
}

void uart_open(UINT8 port)
{
    UINT32 param;
    UINT32 reg;
    UINT32 intr_status;

    intc_service_register(IRQ_UART(port), PRI_IRQ_UART(port), port == UART_PORT_1 ? uart1_isr : uart2_isr);

    param = PWD_UART_CLK_BIT(port);
    sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_UP, &param);

    param = GFUNC_MODE_UART(port);
    sddev_control(GPIO_DEV_NAME, CMD_GPIO_ENABLE_SECOND, &param);

    /*irq enable, Be careful: it is best that irq enable at open routine*/
    intr_status = REG_READ(REG_UART_INTR_STATUS(port));
    REG_WRITE(REG_UART_INTR_STATUS(port), intr_status);

    param = IRQ_UART_BIT(port);
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_ENABLE, &param);
}

void uart_close(UINT8 port)
{
    UINT32 i;
    UINT32 reg;
    UINT32 rx_count;
    UINT32 param;

    /*irq enable, Be careful: it is best that irq enable at close routine*/
    param = IRQ_UART_BIT(port);
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_DISABLE, &param);

    /*disable rtx intr*/
    reg = REG_READ(REG_UART_INTR_ENABLE(port));
    reg &= (~(RX_FIFO_NEED_READ_EN | UART_RX_STOP_END_EN));
    REG_WRITE(REG_UART_INTR_ENABLE(port), reg);

    /* flush fifo*/
    uart_fifo_flush(port);

    /* disable rtx*/
    reg = REG_READ(REG_UART_CONFIG(port));
    reg = reg & (~(UART_TX_ENABLE | UART_RX_ENABLE));
    REG_WRITE(REG_UART_CONFIG(port), reg);

    /* double discard fifo data*/
    reg = REG_READ(REG_UART_FIFO_STATUS(port));
    rx_count = (reg >> RX_FIFO_COUNT_POSI) & RX_FIFO_COUNT_MASK;
    for(i = 0; i < rx_count; i ++)
    {
        UART_READ_BYTE_DISCARD(port);
    }

    param = PWD_UART_CLK_BIT(port);
    sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_DOWN, &param);
}

void uart_config(UINT8 port, uart_config_t *uart_config)
{
    UINT32 reg;
    UINT32 baud_div;
    UINT32 	width;
    uart_parity_t 	     parity_en;
    uart_stop_bits_t	  stop_bits;
    uart_flow_control_t  flow_control;
    UINT8 parity_mode = 0;

    REG_WRITE(REG_UART_INTR_ENABLE(port), 0);//disable int

    baud_div = UART_CLOCK / uart_config->baud_rate;
    baud_div = baud_div - 1;
    width = uart_config->data_width;
    parity_en = uart_config->parity;
    stop_bits = uart_config->stop_bits;
    flow_control = uart_config->flow_control;

    if(parity_en)
    {
        if(parity_en == ODD_PARITY)
            parity_mode = 1;
        else
            parity_mode = 0;
        parity_en = 1;
    }

    reg = UART_TX_ENABLE
          | UART_RX_ENABLE
          & (~UART_IRDA)
          | ((width & UART_DATA_LEN_MASK) << UART_DATA_LEN_POSI)
          | (parity_en << 5)
          | (parity_mode << 6)
          | (stop_bits << 7)
          | ((baud_div & UART_CLK_DIVID_MASK) << UART_CLK_DIVID_POSI);

    width = ((width & UART_DATA_LEN_MASK) << UART_DATA_LEN_POSI);
    REG_WRITE(REG_UART_CONFIG(port), reg);

    reg = ((TX_FIFO_THRD & TX_FIFO_THRESHOLD_MASK) << TX_FIFO_THRESHOLD_POSI)
          | ((RX_FIFO_THRD & RX_FIFO_THRESHOLD_MASK) << RX_FIFO_THRESHOLD_POSI)
          | ((RX_STOP_DETECT_TIME32 & RX_STOP_DETECT_TIME_MASK) << RX_STOP_DETECT_TIME_POSI);
    REG_WRITE(REG_UART_FIFO_CONFIG(port), reg);

    REG_WRITE(REG_UART_FLOW_CONFIG(port), 0);
    REG_WRITE(REG_UART_WAKE_CONFIG(port), 0);

    reg = RX_FIFO_NEED_READ_EN | UART_RX_STOP_END_EN;
    REG_WRITE(REG_UART_INTR_ENABLE(port), reg);
}

INT32 uart_read_byte( UINT8 port, UINT8 *byte )
{
    UINT32 val;

    if(REG_READ(REG_UART_FIFO_STATUS(port)) & FIFO_RD_READY)
    {
        UART_READ_BYTE(port, val);
        *byte = (UINT8)val;
        return 0;
    }

    return -1;
}

VOID uart_write_byte( UINT8 port, UINT8 byte )
{
    UART_WRITE_BYTE(port, byte);
}

UINT8 uart_get_tx_fifo_cnt(UINT8 port)
{
    return REG_READ(REG_UART_FIFO_STATUS(port)) >> TX_FIFO_COUNT_POSI & TX_FIFO_COUNT_MASK;
}

UINT8 uart_is_tx_fifo_empty(UINT8 port)
{
    return (REG_READ(REG_UART_FIFO_STATUS(port)) & TX_FIFO_EMPTY) != 0 ? 1 : 0;
}

UINT8 uart_is_tx_fifo_full(UINT8 port)
{
    return (REG_READ(REG_UART_FIFO_STATUS(port)) & TX_FIFO_FULL) != 0 ? 1 : 0;
}

VOID uart_set_tx_stop_end_int(UINT8 port, UINT8 set)
{
    UINT32 reg;

    reg = REG_READ(REG_UART_INTR_ENABLE(port));
    
    if(set == 1)
    {
        reg |= UART_TX_STOP_END_EN;
    }
    else
    {
        reg &= ~UART_TX_STOP_END_EN;
    }
    
    REG_WRITE(REG_UART_INTR_ENABLE(port), reg);
}

// EOF

