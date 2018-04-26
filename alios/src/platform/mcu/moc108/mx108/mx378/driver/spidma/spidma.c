#include "include.h"
#include "arm_arch.h"

#if CFG_USE_SPIDMA

#include "uart_pub.h"
#include "spidma_pub.h"
#include "spidma.h"

#include "mem_pub.h"
#include "intc_pub.h"

#include "icu_pub.h"
#include "gpio_pub.h"


SPIDMA_DESC_PTR p_spidma_desc;
static DD_OPERATIONS spidma_op =
{
    spidma_open,
    spidma_close,
    NULL,
    NULL,
    spidma_ctrl
};

static void spidma_config_sck_invert(UINT32 negedge)
{
    UINT32 reg;
    reg = REG_READ(SPI_DMA_REG0);
    if(negedge)
        reg |= SPIDMA_REG0_SCK_INV;
    else
        reg &= ~(SPIDMA_REG0_SCK_INV);
    REG_WRITE(SPI_DMA_REG0, reg);
}

static void spidma_config_LSB_first_transfer(UINT32 ture)
{
    UINT32 reg;
    reg = REG_READ(SPI_DMA_REG0);
    if(ture)
        reg |= SPIDMA_REG0_LSB_FIRST;
    else
        reg &= ~(SPIDMA_REG0_LSB_FIRST);
    REG_WRITE(SPI_DMA_REG0, reg);
}

static void spidma_config_3wire_mode(UINT32 ture)
{
    UINT32 reg;
    reg = REG_READ(SPI_DMA_REG0);
    if(ture)
        reg |= SPIDMA_REG0_WIRE3_EN;
    else
        reg &= ~(SPIDMA_REG0_WIRE3_EN);
    REG_WRITE(SPI_DMA_REG0, reg);
}

static void spidma_config_rxdata_timeout_val(UINT32 timeout)
{
    REG_WRITE(SPI_DMA_REG1, timeout & SPIDMA_REG1_TIMEOUT_MASK);
}

static void spidma_config_rxbuf_start_addr(UINT8 *rxbufptr)
{
    REG_WRITE(SPI_DMA_REG2_RXBUF_ADDR, (UINT32)rxbufptr);
}

#ifdef SPIDMA_GET_RX_BUF_ADDR
static UINT8 *spidma_get_rxbuf_start_addr(void)
{
    return (UINT8 *)REG_READ(SPI_DMA_REG2_RXBUF_ADDR);
}
#endif

static void spidma_config_rxbuf_length(UINT32 rxbuflen)
{
    UINT32 reg;
    switch(rxbuflen)
    {
    case 1024:
        reg = SPIDMA_RXBUFLEN_1024B;
        break;
    case 2048:
        reg = SPIDMA_RXBUFLEN_2048B;
        break;
    case 4096:
        reg = SPIDMA_RXBUFLEN_4096B;
        break;
    case 8192:
        reg = SPIDMA_RXBUFLEN_8192B;
        break;
    case 16384:
        reg = SPIDMA_RXBUFLEN_16384B;
        break;
    case 32768:
        reg = SPIDMA_RXBUFLEN_32768B;
        break;
    case 65536:
        reg = SPIDMA_RXBUFLEN_65536B;
        break;
    default:
        reg = SPIDMA_RXBUFLEN_1024B;
        break;
    }

    REG_WRITE(SPI_DMA_REG3_RXBUF_LEN, reg);
}

static void spidma_config_rx_threshold_int(UINT32 threshold)
{
    REG_WRITE(SPI_DMA_REG4_RXBUF_THRE, (threshold - 1)&SPIDMA_REG4_RXBUF_THRE_MASK);
}

static UINT32 spidma_get_rx_threshold_val(void)
{
    return (REG_READ(SPI_DMA_REG4_RXBUF_THRE)&SPIDMA_REG4_RXBUF_THRE_MASK) + 1;
}

static UINT32 spidma_config_get_rxbuf_readptr(void)
{
    return REG_READ(SPI_DMA_REG5_RXBUF_RDPTR) & SPIDMA_REG5_RXBUF_RDPTR_MASK;
}

static void spidma_config_set_rxbuf_readptr(UINT32 rdptr)
{
    REG_WRITE(SPI_DMA_REG5_RXBUF_RDPTR, rdptr & SPIDMA_REG5_RXBUF_RDPTR_MASK);
}

