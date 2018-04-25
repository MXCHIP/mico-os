#include "include.h"

// led show spi data state for tianzhiheng
#if CFG_SUPPORT_TIANZHIHENG_DRONE
#include "mem_pub.h"
#include "gpio_pub.h"
#include "app_led.h"


#include "uart_pub.h"
#define LED_DEBUG
#ifdef LED_DEBUG
#define LED_PRT      os_printf
#define LED_WARN     warning_prf
#define LED_FATAL    fatal_prf
#else
#define LED_PRT      null_prf
#define LED_WARN     null_prf
#define LED_FATAL    null_prf
#endif

#define LED_GPIO_INDEX              GPIO31     // GPIO31
#define LED_INITIAL_VAL             1          // LOW   LED ON
#define LED_ON_VAL                  0
#define LED_NO_SPI_DAT_VAL          (CLOCK_SECOND/8)
#define LED_DET_SPI_DAT_VAL         (CLOCK_SECOND)


typedef struct led_st
{
    struct etimer led_timer;
    DEV_STATE state;
    GPIO_INDEX gpio_idx;
} LED_ST, LED_PTR;

static LED_ST ledctr;


/*---------------------------------------------------------------------------*/
PROCESS(app_led_process, "app_led_process");
/*---------------------------------------------------------------------------*/

void app_led_intial(void)
{
    os_memset(&ledctr, 0, sizeof(LED_ST));
    ledctr.state = STA_NONE;

    ledctr.gpio_idx = LED_GPIO_INDEX;
    GPIO_CONFIG_OUTPUT(ledctr.gpio_idx);
    GPIO_OUTPUT(ledctr.gpio_idx, LED_INITIAL_VAL);

    process_start(&app_led_process, NULL);
}

static void app_led_timer_handler(void)
{
    if(etimer_expired(&ledctr.led_timer))
    {
        GPIO_OUTPUT_REVERSE(ledctr.gpio_idx);
        etimer_reset(&ledctr.led_timer);
    }
}

static void app_led_poll_handler(DEV_STATE next_sta)
{
    clock_time_t intval = 0;

    if(ledctr.state == next_sta)
        return;

    if(next_sta == POWER_ON)
    {
        intval = 0;
        ledctr.state = POWER_ON;
        GPIO_OUTPUT(ledctr.gpio_idx, LED_ON_VAL);
    }
    else if(next_sta == NO_SPI_DATA)
    {
        intval = LED_NO_SPI_DAT_VAL;
        ledctr.state = NO_SPI_DATA;
    }
    else if(next_sta == DET_SPI_DATA)
    {
        intval = LED_DET_SPI_DAT_VAL;
        ledctr.state = DET_SPI_DATA;
    }

    if(intval)
    {
        PROCESS_CONTEXT_BEGIN(&app_led_process);
        etimer_set(&ledctr.led_timer, intval);
        PROCESS_CONTEXT_END(&app_led_process);
    }
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(app_led_process, ev, data)
{
    PROCESS_BEGIN();

    while(1)
    {
        PROCESS_YIELD();
        if(ev == PROCESS_EVENT_EXIT)
        {
            LED_PRT("app_led_process exit process\r\n");
        }
        else if(ev == PROCESS_EVENT_EXITED)
        {
            struct process *exit_p = (struct process *)data;
            LED_PRT("%s exit in app_led_process\r\n",
                    PROCESS_NAME_STRING(exit_p));
        }
        else if(ev == PROCESS_EVENT_TIMER)
        {
            app_led_timer_handler();
        }
        else if(ev == PROCESS_EVENT_POLL)
        {
            int sta = (int)data;
            app_led_poll_handler((DEV_STATE)sta);
        }
    }

    PROCESS_END();
}
/*---------------------------------------------------------------------------*/

#endif

