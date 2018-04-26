#include "include.h"
#include "arm_arch.h"


#include "target_util_pub.h"
#include "mem_pub.h"
#include "drv_model_pub.h"
#include "sys_ctrl_pub.h"
#include "saradc_pub.h"
#include "uart_pub.h"

#include "rtos_pub.h"
#include "fake_clock_pub.h"

#include "temp_detect_pub.h"
#include "temp_detect.h"

#if CFG_USE_TEMPERATURE_DETECT
DD_HANDLE tmp_detect_hdl = DD_HANDLE_UNVALID;
saradc_desc_t tmp_detect_desc;
UINT16 tmp_detect_buff[ADC_TEMP_BUFFER_SIZE];
TEMP_DETECT_CONFIG_ST g_temp_detect_config;
mico_semaphore_t g_temp_detct_rx_sema;

#if CFG_SUPPORT_CALIBRATION
extern int calibration_main(void);
extern INT32 rwnx_cal_load_trx_rcbekn_reg_val(void);
extern INT32 rwnx_cal_save_trx_rcbekn_reg_val(void);
extern void do_calibration_in_temp_dect(void);
#endif

static void temp_detect_handler(void);
static void temp_detect_main(UINT32 data);

static void temp_detect_desc_init(void)
{
    os_memset(&tmp_detect_buff[0], 0, sizeof(UINT16)*ADC_TEMP_BUFFER_SIZE);

    tmp_detect_desc.channel = ADC_TEMP_SENSER_CHANNEL;
    tmp_detect_desc.pData = &tmp_detect_buff[0];
    tmp_detect_desc.data_buff_size = ADC_TEMP_BUFFER_SIZE;
    tmp_detect_desc.mode = (ADC_CONFIG_MODE_CONTINUE << 0)
                           | (ADC_CONFIG_MODE_36DIV << 2);

    tmp_detect_desc.has_data                = 0;
    tmp_detect_desc.current_read_data_cnt   = 0;
    tmp_detect_desc.current_sample_data_cnt = 0;
    tmp_detect_desc.p_Int_Handler = temp_detect_handler;
}

static void temp_detect_enable_config_sysctrl(void)
{
    UINT32 param;

    param = BLK_BIT_TEMPRATURE_SENSOR;
    sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_BLK_ENABLE, &param);
}

static void temp_detect_disable_config_sysctrl(void)
{
    UINT32 param;
    param = BLK_BIT_TEMPRATURE_SENSOR;
    sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_BLK_DISABLE, &param);
}

UINT32 temp_detect_init(void)
{   
    int ret;

    mico_rtos_init_semaphore(&g_temp_detct_rx_sema, 10);

    ret = mico_rtos_create_thread(NULL,
                                  MICO_DEFAULT_WORKER_PRIORITY,
                                  "cli",
                                  temp_detect_main,
                                  4096,
                                  0);
    if (ret != kNoErr)
    {
        os_printf("Error: Failed to create temp_detect: %d\r\n",
                  ret);
        goto init_general_err;
    }

    return kNoErr;

init_general_err:
    return kGeneralErr;

}

UINT32 temp_detect_uninit(void)
{
    return 0;
}

static UINT32 temp_detect_open(void)
{
    UINT32 status;

    if(tmp_single_hdl !=  DD_HANDLE_UNVALID ) {
        TMP_DETECT_FATAL("Can't open saradc, temp sibgle get has ready open it\r\n");
        return SARADC_FAILURE;        
    }

    tmp_detect_hdl = ddev_open(SARADC_DEV_NAME, &status, (UINT32)&tmp_detect_desc);
    status = status;
    if(DD_HANDLE_UNVALID == tmp_detect_hdl)
    {
        return SARADC_FAILURE;
    }

    return SARADC_SUCCESS;
}

