/**
 ****************************************************************************************
 *
 * @file me_mic.c
 *
 * @brief The MIC Calculator utility generic implementation.
 *
 * Copyright (C) RivieraWaves 2011-2016
 *
 ****************************************************************************************
 */

/** @addtogroup MIC_CALC
 * @{
 */

/*
 * INCLUDE FILES
 ****************************************************************************************
 */

#include "me_mic.h"
#include "co_utils.h"
#include "arch.h"

/**
 * DEFINES
 ****************************************************************************************
 */

/**
 * At the end of the MIC Calculation, a padding is added that consists of the signature
 * byte 0x5A and number of padding bytes that are set to zeroes. to make sure the over all
 * length of the message can be divided by 4 without remainder.
 */
#define MIC_END_SIGNATURE   (0x0000005A)

/**
 *  Mask to be applied to the received TID value (three lower bits)
 */
#define MIC_TID_MASK        (0x00000007)

/*
 * MACROS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief A macro for the right rotation instruction.
 *        Rotates L by Q bits to the right
 *
 * @param[in]  L   The value to be rotated.
 * @param[in]  Q   The number of bits to rotate L with.
 *
 * @return          The value of L after being rotated Q bits to the right.
 ****************************************************************************************
 */
#define ROR32(L,Q)  ( ( (uint32_t)(L) >> (Q) ) | ( (uint32_t)(L) << (32-(Q)) ) )

/**
 ****************************************************************************************
 * @brief A macro for the right rotation instruction.
 *        Rotates L by Q bits to the right
 *
 * @param[in]  L   The value to be rotated.
 * @param[in]  Q   The number of bits to rotate L with.
 *
 * @return          The value of L after being rotated Q bits to the right.
 ****************************************************************************************
 */
#define ROL32(L,Q)  ( ( (uint32_t)(L) << (Q) ) | ( (uint32_t)(L) >> (32-(Q)) ) )

/**
 ****************************************************************************************
 * @brief A macro for swap operation.
 *        Swaps the 1st and 2nd bytes, and swaps the 3rd and 4th bytes
 *
 * @param[in]  L   The value to apply swap on
 *
 * @return          The value of L after swapping as described above
 ****************************************************************************************
 */
#define XSWAP32(L)    ( ( ( L & 0xFF00FF00 ) >> 8 ) | \
                        ( ( L & 0x00FF00FF ) << 8 ) )

/**
 * @brief A macro for multiplication by 8
 *
 * This macro is used to convert the lengths
 * from the byte unit to the bit unit
 *
 * @param[in]  X   the length in bytes
 * @return          the length in bits (i.e. X << 3)
 */
/**
 ****************************************************************************************
 * @brief This macro is used to convert the lengths from the byte unit to the bit unit
 *
 * @param[in]  X   The number of bytes to be converted in bits
 *
 * @return          The provided value converted in bits
 ****************************************************************************************
 */
#define LEN_IN_BITS(X) ( (X) << 3 )

/**
 ****************************************************************************************
 * @brief This macro is used to convert the lengths from the byte unit to the bit unit
 *
 * @param[in]  X   The number of bytes to be converted in bits
 *
 * @return          The provided value converted in bits
 ****************************************************************************************
 */
#define SHIFTR(X, S) (((S) == 32)? 0 : ((X) >> (S)) )

/*
 * PRIVATE FUNCTION IMPLEMENTATIONS
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Michael block function implementation
 *        Implement the Feistel-type Michael block function b as defined in the IEEE
 *        standard 802.11-2012 (section 11.4.2.3 - Figure 11-10 and 11-11).
 *
 *        Input (l, r, M(i))
 *        Output (l, r)
 *        b(l, r, M(i))
 *           l <- l XOR M(i)
 *           r <- r XOR (l <<< 17)
 *           l <- (l + r) mod 2^32
 *           r <- r XOR XSWAP(l)
 *           l <- (l + r) mod 2^32
 *           r <- r XOR (l <<< 3)
 *           l <- (l + r) mod 2^32
 *           r <- r XOR (l >>> 2)
 *           l <- (l + r) mod 2^32
 *           return (l, r)
 *
 *        where <<< denotes the rotate-left operator on 32-bit
 *              >>> the rotate-right operator
 *              XSWAP a function that swaps the position of the 2 least significant octets.
 *
 * @param[in]  mic_calc_ptr   A pointer to an array of two 32-bit words that holds the MIC key.
 * @param[in]  block          32-bit word M(i) on which function has to be applied
 *
 ****************************************************************************************
 */
static void michael_block(struct mic_calc *mic_calc_ptr, uint32_t block)
{
    uint32_t l = mic_calc_ptr->mic_key_least;
    uint32_t r = mic_calc_ptr->mic_key_most;

    l   ^= block;
    r   ^=  ROL32(l, 17);
    l   +=  r;
    r   ^=  XSWAP32(l);
    l   +=  r;
    r   ^=  ROL32(l, 3);
    l   +=  r;
    r   ^=  ROR32(l, 2);
    l   +=  r;

    mic_calc_ptr->mic_key_most  = r;
    mic_calc_ptr->mic_key_least = l;
}

