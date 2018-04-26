#include "include.h"
#include "arm_arch.h"

#include "sys_ctrl_pub.h"
#include "sys_ctrl.h"
#include "target_util_pub.h"

#include "drv_model_pub.h"

#include "uart_pub.h"
#include "flash_pub.h"

#include "intc_pub.h"
#include "icu_pub.h"
#include "gpio_pub.h"

static SDD_OPERATIONS sctrl_op =
{
    sctrl_ctrl
};

/**********************************************************************/
#if DPLL_DELAY_EN
void sctrl_dpll_delay10us(void)
{
    volatile UINT32 i = 0;
	
    for(i = 0; i < DPLL_DELAY_TIME_10US; i ++)
    {
        ;
    }
}

void sctrl_dpll_delay200us(void)
{
    volatile UINT32 i = 0;
	
    for(i = 0; i < DPLL_DELAY_TIME_200US; i ++)
    {
        ;
    }
}
#endif

void sctrl_cali_dpll(void)
{
    UINT32 param;
   
    param = REG_READ(SCTRL_ANALOG_CTRL0);
    param &= ~(SPI_TRIG_BIT);
    REG_WRITE(SCTRL_ANALOG_CTRL0, param);
    
    #if DPLL_DELAY_EN
    sctrl_dpll_delay10us();
    #endif
    
    param |= (SPI_TRIG_BIT);
    REG_WRITE(SCTRL_ANALOG_CTRL0, param);   

    param = REG_READ(SCTRL_ANALOG_CTRL0);
    param &= ~(SPI_DET_EN);
    REG_WRITE(SCTRL_ANALOG_CTRL0, param);

    #if DPLL_DELAY_EN
    sctrl_dpll_delay200us();
    #endif
    param = REG_READ(SCTRL_ANALOG_CTRL0);
    param |= (SPI_DET_EN);
    REG_WRITE(SCTRL_ANALOG_CTRL0, param);
}

void sctrl_dpll_isr(void)
{
	sctrl_cali_dpll();
	
    sddev_control(GPIO_DEV_NAME, CMD_GPIO_CLR_DPLL_UNLOOK_INT, NULL);

    SCTRL_PRT("DPLL Unlock\r\n");
}

void sctrl_dpll_int_open(void)
{
    UINT32 param;
    param = (FIQ_DPLL_UNLOCK_BIT);
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_ENABLE, &param);
}

void sctrl_dpll_int_close(void)
{
    UINT32 param;
    param = (FIQ_DPLL_UNLOCK_BIT);
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_DISABLE, &param);
}

