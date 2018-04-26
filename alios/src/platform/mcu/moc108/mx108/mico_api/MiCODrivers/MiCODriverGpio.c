#include "include.h"
#include "rtos_pub.h"
#include "BkDriverGpio.h"

void gpio_test_func(uint8_t cmd, uint8_t id, uint32_t mode, void(*p_handle)(char))
{
    if(cmd == 1)
        BkGpioEnableIRQ(id, mode, p_handle, NULL);
    else
        BkGpioDisableIRQ(id);
}
OSStatus MicoGpioEnableIRQ( uint32_t gpio, platform_gpio_irq_trigger_t trigger, platform_gpio_irq_callback_t handler, void *arg )
{
    BkGpioEnableIRQ(gpio, trigger, handler, arg);
}
OSStatus MicoGpioDisableIRQ( uint32_t gpio )
{
    BkGpioDisableIRQ(gpio);
}

OSStatus MicoGpioOp(char cmd, uint32_t id, char mode)
{
    uint32_t command, mode_set;

    if(cmd == '0')
        command = CMD_GPIO_CFG;
    else if(cmd == '1')
        command = CMD_GPIO_INPUT;
    else if(cmd == '2')
        command = CMD_GPIO_OUTPUT;
    else
        command = CMD_GPIO_OUTPUT_REVERSE;

    if(mode == '0')
        mode_set = GMODE_INPUT_PULLDOWN;
    else if(mode == '1')
        mode_set = GMODE_OUTPUT;
    else if(mode == '2')
        mode_set = GMODE_INPUT_PULLUP;
    else
        mode_set = GMODE_INPUT;
    mode_set = (mode_set << 8) | id;

    return sddev_control(GPIO_DEV_NAME, command, &mode_set);
}