static UINT32 temp_detect_close(void)
{
    UINT32 status;

    status = ddev_close(tmp_detect_hdl);
    if(DRV_FAILURE == status )
    {
        return SARADC_FAILURE;
    }
    tmp_detect_hdl = DD_HANDLE_UNVALID;

    return SARADC_SUCCESS;
}

static UINT32 temp_detect_enable(void)
{
    UINT32 err = SARADC_SUCCESS;

    temp_detect_desc_init();
    temp_detect_enable_config_sysctrl();

    err = temp_detect_open();
    if(err == SARADC_FAILURE)
    {
        TMP_DETECT_FATAL("Can't open saradc, have you register this device?\r\n");
        return err;
    }
    TMP_DETECT_PRT("saradc_open is ok \r\n");

    return SARADC_SUCCESS;
}

static void temp_detect_disable(void)
{
    temp_detect_close();
    temp_detect_disable_config_sysctrl();
    TMP_DETECT_PRT("saradc_open is close \r\n");
}

static UINT8 temp_detect_get_wifi_traffic_idle(void)
{
    return 1;
}

void temp_detect_do_calibration(void)
{
#if CFG_SUPPORT_CALIBRATION
    TMP_DETECT_WARN("do calibration...\r\n");
    g_temp_detect_config.do_cali_flag = 1;
    rwnx_cal_save_trx_rcbekn_reg_val();
    calibration_main();
    rwnx_cal_load_trx_rcbekn_reg_val();
    g_temp_detect_config.do_cali_flag = 0;
    TMP_DETECT_WARN("calibration done!!!\r\n");
#endif
}

void temp_detect_do_calibration_temp(void)
{
#if CFG_SUPPORT_CALIBRATION
    TMP_DETECT_WARN("do calibration...\r\n");
    g_temp_detect_config.do_cali_flag = 1;
    rwnx_cal_save_trx_rcbekn_reg_val();
    do_calibration_in_temp_dect();
    rwnx_cal_load_trx_rcbekn_reg_val();
    g_temp_detect_config.do_cali_flag = 0;
    TMP_DETECT_WARN("calibration done!!!\r\n");
#endif
}

static void temp_detect_timer_handler(void *data)
{
    if(temp_detect_enable() == SARADC_SUCCESS){
        OSStatus err;
        // stop detect time
        err = rtos_stop_timer(&g_temp_detect_config.detect_timer);
        ASSERT(kNoErr == err);
        TMP_DETECT_PRT("stop detect timer, start ADC\r\n");  
    }     
}

static void temp_calibration_timer_handler(void *data)
{

    if(temp_detect_get_wifi_traffic_idle())
    {
        OSStatus err;
        // stop calibration time
        err = rtos_stop_timer(&g_temp_detect_config.calibration_timer);
        ASSERT(kNoErr == err);
        TMP_DETECT_PRT("stop calibration timer, do temp calib\r\n"); 
        
        ////temp_detect_do_calibration();
        TMP_DETECT_PRT("do temp calib\r\n"); 
        //temp_detect_do_calibration_temp();

        // restart dectect timer
        TMP_DETECT_PRT("after temp calib, restart detect timer\r\n");
        err = rtos_reload_timer(&g_temp_detect_config.detect_timer);
        ASSERT(kNoErr == err);
    }
    else
    {
        // continue calibration timer
        TMP_DETECT_PRT("continue calibration\r\n");
    }
}