void sctrl_init(void)
{
    UINT32 param, reg;

    sddev_register_dev(SCTRL_DEV_NAME, &sctrl_op);

    /*enable blk clk*/
    param = BLK_BIT_26M_XTAL | BLK_BIT_DPLL_480M | BLK_BIT_XTAL2RF;
    sctrl_ctrl(CMD_SCTRL_BLK_ENABLE, &param);

    /*config main clk*/
    param = REG_READ(SCTRL_CONTROL);
    param &= ~(MCLK_DIV_MASK << MCLK_DIV_POSI);
    param &= ~(MCLK_MUX_MASK << MCLK_MUX_POSI);
    param |= ((MCLK_DIV_3 & MCLK_DIV_MASK) << MCLK_DIV_POSI);
    param |= ((MCLK_FIELD_DPLL & MCLK_MUX_MASK) << MCLK_MUX_POSI);
    REG_WRITE(SCTRL_CONTROL, param);

    /*sys_ctrl <0x4c> */
    param = 0x00171710;//0x00151510;    LDO BIAS CALIBRATION
    REG_WRITE(SCTRL_BIAS, param);

    /*mac & modem power up */
    sctrl_ctrl(CMD_SCTRL_MAC_POWERUP, NULL);
    sctrl_ctrl(CMD_SCTRL_MODEM_POWERUP, NULL);

    /*sys_ctrl <0x16>, trig spi */
    //170209,from 0x819A54B to 0x819A55B for auto detect dpll unlock
    //170614 from 0x819A55B to 0x819A59B for more easy to trigger
    param = 0x819A59B;
    REG_WRITE(SCTRL_ANALOG_CTRL0, param);

    sctrl_cali_dpll();

    param = 0x6AC03102;
    REG_WRITE(SCTRL_ANALOG_CTRL1, param);
    param = 0x24006000;
    REG_WRITE(SCTRL_ANALOG_CTRL2, param);
    param = 0x4FE06C50;
    REG_WRITE(SCTRL_ANALOG_CTRL3, param);

    /*sys_ctrl <0x1a> */
    param = 0x59E04520;
    REG_WRITE(SCTRL_ANALOG_CTRL4, param);

    /*regist intteruppt handler for Dpll unlock*/
    intc_service_register(FIQ_DPLL_UNLOCK, PRI_FIQ_DPLL_UNLOCK, sctrl_dpll_isr);

    sctrl_sub_reset();

	/*sys ctrl clk gating, for rx dma dead*/
	REG_WRITE(SCTRL_CLK_GATING, 0x3f);

	/* increase VDD voltage*/
	reg = REG_READ(SCTRL_DIGTAL_VDD);
	reg &= (~(DIG_VDD_ACTIVE_MASK << DIG_VDD_ACTIVE_POSI));
	reg |= (5 << DIG_VDD_ACTIVE_POSI);
	REG_WRITE(SCTRL_DIGTAL_VDD, reg);
}

void sctrl_exit(void)
{
    sddev_unregister_dev(SCTRL_DEV_NAME);
}

void sctrl_modem_core_reset(void)
{
    sctrl_ctrl(CMD_SCTRL_MODEM_CORE_RESET, 0);
}

void sctrl_sub_reset(void)
{
    sctrl_ctrl(CMD_SCTRL_MPIF_CLK_INVERT, 0);
    sctrl_ctrl(CMD_SCTRL_MODEM_CORE_RESET, 0);
    sctrl_ctrl(CMD_SCTRL_MODEM_SUBCHIP_RESET, 0);
    sctrl_ctrl(CMD_SCTRL_MAC_SUBSYS_RESET, 0);
    sctrl_ctrl(CMD_SCTRL_USB_SUBSYS_RESET, 0);
}

void sctrl_enter_sleep(void)
{
    DD_HANDLE flash_hdl;
    UINT32 status;
    UINT32 reg;

    /* MAC AHB slave clock disable*/
    reg = REG_READ(SCTRL_MODEM_CORE_RESET_PHY_HCLK);
    reg &= ~MAC_HCLK_EN_BIT;
    REG_WRITE(SCTRL_MODEM_CORE_RESET_PHY_HCLK, reg);

    /* Mac Subsystem clock 480m disable*/
    reg = REG_READ(SCTRL_CONTROL);
    REG_WRITE(SCTRL_CONTROL, reg | MAC_CLK480M_PWD_BIT);

    /* Modem AHB clock disable*/
    reg = REG_READ(SCTRL_MODEM_CORE_RESET_PHY_HCLK);
    reg &= ~PHY_HCLK_EN_BIT;
    REG_WRITE(SCTRL_MODEM_CORE_RESET_PHY_HCLK, reg);

    /* Modem Subsystem clock 480m disable*/
    reg = REG_READ(SCTRL_CONTROL);
    REG_WRITE(SCTRL_CONTROL, reg | MODEM_CLK480M_PWD_BIT);

    /* Modem pwd*/
    reg = REG_READ(SCTRL_PWR_MAC_MODEM);
    reg &= ~(MODEM_PWD_MASK << MODEM_PWD_POSI);
    reg = reg | (MODEM_PWD << MODEM_PWD_POSI);
    REG_WRITE(SCTRL_PWR_MAC_MODEM, reg);

    /* Flash 26MHz clock select dco clock*/
    flash_hdl = ddev_open(FLASH_DEV_NAME, &status, 0);
    ASSERT(DD_HANDLE_UNVALID != flash_hdl);
    ddev_control(flash_hdl, CMD_FLASH_SET_DCO, 0);

    /* MCLK(main clock) select:dco*/ /* MCLK division*/
    reg = REG_READ(SCTRL_CONTROL);
    reg &= ~(MCLK_MUX_MASK << MCLK_MUX_POSI);
    reg &= ~(MCLK_DIV_MASK << MCLK_DIV_POSI);
    reg = reg | (MCLK_FIELD_DCO << MCLK_MUX_POSI);
    reg = reg | (0 << MCLK_DIV_POSI);
    REG_WRITE(SCTRL_CONTROL, reg);

    /* dpll (480m) disable*/
    reg = REG_READ(SCTRL_BLOCK_EN_CFG);
    reg &= ~(BLOCK_EN_WORD_MASK << BLOCK_EN_WORD_POSI);
    reg = reg | (BLOCK_EN_WORD_PWD << BLOCK_EN_WORD_POSI);
    reg &= ~BLK_EN_DPLL_480M;
    REG_WRITE(SCTRL_BLOCK_EN_CFG, reg);

    /* 26m xtal disable*/
    reg = REG_READ(SCTRL_BLOCK_EN_CFG);
    reg &= ~(BLOCK_EN_WORD_MASK << BLOCK_EN_WORD_POSI);
    reg = reg | (BLOCK_EN_WORD_PWD << BLOCK_EN_WORD_POSI);
    reg &= ~BLK_EN_26M_XTAL;
    REG_WRITE(SCTRL_BLOCK_EN_CFG, reg);
}

