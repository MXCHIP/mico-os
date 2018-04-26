#include "include.h"
#include "arm_arch.h"

#include "saradc.h"
#include "saradc_pub.h"

#include "drv_model_pub.h"
#include "intc_pub.h"

#include "icu_pub.h"
#include "gpio_pub.h"

#include "uart_pub.h"

saradc_desc_t *saradc_desc = NULL;

static DD_OPERATIONS saradc_op = {
            saradc_open,
            saradc_close,
            NULL,
            NULL,
            saradc_ctrl
};

void saradc_init(void)
{
	intc_service_register(IRQ_SARADC, PRI_IRQ_SARADC, saradc_isr); 

	ddev_register_dev(SARADC_DEV_NAME, &saradc_op);
}

void saradc_exit(void)
{
	ddev_unregister_dev(SARADC_DEV_NAME);
}

static void saradc_enable_icu_config(void)
{
    UINT32 param;
    param = PWD_SARADC_CLK_BIT;
	sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_UP, &param);
}

static void saradc_disable_icu_config(void)
{
    UINT32 param;
    param = PWD_SARADC_CLK_BIT;
	sddev_control(ICU_DEV_NAME, CMD_CLK_PWR_DOWN, &param);
}

static void saradc_enable_interrupt(void)
{
    UINT32 param;    
    param = (IRQ_SARADC_BIT);
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_ENABLE, &param);
}

static void saradc_disable_interrupt(void)
{
    UINT32 param;    
    param = (IRQ_SARADC_BIT);
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_DISABLE, &param);
}

static void saradc_gpio_config(void)
{
	UINT32 param;
    
    switch (saradc_desc->channel)
    {
        case 0:
        {
            param = GFUNC_MODE_ADC1;
	        sddev_control(GPIO_DEV_NAME, CMD_GPIO_ENABLE_SECOND, &param);
            break;
        }
        case 1:
        {
            param = GFUNC_MODE_ADC2;
	        sddev_control(GPIO_DEV_NAME, CMD_GPIO_ENABLE_SECOND, &param);
            break;
        }
        case 2:
        {
            param = GFUNC_MODE_ADC3;
	        sddev_control(GPIO_DEV_NAME, CMD_GPIO_ENABLE_SECOND, &param);
            break;
        }
        default:    
            break;
    }

}

static UINT32 saradc_open(UINT32 op_flag)
{
	UINT32 value = 0;

    saradc_desc = (saradc_desc_t*)op_flag;

	if(saradc_desc->pData == NULL){
		return SARADC_FAILURE;
	}

	if(saradc_desc->channel > 7){
		return SARADC_FAILURE;
	}

	if(saradc_desc->data_buff_size == 0){
		return SARADC_FAILURE;
	}

	saradc_enable_icu_config();
    
	saradc_desc->has_data = 0;
    saradc_desc->current_read_data_cnt = 0;
    saradc_desc->current_sample_data_cnt = 0;

	saradc_gpio_config();

    value = ((saradc_desc->mode&0x03) << SARADC_ADC_MODE_POSI)
        | SARADC_ADC_CHNL_EN
        | ((saradc_desc->channel & SARADC_ADC_CHNL_MASK) << SARADC_ADC_CHNL_POSI)
        | ((saradc_desc->mode & 0x0C) << (SARADC_ADC_SAMPLE_RATE_POSI - 2))
        | ( 0 << SARADC_ADC_DELAY_CLK_POSI)
        | SARADC_ADC_INT_CLR
        | ( 0x10 << SARADC_ADC_PRE_DIV_POSI);
    REG_WRITE(SARADC_ADC_CONFIG, value);

    if(saradc_desc->p_Int_Handler != NULL)
    saradc_enable_interrupt();

	return SARADC_SUCCESS;
}

static UINT32 saradc_close(void)
{
	UINT32 value;

    if(saradc_desc->p_Int_Handler != NULL)
    saradc_disable_interrupt();

	value = REG_READ(SARADC_ADC_CONFIG);
	value &= ~SARADC_ADC_MODE_MASK;
	REG_WRITE(SARADC_ADC_CONFIG, value);

    saradc_disable_icu_config();

	return SARADC_SUCCESS;
}

static UINT32 saradc_set_mode(UINT8 mode)
{
	UINT32 value;
	
	if(mode > 3){
		return SARADC_FAILURE;
	}

	value = REG_READ(SARADC_ADC_CONFIG);
	value &= ~(SARADC_ADC_MODE_MASK << SARADC_ADC_MODE_POSI);
	value |= (mode << SARADC_ADC_MODE_POSI);
	REG_WRITE(SARADC_ADC_CONFIG, value);

	return SARADC_SUCCESS;
}

static UINT32 saradc_set_channel(saradc_chan_t *p_chan)
{
	UINT32 value;

	value = REG_READ(SARADC_ADC_CONFIG);
	if(p_chan->enable == 0){
		value &= ~SARADC_ADC_CHNL_EN;
	}else{
		value &= ~(SARADC_ADC_CHNL_MASK << SARADC_ADC_CHNL_POSI);
		value |= (p_chan->channel << SARADC_ADC_CHNL_POSI);
		value |= SARADC_ADC_CHNL_EN;
	}
	REG_WRITE(SARADC_ADC_CONFIG, value);

	return SARADC_SUCCESS;
}

