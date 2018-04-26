#ifndef __APP_LED_H__
#define __APP_LED_H__

typedef enum
{
    STA_NONE         = 0,
    POWER_ON,
    NO_SPI_DATA,
    DET_SPI_DATA,
} DEV_STATE;


void app_led_intial(void);

#endif