/*
 * PUBLIC FUNCTION IMPLEMENTATIONS
 ****************************************************************************************
 */

void me_mic_init(struct mic_calc *mic_calc_ptr, uint32_t *mic_key_ptr,
                 struct mac_addr *da, struct mac_addr *sa, uint8_t tid)
{
    // First blocks
    uint32_t m_0, m_1, m_2, m_3;

    // Initialize MIC value
    mic_calc_ptr->mic_key_least = mic_key_ptr[0];
    mic_calc_ptr->mic_key_most  = mic_key_ptr[1];

    mic_calc_ptr->last_m_i      = 0;
    mic_calc_ptr->last_m_i_len  = 0;

    // Fulfill the different blocks
    m_0 = (uint32_t)da->array[0] | (((uint32_t)da->array[1]) << 16);
    m_1 = (uint32_t)da->array[2] | (((uint32_t)sa->array[0]) << 16);
    m_2 = (uint32_t)sa->array[1] | (((uint32_t)sa->array[2]) << 16);
    if (tid == 0xFF)
        m_3 = 0;
    else
        m_3 = (uint32_t)tid & MIC_TID_MASK;

    // Apply Michael block function on first 4 blocks
    michael_block(mic_calc_ptr, m_0);
    michael_block(mic_calc_ptr, m_1);
    michael_block(mic_calc_ptr, m_2);
    michael_block(mic_calc_ptr, m_3);
}

void me_mic_calc(struct mic_calc *mic_calc_ptr, uint32_t start_ptr,
                 uint32_t data_len)
{
    uint32_t block_cnt;
    uint32_t m_i;
    uint32_t rem_len = data_len;
    uint32_t nb_blocks;
    uint32_t *u32_ptr = HW2CPU(start_ptr & ~0x03);
    uint32_t val = *u32_ptr++;
    uint8_t cut = start_ptr & 0x03;
    uint8_t valid = 4 - cut;
    uint32_t last_m_i_len = mic_calc_ptr->last_m_i_len;
    uint32_t last_m_i = mic_calc_ptr->last_m_i;

    val >>= LEN_IN_BITS(cut);
    if (data_len < valid)
    {
        val &= 0xFFFFFFFF >> LEN_IN_BITS(4 - data_len);
        valid = data_len;
        rem_len = 0;
    }
    else
    {
        rem_len -= valid;
    }

    if ((last_m_i_len + valid) < 4)
    {
        last_m_i |= val << LEN_IN_BITS(last_m_i_len);
        last_m_i_len += valid;
    }
    else
    {
        m_i = last_m_i | (val << LEN_IN_BITS(last_m_i_len));

        last_m_i = SHIFTR(val, LEN_IN_BITS(4 - last_m_i_len));
        last_m_i_len += valid - 4;

        // Apply Michael block function
        michael_block(mic_calc_ptr, m_i);
    }

    // Compute the number of remaining blocks
    nb_blocks = (rem_len >> 2);

    for (block_cnt = 0; block_cnt < nb_blocks; block_cnt++)
    {
        // Read memory
        val = *u32_ptr++;

        // Extract the block value
        m_i = last_m_i | (val << LEN_IN_BITS(last_m_i_len));

        // Save last MI
        last_m_i = SHIFTR(val, LEN_IN_BITS(4 - last_m_i_len));

        // Apply Michael block function
        michael_block(mic_calc_ptr, m_i);
    }

    // If pending bytes store them inside the mic_calc structure
    if (rem_len > (nb_blocks << 2))
    {
        uint32_t add_bytes = rem_len - (nb_blocks << 2);

        val = (*u32_ptr) & (SHIFTR(0xFFFFFFFF, LEN_IN_BITS(4 - add_bytes)));
        if ((last_m_i_len + add_bytes) > 3)
        {
            // Extract the block value
            m_i = last_m_i | (val << LEN_IN_BITS(last_m_i_len));

            // Save last MI
            last_m_i = SHIFTR(val, LEN_IN_BITS(4 - last_m_i_len));
            last_m_i_len += add_bytes - 4;

            // Apply Michael block function
            michael_block(mic_calc_ptr, m_i);
        }
        else
        {
            last_m_i |= val << LEN_IN_BITS(last_m_i_len);
            last_m_i_len += add_bytes;
        }
    }

    mic_calc_ptr->last_m_i     = last_m_i;
    mic_calc_ptr->last_m_i_len = last_m_i_len;
}

void me_mic_end(struct mic_calc *mic_calc_ptr)
{
    // M(n-2) block
    uint32_t m_n_2 = mic_calc_ptr->last_m_i;

    ASSERT_ERR(mic_calc_ptr->last_m_i_len < 4);

    m_n_2 |= (MIC_END_SIGNATURE << LEN_IN_BITS(mic_calc_ptr->last_m_i_len));

    // Apply Michael block function to the last 2 blocks
    michael_block(mic_calc_ptr, m_n_2);
    // M(n-1) = 0 by construction
    michael_block(mic_calc_ptr, 0);
}

/// @}
