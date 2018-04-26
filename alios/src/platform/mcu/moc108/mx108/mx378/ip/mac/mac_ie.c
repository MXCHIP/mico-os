/**
 ****************************************************************************************
 *
 * @file mac_ie.c
 *
 * @brief MAC Information Elements related API definitions.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @addtogroup CO_MAC_IE
 * @{
 ****************************************************************************************
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */
// minimum default inclusion directive
#include "mac_ie.h"
// for memcmp and memmove
#include <string.h>
// for ASSERT_ERR
#include "arch.h"
// for management frame related constants
#include "mac_frame.h"
#include "co_utils.h"


/*
 * FUNCTION DEFINITIONS
 ****************************************************************************************
 */
uint32_t mac_ie_find(uint32_t addr,
                     uint16_t buflen,
                     uint8_t ie_id)
{
    uint32_t end = addr + buflen;
    // loop as long as we do not go beyond the frame size
    while (addr < end)
    {
        // check if the current IE is the one looked for
        if (ie_id == co_read8p(addr))
        {
            // the IE id matches and it is not OUI IE, return the pointer to this IE
            return addr;
        }
        // move on to the next IE
        addr += co_read8p(addr + MAC_INFOELT_LEN_OFT) + MAC_INFOELT_INFO_OFT;
    }

    // sanity check: the offset can not be greater than the length
    ASSERT_ERR(addr == end);

    return 0;
}


uint32_t mac_vsie_find(uint32_t addr,
                       uint16_t buflen,
                       uint8_t const *oui,
                       uint8_t ouilen)
{
    uint32_t end = addr + buflen;

    // loop as long as we do not go beyond the frame size
    while (addr < end)
    {
        // check if the current IE is the one looked for
        if (co_read8p(addr) == MAC_ELTID_OUI)
        {
            // check if the OUI matches the one we are looking for
            if (co_cmp8p(addr + MAC_INFOELT_INFO_OFT, oui, ouilen))
            {
                // the OUI matches, return the pointer to this IE
                return addr;
            }
        }
        // move on to the next IE
        addr += co_read8p(addr + MAC_INFOELT_LEN_OFT) + MAC_INFOELT_INFO_OFT;
    }

    // sanity check: the offset can not be greater than the length
    ASSERT_ERR(addr == end);

    return 0;
}

//bool mac_vsie_remove(uint8_t *buffer,
//                     uint16_t *buflen,
//                     uint8_t const *oui,
//                     uint8_t ouilen)
//{
//    uint8_t *ie = NULL;
//
//    // find the IE (cast is necessary because finding returns const pointer)
//    ie = (uint8_t *)mac_vsie_find(buffer, *buflen, oui, ouilen);
//    if (ie == NULL)
//    {
//        // the IE was not found, return false
//        return false;
//    }
//
//    // delete the IE
//    mac_ie_delete(buffer, buflen, ie);
//
//    return true;
//}
//
//void mac_vsie_add(uint8_t *buffer,
//                  uint16_t *buflen,
//                  uint8_t const *vsie,
//                  uint8_t ouilen)
//{
//    // sanity check
//    ASSERT_ERR(vsie[MAC_INFOELT_ID_OFT] == MAC_ELTID_OUI);
//
//    // start by removing the previous identical IE
//    mac_vsie_remove(buffer, buflen, &(vsie[MAC_INFOELT_INFO_OFT]), ouilen);
//
//    // insert the new IE at the right location
//    mac_ie_insert(buffer,buflen, vsie[MAC_INFOELT_ID_OFT], vsie);
//}
//
/// @}