static void temp_detect_polling_handler(void)
{
    OSStatus err;
    UINT32 dist, cur_val, last_val;

    cur_val = tmp_detect_desc.pData[ADC_TEMP_BUFFER_SIZE-1];
    last_val = g_temp_detect_config.last_detect_val;
    dist = (cur_val > last_val) ? (cur_val - last_val) : (last_val - cur_val);

    TMP_DETECT_WARN("%d seconds: last:%d, cur:%d, thr:%d\r\n",
                    g_temp_detect_config.detect_intval,
                    last_val,
                    cur_val,
                    g_temp_detect_config.detect_thre);

    if(g_temp_detect_config.last_detect_val == 0){
        // change to normal detect intval
        temp_detect_change_configuration(ADC_TMEP_DETECT_INTVAL, 0);
    }

    if(dist > g_temp_detect_config.detect_thre)
    {
        g_temp_detect_config.last_detect_val = cur_val;

        // start or restart the calib timer
        TMP_DETECT_PRT("start calibration timer\r\n");  
        
        if(g_temp_detect_config.calibration_timer.function) { 
            err = rtos_reload_timer(&g_temp_detect_config.calibration_timer);
    	    ASSERT(kNoErr == err); 
        } else {
        	err = rtos_init_timer(&g_temp_detect_config.calibration_timer, 
						fclk_from_sec_to_tick(ADC_TMEP_CALIB_INTVAL),  
						temp_calibration_timer_handler, 
						(void *)0);
            ASSERT(kNoErr == err);
        	err = rtos_start_timer(&g_temp_detect_config.calibration_timer);
        	ASSERT(kNoErr == err);   
        }
    } else {
        // no need to do tmp calib, restart detect timer
        // restart dectect timer
        TMP_DETECT_PRT("no need restart cali timer, restart detect timer\r\n");
        err = rtos_reload_timer(&g_temp_detect_config.detect_timer);
        ASSERT(kNoErr == err);
    }
}

static void temp_detect_main(UINT32 data)
{
    {
        OSStatus err;
        g_temp_detect_config.last_detect_val = 0;
        g_temp_detect_config.detect_thre = ADC_TMEP_DO_CALIBRATION_TXOUTPOWR_THRE;
        g_temp_detect_config.detect_intval = ADC_TMEP_FIRST_DETECT_INTVAL;
        g_temp_detect_config.do_cali_flag = 0;

    	err = rtos_init_timer(&g_temp_detect_config.detect_timer, 
    							fclk_from_sec_to_tick(g_temp_detect_config.detect_intval), 
    							temp_detect_timer_handler, 
    							(void *)0);
        ASSERT(kNoErr == err);
    	err = rtos_start_timer(&g_temp_detect_config.detect_timer);
    	ASSERT(kNoErr == err);
    }

    while(1)
    {
        mico_rtos_get_semaphore(&g_temp_detct_rx_sema, MICO_NEVER_TIMEOUT);
        temp_detect_polling_handler();
    }

    temp_detect_disable();
    mico_rtos_delete_thread(NULL);
}

static void temp_detect_handler(void)
{
    if(tmp_detect_desc.current_sample_data_cnt >= tmp_detect_desc.data_buff_size) 
    {
        TMP_DETECT_PRT("buff:%p,%d,%d,%d,%d,%d\r\n", tmp_detect_desc.pData,
                       tmp_detect_desc.pData[0], tmp_detect_desc.pData[1],
                       tmp_detect_desc.pData[2], tmp_detect_desc.pData[3], 
                       tmp_detect_desc.pData[4]);
        temp_detect_disable();
        mico_rtos_set_semaphore(&g_temp_detct_rx_sema);
    }
}


void temp_detect_change_configuration(UINT32 intval, UINT32 thre)
{
    UINT32 interval, is_running = 0;
    OSStatus err;

    if(intval == 0)
        intval = ADC_TMEP_DETECT_INTVAL;
    if(thre == 0)
        thre = ADC_TMEP_DO_CALIBRATION_TXOUTPOWR_THRE;

    TMP_DETECT_PRT("config: intval:0x%x, thre:0x%x\r\n", intval, thre);

    g_temp_detect_config.detect_thre = thre;
    g_temp_detect_config.detect_intval = intval;

    if(rtos_is_timer_running(&g_temp_detect_config.detect_timer))
        is_running = 1;
    
    if(g_temp_detect_config.detect_timer.function) {
        err = rtos_deinit_timer(&g_temp_detect_config.detect_timer); 
        ASSERT(kNoErr == err); 
    } 
    

	err = rtos_init_timer(&g_temp_detect_config.detect_timer, 
							fclk_from_sec_to_tick(g_temp_detect_config.detect_intval), 
							temp_detect_timer_handler, 
							(void *)0);
    ASSERT(kNoErr == err);

    if(is_running) {
	    err = rtos_start_timer(&g_temp_detect_config.detect_timer);
	    ASSERT(kNoErr == err); 
    }

}

