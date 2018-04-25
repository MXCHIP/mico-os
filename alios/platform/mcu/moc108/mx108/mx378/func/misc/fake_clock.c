#include "include.h"
#include "fake_clock_pub.h"
#include "pwm_pub.h"
#include "icu_pub.h"
#include "drv_model_pub.h"
#include "uart_pub.h"

#include "mico_rtos.h"
#include "ll.h"
#include "k_api.h"

#if CFG_BK7221_MDM_WATCHDOG_PATCH
void rc_reset_patch(void);
#endif

static volatile UINT32 current_clock = 0;
static volatile UINT32 current_seconds = 0;
static UINT32 second_countdown = FCLK_SECOND;

void fclk_hdl(UINT8 param)
{
	//GLOBAL_INT_DECLARATION();

    current_clock ++;

    #if CFG_BK7221_MDM_WATCHDOG_PATCH
    rc_reset_patch();
    #endif

    if (--second_countdown == 0)
    {
	current_seconds ++;
	second_countdown = FCLK_SECOND;
    }

    krhino_tick_proc();
    
    #if 0
	/* Increment the tick counter. */
	GLOBAL_INT_DISABLE();
	if( xTaskIncrementTick() != pdFALSE )
	{
		/* Select a new task to run. */
		vTaskSwitchContext();
	}
	GLOBAL_INT_RESTORE();
    #endif
}

UINT32 fclk_get_tick(void)
{
    return current_clock;
}

UINT32 fclk_get_second(void)
{
	return current_seconds;
}

UINT32 fclk_from_sec_to_tick(UINT32 sec)
{
	return sec * FCLK_SECOND;
}
	
void fclk_reset_count(void)
{
	current_clock = 0;
	current_seconds = 0;
}

UINT32 fclk_cal_endvalue(UINT32 mode)
{
	UINT32 value = 1;
	
	if(PWM_CLK_32K == mode)
	{	/*32k clock*/
		value = FCLK_DURATION_MS * 32;
	}
	else if(PWM_CLK_26M == mode)
	{	/*26m clock*/
		value = 26000000 / RHINO_CONFIG_TICKS_PER_SECOND;
	}

	return value;
}

void fclk_init(void)
{
	UINT32 ret;
	pwm_param_t param;

	/*init pwm*/
	param.channel         = FCLK_PWM_ID;	
	param.cfg.bits.en     = PWM_ENABLE;
	param.cfg.bits.int_en = PWM_INT_EN;
	param.cfg.bits.mode   = PMODE_TIMER;
    
#if(CFG_RUNNING_PLATFORM == FPGA_PLATFORM)  // FPGA:PWM0-2-32kCLK, pwm3-5-24CLK
	param.cfg.bits.clk    = PWM_CLK_32K; 
#else
	param.cfg.bits.clk    = PWM_CLK_26M; 
#endif

	param.p_Int_Handler   = fclk_hdl;
	param.duty_cycle      = 0;
	param.end_value       = fclk_cal_endvalue((UINT32)param.cfg.bits.clk);
	
	ret = sddev_control(PWM_DEV_NAME, CMD_PWM_INIT_PARAM, &param);
	ASSERT(PWM_SUCCESS == ret);
}

// eof