void sctrl_exit_sleep(void)
{
    UINT32 reg;

    /* 26m xtal enable*/
    reg = REG_READ(SCTRL_BLOCK_EN_CFG);
    reg &= ~(BLOCK_EN_WORD_MASK << BLOCK_EN_WORD_POSI);
    reg |= (BLOCK_EN_WORD_PWD << BLOCK_EN_WORD_POSI);
    reg |= BLK_EN_26M_XTAL;
    REG_WRITE(SCTRL_BLOCK_EN_CFG, reg);

    /* dpll (480m) enable*/
    reg = REG_READ(SCTRL_BLOCK_EN_CFG);
    reg &= ~(BLOCK_EN_WORD_MASK << BLOCK_EN_WORD_POSI);
    reg = reg | (BLOCK_EN_WORD_PWD << BLOCK_EN_WORD_POSI);
    reg |= BLK_EN_DPLL_480M;
    REG_WRITE(SCTRL_BLOCK_EN_CFG, reg);

    /* MCLK(main clock) select:dpll*//* MCLK division*/
    reg = REG_READ(SCTRL_CONTROL);
    reg &= ~(MCLK_MUX_MASK << MCLK_MUX_POSI);
    reg = reg | (MCLK_FIELD_DPLL << MCLK_MUX_POSI);
    reg &= ~(MCLK_DIV_MASK << MCLK_DIV_POSI);
    reg = reg | (5 << MCLK_DIV_POSI);
    REG_WRITE(SCTRL_CONTROL, reg);

    /* Flash 26MHz clock select dpll clock
    {
        DD_HANDLE flash_hdl;
        UINT32 status;
        flash_hdl = ddev_open(FLASH_DEV_NAME, &status, 0);
        ASSERT(DD_HANDLE_UNVALID != flash_hdl);
        ddev_control(flash_hdl, CMD_FLASH_SET_DPLL, 0);
    }*/

    /* MAC AHB slave clock enable*/
    reg = REG_READ(SCTRL_MODEM_CORE_RESET_PHY_HCLK);
    REG_WRITE(SCTRL_MODEM_CORE_RESET_PHY_HCLK, reg | MAC_HCLK_EN_BIT);

    /* Mac Subsystem clock 480m enable*/
    reg = REG_READ(SCTRL_CONTROL);
    reg &= ~MAC_CLK480M_PWD_BIT;
    REG_WRITE(SCTRL_CONTROL, reg);

    /* Modem AHB clock enable*/
    reg = REG_READ(SCTRL_MODEM_CORE_RESET_PHY_HCLK);
    REG_WRITE(SCTRL_MODEM_CORE_RESET_PHY_HCLK, reg | PHY_HCLK_EN_BIT);

    /* Modem Subsystem clock 480m enable*/
    reg = REG_READ(SCTRL_CONTROL);
    reg &= ~MODEM_CLK480M_PWD_BIT;
    REG_WRITE(SCTRL_CONTROL, reg);

    /* Modem pwd*/
    reg = REG_READ(SCTRL_PWR_MAC_MODEM);
    reg &= ~(MODEM_PWD_MASK << MODEM_PWD_POSI);
    reg = reg | (0 << MODEM_PWD_POSI);
    REG_WRITE(SCTRL_PWR_MAC_MODEM, reg);
}

