/**
 ******************************************************************************
 * @file    gpio_fast.c
 * @author  Yang Haibo
 * @version V1.0.0
 * @date    18-July-2018
 * @brief   Fast GPIO output
 ******************************************************************************
 *
 *  The MIT License
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in
 *  all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 *  WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR
 *  IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 ******************************************************************************
 */
#include "platform.h"
#include "platform_peripheral.h"

#define GPIO_DR 0x40001000

static const uint32_t gpio_pins[] =
{
    [MICO_GPIO_1 ] = 1<<14,
    [MICO_GPIO_2 ] = 1<<15,
    [MICO_GPIO_7 ] = 1<<22,
    [MICO_GPIO_8 ] = 1<<19,
    [MICO_GPIO_9 ] = 1<<23,
    [MICO_GPIO_10] = 1<<18,
    [MICO_GPIO_12] = 1<<0,
    [MICO_GPIO_13] = 1<<12,
    [MICO_GPIO_14] = 1<<5,
    [MICO_GPIO_19] = 1<<11,
    [MICO_GPIO_21] = 1<<30,
    [MICO_GPIO_22] = 1<<29,
    [MICO_GPIO_23] = 1<<8,
};

__attribute__((section(".fast")))
OSStatus gpio_output_high( mico_gpio_t gpio )
{
    uint32_t RegValue;
    uint32_t *reg_addr = (uint32_t*)GPIO_DR;
    
    RegValue =  (*reg_addr);
    RegValue |= gpio_pins[gpio]; 
    (*reg_addr) = RegValue;
  	return 0;
}

__attribute__((section(".fast")))
OSStatus gpio_output_low( mico_gpio_t gpio )
{
    uint32_t RegValue;
    uint32_t *reg_addr = (uint32_t*)GPIO_DR;
    
    RegValue =  (*reg_addr);
    RegValue &= ~gpio_pins[gpio];
    (*reg_addr) = RegValue;
  	return 0;
}

#ifdef __GNUC__
#pragma GCC optimize ("O0") // 关闭优化，否则直接反转 IO 会被优化
#endif /* ifdef __GNUC__ */
/* demo: output gpio22 fast */
__attribute__((section(".fast"))) // 代码放到RAM运行
void gpio_fast_demo(mico_gpio_t gpio)
{
    int i;
    uint32_t RegValue, high, low;
    uint32_t *reg_addr = (uint32_t*)GPIO_DR;
    
    RegValue =  (*reg_addr);
    high = RegValue|gpio_pins[gpio]; 
    low = RegValue &(~gpio_pins[gpio]);

    __set_FAULTMASK(1); // 关闭调度器
    for(i=0; i<1000000; i++) {
        (*reg_addr) = high; //输出高
        (*reg_addr) = low;  // 输出低
        (*reg_addr) = high;
        (*reg_addr) = low;
        (*reg_addr) = high;
        (*reg_addr) = low;
    }
    __set_FAULTMASK(0); // 打开调度器
}

