#include "include.h"
#include "arm_arch.h"

#include "i2s.h"
#include "i2s_pub.h"

#include "drv_model_pub.h"
#include "intc_pub.h"


static SDD_OPERATIONS i2s_op =
{
    i2s_ctrl
};

static void i2s_active(int enable)
{
    UINT32 value_ctrl, value_cn;

    value_ctrl = REG_READ(PCM_CTRL);
    value_cn = REG_READ(PCM_CN);
    if(enable)
    {
        value_ctrl |= I2S_PCM_EN;
        value_cn |= TX_INT0_EN | TX_UDF0_EN;
    }
    else
    {
        value_ctrl &= ~I2S_PCM_EN;
        value_cn &= ~(TX_INT0_EN | TX_UDF0_EN | RX_INT_EN | RX_OVF_EN);
    }
    REG_WRITE(PCM_CTRL, value_ctrl);
    REG_WRITE(PCM_CN, value_cn);
}

static void i2s_rx_active(int enable)
{
    UINT32 value_cn;

    value_cn = REG_READ(PCM_CN);
    if(enable)
    {
        value_cn |= (RX_INT_EN | RX_OVF_EN | RX_FIFO_CLR);
    }
    else
    {
        value_cn &= ~(RX_INT_EN | RX_OVF_EN);
    }
    REG_WRITE(PCM_CN, value_cn);
}

static void i2s_set_master(i2s_rate_t *p_rate)
{
    UINT32 bitratio, value = 0;

    if(p_rate->freq != 8000 && p_rate->freq != 16000 && p_rate->freq != 44100 && p_rate->freq != 48000)
    {
        return;
    }

    if(p_rate->datawidth > 32)
    {
        return;
    }

    bitratio = ((I2S_SYS_CLK / (p_rate->freq * p_rate->datawidth ) / 4) - 1) & 0xFF;

    value = ((p_rate->datawidth - 1) << DATALEN_POSI)
            | ((p_rate->datawidth - 1) << SMPRATIO_POSI)
            | (bitratio << BITRATIO_POSI)
            | MSTEN;
    REG_WRITE(PCM_CTRL, value);
}

static void i2s_select_mode(UINT8 mode)
{
    UINT32 value;

    if(mode == 3 || mode > 7)
    {
        return;
    }

    value = REG_READ(PCM_CTRL);
    value &= ~(MODE_SEL_MASK << MODE_SEL_POSI);
    value |= (mode << MODE_SEL_POSI);
    REG_WRITE(PCM_CTRL, value);
}

static void i2s_set_level(i2s_level_t *p_level)
{
    UINT32 value;

    value = REG_READ(PCM_CN);

    value &= ~((RX_FIFO_LEVEL_MASK << RX_FIFO_LEVEL_POSI) | (TX_FIFO0_LEVEL_MASK << TX_FIFO0_LEVEL_POSI));
    value |= ((p_level->rx_level << RX_FIFO_LEVEL_POSI) | (p_level->tx_level << TX_FIFO0_LEVEL_POSI));

    REG_WRITE(PCM_CN, value);
}

void i2s_init(void)
{
    intc_service_register(IRQ_I2S_PCM, PRI_IRQ_I2S_PCM, i2s_isr);

    sddev_register_dev(I2S_DEV_NAME, &i2s_op);
}

void i2s_exit(void)
{
    sddev_unregister_dev(I2S_DEV_NAME);
}

static UINT32 i2s_ctrl(UINT32 cmd, void *param)
{
    UINT8 ret = I2S_SUCCESS;

    switch(cmd)
    {
    case I2S_CMD_ACTIVE:
        i2s_active(*(UINT8 *)param);
        break;
    case I2S_CMD_RXACTIVE:
        i2s_rx_active(*(UINT8 *)param);
        break;
    case I2S_CMD_SET_MASTER:
        i2s_set_master((i2s_rate_t *)param);
        break;
    case I2S_CMD_SELECT_MODE:
        i2s_select_mode(*(UINT8 *)param);
        break;
    case I2S_CMD_SET_LEVEL:
        i2s_set_level((i2s_level_t *)param);
        break;
    default:
        ret = I2S_FAILURE;
        break;
    }
    return ret;
}

void i2s_isr(void)
{
}
