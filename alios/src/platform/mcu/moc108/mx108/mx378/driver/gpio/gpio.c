#include "include.h"
#include "arm_arch.h"

#include "gpio_pub.h"
#include "gpio.h"

#include "drv_model_pub.h"
#include "sys_ctrl_pub.h"
#include "uart_pub.h"
#include "intc_pub.h"
#include "icu_pub.h"

static void (*p_gpio_Intr_handler[GPIONUM])(unsigned char );

static SDD_OPERATIONS gpio_op =
{
    gpio_ctrl
};

void gpio_get_conguration(void *setting)
{
}

void gpio_isr(void)
{
    int i;
    unsigned long ulIntStatus;

    ulIntStatus = *(volatile UINT32 *)REG_GPIO_INTSTA;
    for (i = 0; i < GPIONUM; i++)
    {
        if (ulIntStatus & (0x01UL << i))
        {
            if (p_gpio_Intr_handler[i] != NULL)
            {
                (void)p_gpio_Intr_handler[i]((unsigned char)i);
            }
        }
    }

    *(volatile UINT32 *)REG_GPIO_INTSTA = ulIntStatus;
}

static UINT32 gpio_ops_filter(UINT32 index)
{
    UINT32 ret;

    ret = GPIO_FAILURE;

#ifdef JTAG_GPIO_FILTER
    if((GPIO20 == index)
            || (GPIO21 == index)
            || (GPIO22 == index)
            || (GPIO23 == index))
    {
        WARN_PRT("gpio_filter_%d\r\n", index);
        ret = GPIO_SUCCESS;

        goto filter_exit;
    }
#endif

#ifdef JTAG_GPIO_FILTER
filter_exit:
#endif

    return ret;
}

void gpio_config(UINT32 index, UINT32 mode)
{
    UINT32 val;
    UINT32 overstep = 0;
    volatile UINT32 *gpio_cfg_addr;

    if(GPIO_SUCCESS == gpio_ops_filter(index))
    {
        goto cfg_exit;
    }

    if(index >= GPIONUM)
    {
        WARN_PRT("gpio_id_cross_border\r\n");

        goto cfg_exit;
    }
    gpio_cfg_addr = (volatile UINT32 *)(REG_GPIO_CFG_BASE_ADDR + index * 4);

    switch(mode)
    {
    case GMODE_INPUT_PULLDOWN:
        val = 0x2C;
        break;

    case GMODE_OUTPUT:
        val = 0x00;
        break;

    case GMODE_SECOND_FUNC:
        val = 0x40;
        break;

    case GMODE_INPUT_PULLUP:
        val = 0x3C;
        break;

    case GMODE_INPUT:
        val = 0x0C;
        break;
    case GMODE_SECOND_FUNC_PULL_UP:
        val = 0x70;
        break;

    default:
        overstep = 1;
        WARN_PRT("gpio_mode_exception:%d\r\n", mode);
        break;
    }

    if(0 == overstep)
    {
        REG_WRITE(gpio_cfg_addr, val);
    }

cfg_exit:
    return;
}

