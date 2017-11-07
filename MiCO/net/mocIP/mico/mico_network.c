/**
 ******************************************************************************
 * @file    mico_network.c
 * @author  William Xu
 * @version V1.0.0
 * @date    06-Nov-2017
 * @brief   This file provide the MiCO network management abstract layer functions.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2017 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */


#include <string.h>
#include <stdlib.h>

#include "mico_common.h"
#include "mico_debug.h"
#include "mico_network.h"

#include "mico_rtos.h"
#include "mico_eth.h"
#include "mico_wlan.h"

/******************************************************
 *                      Macros
 ******************************************************/

#define net_log(M, ...) MICO_LOG(MICO_DEBUG_ON, "ETH", M, ##__VA_ARGS__)

/******************************************************
 *                    Constants
 ******************************************************/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *               Function Declarations
 ******************************************************/

/******************************************************
 *               Variables Definitions
 ******************************************************/

netif_t netif_prioritys[INTERFACE_MAX] = { INTERFACE_ETH, INTERFACE_STA, INTERFACE_UAP };

/******************************************************
 *               Function Definitions
 ******************************************************/


void mico_network_set_interface_priority( netif_t prioritys[INTERFACE_MAX] )
{
    return;
}


OSStatus mico_network_switch_interface_auto( void )
{
    return kUnsupportedErr;
}


OSStatus mico_network_switch_interface_manual( netif_t interface )
{
    return kUnsupportedErr;
}

void mico_network_set_default_interface( netif_t prioritys[] )
{
    return;
}