UINT8 temp_detct_get_cali_flag(void)
{
    return (UINT8)(g_temp_detect_config.do_cali_flag);
}
////////////////////////////////////////////////////////////////////////
#endif  // CFG_USE_TEMPERATURE_DETECT

saradc_desc_t tmp_single_desc;
UINT16 tmp_single_buff[ADC_TEMP_BUFFER_SIZE];
DD_HANDLE tmp_single_hdl = DD_HANDLE_UNVALID;
static void temp_single_get_desc_init(void)
{
    os_memset(&tmp_single_buff[0], 0, sizeof(UINT16)*ADC_TEMP_BUFFER_SIZE);

    tmp_single_desc.channel = ADC_TEMP_SENSER_CHANNEL;
    tmp_single_desc.pData = &tmp_single_buff[0];
    tmp_single_desc.data_buff_size = ADC_TEMP_BUFFER_SIZE;
    tmp_single_desc.mode = (ADC_CONFIG_MODE_CONTINUE << 0)
                           | (ADC_CONFIG_MODE_36DIV << 2);
                           
    tmp_single_desc.has_data                = 0;
    tmp_single_desc.current_read_data_cnt   = 0;
    tmp_single_desc.current_sample_data_cnt = 0;
    tmp_single_desc.p_Int_Handler = NULL;
}

static UINT32 temp_single_get_enable(void)
{
    UINT32 status;;
    
#if CFG_USE_TEMPERATURE_DETECT
    if(tmp_detect_hdl !=  DD_HANDLE_UNVALID ) {
        TMP_DETECT_FATAL("Can't open saradc, temp detect has ready open it\r\n");
        return SARADC_FAILURE;        
    }
#endif

    temp_single_get_desc_init();

    status = BLK_BIT_TEMPRATURE_SENSOR;
    sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_BLK_ENABLE, &status);

    tmp_single_hdl = ddev_open(SARADC_DEV_NAME, &status, (UINT32)&tmp_single_desc);
    if(DD_HANDLE_UNVALID == tmp_single_hdl)
    {
        TMP_DETECT_FATAL("Can't open saradc, have you register this device?\r\n");
        return SARADC_FAILURE;
    }

    return SARADC_SUCCESS;
}

static void temp_single_get_disable(void)
{
    UINT32 status;

    status = ddev_close(tmp_single_hdl);
    if(DRV_FAILURE == status )
    {
        return;
    }
    tmp_single_hdl = DD_HANDLE_UNVALID;
    
    status = BLK_BIT_TEMPRATURE_SENSOR;
    sddev_control(SCTRL_DEV_NAME, CMD_SCTRL_BLK_DISABLE, &status);
    
    TMP_DETECT_PRT("saradc_open is close \r\n");
}

UINT32 temp_single_get_current_temperature(UINT32 *temp_value)
{
    UINT32 ret;
    *temp_value = 0;
    temp_single_get_enable();
    
    ret = ddev_control(tmp_single_hdl, SARADC_CMD_GET_VAULE_WITHOUT_ISR, NULL);
    if(ret == SARADC_FAILURE){
        return ret;
    }

    TMP_DETECT_PRT("buff:%p,%d,%d,%d,%d,%d\r\n", tmp_single_desc.pData,
                   tmp_single_desc.pData[0], tmp_single_desc.pData[1],
                   tmp_single_desc.pData[2], tmp_single_desc.pData[3], 
                   tmp_single_desc.pData[4]);
        
    *temp_value = tmp_single_desc.pData[0];

    temp_single_get_disable();
        
    return ret;
}





//EOF