void sctrl_subsys_power(UINT32 cmd)
{
    UINT32 reg = 0;
    UINT32 reg_val;
    UINT32 reg_word = 0;

    switch(cmd)
    {
    case CMD_SCTRL_DSP_POWERDOWN:
        reg = SCTRL_DSP_PWR;
        reg_word = DSP_PWD;
        break;

    case CMD_SCTRL_DSP_POWERUP:
        reg = SCTRL_DSP_PWR;
        reg_word = DSP_PWU;
        break;

    case CMD_SCTRL_USB_POWERDOWN:
        reg = SCTRL_USB_PWR;
        reg_word = USB_PWD;
        break;

    case CMD_SCTRL_USB_POWERUP:
        reg = SCTRL_USB_PWR;
        reg_word = USB_PWU;
        break;

    case CMD_SCTRL_MAC_POWERDOWN:
        reg = SCTRL_PWR_MAC_MODEM;
        reg_val = REG_READ(SCTRL_PWR_MAC_MODEM);
        reg_val &= ~(MAC_PWD_MASK << MAC_PWD_POSI);
        reg_val |= MAC_PWD << MAC_PWD_POSI;
        reg_word = reg_val;
        break;

    case CMD_SCTRL_MAC_POWERUP:
        reg = SCTRL_PWR_MAC_MODEM;
        reg_val = REG_READ(SCTRL_PWR_MAC_MODEM);
        reg_val &= ~(MAC_PWD_MASK << MAC_PWD_POSI);
        reg_val |= MAC_PWU << MAC_PWD_POSI;
        reg_word = reg_val;
        break;

    case CMD_SCTRL_MODEM_POWERDOWN:
        reg = SCTRL_PWR_MAC_MODEM;
        reg_val = REG_READ(SCTRL_PWR_MAC_MODEM);
        reg_val &= ~(MODEM_PWD_MASK << MODEM_PWD_POSI);
        reg_val |= MODEM_PWD << MODEM_PWD_POSI;
        reg_word = reg_val;
        break;

    case CMD_SCTRL_MODEM_POWERUP:
        reg = SCTRL_PWR_MAC_MODEM;
        reg_val = REG_READ(SCTRL_PWR_MAC_MODEM);
        reg_val &= ~(MODEM_PWD_MASK << MODEM_PWD_POSI);
        reg_val |= MODEM_PWU << MODEM_PWD_POSI;
        reg_word = reg_val;
        break;

    default:
        break;
    }

    if(reg)
    {
        REG_WRITE(reg, reg_word);
    }
}

