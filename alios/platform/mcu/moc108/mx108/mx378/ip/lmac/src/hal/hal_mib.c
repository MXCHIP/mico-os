/**
 ****************************************************************************************
 *
 * @file hal_mib.c
 *
 * @brief File containing the definition of HW descriptors.
 *
 * File containing the definition of the structure and API function used to initialize
 * the pool.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 *****************************************************************************************
 * @addtogroup HWDESC
 * @{
 *****************************************************************************************
 */

/*
 * INCLUDE FILES
 *****************************************************************************************
 */
#include "hal_machw_mib.h"
#include "compiler.h"

/*
 * GLOBAL VARIABLES
 *****************************************************************************************
 */
// In embedded, defined at specific address; otherwise, defined by other means
volatile struct machw_mib_tag machw_mib;
/// @}  // end of group
