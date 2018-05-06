/**
 ******************************************************************************
 * @file    platform_flash.h
 * @author  William Xu
 * @version V1.0.0
 * @date    01-May-2018
 * @brief   This file provide mico platform driver header file
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2018 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */
#ifndef __PLATFORM_FLASH_H__
#define __PLATFORM_FLASH_H__


#ifdef __cplusplus
extern "C" {
#endif

/******************************************************
 *                      Macros
 ******************************************************/

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/
 
typedef enum {
    FLASH_TYPE_EMBEDDED,
    FLASH_TYPE_SPI,
    FLASH_TYPE_QSPI,
} platform_flash_type_t;

/******************************************************
 *                 Type Definitions
 ******************************************************/

typedef struct
{
    int32_t                    partition_owner;
    const char*                partition_description;
    uint32_t                   partition_start_addr;
    uint32_t                   partition_length;
    uint32_t                   partition_options;
} platform_logic_partition_t;

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/



/******************************************************
 *                    Structures
 ******************************************************/


/******************************************************
 *                 Global Variables
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/




#ifdef __cplusplus
} /*"C" */
#endif

#endif