void sctrl_subsys_reset(UINT32 cmd)
{
    UINT32 reg = 0;
    UINT32 reset_word = 0;

    switch(cmd)
    {
    case CMD_SCTRL_MODEM_SUBCHIP_RESET:
        reg = SCTRL_MODEM_SUBCHIP_RESET_REQ;
        reset_word = MODEM_SUBCHIP_RESET_WORD;
        break;

    case CMD_SCTRL_MAC_SUBSYS_RESET:
        reg = SCTRL_MAC_SUBSYS_RESET_REQ;
        reset_word = MAC_SUBSYS_RESET_WORD;
        break;

    case CMD_SCTRL_USB_SUBSYS_RESET:
        reg = SCTRL_USB_SUBSYS_RESET_REQ;
        reset_word = USB_SUBSYS_RESET_WORD;
        break;

    case CMD_SCTRL_DSP_SUBSYS_RESET:
        reg = SCTRL_DSP_SUBSYS_RESET_REQ;
        reset_word = DSP_SUBSYS_RESET_WORD;
        break;

    default:
        break;
    }

    if(reg)
    {
        REG_WRITE(reg, reset_word);
        delay(10);
        REG_WRITE(reg, 0);
    }

    return;
}

UINT32 sctrl_ctrl(UINT32 cmd, void *param)
{
    UINT32 ret;
    UINT32 reg;

    ret = SCTRL_SUCCESS;
    switch(cmd)
    {
    case CMD_SCTRL_SLEEP:
        sctrl_enter_sleep();
        break;

    case CMD_SCTRL_WAKEUP:
        sctrl_exit_sleep();
        break;

    case CMD_GET_CHIP_ID:
        ret = REG_READ(SCTRL_CHIP_ID);
        break;

    case CMD_SCTRL_SET_FLASH_DPLL:
        reg = REG_READ(SCTRL_CONTROL);
        reg |= FLASH_26M_MUX_BIT;
        REG_WRITE(SCTRL_CONTROL, reg);
        break;

    case CMD_SCTRL_SET_FLASH_DCO:
        reg = REG_READ(SCTRL_CONTROL);
        reg &= ~FLASH_26M_MUX_BIT;
        REG_WRITE(SCTRL_CONTROL, reg);
        break;

    case CMD_SCTRL_DSP_POWERDOWN:
    case CMD_SCTRL_USB_POWERDOWN:
    case CMD_SCTRL_MODEM_POWERDOWN:
    case CMD_SCTRL_MAC_POWERDOWN:
    case CMD_SCTRL_DSP_POWERUP:
    case CMD_SCTRL_USB_POWERUP:
    case CMD_SCTRL_MAC_POWERUP:
    case CMD_SCTRL_MODEM_POWERUP:
        sctrl_subsys_power(cmd);
        break;

    case CMD_GET_DEVICE_ID:
        ret = REG_READ(SCTRL_DEVICE_ID);
        break;

    case CMD_GET_SCTRL_CONTROL:
        *((UINT32 *)param) = REG_READ(SCTRL_CONTROL);
        break;

    case CMD_SET_SCTRL_CONTROL:
        REG_WRITE(SCTRL_CONTROL, *((UINT32 *)param));
        break;

    case CMD_SCTRL_MCLK_SELECT:
        reg = REG_READ(SCTRL_CONTROL);
        reg &= ~(MCLK_MUX_MASK << MCLK_MUX_POSI);
        reg |= ((*(UINT32 *)param) & MCLK_MUX_MASK) << MCLK_MUX_POSI;
        REG_WRITE(SCTRL_CONTROL, reg);
        break;

    case CMD_SCTRL_MCLK_DIVISION:
        reg = REG_READ(SCTRL_CONTROL);
        reg &= ~(MCLK_DIV_MASK << MCLK_DIV_POSI);
        reg |= ((*(UINT32 *)param) & MCLK_DIV_MASK) << MCLK_DIV_POSI;
        REG_WRITE(SCTRL_CONTROL, reg);
        break;

    case CMD_SCTRL_RESET_SET:
        reg = REG_READ(SCTRL_RESET);
        reg |= ((*(UINT32 *)param) & SCTRL_RESET_MASK);
        REG_WRITE(SCTRL_RESET, reg);
        break;

    case CMD_SCTRL_RESET_CLR:
        reg = REG_READ(SCTRL_RESET);
        reg &= ~((*(UINT32 *)param) & SCTRL_RESET_MASK);
        REG_WRITE(SCTRL_RESET, reg);
        break;

    case CMD_SCTRL_MODEM_SUBCHIP_RESET:
    case CMD_SCTRL_MAC_SUBSYS_RESET:
    case CMD_SCTRL_USB_SUBSYS_RESET:
    case CMD_SCTRL_DSP_SUBSYS_RESET:
        sctrl_subsys_reset(cmd);
        break;

    case CMD_SCTRL_MODEM_CORE_RESET:
        ret = REG_READ(SCTRL_MODEM_CORE_RESET_PHY_HCLK);
        ret = ret & (~((MODEM_CORE_RESET_MASK) << MODEM_CORE_RESET_POSI));
        reg = ret | ((MODEM_CORE_RESET_WORD & MODEM_CORE_RESET_MASK)
                     << MODEM_CORE_RESET_POSI);
        REG_WRITE(SCTRL_MODEM_CORE_RESET_PHY_HCLK, reg);

        delay(5);
        reg = ret;
        REG_WRITE(SCTRL_MODEM_CORE_RESET_PHY_HCLK, reg);

        /*resetting, and waiting for done*/
        reg = REG_READ(SCTRL_RESET);
        while(reg & MODEM_CORE_RESET_BIT)
        {
            delay(10);
            reg = REG_READ(SCTRL_RESET);
        }
        ret = SCTRL_SUCCESS;
        break;

    case CMD_SCTRL_MPIF_CLK_INVERT:
        reg = REG_READ(SCTRL_CONTROL);
        reg |= MPIF_CLK_INVERT_BIT;
        REG_WRITE(SCTRL_CONTROL, reg);
        break;

    case CMD_SCTRL_BLK_ENABLE:
        reg = REG_READ(SCTRL_BLOCK_EN_CFG);
        reg &= (~(BLOCK_EN_WORD_MASK << BLOCK_EN_WORD_POSI));
        reg |= (BLOCK_EN_WORD_PWD & BLOCK_EN_WORD_MASK) << BLOCK_EN_WORD_POSI;
        reg |= ((*(UINT32 *)param) & BLOCK_EN_VALID_MASK);
        REG_WRITE(SCTRL_BLOCK_EN_CFG, reg);
        break;

    case CMD_SCTRL_BLK_DISABLE:
        reg = REG_READ(SCTRL_BLOCK_EN_CFG);
        reg &= ~((*(UINT32 *)param) & BLOCK_EN_VALID_MASK);
        REG_WRITE(SCTRL_BLOCK_EN_CFG, reg);
        break;

    case CMD_SCTRL_BIAS_REG_SET:
        reg = REG_READ(SCTRL_BIAS);
        reg |= (*(UINT32 *)param);
        REG_WRITE(SCTRL_BIAS, reg);
        break;

    case CMD_SCTRL_BIAS_REG_CLEAN:
        reg = REG_READ(SCTRL_BIAS);
        reg &= ~(*(UINT32 *)param);
        REG_WRITE(SCTRL_BIAS, reg);
        break;

    case CMD_SCTRL_BIAS_REG_READ:
        ret = REG_READ(SCTRL_BIAS);
        break;

    case CMD_SCTRL_BIAS_REG_WRITE:
        REG_WRITE(SCTRL_BIAS, *(UINT32 *)param);
        break;

    case CMD_SCTRL_ANALOG_CTRL4_SET:
        reg = REG_READ(SCTRL_ANALOG_CTRL4);
        reg |= (*(UINT32 *)param);
        REG_WRITE(SCTRL_ANALOG_CTRL4, reg);
        break;

    case CMD_SCTRL_ANALOG_CTRL4_CLEAN:
        reg = REG_READ(SCTRL_ANALOG_CTRL4);
        reg &= ~(*(UINT32 *)param);
        REG_WRITE(SCTRL_ANALOG_CTRL4, reg);
        break;

    case CMD_SCTRL_CALI_DPLL:
        sctrl_cali_dpll();
        break;

    default:
        ret = SCTRL_FAILURE;
        break;
    }

    return ret;
}

// EOF
