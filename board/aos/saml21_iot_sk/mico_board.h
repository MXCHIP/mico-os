/**
******************************************************************************
* @file    mico_board_conf.h
* @author  William Xu
* @version V1.0.0
* @date    24-May-2018
* @brief   This file provides common configuration for current platform.
******************************************************************************
*
*  The MIT License
*  Copyright (c) 2018 MXCHIP Inc.
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
******************************************************************************
*/ 

#ifndef __MICO_BOARD__
#define __MICO_BOARD__

#ifdef __cplusplus
extern "C"
{
#endif

#define EasyLink_BUTTON        0
#define MICO_SYS_LED           0
#define MICO_I2C_CP            0
#define MICO_MFG_TEST           0

    enum
    {
      MICO_PARTITION_FILESYS,
      MICO_PARTITION_USER_MAX
    };

    enum
    {
        MICO_PARTITION_ERROR = -1,
        MICO_PARTITION_BOOTLOADER = MICO_PARTITION_USER_MAX,
        MICO_PARTITION_APPLICATION,
        MICO_PARTITION_ATE,
        MICO_PARTITION_OTA_TEMP,
        MICO_PARTITION_RF_FIRMWARE,
        MICO_PARTITION_PARAMETER_1,
        MICO_PARTITION_PARAMETER_2,
        MICO_PARTITION_MAX,
        MICO_PARTITION_NONE,
    };

#ifdef __cplusplus
} /*extern "C" */
#endif

#endif //__MICO_BOARD_CONF__