static UINT32 saradc_set_sample_rate(UINT8 rate)
{
	UINT32 value;

	if(rate > 3){
		return SARADC_FAILURE;
	}
	
	value = REG_READ(SARADC_ADC_CONFIG);
	value &= ~(SARADC_ADC_SAMPLE_RATE_MASK << SARADC_ADC_SAMPLE_RATE_POSI);
	value |= (rate << SARADC_ADC_SAMPLE_RATE_POSI);
	REG_WRITE(SARADC_ADC_CONFIG, value);

	return SARADC_SUCCESS;
}

static UINT32 saradc_set_waiting_time(UINT8 time)
{
	UINT32 value, mode;

	value = REG_READ(SARADC_ADC_CONFIG);

	mode = value & (SARADC_ADC_MODE_MASK << SARADC_ADC_MODE_POSI);
	if(mode == 0 || mode == 3){
		return SARADC_FAILURE;
	}
	
	if(time == 0){
		value &= ~SARADC_ADC_SETTING;
	}else{
		value |= SARADC_ADC_SETTING;
	}
	
	REG_WRITE(SARADC_ADC_CONFIG, value);

	return SARADC_SUCCESS;
}

static UINT32 saradc_set_valid_mode(UINT8 mode)
{

	return SARADC_SUCCESS;
}

static void saradc_int_clr(void)
{
	UINT32 value;

	value = REG_READ(SARADC_ADC_CONFIG);
	value |= SARADC_ADC_INT_CLR;
	REG_WRITE(SARADC_ADC_CONFIG, value);
}

static UINT32 saradc_set_clk_rate(UINT8 rate)
{
	UINT32 value;

	if(rate > SARADC_ADC_PRE_DIV_MASK){
		return SARADC_FAILURE;
	}

	value = REG_READ(SARADC_ADC_CONFIG);
	value &= ~(SARADC_ADC_PRE_DIV_MASK << SARADC_ADC_PRE_DIV_POSI);
	value |= (rate << SARADC_ADC_PRE_DIV_POSI);
	REG_WRITE(SARADC_ADC_CONFIG, value);

	return SARADC_SUCCESS;
}

UINT32 saradc_get_value_without_isr(void)
{
	UINT32 value;

    if(!saradc_desc)
        return SARADC_FAILURE;

	value = REG_READ(SARADC_ADC_CONFIG);
    while((value & SARADC_ADC_FIFO_EMPTY) == 0) {
        UINT16 dac_val;
        if (saradc_desc->current_sample_data_cnt >= saradc_desc->data_buff_size){
            saradc_desc->current_sample_data_cnt = 0;
        }

        dac_val = REG_READ(SARADC_ADC_DATA)&0x03FF;
        saradc_desc->pData[saradc_desc->current_sample_data_cnt++] = dac_val;
        saradc_desc->has_data = 1;
                      
        value = REG_READ(SARADC_ADC_CONFIG);
    }
    saradc_int_clr();

    return SARADC_SUCCESS;
}


static UINT32 saradc_ctrl(UINT32 cmd, void *param)
{
	UINT32 ret = SARADC_SUCCESS;

	switch(cmd){
	case SARADC_CMD_SET_MODE:
		ret = saradc_set_mode(*(UINT8 *)param);
		break;
	case SARADC_CMD_SET_CHANNEL:
		ret = saradc_set_channel((saradc_chan_t *)param);
		break;
	case SARADC_CMD_SET_SAMPLE_RATE:
		ret = saradc_set_sample_rate(*(UINT8 *)param);
		break;
	case SARADC_CMD_SET_WAITING_TIME:
		ret = saradc_set_waiting_time(*(UINT8 *)param);
		break;
	case SARADC_CMD_SET_VALID_MODE:
		ret = saradc_set_valid_mode(*(UINT8 *)param);
		break;
	case SARADC_CMD_CLEAR_INT:
		saradc_int_clr();
		break;
	case SARADC_CMD_SET_CLK_RATE:
		ret = saradc_set_clk_rate(*(UINT8 *)param);
		break;
    case SARADC_CMD_GET_VAULE_WITHOUT_ISR:
        ret = saradc_get_value_without_isr();
        break;
	default:
		ret = SARADC_FAILURE;
		break;
	}

	return ret;
}

void saradc_isr(void)
{
	UINT32 value;

	value = REG_READ(SARADC_ADC_CONFIG);
    while((value & SARADC_ADC_FIFO_EMPTY) == 0) {
        UINT16 dac_val;
        if (saradc_desc->current_sample_data_cnt >= saradc_desc->data_buff_size){
            saradc_desc->current_sample_data_cnt = 0;
        }
        //saradc_gpio_config();
        dac_val = REG_READ(SARADC_ADC_DATA)&0x03FF;
        saradc_desc->pData[saradc_desc->current_sample_data_cnt++] = dac_val;
        saradc_desc->has_data = 1;
                
        if(saradc_desc->p_Int_Handler != NULL){
            (void)saradc_desc->p_Int_Handler();
        }
        value = REG_READ(SARADC_ADC_CONFIG);
    }
    saradc_int_clr();
}