static UINT32 spidma_get_rxbuf_writeptr(void)
{
    return REG_READ(SPI_DMA_REG6_RXBUF_WRPTR)&SPIDMA_REG6_RXBUF_WRPTR_MASK;
}

static void spidma_config_txbuf_start_addr(UINT8 *txbufptr)
{
    REG_WRITE(SPI_DMA_REG7_TXBUF_ADDR, (UINT32)txbufptr);
}

static void spidma_config_txbuf_length(UINT32 txbuflen)
{
    REG_WRITE(SPI_DMA_REG8_TXBUF_LEN, txbuflen);
}

static void spidma_set_txbuf_valid(UINT32 ture)
{
    if(ture)
        REG_WRITE(SPI_DMA_REG9_TXBUF_VALID, 0x1);
    else
        REG_WRITE(SPI_DMA_REG9_TXBUF_VALID, 0x0);
}

static UINT32 spidma_set_rxbuf_valid_datalen(void)
{
    return REG_READ(SPI_DMA_REG11) & SPIDMA_REG11_RXBUF_VLDNUM_MASK;
}

static void spidma_enable_interrupt(void)
{
    UINT32 param;
    param = (FIQ_SPI_DMA_BIT);
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_ENABLE, &param);
}

static void spidma_disable_interrupt(void)
{
    UINT32 param;
    param = (FIQ_SPI_DMA_BIT);
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_DISABLE, &param);
}

static void spidma_gpio_config(void)
{
    UINT32 param;
    param = GFUNC_MODE_SPI_DMA;
    sddev_control(GPIO_DEV_NAME, CMD_GPIO_ENABLE_SECOND, &param);
}

static void spidma_software_init(void)
{
    p_spidma_desc = NULL;
    ddev_register_dev(SPIDMA_DEV_NAME, &spidma_op);
}

static void spidma_hardware_init(void)
{
    UINT32 reg;
    /* register interrupt */
    intc_service_register(FIQ_SPI_DMA, PRI_FIQ_SPI_DMA, spidma_isr);

    /* config spidma register0 to 0 */
    REG_WRITE(SPI_DMA_REG0, 0);

    /* config spidma registers, rx first */
    spidma_config_rxdata_timeout_val(SPIDMA_DEF_RXDATA_TIMEOUT_VAL);
    spidma_config_rxbuf_start_addr(NULL);
    spidma_config_rxbuf_length(0);
    spidma_config_set_rxbuf_readptr(0);
    spidma_config_rx_threshold_int(SPIDMA_DEF_RXDATA_THRE_INT);

    /* tx */
    spidma_config_txbuf_start_addr(NULL);
    spidma_config_txbuf_length(0);
    spidma_set_txbuf_valid(0);

    /* reset int status */
    reg = REG_READ(SPI_DMA_REG10_INT_STATUS);
    REG_WRITE(SPI_DMA_REG10_INT_STATUS, reg);
    REG_WRITE(SPI_DMA_REG11, 0);

    /* config gpio for spidam  */
    spidma_gpio_config();

}

void spidma_init(void)
{
    spidma_software_init();
    spidma_hardware_init();
}

static UINT32 spidma_open(UINT32 op_flag)
{
    UINT32 reg;

    if(!op_flag)
    {
        SPI_DMA_PRT("spidma_open op_flag is NULL\r\n");
        return SPIDMA_FAILURE;
    }

    p_spidma_desc = (SPIDMA_DESC_PTR)op_flag;

    if(!p_spidma_desc->node_len)
    {
        SPI_DMA_PRT("spidma_open node_len is 0\r\n");
        return SPIDMA_FAILURE;
    }

    p_spidma_desc->rx_callback = spidma_rx_callback;

    spidma_config_rxdata_timeout_val(p_spidma_desc->timeout_val);
    spidma_config_rxbuf_start_addr(p_spidma_desc->rxbuf);
    spidma_config_rxbuf_length((p_spidma_desc->rxbuf_len));
    spidma_config_set_rxbuf_readptr(0);
    spidma_config_rx_threshold_int((p_spidma_desc->mode >> SPIDMA_DESC_RX_THRED_POSI)
                                   &SPIDMA_DESC_RX_THRED_MASK);

    //spidma_config_txbuf_start_addr(p_spidma_desc->txbuf);
    spidma_config_txbuf_start_addr(NULL);
    spidma_config_txbuf_length(0);
    spidma_set_txbuf_valid(0);

    spidma_config_sck_invert((p_spidma_desc->mode >> SPIDMA_DESC_SCK_MODE_POSI)
                             &SPIDMA_DESC_SCK_MODE_MASK);
    spidma_config_LSB_first_transfer((p_spidma_desc->mode >> SPIDMA_DESC_LSB_FIRST_POSI)
                                     &SPIDMA_DESC_LSB_FIRST_MASK);
    spidma_config_3wire_mode((p_spidma_desc->mode >> SPIDMA_DESC_3WIRE_MODE_POSI)
                             &SPIDMA_DESC_3WIRE_MODE_MASK);
    reg = SPIDMA_REG0_SPI_EN
          | SPIDMA_REG0_DMA_EN
          | SPIDMA_REG0_TIMEOUT_INTEN
          | SPIDMA_REG0_RXBUF_THRED_INTEN
          | SPIDMA_REG0_TX_FINISH_INTEN
          | SPIDMA_REG0_RXEN;

    REG_WRITE(SPI_DMA_REG0, reg);
    spidma_enable_interrupt();

    return SPIDMA_SUCCESS;
}

