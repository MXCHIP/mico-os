#include "include.h"
#include "cmd_rx_sensitivity.h"
#include "mem_pub.h"
#include "str_pub.h"
#include "rx_sensitivity_pub.h"
#include "fake_clock_pub.h"
#include "uart_debug_pub.h"
#include "schedule_pub.h"
#include "rtos_pub.h"

#if CFG_RX_SENSITIVITY_TEST
mico_timer_t rx_sens_tmr = {0};
UINT32 g_rxsens_start = 0;
#endif

void rxsens_ct_hdl(void *param)
{
#if CFG_RX_SENSITIVITY_TEST
    rx_get_rx_result_end();
    rx_get_rx_result_begin();
#endif // CFG_RX_SENSITIVITY_TEST
}

void rxsens_ct_show_hdl(void *param)
{
#if CFG_RX_SENSITIVITY_TEST
    rx_get_rx_result_end();
#endif // CFG_RX_SENSITIVITY_TEST
}

int do_rx_sensitivity(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
    OSStatus err;
    char cmd0 = 0;
    char cmd1 = 0;
    UINT8 fail = 0;

    UINT32 mode = RXSENS_DEFUALT_MODE;
    UINT32 duration = RXSENS_DEFUALT_DURATION;
    UINT32 channel = RXSENS_DEFUALT_CHANNEL;

#if CFG_RX_SENSITIVITY_TEST
    UINT8 ret;
#endif

    UINT32 arg_id = 1;
    UINT32 arg_cnt = argc;

#if CFG_RX_SENSITIVITY_TEST
    uint32_t t_ms = 0;
#endif

    /*step0, parameter conversion*/
    while(arg_cnt > 1)
    {
        if(arg_cnt > 1)
        {
            cmd0 = argv[arg_id][0];
            cmd1 = argv[arg_id][1];
        }

        switch(cmd0)
        {
        case '-':
        {
            arg_cnt -= 1;

            if(arg_cnt < 1)
            {
                fail = 1;
                break;
            }

            arg_cnt -= 1;
            switch(cmd1)
            {
            case 'b':  // bandwidth 0:20M, 1:40M
                mode = os_strtoul(argv[arg_id + 1], NULL, 10);
                break;

            case 'd':  // timer intval
                duration = os_strtoul(argv[arg_id + 1], NULL, 10);
                break;

            case 'c':  // channel
                channel = os_strtoul(argv[arg_id + 1], NULL, 10);
                break;

            case 'e': { //  0: exit, 1: enter, 2: stop last rx
#if CFG_RX_SENSITIVITY_TEST
                UINT32 op = os_strtoul(argv[arg_id + 1], NULL, 10);
                if(op < RXSENS_G_MAX) {
                    if (op == RXSENS_G_STOP_LASTRX) {
                        g_rxsens_start = 0;
                        if(rx_sens_tmr.function) {
                            err = mico_rtos_deinit_timer(&rx_sens_tmr); 
                            ASSERT(kNoErr == err); 
                        }
    	            } else {
                        FUNCPTR reboot = 0;
                        os_printf("reboot\r\n");
                        (*reboot)();
                    }
                return 0;
                } else {
                    return -1;
                }
#endif
            }

            case 's': {  // start / stop
#if CFG_RX_SENSITIVITY_TEST            
                UINT32 sta = os_strtoul(argv[arg_id + 1], NULL, 10);
                if(sta) { 
                    rx_clean_rx_statistic_result();
                }
                else {
                    rx_get_rx_result_end();
                }
#endif
                return 0;
            }

            case 'g': {  // get statistic  0: clean, 1:20M, 2:40M
 #if CFG_RX_SENSITIVITY_TEST            
                UINT32 g_type = os_strtoul(argv[arg_id + 1], NULL, 10);
                if(g_type < RXSENS_RTYPTE_MAX ) 
                {
                    if(g_type == RXSENS_RTYPTE_CLEAN) { 
                        rx_clean_rx_statistic_result();
                    }
                    else if(g_type == RXSENS_RTYPTE_20M) { 
                        rx_get_rx20M_statistic_result();
                    }
                    
                    else if(g_type == RXSENS_RTYPTE_40M){
                        rx_get_rx40M_statistic_result();
                    }
                    else if(g_type == RXSENS_RTYPTE_SIG_RES){
                        rxsens_ct_hdl(NULL);
                    }
                    return 0;
                } else 
                    return -1;
#endif
            }            

            default:
                fail = 1;
                break;
            }
        }
        break;

        default:
            fail = 1;
            break;
        }

        if(fail)
        {
            return 1;
        }

        arg_id += 2;
    }

    /*step1, parameter check*/
    if(!((0 < channel)
            || (14 > channel)))
    {
        return 1;
    }

    if((mode != 1) && (mode != 0) )
    {
        return 1;
    }

    /*step2, handle*/
#if CFG_RX_SENSITIVITY_TEST
    ret = rs_set_mode(mode);
    if(ret)
    {
        return 1;
    }

    ret = rs_set_channel(channel);
    if(ret)
    {
        return 1;
    }

    rs_test();
    g_rxsens_start = 1;

    if(duration) {
    rx_get_rx_result_begin();
    t_ms = fclk_from_sec_to_tick(duration);

        if(rx_sens_tmr.function) {
            err = mico_rtos_deinit_timer(&rx_sens_tmr); 
            ASSERT(kNoErr == err); 
        } 
        
    	err = mico_rtos_init_timer(&rx_sens_tmr, 
							t_ms, 
							rxsens_ct_hdl, 
							(void *)0);
    ASSERT(kNoErr == err);
    	err = mico_rtos_start_timer(&rx_sens_tmr);
	ASSERT(kNoErr == err);
    } else {
        if(rx_sens_tmr.function) {
            err = mico_rtos_deinit_timer(&rx_sens_tmr); 
            ASSERT(kNoErr == err); 
        } 
    }
    
    
#endif // CFG_RX_SENSITIVITY_TEST

    return 0;
}

// eof

