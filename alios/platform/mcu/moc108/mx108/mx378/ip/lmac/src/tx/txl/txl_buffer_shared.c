/**
 ****************************************************************************************
 *
 * @file txl_buffer_shared.c
 *
 * @brief Definition of the Tx buffer area that has to be shared with MAC HW
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "txl_buffer.h"
#include "compiler.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
struct txl_buffer_hw_desc_tag txl_buffer_hw_desc[TX_BUFFER_POOL_MAX] ;
struct txl_buffer_control txl_buffer_control_desc[NX_REMOTE_STA_MAX][2];
struct txl_buffer_control txl_buffer_control_desc_bcmc[NX_VIRT_DEV_MAX];
/// @}

