/**
 ****************************************************************************************
 *
 * @file mac.c
 *
 * @brief MAC related definitions.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup CO_MAC
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
// for other MAC definitions
#include "mac.h"

/*
 * GLOBAL VARIABLES
 ****************************************************************************************
 */
const uint8_t mac_tid2ac[TID_MAX] =
{
    AC_BE,    // TID0
    AC_BK,    // TID1
    AC_BK,    // TID2
    AC_BE,    // TID3
    AC_VI,    // TID4
    AC_VI,    // TID5
    AC_VO,    // TID6
    AC_VO,    // TID7
    AC_VO     // TIDMGT
};

const uint8_t mac_ac2uapsd[AC_MAX] =
{
    MAC_QOS_INFO_STA_UAPSD_ENABLED_BK,
    MAC_QOS_INFO_STA_UAPSD_ENABLED_BE,
    MAC_QOS_INFO_STA_UAPSD_ENABLED_VI,
    MAC_QOS_INFO_STA_UAPSD_ENABLED_VO,
};

const uint8_t mac_id2rate[MAC_RATESET_LEN] =
{
    MAC_RATE_1MBPS,
    MAC_RATE_2MBPS,
    MAC_RATE_5_5MBPS,
    MAC_RATE_11MBPS,
    MAC_RATE_6MBPS,
    MAC_RATE_9MBPS,
    MAC_RATE_12MBPS,
    MAC_RATE_18MBPS,
    MAC_RATE_24MBPS,
    MAC_RATE_36MBPS,
    MAC_RATE_48MBPS,
    MAC_RATE_54MBPS
};

const struct mac_addr mac_addr_bcst = {{0xFFFF, 0xFFFF, 0xFFFF}};


const uint16_t mac_mcs_params_20[77] =
{
    13  ,
    26  ,
    39  ,
    52  ,
    78  ,
    104 ,
    117 ,
    130 ,
    26  ,
    52  ,
    78  ,
    104 ,
    156 ,
    208 ,
    234 ,
    260 ,
    39  ,
    78  ,
    117 ,
    156 ,
    234 ,
    312 ,
    351 ,
    390 ,
    52  ,
    104 ,
    156 ,
    208 ,
    312 ,
    416 ,
    468 ,
    520 ,
    6   ,
    78  ,
    104 ,
    130 ,
    117 ,
    156 ,
    195 ,
    104 ,
    130 ,
    130 ,
    156 ,
    182 ,
    182 ,
    208 ,
    156 ,
    195 ,
    195 ,
    234 ,
    273 ,
    273 ,
    312 ,
    130 ,
    156 ,
    182 ,
    156 ,
    182 ,
    208 ,
    234 ,
    208 ,
    234 ,
    260 ,
    260 ,
    286 ,
    195 ,
    234 ,
    273 ,
    234 ,
    273 ,
    312 ,
    351 ,
    312 ,
    351 ,
    390 ,
    390 ,
    429
};

const uint16_t mac_mcs_params_40[77] =
{
    27  ,
    54  ,
    81  ,
    108 ,
    162 ,
    216 ,
    243 ,
    270 ,
    54  ,
    108 ,
    162 ,
    216 ,
    324 ,
    432 ,
    486 ,
    540 ,
    81  ,
    162 ,
    243 ,
    324 ,
    486 ,
    648 ,
    729 ,
    810 ,
    108 ,
    216 ,
    324 ,
    432 ,
    648 ,
    864 ,
    972 ,
    1080,
    12  ,
    162 ,
    216 ,
    270 ,
    243 ,
    324 ,
    405 ,
    216 ,
    270 ,
    270 ,
    324 ,
    378 ,
    378 ,
    432 ,
    324 ,
    405 ,
    405 ,
    486 ,
    567 ,
    567 ,
    648 ,
    270 ,
    324 ,
    378 ,
    324 ,
    378 ,
    432 ,
    486 ,
    432 ,
    486 ,
    540 ,
    540 ,
    594 ,
    405 ,
    486 ,
    567 ,
    486 ,
    567 ,
    648 ,
    729 ,
    648 ,
    729 ,
    810 ,
    810 ,
    891
};


/// @}