void spidma_uninit(void)
{

}

static UINT32 spidma_close(void)
{
    UINT32 reg;

    spidma_disable_interrupt();

    reg = REG_READ(SPI_DMA_REG0);
    reg &= ~(SPIDMA_REG0_SPI_EN
             | SPIDMA_REG0_DMA_EN
             | SPIDMA_REG0_TIMEOUT_INTEN
             | SPIDMA_REG0_RXBUF_THRED_INTEN
             | SPIDMA_REG0_TX_FINISH_INTEN
             | SPIDMA_REG0_RXEN);
    REG_WRITE(SPI_DMA_REG0, reg);

    return SPIDMA_SUCCESS;
}

static void spidma_rx_callback(UINT16 used_data_len)
{
}

UINT32 gcur_posi = 0;
static void spidma_rx_handler(UINT32 timeout)
{
#if 1
    UINT32 new_len = 0, thre = 0, valid = 0;
    UINT8 *rdaddr;
    UINT32 cur_posi = spidma_config_get_rxbuf_readptr();

    //REG_WRITE((0x00802800+(7*4)), 0x02);

    valid = REG_READ(SPI_DMA_REG11) & SPIDMA_REG11_RXBUF_VLDNUM_MASK;
    //thre = (REG_READ(SPI_DMA_REG4_RXBUF_THRE)&SPIDMA_REG4_RXBUF_THRE_MASK) + 1;
    thre = p_spidma_desc->node_len;

    while(valid >= p_spidma_desc->node_len )
    {
        new_len = (valid < thre) ? valid : thre;
        rdaddr = p_spidma_desc->rxbuf + (cur_posi & (p_spidma_desc->rxbuf_len - 1));
        if(p_spidma_desc->rx_handler != NULL)
        {
            p_spidma_desc->rx_handler(rdaddr, new_len);
        }
        valid -= new_len;
        cur_posi += new_len;
    }
    spidma_config_set_rxbuf_readptr(cur_posi);

    if(timeout)
        REG_WRITE(SPI_DMA_REG10_INT_STATUS, SPIDMA_INTSTA_RX_TIMEOUT);
    else
        REG_WRITE(SPI_DMA_REG10_INT_STATUS, SPIDMA_INTSTA_RXBUF_THRE);

    //REG_WRITE((0x00802800+(7*4)), 0x00);
#else
    UINT32 new_len = 0, thre = 0, valid = 0;
    UINT8 *rdaddr;
#if 0
    spidma_config_set_rxbuf_readptr(gcur_posi);
    valid = spidma_set_rxbuf_valid_datalen();
    thre = spidma_get_rx_threshold_val();
    new_len = (valid < thre) ? valid : thre;
    rdaddr = spidma_get_rxbuf_start_addr() + (gcur_posi & ((p_spidma_desc->rxbuf_len - 1)));
    gcur_posi += new_len;
#else
    UINT32 cur_posi = spidma_config_get_rxbuf_readptr();
    REG_WRITE((0x00802800 + (7 * 4)), 0x02);
    rdaddr = p_spidma_desc->rxbuf + (cur_posi & (p_spidma_desc->rxbuf_len - 1));
    valid = spidma_set_rxbuf_valid_datalen();
    thre = spidma_get_rx_threshold_val();
    new_len = (valid < thre) ? valid : thre;
    cur_posi += new_len;
    spidma_config_set_rxbuf_readptr(cur_posi);
#endif
    if(timeout)
        REG_WRITE(SPI_DMA_REG10_INT_STATUS, SPIDMA_INTSTA_RX_TIMEOUT);
    else
        REG_WRITE(SPI_DMA_REG10_INT_STATUS, SPIDMA_INTSTA_RXBUF_THRE);

    if(p_spidma_desc->rx_handler != NULL)
    {
        p_spidma_desc->rx_handler(rdaddr, new_len);
    }
    REG_WRITE((0x00802800 + (7 * 4)), 0x00);

#endif
}

