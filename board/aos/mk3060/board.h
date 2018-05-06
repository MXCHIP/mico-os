/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "mico_board.h"
#include "mico_board_conf.h"


/******************************************************
 *                   Enumerations
 ******************************************************/


#ifdef BOOTLOADER
#define STDIO_UART 0
#define STDIO_UART_BUADRATE 921600
#else
#define STDIO_UART 0
#define STDIO_UART_BUADRATE 921600
#endif
