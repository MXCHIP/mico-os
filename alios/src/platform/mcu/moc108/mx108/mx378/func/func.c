#include "include.h"
#include "func_pub.h"
#include "intc.h"
#include "rwnx.h"
#include "uart_pub.h"
#include "lwip_intf.h"
#include "param_config.h"

#if CFG_SUPPORT_CALIBRATION
#include "bk7011_cal_pub.h"
#endif

#if CFG_UART_DEBUG 
#include "uart_debug_pub.h"
#endif

#if CFG_SDIO
#include "sdio_intf_pub.h"
#endif

#if CFG_USB
#include "fusb_pub.h"
#endif

UINT32 func_init(void)
{
#ifndef AOS_NO_WIFI
	cfg_param_init();
	
    FUNC_PRT("[FUNC]rwnxl_init\r\n");
    rwnxl_init();

#if CFG_SUPPORT_CALIBRATION

    #ifndef KEIL_SIMULATOR
    calibration_main();
    #if CFG_SUPPORT_MANUAL_CALI
    manual_cal_load_default_txpwr_tab(manual_cal_load_txpwr_tab_flash());
    #endif
    #endif
#endif
#endif

#if CFG_UART_DEBUG 
	#ifndef KEIL_SIMULATOR
    FUNC_PRT("[FUNC]uart_debug_init\r\n");   
    uart_debug_init();
	#endif
#endif

    FUNC_PRT("[FUNC]intc_init\r\n");
    intc_init();

#if CFG_SDIO
    FUNC_PRT("[FUNC]sdio_intf_init\r\n");
    sdio_intf_init();
#endif

#if CFG_SDIO_TRANS
    FUNC_PRT("[FUNC]sdio_intf_trans_init\r\n");
    sdio_trans_init();
#endif


#if CFG_USB
    FUNC_PRT("[FUNC]fusb_init\r\n");
    fusb_init();
#endif

    FUNC_PRT("[FUNC]func_init OVER!!!\r\n\r\n");
    return 0;
}

// eof