static void gpio_enable_second_function(UINT32 func_mode)
{
    UINT32 i;
    UINT32 reg;
    UINT32 modul_select = GPIO_MODUL_NONE;
    UINT32 pmode = PERIAL_MODE_1;
    UINT32 end_index = 0;
    UINT32 start_index = 0;
    UINT32 config_pull_up = 0;

    switch(func_mode)
    {
    case GFUNC_MODE_UART2:
        start_index = 0;
        end_index = 1;
        pmode = PERIAL_MODE_1;
        config_pull_up = 1;
        break;

    case GFUNC_MODE_I2C2:
        start_index = 0;
        end_index = 1;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_I2S:
        start_index = 2;
        end_index = 5;
        pmode = PERIAL_MODE_1;
        break;

    case GFUNC_MODE_ADC1:
        start_index = 4;
        end_index = 4;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_ADC2:
        start_index = 5;
        end_index = 5;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_CLK13M:
        start_index = 6;
        end_index = 6;
        pmode = PERIAL_MODE_1;
        break;

    case GFUNC_MODE_PWM0:
        start_index = 6;
        end_index = 6;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_PWM1:
        start_index = 7;
        end_index = 7;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_PWM2:
        start_index = 8;
        end_index = 8;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_PWM3:
        start_index = 9;
        end_index = 9;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_BT_PRIORITY:
        start_index = 9;
        end_index = 9;
        pmode = PERIAL_MODE_1;
        break;

    case GFUNC_MODE_WIFI_ACTIVE:
        start_index = 7;
        end_index = 7;
        pmode = PERIAL_MODE_1;
        break;

    case GFUNC_MODE_BT_ACTIVE:
        start_index = 8;
        end_index = 8;
        pmode = PERIAL_MODE_1;
        break;

    case GFUNC_MODE_UART1:
        start_index = 10;
        end_index = 13;
        pmode = PERIAL_MODE_1;
        config_pull_up = 1;
        break;

    case GFUNC_MODE_SD_DMA:
        start_index = 14;
        end_index = 19;
        pmode = PERIAL_MODE_1;
        modul_select = GPIO_SD_DMA_MODULE;
        break;

    case GFUNC_MODE_SD_HOST:
        start_index = 14;
        end_index = 19;
        pmode = PERIAL_MODE_1;
        modul_select = GPIO_SD_HOST_MODULE;
        break;

    case GFUNC_MODE_SPI_DMA:
        start_index = 14;
        end_index = 17;
        pmode = PERIAL_MODE_2;
        modul_select = GPIO_SPI_DMA_MODULE;
        //REG_WRITE(REG_A2_CONFIG, reg);
        break;

    case GFUNC_MODE_SPI:
        start_index = 14;
        end_index = 17;
        pmode = PERIAL_MODE_2;
        modul_select = GPIO_SPI_MODULE;
        break;

    case GFUNC_MODE_PWM4:
        start_index = 18;
        end_index = 18;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_PWM5:
        start_index = 19;
        end_index = 19;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_I2C1:
        start_index = 20;
        end_index = 21;
        pmode = PERIAL_MODE_1;
        break;

    case GFUNC_MODE_JTAG:
        start_index = 20;
        end_index = 23;
        pmode = PERIAL_MODE_2;
        break;

    case GFUNC_MODE_CLK26M:
        start_index = 22;
        end_index = 22;
        pmode = PERIAL_MODE_1;
        break;

    case GFUNC_MODE_ADC3:
        start_index = 23;
        end_index = 23;
        pmode = PERIAL_MODE_1;
        break;

    default:
        break;
    }

    for(i = start_index; i <= end_index; i++)
    {
        if(config_pull_up == 0)
            gpio_config(i, GMODE_SECOND_FUNC);
        else
            gpio_config(i, GMODE_SECOND_FUNC_PULL_UP);

        reg = REG_READ(REG_GPIO_FUNC_CFG);
        if(PERIAL_MODE_1 == pmode)
        {
            reg &= ~(BIT(i));
        }
        else /*(PERIAL_MODE_2 == pmode)*/
        {
            reg |= BIT(i);
        }
        REG_WRITE(REG_GPIO_FUNC_CFG, reg);
    }

    if(modul_select != GPIO_MODUL_NONE)
    {
        reg = modul_select & 0x3;
        REG_WRITE(REG_GPIO_MODULE_SELECT, reg);
    }
    return;
}

static UINT32 gpio_input(UINT32 id)
{
    UINT32 val = 0;
    volatile UINT32 *gpio_cfg_addr;

    if(GPIO_SUCCESS == gpio_ops_filter(id))
    {
        WARN_PRT("gpio_input_fail\r\n");
        goto input_exit;
    }

    gpio_cfg_addr = (volatile UINT32 *)(REG_GPIO_CFG_BASE_ADDR + id * 4);
    val = REG_READ(gpio_cfg_addr);

input_exit:
    return (val & GCFG_INPUT_BIT);
}

static void gpio_output(UINT32 id, UINT32 val)
{
    UINT32 reg_val;
    volatile UINT32 *gpio_cfg_addr;

    if(GPIO_SUCCESS == gpio_ops_filter(id))
    {
        WARN_PRT("gpio_output_fail\r\n");
        goto output_exit;
    }

    gpio_cfg_addr = (volatile UINT32 *)(REG_GPIO_CFG_BASE_ADDR + id * 4);
    reg_val = REG_READ(gpio_cfg_addr);

    reg_val &= ~GCFG_OUTPUT_BIT;
    reg_val |= (val & 0x01) << GCFG_OUTPUT_POS;
    REG_WRITE(gpio_cfg_addr, reg_val);

output_exit:
    return;
}

static void gpio_output_reverse(UINT32 id)
{
    UINT32 reg_val;
    volatile UINT32 *gpio_cfg_addr;

    if(GPIO_SUCCESS == gpio_ops_filter(id))
    {
        WARN_PRT("gpio_output_reverse_fail\r\n");
        goto reverse_exit;
    }

    gpio_cfg_addr = (volatile UINT32 *)(REG_GPIO_CFG_BASE_ADDR + id * 4);
    reg_val = REG_READ(gpio_cfg_addr);

    reg_val ^= GCFG_OUTPUT_BIT;
    REG_WRITE(gpio_cfg_addr, reg_val);

reverse_exit:
    return;
}


static void gpio_disable_jtag(void)
{
#ifndef JTAG_GPIO_FILTER
    int id;

    /*config GPIO_PCFG*/
    //#error "todo"

    for(id = GPIO20; id <= GPIO23; id ++)
    {
        //gpio_config(id, GMODE_OUTPUT);
    }
#endif
}

void gpio_test_isr(unsigned char ucChannel)
{
    gpio_output(4, 1);      // 161ms
}

void gpio_int_disable(UINT32 index)
{
    *(volatile UINT32 *)REG_GPIO_INTEN &= ~(0x01 << index);
}

