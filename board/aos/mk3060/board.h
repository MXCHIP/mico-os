/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#define HARDWARE_REVISION   "V1.0"
#define MODEL               "EMW3060"

/******************************************************
 *                   Enumerations
 ******************************************************/

enum
{
    MICO_GPIO_4,
    MICO_GPIO_22,
    MICO_GPIO_23,
    MICO_GPIO_20,
    MICO_GPIO_21,
    MICO_GPIO_31,
    MICO_GPIO_0,
    MICO_GPIO_1,
    MICO_GPIO_15,
    MICO_GPIO_17,
    MICO_GPIO_16,
    MICO_GPIO_14,
    MICO_GPIO_30,
    MICO_GPIO_29,
    MICO_GPIO_MAX, /* Denotes the total number of GPIO port aliases. Not a valid GPIO alias */
    MICO_GPIO_NONE,
    MICO_SYS_LED = MICO_GPIO_4,
    MICO_RF_LED = MICO_GPIO_NONE,
    BOOT_SEL = MICO_GPIO_NONE,
    MFG_SEL = MICO_GPIO_NONE,
    EasyLink_BUTTON = MICO_GPIO_29,
    STDIO_UART_RX = MICO_GPIO_NONE,
    STDIO_UART_TX = MICO_GPIO_NONE,
};



#ifdef BOOTLOADER
#define STDIO_UART 0
#define STDIO_UART_BUADRATE 921600
#else
#define STDIO_UART 0
#define STDIO_UART_BUADRATE 921600
#endif