static void spidma_txfinish_handler(void)
{
    if(p_spidma_desc->tx_handler != NULL)
        p_spidma_desc->tx_handler();
    REG_WRITE(SPI_DMA_REG10_INT_STATUS, SPIDMA_INTSTA_TXFINISH);
}

static void spidma_isr(void)
{
    UINT32 spidma_sta;

    /*0, get isr status*/
    spidma_sta = REG_READ(SPI_DMA_REG10_INT_STATUS);

    /*1, handle isr branch*/
    if(spidma_sta & SPIDMA_INTSTA_RXBUF_THRE)
    {
        SPI_DMA_PRT("0\r\n");
        spidma_rx_handler(0);
    }
    else if(spidma_sta & SPIDMA_INTSTA_RX_TIMEOUT)
    {
        SPI_DMA_PRT("1\r\n");
        spidma_rx_handler(1);
    }
    if(spidma_sta & SPIDMA_INTSTA_TXFINISH)
    {
        SPI_DMA_PRT("2\r\n");
        spidma_txfinish_handler();
    }
}

static UINT32 spidma_ctrl(UINT32 cmd, void *param)
{
    UINT32 ret = SPIDMA_SUCCESS;
    switch(cmd)
    {
    case SPIDMA_CMD_CONF_SCK_INV:
        spidma_config_sck_invert(*((UINT32 *)param));
        break;
    case SPIDMA_CMD_CONF_LSB_FIRST:
        spidma_config_LSB_first_transfer(*((UINT32 *)param));
        break;
    case SPIDMA_CMD_CONF_3WIRE_EN:
        spidma_config_3wire_mode(*((UINT32 *)param));
        break;
    case SPIDMA_CMD_CONF_TIMEOUT_VAL:
        spidma_config_rxdata_timeout_val(*((UINT32 *)param));
        break;
    case SPIDMA_CMD_CONF_RXBUF_ADDR:
        spidma_config_rxbuf_start_addr((UINT8 *)param);
        break;
    case SPIDMA_CMD_CONF_RXBUF_LEN:
        spidma_config_rxbuf_length(*((UINT32 *)param));
        break;
    case SPIDMA_CMD_CONF_RXDATA_THRE:
        spidma_config_rx_threshold_int(*((UINT32 *)param));
        break;
    case SPIDMA_CMD_GET_RXDATA_THRE:
        ret = spidma_get_rx_threshold_val();
        break;
    case SPIDMA_CMD_GET_RXBUF_RDPTR:
        ret = spidma_config_get_rxbuf_readptr();
        break;
    case SPIDMA_CMD_SET_RXBUF_RDPTR:
        spidma_config_set_rxbuf_readptr(*((UINT32 *)param));
        break;
    case SPIDMA_CMD_GET_RXBUF_WRPTR:
        ret = spidma_get_rxbuf_writeptr();
        break;
    case SPIDMA_CMD_CONF_TXBUF_ADDR:
        spidma_config_txbuf_start_addr((UINT8 *)param);
        break;
    case SPIDMA_CMD_CONF_TXBUF_LEN:
        spidma_config_txbuf_length(*((UINT32 *)param));
        break;
    case SPIDMA_CMD_CONF_TXBUF_VALID:
        spidma_set_txbuf_valid(*((UINT32 *)param));
        break;
    case SPIDMA_CMD_GET_RXBUF_VALID_DATALEN:
        ret =  spidma_set_rxbuf_valid_datalen();
        break;
    default:
        break;
    }

    return ret;
}

#endif  // CFG_USE_SPIDMA

