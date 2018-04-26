#include <stdio.h>
#include "include.h"
#include "arm_arch.h"

#include "uart_pub.h"
#include "uart.h"

#include "drv_model_pub.h"
#include "sys_ctrl_pub.h"
#include "mem_pub.h"
#include "icu_pub.h"
#include "gpio_pub.h"
#include "gpio_uart.h"


#include "ll.h"
#include "mem_pub.h"
#include "intc_pub.h"

#include "icu_pub.h"

void gu_delay(uint32_t usec) {
    volatile uint32_t loops;

    while (usec--) {
        loops=40;
        while (loops--);
    }
}


#ifdef CONFIG_GPIO_SIMU_UART_TX
void gpio_uart_send_init(void)
{
    UINT32 param;
    param = SIMU_UART_GPIONUM| (GMODE_OUTPUT<<8);
    gpio_ctrl(CMD_GPIO_CFG, &param);

}


void gpio_uart_send_byte(unsigned char *buff, unsigned int len)
{
	volatile unsigned char c,n,loops;
	volatile   UINT32 param;
	
    GLOBAL_INT_DECLARATION();
    GLOBAL_INT_DISABLE();    
    
	param = SIMU_UART_GPIONUM | (1<<8);
	gpio_ctrl(CMD_GPIO_OUTPUT, &param);
	gu_delay(60);	
	
	while(len--)
	{
		param = SIMU_UART_GPIONUM | (0<<8);
    	gpio_ctrl(CMD_GPIO_OUTPUT, &param);
		gu_delay(33);	
		loops=10;
        while (loops--);
		c = *buff++;
		n = 8;
		while(n--)
		{
			param = SIMU_UART_GPIONUM | ((c&0x01)<<8);
			gpio_ctrl(CMD_GPIO_OUTPUT, &param);
			gu_delay(33);	
			loops=6;
        	while (loops--);
			c >>= 1;
		}
		param = SIMU_UART_GPIONUM | (1<<8);
    	gpio_ctrl(CMD_GPIO_OUTPUT, &param);
	    gu_delay(33);	
		loops=6;
        while (loops--);
	}
    GLOBAL_INT_RESTORE();
}

int guart_fputc(int ch, FILE *f)
{
    gpio_uart_send_byte((unsigned char *)&ch,1);

    return ch;
}

#endif


#ifdef CONFIG_GPIO_SIMU_UART_TX

void GPIO_Simu_Isr(unsigned char ucChannel)
{
    UINT32 gpiosts ;
    UINT8 c = 0,n,loops;

    if(ucChannel == SIMU_UART_GPIO_RX)
    {
        gu_delay(21);
        loops=5;
        while (loops--);           
        for(n=0; n<8; n++)
        {
            c >>= 1;
            if(gpio_input(SIMU_UART_GPIO_RX))
                c |= 0x80;
            
            gu_delay(33);
            loops=20;
            while (loops--); 
            loops++;
        }
    }
}


void gpio_uart_recv_init(void)
{
    UINT32 param;
    param = SIMU_UART_GPIO_RX| (GMODE_INPUT_PULLUP<<8);
    gpio_ctrl(CMD_GPIO_CFG, &param);
    
    intc_service_register(IRQ_GPIO, PRI_IRQ_GPIO, gpio_isr);    
    param = IRQ_GPIO_BIT;
    sddev_control(ICU_DEV_NAME, CMD_ICU_INT_ENABLE, &param);
    gpio_int_enable(SIMU_UART_GPIO_RX,0,GPIO_Simu_Isr);
}

#endif
// eof

