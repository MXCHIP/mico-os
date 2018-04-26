#include "include.h"
#include "arm_arch.h"

#include "wdt_pub.h"
#include "wdt.h"

#include "drv_model_pub.h"

static SDD_OPERATIONS wdt_op = {
            wdt_ctrl
};

/*******************************************************************/
#if 1
void wdt_init(void)
{
	sddev_register_dev(WDT_DEV_NAME, &wdt_op);
}

void wdt_exit(void)
{
	sddev_unregister_dev(WDT_DEV_NAME);
}

UINT32 wdt_ctrl(UINT32 cmd, void *param)
{
	UINT32 ret;
	UINT32 reg;

	ret = WDT_SUCCESS;
	
	switch(cmd)
	{		
		case WCMD_CLEAR_COUNTER:
			reg = REG_READ(WDT_CTRL_REG);
			reg &= ~(WDT_KEY_MASK << WDT_KEY_POSI);
			reg |= WDT_1ST_KEY << WDT_KEY_POSI;
			REG_WRITE(WDT_CTRL_REG, reg);
			
			reg = REG_READ(WDT_CTRL_REG);
			reg &= ~(WDT_KEY_MASK << WDT_KEY_POSI);
			reg |= WDT_2ND_KEY << WDT_KEY_POSI;
			REG_WRITE(WDT_CTRL_REG, reg);
			break;

		case WCMD_SET_PERIOD:		
			ASSERT(param);
			reg = WDT_1ST_KEY << WDT_KEY_POSI;
			reg |= (*(UINT32 *)param & WDT_PERIOD_MASK) << WDT_PERIOD_POSI;
			REG_WRITE(WDT_CTRL_REG, reg);

			reg = WDT_2ND_KEY << WDT_KEY_POSI;
			reg |= (*(UINT32 *)param & WDT_PERIOD_MASK) << WDT_PERIOD_POSI;
			REG_WRITE(WDT_CTRL_REG, reg);	
			break;
			
		default:
			break;
	}
	
    return ret;
}
#endif

// EOF