void gpio_int_enable(UINT32 index, UINT32 mode, void (*p_Int_Handler)(unsigned char))
{
    UINT32 param;
    intc_service_register(IRQ_GPIO, PRI_IRQ_GPIO, gpio_isr);
    param = IRQ_GPIO_BIT;
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_ENABLE, &param);

    if(index >= GPIONUM)
    {
        WARN_PRT("gpio_id_cross_border\r\n");

        return;
    }

    mode &= 0x03;
    if ((mode == 0) || (mode == 3))
    {
        gpio_config(index, GMODE_INPUT_PULLUP);
    }
    else
    {
        gpio_config(index, GMODE_INPUT_PULLDOWN);
    }

    if (index < 16)
    {
        *(volatile UINT32 *)REG_GPIO_INTLV0 = (*(volatile UINT32 *)REG_GPIO_INTLV0 & (~(0x03 << (index << 1)))) | (mode << (index << 1));
    }
    else
    {
        *(volatile UINT32 *)REG_GPIO_INTLV1 = (*(volatile UINT32 *)REG_GPIO_INTLV1 & (~(0x03 << ((index - 16) << 1)))) | (mode << ((index - 16) << 1));
    }

    p_gpio_Intr_handler[index] = p_Int_Handler;
    *(volatile UINT32 *)REG_GPIO_INTEN |= (0x01 << index);
}

void gpio_int_clear(UINT32 index)
{
    *(volatile UINT32 *)REG_GPIO_INTSTA &= ~(0x01 << index);
}

/*******************************************************************/
#if 1
void gpio_init(void)
{
#if CFG_SYS_START_TIME
    UINT32 param;
#endif
    gpio_disable_jtag();

    sddev_register_dev(GPIO_DEV_NAME, &gpio_op);
#if CFG_SYS_START_TIME
    param = 3 | (GMODE_OUTPUT << 8);
    gpio_ctrl(CMD_GPIO_CFG, &param);
    gpio_output(3, 0);					//141ms  delta time = 13ms
    param = 4 | (GMODE_OUTPUT << 8);
    gpio_ctrl(CMD_GPIO_CFG, &param);
    gpio_output(4, 0);
    param = 5 | (GMODE_OUTPUT << 8);
    gpio_ctrl(CMD_GPIO_CFG, &param);
    gpio_output(5, 0);

    intc_service_register(IRQ_GPIO, PRI_IRQ_GPIO, gpio_isr);
    param = IRQ_GPIO_BIT;
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_ENABLE, &param);

    gpio_int_enable(2, 2, gpio_test_isr);
    gpio_output(3, 1);					// delta time = 3.1ms, delta time = 780us
#endif
}

void gpio_exit(void)
{
    sddev_unregister_dev(GPIO_DEV_NAME);
}

UINT32 gpio_ctrl(UINT32 cmd, void *param)
{
    UINT32 ret;
    ret = GPIO_SUCCESS;

    switch(cmd)
    {
    case CMD_GPIO_CFG:
    {
        UINT32 id;
        UINT32 mode;

        id = GPIO_CFG_PARAM_DEMUX_ID(*(UINT32 *)param);
        mode = GPIO_CFG_PARAM_DEMUX_MODE(*(UINT32 *)param);

        gpio_config(id, mode);

        break;
    }

    case CMD_GPIO_OUTPUT_REVERSE:
        ASSERT(param);

        gpio_output_reverse(*(UINT32 *)param);
        break;

    case CMD_GPIO_OUTPUT:
    {
        UINT32 id;
        UINT32 val;

        id = GPIO_OUTPUT_DEMUX_ID(*(UINT32 *)param);
        val = GPIO_OUTPUT_DEMUX_VAL(*(UINT32 *)param);

        gpio_output(id, val);
        break;
    }

    case CMD_GPIO_INPUT:
    {
        UINT32 id;
        UINT32 val;

        ASSERT(param);

        id = *(UINT32 *)param;
        val = gpio_input(id);

        ret = val;
        break;
    }

    case CMD_GPIO_ENABLE_SECOND:
    {
        UINT32 second_mode;

        ASSERT(param);

        second_mode = *(UINT32 *)param;
        gpio_enable_second_function(second_mode);
        break;
    }

    case CMD_GPIO_CLR_DPLL_UNLOOK_INT:
        REG_WRITE(REG_GPIO_DPLL_UNLOCK, 0x1);
        break;

    case CMD_GPIO_INT_ENABLE:
    {
        GPIO_INT_ST *ptr = param;
        gpio_int_enable(ptr->id, ptr->mode, ptr->phandler);
        break;
    }
    case CMD_GPIO_INT_DISABLE:
    {
        UINT32 id ;
        id = *(UINT32 *)param;
        gpio_int_disable(id);
        break;
    }
    case CMD_GPIO_INT_CLEAR:
    {
        UINT32 id ;
        id = *(UINT32 *)param;
        gpio_int_clear(id);
        break;
    }
    default:
        break;
    }

    return ret;
}
#endif

// EOF
