/**
 ****************************************************************************************
 *
 * @file rc.c
 *
 * Copyright (C) RivieraWaves 2015
 *
 * @brief The Rate Control module implementation.
 *
 ****************************************************************************************
 */

/** @addtogroup RC_INIT
* @{
*/

/**
 * INCLUDE FILES
 ****************************************************************************************
 */
#include "me.h"
#include "mm.h"
#include "me_utils.h"

#include "include.h"
#include "uart_pub.h"

#if RC_ENABLE
#include "rc.h"

extern struct me_env_tag me_env;

/**
 * PRIVATE VARIABLES
 ****************************************************************************************
 */

/// Array of rate control statistics
struct rc_sta_stats sta_stats[NX_REMOTE_STA_MAX];

// Transmission duration in nsecs of 1200 bytes.
// Index of the table is (MCS_IDX << 3) | (BW << 1) | (GI_400)
//  where BW is 0 for 20 MHz, 1 for 40MHz and 2 for 80MHz
//        GI_400 is 1 if packet is being sent with 400ns GI, 0 if 800ns GI
const uint32_t rc_duration_ht_ampdu[10*4*2] =
{
    //BW20,GI800          BW20,GI400            BW40,GI800            BW40,GI400           MCS Index
    [  0] =  1477000,     [  1] =  1329350,     [  2] =   711250,     [  3] =   640175, // MCS 0
    [  8] =   738500,     [  9] =   664700,     [ 10] =   355750,     [ 11] =   320225, // MCS 1
    [ 16] =   492500,     [ 17] =   443300,     [ 18] =   237250,     [ 19] =   213575, // MCS 2
    [ 24] =   369250,     [ 25] =   332375,     [ 26] =   178000,     [ 27] =   160250, // MCS 3
    [ 32] =   246250,     [ 33] =   221675,     [ 34] =   118750,     [ 35] =   106925, // MCS 4
    [ 40] =   184750,     [ 41] =   166325,     [ 42] =    89000,     [ 43] =    80150, // MCS 5
    [ 48] =   164250,     [ 49] =   147875,     [ 50] =    79250,     [ 51] =    71375, // MCS 6
    [ 56] =   147750,     [ 57] =   133025,     [ 58] =    71250,     [ 59] =    64175, // MCS 7
    [ 64] =   123250,     [ 65] =   110975,     [ 66] =    59500,     [ 67] =    53600, // MCS 8
    [ 72] =   111000,     [ 73] =    99950,     [ 74] =    53500,     [ 75] =    48200, // MCS 9
    //BW80,GI800          BW80,GI400            BW160,GI800           BW160,GI400          MCS Index
    [  4] =   328250,     [  5] =   295475,     [  6] =   164250,     [  7] =   147875, // MCS 0
    [ 12] =   164250,     [ 13] =   147875,     [ 14] =    82250,     [ 15] =    74075, // MCS 1
    [ 20] =   109500,     [ 21] =    98600,     [ 22] =    54750,     [ 23] =    49325, // MCS 2
    [ 28] =    82250,     [ 29] =    74075,     [ 30] =    40000,     [ 31] =    36050, // MCS 3
    [ 36] =    54750,     [ 37] =    49325,     [ 38] =    27500,     [ 39] =    24800, // MCS 4
    [ 44] =    41250,     [ 45] =    37175,     [ 46] =    20750,     [ 47] =    18725, // MCS 5
    [ 52] =    36500,     [ 53] =    32900,     [ 54] =    18250,     [ 55] =    16475, // MCS 6
    [ 60] =    33000,     [ 61] =    29750,     [ 62] =    16500,     [ 63] =    14900, // MCS 7
    [ 68] =    27500,     [ 69] =    24800,     [ 70] =    13750,     [ 71] =    12425, // MCS 8
    [ 76] =    24750,     [ 77] =    22325,     [ 78] =    12500,     [ 79] =    11300, // MCS 9
};

// Transmission duration in nsecs of 1200 bytes.
// Index of the table is (MCS_IDX<<1) | (pre_type)
//  where pre_type is 0 for long and short preamble
//                    1 for long preamble only
const uint32_t rc_duration_cck[4*2] =
{
    //short preamble      long preamble       MCS Index
    [  0] = 10452000,     [  1] = 10548000, // MCS 0
    [  2] =  5380000,     [  3] =  5476000, // MCS 1
    [  4] =  2315000,     [  5] =  2411000, // MCS 2
    [  6] =  1439000,     [  7] =  1535000, // MCS 3
};

// Transmission duration in nsecs of 1200 bytes.
// Index of the table is (MCS_IDX - 4)
const uint32_t rc_duration_non_ht[8] =
{
                      // MCS Index
    [  0] =  1600000, // MCS 4
    [  1] =  1068000, // MCS 5
    [  2] =   800000, // MCS 6
    [  3] =   536000, // MCS 7
    [  4] =   400000, // MCS 8
    [  5] =   268000, // MCS 9
    [  6] =   200000, // MCS 10
    [  7] =   180000, // MCS 11
};

/**
 * PRIVATE FUNCTIONS DECLARATION
 ****************************************************************************************
 */
static void rc_calc_prob_ewma(struct rc_rate_stats *rc_rs);
static uint16_t rc_set_previous_mcs_index(struct rc_sta_stats *rc_ss, uint16_t rate_config);
static uint16_t rc_set_next_mcs_index(struct rc_sta_stats *rc_ss, uint16_t rate_config);
static bool rc_check_rate_duplicated(struct rc_sta_stats *rc_ss, uint16_t rate_config);
static uint16_t rc_new_random_rate(struct rc_sta_stats *ss, uint8_t bw_min);
static bool rc_set_trial_tx(struct rc_sta_stats *rc_ss);
static void rc_revert_trial_tx(struct rc_sta_stats *rc_ss);
static bool rc_update_stats(struct rc_sta_stats *rc_ss, bool init);
static void rc_update_retry_chain(struct rc_sta_stats *rc_ss, uint32_t *cur_tp);
static void rc_get_new_samples(struct rc_sta_stats *rc_ss, uint32_t *cur_tp);
static bool rc_update_stats_fixed_rate(struct rc_sta_stats *rc_ss);

/**
 * PRIVATE FUNCTIONS DEFINITION
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief Perform EWMA (Exponentially Weighted Moving Average) calculation;
 * @param[in] old_val old value
 * @param[in] new_val new value
 * @param[in] weight weight to be used in the EWMA
 * @return Result of EWMA.
 ****************************************************************************************
 */
static inline uint32_t ewma(uint32_t old_val, uint32_t new_val, uint32_t weight)
{
    return ((new_val * (EWMA_DIV - weight) + old_val * weight) / EWMA_DIV);
}

/**
 ****************************************************************************************
 * @brief Extract the format from the rate configuration.
 * @param[in] rate_config rate configuration to be updated
 * @return Format.
 ****************************************************************************************
 */
static uint8_t rc_get_format_mod(uint16_t rate_config)
{
    return (rate_config & FORMAT_MOD_TX_RCX_MASK) >> FORMAT_MOD_TX_RCX_OFT;
}

/**
 ****************************************************************************************
 * @brief Extract the bandwidth from the rate configuration.
 * @param[in] rate_config rate configuration to be updated
 * @return Bandwidth.
 ****************************************************************************************
 */
static uint8_t rc_get_bw(uint16_t rate_config)
{
    return (rate_config & BW_TX_RCX_MASK) >> BW_TX_RCX_OFT;
}

/**
 ****************************************************************************************
 * @brief Extract the number of spatial streams from the rate configuration.
 * @param[in] rate_config rate configuration to be updated
 * @return Number of spatial streams.
 ****************************************************************************************
 */
static uint8_t rc_get_nss(uint16_t rate_config)
{
    uint8_t nss = 0;
    uint8_t format_mod = rc_get_format_mod(rate_config);

    switch (format_mod)
    {
        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            nss = (rate_config & HT_NSS_MASK) >> HT_NSS_OFT;
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            nss = (rate_config & VHT_NSS_MASK) >> VHT_NSS_OFT;
        } break;
        #endif

        default:
            break;
    }

    return nss;
}

/**
 ****************************************************************************************
 * @brief Extract the MCS / rate index from the rate configuration.
 * @param[in] rate_config rate configuration to be updated
 * @return MCS / rate index.
 ****************************************************************************************
 */
static uint8_t rc_get_mcs_index(uint16_t rate_config)
{
    uint8_t format_mod = rc_get_format_mod(rate_config);
    uint8_t mcs = 0;

    switch (format_mod)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            mcs = (rate_config & MCS_INDEX_TX_RCX_MASK) >> MCS_INDEX_TX_RCX_OFT;
        } break;
        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            mcs = (rate_config & HT_MCS_MASK) >> HT_MCS_OFT;
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            mcs = (rate_config & VHT_MCS_MASK) >> VHT_MCS_OFT;
        } break;
        #endif

        default:
            break;
    }

    return mcs;
}

/**
 ****************************************************************************************
 * @brief Extract the guard interval from the rate configuration.
 * @param[in] rate_config rate configuration to be updated
 * @return Guard interval.
 ****************************************************************************************
 */
static uint8_t rc_get_gi(uint32_t rate_config)
{
    return ((rate_config & SHORT_GI_TX_RCX_MASK) >> SHORT_GI_TX_RCX_OFT);
}

/**
 ****************************************************************************************
 * @brief Extract the preamble type from the rate configuration.
 * @param[in] rate_config rate configuration to be updated
 * @return Preamble type.
 ****************************************************************************************
 */
static uint8_t rc_get_pre_type(uint16_t rate_config)
{
    return (rate_config & PRE_TYPE_TX_RCX_MASK) >> PRE_TYPE_TX_RCX_OFT;
}

/**
 ****************************************************************************************
 * @brief Calculates probability of success with EWMA.
 * @param[in] rc_ss pointer to rate control station statistics structure
 ****************************************************************************************
 */
static void rc_calc_prob_ewma(struct rc_rate_stats *rc_rs)
{
    uint64_t success = rc_rs->success;
    uint32_t attempts = rc_rs->attempts;

    if (attempts > 0)
    {
        rc_rs->sample_skipped = 0;
        uint32_t cur_prob = RC_FRAC(success, attempts);

        if (0 == rc_rs->old_prob_available)
        {
            if (cur_prob > 0)
            {
                rc_rs->probability = cur_prob - 1;
            }
            else
            {
                rc_rs->probability = 0;
            }
        }
        else
        {
            rc_rs->probability = ewma(rc_rs->probability, cur_prob, EWMA_LEVEL);
        }
        rc_rs->old_prob_available = 1;
    }
    else
    {
        if (rc_rs->sample_skipped < 0xFF)
        {
            rc_rs->sample_skipped++;
        }
    }
}

/**
 ****************************************************************************************
 * @brief Updates the MCS / rate index of the rate configuration, setting the previous
 * if available.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] rate_config rate configuration to be updated
 * @return Rate configuration updated.
 ****************************************************************************************
 */
static uint16_t rc_set_previous_mcs_index(struct rc_sta_stats *rc_ss, uint16_t rate_config)
{
    uint16_t new_config = rate_config;
    uint8_t format_mod = rc_get_format_mod(rate_config);
    uint8_t mcs = rc_get_mcs_index(rate_config);

    switch (format_mod)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            if (mcs > rc_ss->r_idx_min)
            {
                new_config = (rate_config & ~MCS_INDEX_TX_RCX_MASK) | (mcs-1);
            }
        } break;
        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            if (mcs > 0)
            {
                new_config = (rate_config & ~HT_MCS_MASK) | (mcs-1);
                if (rc_ss->short_gi)
                {
                    new_config |= SHORT_GI_TX_RCX_MASK;
                }
            }
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            if (mcs > 0)
            {
                new_config = (rate_config & ~VHT_MCS_MASK) | (mcs-1);
                if (rc_ss->short_gi)
                {
                    new_config |= SHORT_GI_TX_RCX_MASK;
                }
            }
        } break;
        #endif

        default:
            break;
    }

    return new_config;
}

/**
 ****************************************************************************************
 * @brief Updates the MCS / rate index of the rate configuration, setting the next
 * if available.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] rate_config rate configuration to be updated
 * @return Rate configuration updated.
 ****************************************************************************************
 */
static uint16_t rc_set_next_mcs_index(struct rc_sta_stats *rc_ss, uint16_t rate_config)
{
    uint16_t new_config = rate_config;
    uint8_t format_mod = rc_get_format_mod(rate_config);
    uint8_t mcs = rc_get_mcs_index(rate_config);

    switch (format_mod)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            if ((mcs < rc_ss->r_idx_max) && (rc_ss->rate_map_l & CO_BIT(mcs+1)))
            {
                new_config = (rate_config & ~MCS_INDEX_TX_RCX_MASK) | (mcs+1);
            }
        } break;

        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            uint8_t nss = rc_get_nss(rate_config);
            if ((mcs < rc_ss->mcs_max) && (rc_ss->rate_map.ht[nss] & CO_BIT(mcs+1)))
            {
                new_config = (rate_config & ~HT_MCS_MASK) | (mcs+1);
                if (rc_ss->short_gi)
                {
                    new_config |= SHORT_GI_TX_RCX_MASK;
                }
            }
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            uint8_t nss = rc_get_nss(rate_config);
            uint8_t mcs_max_ss = 7 + ((rc_ss->rate_map.vht >> (nss << 1)) & MAC_VHT_MCS_MAP_MSK);
            if ((mcs < rc_ss->mcs_max) && ((mcs+1) <= mcs_max_ss))
            {
                new_config = (rate_config & ~VHT_MCS_MASK) | (mcs+1);
                if (rc_ss->short_gi)
                {
                    new_config |= SHORT_GI_TX_RCX_MASK;
                }
            }
        } break;
        #endif

        default:
            break;
    }

    return new_config;
}

/**
 ****************************************************************************************
 * @brief Checks if the rate configuration is already present in the sample table.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] rate_cfg rate configuration to be checked
 * @return Whether the rate configuration is already present in the sample table.
 ****************************************************************************************
 */
static bool rc_check_rate_duplicated(struct rc_sta_stats *rc_ss, uint16_t rate_config)
{
    uint32_t i = 0;
    bool ret = 0;

    while (i < rc_ss->no_samples)
    {
        if (rate_config == rc_ss->rate_stats[i].rate_config)
        {
            ret = 1;
            break;
        }
        i++;
    }
    return ret;
}

#if NX_DEBUG
/**
 ****************************************************************************************
 * @brief Checks if the rate configuration of the sample i of the sample table is correct.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] i index of the sample in the sample table
 ****************************************************************************************
 */
static void rc_check_rate_config(struct rc_sta_stats *rc_ss, uint16_t i)
{
    uint16_t rate_config = rc_ss->rate_stats[i].rate_config;
    uint8_t format_mod = rc_get_format_mod(rate_config);
    uint8_t pre_type = rc_get_pre_type(rate_config);
    uint8_t gi = rc_get_gi(rate_config);
    uint8_t bw = rc_get_bw(rate_config);
    uint8_t nss = rc_get_nss(rate_config);
    uint8_t mcs = rc_get_mcs_index(rate_config);

    #if NX_VHT
    ASSERT_ERR(format_mod <= FORMATMOD_VHT);
    #else
    ASSERT_ERR(format_mod <= FORMATMOD_HT_GF);
    #endif

    // check format and modulation
    switch (rc_ss->format_mod)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            ASSERT_ERR(format_mod < FORMATMOD_HT_MF);
        } break;
        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            if (rc_ss->r_idx_min <= HW_RATE_11MBPS)
            {
                // DSSS/CCK
                ASSERT_ERR(format_mod < FORMATMOD_VHT);
            }
            else
            {
                ASSERT_ERR((format_mod == FORMATMOD_HT_MF) || (format_mod == FORMATMOD_HT_GF));
            }
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            ASSERT_ERR(format_mod == FORMATMOD_VHT);
        } break;
        #endif

        default:
            break;
    }

    switch (format_mod)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            if (rc_ss->p_type == 1)
            {
                ASSERT_ERR(pre_type == 1);
            }
            else
            {
                ASSERT_ERR(pre_type <= 1);
            }
            ASSERT_ERR(gi == 0);
            ASSERT_ERR(bw == BW_20MHZ);
            ASSERT_ERR(nss == 0);
            ASSERT_ERR(mcs >= rc_ss->r_idx_min);
            ASSERT_ERR(mcs <= rc_ss->r_idx_max);
            ASSERT_ERR((rc_ss->rate_map_l & CO_BIT(mcs)));
        }
        break;

        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            ASSERT_ERR(pre_type == 0);
            ASSERT_ERR(gi <= rc_ss->short_gi);
            ASSERT_ERR(bw <= rc_ss->bw_max);
            ASSERT_ERR(nss <= rc_ss->no_ss);
            ASSERT_ERR(mcs <= rc_ss->mcs_max);
            ASSERT_ERR((rc_ss->rate_map.ht[nss] & CO_BIT(mcs)));
        }
        break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            ASSERT_ERR(pre_type == 0);
            ASSERT_ERR(gi <= rc_ss->short_gi);
            ASSERT_ERR(bw <= rc_ss->bw_max);
            ASSERT_ERR(nss <= rc_ss->no_ss);
            ASSERT_ERR(mcs <= rc_ss->mcs_max);
            ASSERT_ERR(mcs <= (7 + (rc_ss->rate_map.vht >> (nss << 1) & MAC_VHT_MCS_MAP_MSK)));
            ASSERT_ERR(((mcs == 6) && (bw == BW_80MHZ) && ((nss == 3) || (nss == 6))) == 0);
            ASSERT_ERR(((mcs == 9) && (bw == BW_20MHZ) && (nss != 2) && (nss != 5)) == 0);
            ASSERT_ERR(((mcs == 9) && (bw == BW_80MHZ) && (nss == 5)) == 0);
            ASSERT_ERR(((mcs == 9) && (bw == BW_160MHZ) && (nss == 2)) == 0);
        }
        break;
        #endif

        default:
            break;
    }
}

/**
 ****************************************************************************************
 * @brief Checks if the rate configurations of the sample table are correct.
 * @param[in] rc_ss pointer to rate control station statistics structure
 ****************************************************************************************
 */
static void rc_check_stats_rate_config(struct rc_sta_stats *rc_ss)
{
    uint32_t i;

    for (i = 0; i < rc_ss->no_samples; i++)
    {
        rc_check_rate_config(rc_ss, i);
    }
}
#endif

/**
 ****************************************************************************************
 * @brief Gets a new random rate configuration.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @return The random rate configuration
 ****************************************************************************************
 */
static uint16_t rc_new_random_rate(struct rc_sta_stats *rc_ss, uint8_t bw_min)
{
    // pick a random rate
    uint16_t rd = co_rand_hword();
    uint8_t format = rc_ss->format_mod;
    uint16_t rate_cfg = format << FORMAT_MOD_TX_RCX_OFT;

    switch (format)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            uint8_t r_idx_l_max = rc_ss->r_idx_max;
            uint8_t r_idx_l_min = rc_ss->r_idx_min;
            uint8_t r_idx = (((rd & MCS_INDEX_TX_RCX_MASK) >> MCS_INDEX_TX_RCX_OFT) %
                    (r_idx_l_max - r_idx_l_min + 1)) + r_idx_l_min;
            if ((rc_ss->rate_map_l & CO_BIT(r_idx)) == 0) // rate index not allowed
            {
                r_idx = r_idx_l_max;
            }

            rate_cfg |= r_idx << MCS_INDEX_TX_RCX_OFT;

            if (r_idx == HW_RATE_1MBPS) // rate index 0 (CCK 1Mbps) allows only long preamble
            {
                rate_cfg |= PRE_TYPE_TX_RCX_MASK;
            }
            else if ((r_idx > HW_RATE_1MBPS) && (r_idx <= HW_RATE_11MBPS)) // CCK rate: set preamble type
            {
                rate_cfg |= (rd & PRE_TYPE_TX_RCX_MASK) | (rc_ss->p_type << PRE_TYPE_TX_RCX_OFT);
            }
        } break;
        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            uint8_t r_idx_max = rc_ss->r_idx_max;
            uint8_t r_idx_min = rc_ss->r_idx_min;

            if ((r_idx_min <= HW_RATE_11MBPS) &&
                (((rd & FORMAT_MOD_TX_RCX_MASK) >> FORMAT_MOD_TX_RCX_OFT) % 2)) // 2.4 GHz
            {
                // CCK
                uint8_t r_idx = (((rd & MCS_INDEX_TX_RCX_MASK) >> MCS_INDEX_TX_RCX_OFT) %
                                 (r_idx_max - r_idx_min + 1)) + r_idx_min;
                rate_cfg = FORMATMOD_NON_HT << FORMAT_MOD_TX_RCX_OFT;
                if ((rc_ss->rate_map_l & CO_BIT(r_idx)) == 0) // rate index not allowed
                {
                    r_idx = r_idx_max;
                }
                rate_cfg |= r_idx << MCS_INDEX_TX_RCX_OFT;
                if (r_idx == HW_RATE_1MBPS) // MCS0 (1Mbps) allows only long preamble
                {
                    rate_cfg |= PRE_TYPE_TX_RCX_MASK;
                }
                else
                {
                    rate_cfg |= (rd & PRE_TYPE_TX_RCX_MASK) | (rc_ss->p_type << PRE_TYPE_TX_RCX_OFT);
                }
            }
            else
            {
                uint8_t bw_max = rc_ss->bw_max;
                uint8_t short_gi = rc_ss->short_gi;
                uint8_t mcs_max = rc_ss->mcs_max;
                uint8_t no_ss_max = rc_ss->no_ss;
                uint8_t nss = ((rd & HT_NSS_MASK) >> HT_NSS_OFT) % (no_ss_max + 1);
                uint8_t mcs = ((rd & HT_MCS_MASK) >> HT_MCS_OFT) % (mcs_max + 1);
                if ((rc_ss->rate_map.ht[nss] & CO_BIT(mcs)) == 0) // rate index not allowed
                {
                    mcs = mcs_max;
                }
                rate_cfg |= (((rd & SHORT_GI_TX_RCX_MASK) >> SHORT_GI_TX_RCX_OFT) % (short_gi + 1)) << SHORT_GI_TX_RCX_OFT;
                #if RC_USE_MAX_BW
                rate_cfg |= bw_max << BW_TX_RCX_OFT;
                #else
                rate_cfg |= ((((rd & BW_TX_RCX_MASK) >> BW_TX_RCX_OFT) % (bw_max - bw_min + 1)) + bw_min) << BW_TX_RCX_OFT;
                #endif
                rate_cfg |= nss << HT_NSS_OFT;
                rate_cfg |= mcs << HT_MCS_OFT;
            }
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            uint8_t bw_max = rc_ss->bw_max;
            uint8_t short_gi = rc_ss->short_gi;
            uint8_t no_ss_max = rc_ss->no_ss;
            uint8_t mcs_max = rc_ss->mcs_max;

            uint8_t mcs = ((rd & VHT_MCS_MASK) >> VHT_MCS_OFT) % (mcs_max + 1);
            uint8_t no_ss = ((rd & VHT_NSS_MASK) >> VHT_NSS_OFT) % (no_ss_max + 1);
            uint8_t bw = 0;

            if (mcs > (7 + (rc_ss->rate_map.vht >> (no_ss << 1) & MAC_VHT_MCS_MAP_MSK)))
            {
                mcs = 7 + (rc_ss->rate_map.vht >> (no_ss << 1) & MAC_VHT_MCS_MAP_MSK);
            }

            #if RC_USE_MAX_BW
            bw = bw_max;
            #else
            bw = (((rd & BW_TX_RCX_MASK) >> BW_TX_RCX_OFT) % (bw_max - bw_min + 1)) + bw_min;
            #endif
            // do not allow some missing VHT rates
            if ((mcs == 6) && (bw == BW_80MHZ) && ((no_ss == 3) || (no_ss == 6)))
            {
                no_ss = no_ss - 1;
            }
            if (mcs == 9)
            {
                if ((bw == BW_20MHZ) && (no_ss != 2) && (no_ss != 5))
                {
                    mcs = 8;
                }
                if ((bw == BW_80MHZ) && (no_ss == 5))
                {
                    no_ss = no_ss - 1;
                }
                if ((bw == BW_160MHZ) && (no_ss == 2))
                {
                    no_ss = no_ss - 1;
                }
            }
            rate_cfg |= ((((rd & SHORT_GI_TX_RCX_MASK) >> SHORT_GI_TX_RCX_OFT) % (short_gi + 1)) << SHORT_GI_TX_RCX_OFT) |
                        (bw << BW_TX_RCX_OFT) |
                        (no_ss << VHT_NSS_OFT) |
                        (mcs << MCS_INDEX_TX_RCX_OFT);
        } break;
        #endif

        default:
            break;
    }

    return rate_cfg;
}

/**
 ****************************************************************************************
 * @brief Checks if the rate configuration corresponds to a CCK rate.
 * @param[in] rate_config rate configuration
 * @return Whether the rate configuration corresponds to a CCK rate
 ****************************************************************************************
 */
static bool is_cck_group(uint16_t rate_config)
{
    uint8_t format = rc_get_format_mod(rate_config);
    uint8_t mcs = rc_get_mcs_index(rate_config);
    bool is_cck = 0;

    if ((format < FORMATMOD_HT_MF) && (mcs < HW_RATE_6MBPS))
    {
        is_cck = 1;
    }

    return is_cck;
}

/**
 ****************************************************************************************
 * @brief Calculates the throughput of an entry of the sample table.
 * Calculates the throughput based on the average A-MPDU length, taking into account
 * the expected number of retransmissions and their expected length.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] sample_idx index of the entry of the sample table
 * @return Calculated throughput.
 ****************************************************************************************
 */
uint32_t rc_calc_tp(struct rc_sta_stats *rc_ss, uint8_t sample_idx)
{
    uint64_t cur_tp;
    uint32_t nsecs = 0;

    uint16_t rate_config = rc_ss->rate_stats[sample_idx].rate_config;
    uint32_t prob = rc_ss->rate_stats[sample_idx].probability;

    if (prob < RC_FRAC(1, 10))
    {
        cur_tp = 0;
    }
    else
    {
        //prob = RC_TRUNC(prob * prob);

        if (!is_cck_group(rate_config))
        {
            // use fixed overhead time of 60 usec (ack) + 48 usec + backoff
            nsecs = 1000 * 218 / RC_TRUNC(rc_ss->avg_ampdu_len);
        }
        nsecs += rc_get_duration(rate_config);

        cur_tp = 1000000 * ((prob * 1000) / nsecs);
        cur_tp = RC_TRUNC(cur_tp);
    }

    return cur_tp;
}

/**
 ****************************************************************************************
 * @brief Selects a new rate to be used as trial rate and updates the retry chain.
 * This function updates the trial period value, depending on the average AMPDU size, and
 * picks a new random rate from the sample table and inserts it in the retry chain.
 * The new rate is discarded if:
 * - it is equal to one of the steps of the current retry chain
 * - probability of the sample is already greater than 95%
 * - in case of AMPDU transmission, HT station 2.4 GHz: the rate is a DSSS/CCK rate.
 * - TX duration of the sample is greater than TX durations of steps 0, 1 and 2,
 *   respecting the samples frequently skipped.
 * - current TP of the sample (taking into account the SW retries) is greater than
 *   TP of step 0 (HT, VHT), respecting the samples frequently skipped.
 * The trial sample is inserted as step 0 of the retry chain if (HT/VHT STA) its expected throughput
 * (calculated taking into account the current number of SW retry request) is greater than
 * the throughput of step 0; (NON-HT STA) if TX duration of the sample is less than TX
 * duration of step 0. Otherwise the trial rate is inserted as step 1 of the retry chain.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @return Whether the retry chain has been modified with the trial rate.
 ****************************************************************************************
 */
static bool rc_set_trial_tx(struct rc_sta_stats *rc_ss)
{
    struct rc_rate_stats *rc_rand;
    struct step random_rate = {0, 0};
    bool trial_tx = 0;
    bool direct_sampling = 1;
    uint32_t cur_max_tp_streams;
    uint32_t sample_streams;
    uint32_t sample_dur;
    uint32_t dur_retry_0;
    uint32_t dur_retry_1;
    uint32_t dur_retry_2;
    uint32_t max_skipped;
    uint8_t format_mod = rc_ss->format_mod;
    #if NX_AMPDU_TX
    bool tx_ampdu = (rc_ss->info & RC_AGG_TX_MASK) >> RC_AGG_TX_OFT;
    #endif

    do
    {
        /// do not sample if fixed rate is set
        if (rc_ss->info & RC_FIX_RATE_EN_MASK)
        {
            break;
        }

        // set trial status check
        rc_ss->trial_status = RC_TRIAL_STATUS_WAIT_CHECK;
        // update countdown for trial tx
        if (format_mod < FORMATMOD_HT_MF)
        {
            // sample less if probability is less than 10% or greater than 95%
            if ((rc_ss->rate_stats[rc_ss->retry[0].idx].probability < RC_FRAC(10, 100)) ||
                (rc_ss->rate_stats[rc_ss->retry[0].idx].probability > RC_FRAC(95, 100)))
            {
                rc_ss->sample_wait = RC_TRIAL_PERIOD >> 1;
            }
            else
            {
                rc_ss->sample_wait = RC_TRIAL_PERIOD;
            }
        }
        else
        {
            rc_ss->sample_wait = 16 + 2 * RC_TRUNC(rc_ss->avg_ampdu_len);
        }
        // pick a random sample from the sample table
        random_rate.idx = co_rand_hword() % rc_ss->no_samples;
        // Sampling might add some overhead (RTS, no aggregation)
        // to the frame. Hence, don't use sampling for the currently
        // used rates.
        if ((random_rate.idx == rc_ss->retry[0].idx) ||
            (random_rate.idx == rc_ss->retry[1].idx) ||
            (random_rate.idx == rc_ss->retry[2].idx))
        {
            break;
        }
        rc_rand = &rc_ss->rate_stats[random_rate.idx];
        // Do not sample if the probability is already higher than 95%
        // to avoid wasting airtime
        if (rc_rand->probability > RC_FRAC(95, 100))
        {
            break;
        }
        #if NX_AMPDU_TX
        // Do not sample if the random sample is a DSSS/CCK rate and
        // next packet is part of a AMPDU
        if ((tx_ampdu == 1) &&
            (rc_get_format_mod(rc_rand->rate_config) < FORMATMOD_HT_MF))
        {
            break;
        }
        #endif

        sample_dur = rc_get_duration(rc_rand->rate_config);
        if (format_mod < FORMATMOD_HT_MF)
        {
            dur_retry_0 = rc_get_duration(rc_ss->rate_stats[rc_ss->retry[0].idx].rate_config);
            if ((sample_dur > dur_retry_0) && (rc_rand->sample_skipped < 20))
            {
                // Indirect sampling
                direct_sampling = 0;
            }
        }
        else
        {
            // Make sure that lower rates get sampled only occasionally,
            // if the link is working perfectly.
            cur_max_tp_streams = rc_get_nss(rc_ss->rate_stats[rc_ss->retry[0].idx].rate_config);
            sample_streams = rc_get_nss(rc_rand->rate_config);
            dur_retry_1 = rc_get_duration(rc_ss->rate_stats[rc_ss->retry[1].idx].rate_config);
            dur_retry_2 = rc_get_duration(rc_ss->rate_stats[rc_ss->retry[2].idx].rate_config);
            max_skipped = rc_ss->rate_stats[random_rate.idx].old_prob_available ? 32 : RC_TRUNC(rc_ss->avg_ampdu_len);
            if ((sample_dur >= dur_retry_1) &&
                (((cur_max_tp_streams - 1) < sample_streams) ||
                 (sample_dur >= dur_retry_2)))
            {
                if (rc_rand->sample_skipped < max_skipped)
                {
                    break;
                }
                rc_ss->sample_slow += 1;
                if (rc_ss->sample_slow > 2)
                {
                    if (rc_ss->sample_slow > 0xF)
                    {
                        rc_ss->sample_slow = 0xF;
                    }
                    break;
                }
            }
            // Check if direct or indirect sampling
            random_rate.tp = rc_calc_tp(rc_ss, random_rate.idx);
            if (((rc_rand->n_retry & 0xF) < 10) && (rc_rand->sample_skipped < max_skipped))
            {
                // Indirect sampling
                direct_sampling = 0;
            }
        }

        // Copy current step 1 in the trial structure
        rc_ss->max_tp_2_trial.tp = rc_ss->retry[1].tp;
        rc_ss->max_tp_2_trial.idx = rc_ss->retry[1].idx;
        // Update the restry chain with the sample rate
        if (direct_sampling)
        {
            // Direct sampling: trial - max tp - max prob
            rc_ss->info &= ~RC_TRIAL_ENTRY_MASK;
            rc_ss->retry[1].tp = rc_ss->retry[0].tp;
            rc_ss->retry[1].idx = rc_ss->retry[0].idx;
            rc_ss->retry[0].tp = random_rate.tp;
            rc_ss->retry[0].idx = random_rate.idx;
        }
        else
        {
            // Indirect sampling: max tp - trial - max prob
            rc_ss->info |= RC_TRIAL_ENTRY_MASK;
            rc_ss->retry[1].tp = random_rate.tp;
            rc_ss->retry[1].idx = random_rate.idx;

        }
        // update trial status update retry chain
        rc_ss->trial_status = RC_TRIAL_STATUS_UPD_CHAIN;
        trial_tx = 1;
    } while (0);

    if (trial_tx == 0)
    {
        rc_ss->trial_status = RC_TRIAL_STATUS_WAIT;
    }

    return trial_tx;
}


/**
 ****************************************************************************************
 * @brief Reverts the retry chain as it was before the trial transmission.
 * @param[in] rc_ss pointer to rate control station statistics structure
 ****************************************************************************************
 */
static void rc_revert_trial_tx(struct rc_sta_stats *rc_ss)
{
    uint8_t trial_entry = rc_ss->info & RC_TRIAL_ENTRY_MASK;

    if (0 == trial_entry)
    {
        rc_ss->retry[0].idx = rc_ss->retry[1].idx;
        rc_ss->retry[0].tp = rc_ss->retry[1].tp;
    }
    rc_ss->retry[1].idx = rc_ss->max_tp_2_trial.idx;
    rc_ss->retry[1].tp = rc_ss->max_tp_2_trial.tp;

    rc_ss->max_tp_2_trial.idx = 0xFF;
    rc_ss->max_tp_2_trial.tp = 0;

    // update trial status wait
    rc_ss->trial_status = RC_TRIAL_STATUS_WAIT;
}

#if NX_AMPDU_TX
/**
 ****************************************************************************************
 * @brief Set next step of the retry chain
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @return Whether the retry chain has been modified.
 ****************************************************************************************
 */
static bool rc_next_step(struct rc_sta_stats *rc_ss)
{
    bool upd = 0;

    if (rc_ss->sw_retry_step < 2)
    {
        rc_ss->sw_retry_step++;
        upd = 1;
    }

    return upd;
}
#endif

/**
 ****************************************************************************************
 * @brief Checks if the retry chain has to be updated bacause of the number of retries
 * of the AMPDU has been reached, next frame is part of an AMPDU and step 0 is a DSSS/CCK
 * rate, or next transmission is a trial transmission.
 * This function updates the aggregation TX flag in the info field, then checks if the
 * retry chain has to be updated with the following order:
 *  - time to select the next step of the retry chain because the max number of retries
 *      has been reached
 *  - time to revert the HT aggregation rate
 *  - time to revert the trial rate
 *  - time to set the trial rate
 *  - time to set the HT aggregation rate
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] tx_ampdu indicates if the next packet could be part of an AMPDU
 * @return Whether the retry chain has been modified.
 ****************************************************************************************
 */
static bool rc_check_next_retry_chain(struct rc_sta_stats *rc_ss)
{
    bool upd = 0;
    uint8_t trial_status = rc_ss->trial_status;

    // check if it's time for a trial transmission
    if (trial_status == RC_TRIAL_STATUS_CHECK)
    {
        // Get a random trial rate from the sample table
        // and check if the rate can be inserted in the retry table
        if (rc_set_trial_tx(rc_ss))
        {
            upd = 1;
        }
    }
    else if (trial_status == RC_TRIAL_STATUS_RVT_CHAIN)
    {
        // Remove the trial rate from the retry table
        rc_revert_trial_tx(rc_ss);
        upd = 1;
    }

    #if NX_AMPDU_TX
    // check if max AMPDU retry has been reached
    if ((rc_ss->info & RC_AGG_TX_MASK) &&
        (rc_ss->info & RC_SW_RETRY_REQ_MASK))
    {
        rc_ss->info &= ~RC_SW_RETRY_REQ_MASK;
        if (rc_next_step(rc_ss))
        {
            upd = 1;
        }
    }
    #endif

    return upd;
}

/**
 ****************************************************************************************
 * @brief Sort sample table from lower to higher throughput
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] cur_tp pointer to the array of current throughputs
 ****************************************************************************************
 */
void rc_sort_samples_tp(struct rc_sta_stats *rc_ss, uint32_t *cur_tp)
{
    struct rc_rate_stats rc_ss_tmp;
    uint32_t temp_tp;
    uint16_t i, j, last;
    uint32_t size = sizeof(rc_ss_tmp);

    last = rc_ss->no_samples;
    j = rc_ss->no_samples - 1;
    while (last > 0)
    {
        last = 0;
        for (i = 1; i < j; i++)
        {
            if (cur_tp[i] > cur_tp[i + 1])
            {
                memmove(&rc_ss_tmp, &rc_ss->rate_stats[i], size);
                memmove(&rc_ss->rate_stats[i], &rc_ss->rate_stats[i+1], size);
                memmove(&rc_ss->rate_stats[i+1], &rc_ss_tmp, size);
                temp_tp = cur_tp[i];
                cur_tp[i] = cur_tp[i+1];
                cur_tp[i+1] = temp_tp;
                last = i;
            }
        }
        j = last;
    }
}

/**
 ****************************************************************************************
 * @brief Calculate statistics, fills the new retry chain, updates the sample table.
 * This function updates throughput and probabilities of the sample table rates;
 * selects the 1st and 2nd maximum throughput rates, maximum probability rate and updates
 * the retry chain table.
 * Then it replaces not useful samples of the sample table with new ones.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] init indicate if the statistics are updated during the initialization
 * @return Whether the retry chain has been modified.
 ****************************************************************************************
 */
static bool rc_update_stats(struct rc_sta_stats *rc_ss, bool init)
{
    bool upd = 0;
    uint16_t i = 0;
    uint32_t cur_tp[RC_MAX_N_SAMPLE];
    struct step old_retry[RATE_CONTROL_STEPS];
    memcpy(old_retry, rc_ss->retry, sizeof(old_retry));

    if (rc_ss->ampdu_packets > 0)
    {
        // Calculate EWMA average number of MPDUs in each AMPDU frame
        rc_ss->avg_ampdu_len = ewma(rc_ss->avg_ampdu_len,
                                    RC_FRAC((uint64_t)rc_ss->ampdu_len, rc_ss->ampdu_packets),
                                    EWMA_LEVEL);
        rc_ss->ampdu_len = 0;
        rc_ss->ampdu_packets = 0;
    }

    rc_ss->sample_slow = 0;

    // Reset current TP and number of retries
    for (i = 0; i < rc_ss->no_samples; i++)
    {
        cur_tp[i] = 0;
        // Reset the number of request of SW retry before the TP calculation: this info is
        // already given by the current probability of success
        rc_ss->rate_stats[i].n_retry = 0;
        rc_ss->rate_stats[i].rate_allowed = 1;
    }

    // Calculate RC algorithm expected throughput for each sample of the stats table,
    // then fill the retry table
    if (rc_ss->fixed_rate_cfg == RC_FIXED_RATE_NOT_SET)
    {
        for (i = 0; i < rc_ss->no_samples; i++)
        {
            struct rc_rate_stats *rc_rs = &rc_ss->rate_stats[i];
            // Recalculate success probabilities and counters for a rate using EWMA
            rc_calc_prob_ewma(rc_rs);
            // Calculate expected throughput based on the average A-MPDU length, taking into
            // account the expected number of retransmissions and their expected length
            cur_tp[i] = rc_calc_tp(rc_ss, i);
        }

        // Sort sample table by TP
        rc_sort_samples_tp(rc_ss, cur_tp);
        // Update the retry chain with: max TP, 2nd max TP, max probability, lowest rate
        rc_update_retry_chain(rc_ss, cur_tp);

        if (init == 0)
        {
            if (rc_ss->no_samples < RC_MAX_N_SAMPLE)
            {
                // Reset statistics
                for (i = 0; i < rc_ss->no_samples; i++)
                {
                    rc_ss->rate_stats[i].attempts = 0;
                    rc_ss->rate_stats[i].success = 0;
                }
            }
            else
            {
                // Replace not useful samples with new ones
                rc_get_new_samples(rc_ss, cur_tp);
                // Reset lowest rate sample statistics
                rc_ss->rate_stats[0].attempts = 0;
                rc_ss->rate_stats[0].success = 0;
            }
        }
    }
    else
    {
        upd = rc_update_stats_fixed_rate(rc_ss);
    }

    // check if the retry chain is changed
    for (i = 0; i < RATE_CONTROL_STEPS; i++)
    {
        if (old_retry[i].idx != rc_ss->retry[i].idx)
        {
            upd = 1;
            break;
        }
    }

    return upd;
}

/**
 ****************************************************************************************
 * @brief Updates the retry chain.
 * This function populates steps 0 and 1 of the retry chain table with the two highest
 * throughputs.
 * Then the function searches the rate, different from the previous two, with max
 * throughput if proability is greater than 95% or with highest probability and uses its
 * index for step 2 of the retry chain.
 * Step 3 is set to the lowest rate (sample 0 of the table) and its TP value is updated.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] cur_tp pointer to the array of current throughputs
 ****************************************************************************************
 */
static void rc_update_retry_chain(struct rc_sta_stats *rc_ss, uint32_t *cur_tp)
{
    uint32_t max_prob_idx = 0;
    uint32_t max_tp = 0;
    uint32_t max_prob = 0;
    uint16_t i;
    uint16_t j;

    // Set new steps 0 and 1 of the retry table
    if (((rc_ss->info & RC_AGG_TX_MASK) == 0) && (cur_tp[0] > cur_tp[rc_ss->no_samples - 1]))
    {
        rc_ss->retry[0].idx = 0;
        rc_ss->retry[0].tp = 0;
        j = 1;
    }
    else
    {
        rc_ss->retry[0].idx = rc_ss->no_samples - 1;
        rc_ss->retry[0].tp = cur_tp[rc_ss->no_samples - 1];
        j = 2;
    }
    // Allow CCK rates only if highest TP rate is a CCK rate
    if (!is_cck_group(rc_ss->rate_stats[rc_ss->retry[0].idx].rate_config))
    {
        // CCK not allowed
        for (i = 0; i < (rc_ss->no_samples - 1); i++)
        {
            if (is_cck_group(rc_ss->rate_stats[i].rate_config))
            {
                rc_ss->rate_stats[i].rate_allowed = 0;
            }
        }
        rc_ss->rate_stats[rc_ss->retry[0].idx].rate_allowed = 1;
    }
    rc_ss->retry[1].idx = rc_ss->retry[0].idx;
    rc_ss->retry[1].tp = rc_ss->retry[0].tp;
    for (i = j; i < rc_ss->no_samples; i++)
    {
        if (rc_ss->rate_stats[rc_ss->no_samples - i].rate_allowed)
        {
            rc_ss->retry[1].idx = rc_ss->no_samples - i;
            rc_ss->retry[1].tp = cur_tp[rc_ss->no_samples - i];
            break;
        }
    }
    // find max probability
    max_prob_idx = rc_ss->retry[1].idx;
    for (i = i + 1; i < rc_ss->no_samples; i++)
    {
        if (rc_ss->rate_stats[rc_ss->no_samples - i].rate_allowed)
        {
            max_prob_idx = rc_ss->no_samples - i;
            break;
        }
    }
    max_tp = cur_tp[max_prob_idx];
    max_prob = rc_ss->rate_stats[max_prob_idx].probability;
    for (i = 0; i < rc_ss->retry[1].idx; i++)
    {
        if (rc_ss->rate_stats[i].rate_allowed == 0)
        {
            continue;
        }
        if (i == rc_ss->retry[0].idx)
        {
            continue;
        }
        if (rc_ss->rate_stats[i].probability >= RC_FRAC(95, 100))
        {
            if (cur_tp[i] >= max_tp)
            {
                max_prob_idx = i;
                max_tp = cur_tp[i];
                max_prob = rc_ss->rate_stats[i].probability;
            }
        }
        else if (rc_ss->rate_stats[i].probability >= max_prob)
        {
            max_prob_idx = i;
            max_tp = cur_tp[i];
            max_prob = rc_ss->rate_stats[i].probability;
        }
    }
    // Set new step 2 of the retry table
    rc_ss->retry[2].idx = max_prob_idx;
    rc_ss->retry[2].tp = cur_tp[max_prob_idx];
    // Set lowest rate (step 3) in the retry table
    rc_ss->retry[3].idx = 0;
    rc_ss->retry[3].tp = cur_tp[0];
}

/**
 ****************************************************************************************
 * @brief Checks if the rate configuration is allowed.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] rate_config rate configuration to be verified
 * @return Whether the rate configuration is allowed.
 ****************************************************************************************
 */
static bool rc_check_valid_rate(struct rc_sta_stats *rc_ss, uint16_t rate_config)
{
    uint8_t format = rc_get_format_mod(rate_config);
    bool rate_allowed = 1;

    switch (format)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            uint8_t r_idx = rc_get_mcs_index(rate_config);
            if ((rc_ss->rate_map_l & CO_BIT(r_idx)) == 0) // rate index not allowed
            {
                rate_allowed = 0;
            }
        } break;
        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            uint8_t nss = rc_get_nss(rate_config);
            uint8_t mcs = rc_get_mcs_index(rate_config);
            if ((rc_ss->rate_map.ht[nss] & CO_BIT(mcs)) == 0) // rate index not allowed
            {
                rate_allowed = 0;
            }
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            uint8_t bw = rc_get_bw(rate_config);
            uint8_t nss = rc_get_nss(rate_config);
            uint8_t mcs = rc_get_mcs_index(rate_config);
            // check if MCS allowed in the rate bitmap
            if (mcs > (7 + (rc_ss->rate_map.vht >> (nss << 1) & MAC_VHT_MCS_MAP_MSK)))
            {
                rate_allowed = 0;
            }
            // do not allow some missing VHT rates
            if ((mcs == 6) && (bw == BW_80MHZ) && ((nss == 3) || (nss == 6)))
            {
                rate_allowed = 0;
            }
            else if (mcs == 9)
            {
                if ((bw == BW_20MHZ) && (nss != 2) && (nss != 5))
                {
                    rate_allowed = 0;
                }
                else if ((bw == BW_80MHZ) && (nss == 5))
                {
                    rate_allowed = 0;
                }
                else if ((bw == BW_160MHZ) && (nss == 2))
                {
                    rate_allowed = 0;
                }
            }
        } break;
        #endif

        default:
            break;
    }

    return rate_allowed;
}

/**
 ****************************************************************************************
 * @brief Selects new samples for the sample table.
 * This function selects a new set of samples:
 * - random rate
 * - same rate configuration of best throughput rate, but with opposite guard interval
 *   (only if HT or VHT station)
 * - MCS index / rate index above the best throughput rate, short GI if allowed
 * - MCS index / rate index below the best throughput rate, short GI if allowed
 * - MCS index / rate index above the 2nd best throughput rate, short GI if allowed
 * - MCS index / rate index below the 2nd best throughput rate, short GI if allowed
 * Samples not available are set to 0xFF.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] new_rate_cfg_array pointer to the array of new samples rate configuration
 * @param[in] n_new_samples number of new samples to be set
 ****************************************************************************************
 */
static void rc_get_new_sample_rates(struct rc_sta_stats *rc_ss,
                                    uint16_t *new_rate_cfg_array,
                                    uint8_t n_new_samples)
{
    uint8_t i = 0;
    uint16_t tmp_rate_cfg;
    uint16_t max_tp_rate_cfg = rc_ss->rate_stats[rc_ss->retry[0].idx].rate_config;
    uint16_t max_tp_2_rate_cfg = rc_ss->rate_stats[rc_ss->retry[1].idx].rate_config;

    memset(new_rate_cfg_array, -1, n_new_samples*sizeof(*new_rate_cfg_array));

    for (i = 0; i < n_new_samples; i++)
    {
        switch (i)
        {
            // Random rate
            case 0:
            {
                new_rate_cfg_array[i] =  rc_new_random_rate(rc_ss, rc_ss->bw_max > BW_20MHZ ? (rc_ss->bw_max - 1) : rc_ss->bw_max);
            }
            break;

            // If max_tp is long guard, pick same configuration but short guard
            // If max_tp is short guard, pick same configuration but long guard
            case 1:
            {
                if ((rc_get_format_mod(max_tp_rate_cfg) >= (uint8_t)FORMATMOD_HT_MF) &&
                    (rc_ss->short_gi == 1))
                {
                    if (0 == (max_tp_rate_cfg && SHORT_GI_TX_RCX_MASK))
                    {
                        new_rate_cfg_array[i] = max_tp_rate_cfg | SHORT_GI_TX_RCX_MASK;
                    }
                    else
                    {
                        new_rate_cfg_array[i] = max_tp_rate_cfg & ~SHORT_GI_TX_RCX_MASK;
                    }
                }
            }
            break;

            // MCS index above the best throughput rate
            case 2:
            {
                tmp_rate_cfg = rc_set_next_mcs_index(rc_ss, max_tp_rate_cfg);
                if ((tmp_rate_cfg != max_tp_rate_cfg) &&
                    (rc_check_valid_rate(rc_ss, tmp_rate_cfg)))
                {
                    new_rate_cfg_array[i] = tmp_rate_cfg;
                }
            }
            break;

            // MCS index below the best throughput rate
            case 3:
            {
                tmp_rate_cfg = rc_set_previous_mcs_index(rc_ss, max_tp_rate_cfg);
                if ((tmp_rate_cfg != max_tp_rate_cfg) &&
                    (rc_check_valid_rate(rc_ss, tmp_rate_cfg)))
                {
                    new_rate_cfg_array[i] = tmp_rate_cfg;
                }
            }
            break;

            // MCS index above the 2nd best throughput rate
            case 4:
            {
                tmp_rate_cfg = rc_set_next_mcs_index(rc_ss, max_tp_2_rate_cfg);
                if ((tmp_rate_cfg != max_tp_2_rate_cfg) &&
                    (rc_check_valid_rate(rc_ss, tmp_rate_cfg)))
                {
                    new_rate_cfg_array[i] = tmp_rate_cfg;
                }
            }
            break;

            // MCS index below the 2nd best throughput rate
            case 5:
            {
                tmp_rate_cfg = rc_set_previous_mcs_index(rc_ss, max_tp_2_rate_cfg);
                if ((tmp_rate_cfg != max_tp_2_rate_cfg) &&
                    (rc_check_valid_rate(rc_ss, tmp_rate_cfg)))
                {
                    new_rate_cfg_array[i] = tmp_rate_cfg;
                }
            }
            break;

            default:
                break;
        }
    }
}

/**
 ****************************************************************************************
 * @brief Replaces not useful samples of the sample table with new ones.
 * This function discards some of the samples from the sample table and replaces them
 * with new ones. The samples are discarded if:
 * - samples are not used in the retry chain
 * - their probability is less than 50% or
 * - samples have already been skipped more than 50 times.
 * The new samples are selected in the function rc_get_new_sample_rates().
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] cur_tp pointer to the array of the calculated throughputs
 ****************************************************************************************
 */
static void rc_get_new_samples(struct rc_sta_stats *rc_ss, uint32_t *cur_tp)
{
    // select new samples and reset statistics
    uint32_t new_sample = 0;
    uint16_t new_rate_cfg_array[RC_MAX_N_SAMPLE-4];
    uint16_t i = 1;

    rc_get_new_sample_rates(rc_ss, new_rate_cfg_array, RC_MAX_N_SAMPLE-4);

    while (i < rc_ss->no_samples)
    {
        struct rc_rate_stats *rc_rs = &rc_ss->rate_stats[i];
        uint16_t tmp_rate_cfg;

        // check if the sample has to be replaced and if a new sample in the list is available
        if (((rc_rs->probability < RC_FRAC(50, 100) || (rc_rs->sample_skipped > 10/*50*/))) &&
            (i != rc_ss->retry[0].idx) &&
            (i != rc_ss->retry[1].idx) &&
            (i != rc_ss->retry[2].idx) &&
            (new_sample < (RC_MAX_N_SAMPLE-4)))
        {
            tmp_rate_cfg = new_rate_cfg_array[new_sample];
            if ((tmp_rate_cfg != (uint16_t)-1) && (!rc_check_rate_duplicated(rc_ss, tmp_rate_cfg)))
            {
                rc_rs->rate_config = tmp_rate_cfg;
                // reset new sample probability
                rc_rs->probability = 0;
                rc_rs->old_prob_available = 0;
                #if NX_DEBUG
                // sanity check
                rc_check_rate_config(rc_ss, i);
                #endif
                // next sample in the table
                i += 1;
            }
            new_sample += 1;
        }
        else
        {
            // go to next sample in the table
            i += 1;
        }
        // reset statistics
        rc_rs->attempts = 0;
        rc_rs->success = 0;
    }
}

/**
 ****************************************************************************************
 * @brief Gets the rate configuration of the lowest allowed rate for the station.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @return Rate configuration of the lowest allowed rate
 ****************************************************************************************
 */
static uint16_t rc_get_lowest_rate_config(struct rc_sta_stats *rc_ss)
{
    uint16_t rate_config = 0;

    switch (rc_ss->format_mod)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            if (rc_ss->r_idx_min == 0) // CCK
            {
                // CCK group: lowest rate has preamble type long, rate index 0, BW_20MHZ
                rate_config = (FORMATMOD_NON_HT << FORMAT_MOD_TX_RCX_OFT) |
                        (1 << PRE_TYPE_TX_RCX_OFT);
            }
            else
            {
                // NON-HT: lowest rate has lowest allowed rate index, BW_20MHZ
                rate_config = (FORMATMOD_NON_HT << FORMAT_MOD_TX_RCX_OFT) |
                        (rc_ss->r_idx_min << MCS_INDEX_TX_RCX_OFT);
            }
        } break;
        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            if (rc_ss->r_idx_min == 0) // CCK
            {
                // CCK group: lowest rate has preamble type long, rate index 0, BW_20MHZ
                rate_config = (FORMATMOD_NON_HT << FORMAT_MOD_TX_RCX_OFT) |
                        (1 << PRE_TYPE_TX_RCX_OFT);
            }
            else
            {
                // HT group: lowest rate is HT, MCS0, BW_20MHZ
                rate_config = (rc_ss->format_mod << FORMAT_MOD_TX_RCX_OFT);
            }
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            // VHT only: lowest rate is MCS0, BW_20MHZ
            rate_config = (rc_ss->format_mod << FORMAT_MOD_TX_RCX_OFT);
        } break;
        #endif

        default:
            break;
    }

    return rate_config;
}

static uint16_t rc_get_initial_rate_config(struct rc_sta_stats *rc_ss)
{
    uint16_t rate_config = 0;

    switch (rc_ss->format_mod)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            rate_config = (rc_ss->format_mod << FORMAT_MOD_TX_RCX_OFT) |
                    (rc_ss->p_type << PRE_TYPE_TX_RCX_OFT) |
                    (rc_ss->r_idx_max << MCS_INDEX_TX_RCX_OFT);
        } break;
        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            uint8_t mcs = 31 - co_clz((uint32_t)rc_ss->rate_map.ht[rc_ss->no_ss]);
            rate_config = (rc_ss->format_mod << FORMAT_MOD_TX_RCX_OFT) |
                    (rc_ss->short_gi << SHORT_GI_TX_RCX_OFT) |
                    (rc_ss->bw_max << BW_TX_RCX_OFT) |
                    (rc_ss->no_ss << HT_NSS_OFT) |
                    (mcs << MCS_INDEX_TX_RCX_OFT);
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            rate_config = (rc_ss->format_mod << FORMAT_MOD_TX_RCX_OFT) |
                    (rc_ss->short_gi << SHORT_GI_TX_RCX_OFT) |
                    (rc_ss->bw_max << BW_TX_RCX_OFT) |
                    (rc_ss->format_mod == FORMATMOD_VHT ? rc_ss->no_ss << VHT_NSS_OFT : rc_ss->no_ss << HT_NSS_OFT) |
                    (7 << MCS_INDEX_TX_RCX_OFT);
        } break;
        #endif

        default:
            break;
    }

    return rate_config;
}


/**
 ****************************************************************************************
 * @brief Gets the rate configuration of the highest allowed rate for the station.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @return Rate configuration of the highest allowed rate
 ****************************************************************************************
 */
uint16_t rc_get_highest_rate_config(struct rc_sta_stats *rc_ss)
{
    uint16_t rate_config = 0;

    if (rc_ss->format_mod >= FORMATMOD_HT_MF)
    {
        rate_config = (rc_ss->format_mod << FORMAT_MOD_TX_RCX_OFT) |
                      (rc_ss->short_gi << SHORT_GI_TX_RCX_OFT) |
                      (rc_ss->bw_max << BW_TX_RCX_OFT) |
                      #if NX_VHT
                      (rc_ss->format_mod == FORMATMOD_VHT ? rc_ss->no_ss << VHT_NSS_OFT : rc_ss->no_ss << HT_NSS_OFT) |
                      #else
                      (rc_ss->no_ss << HT_NSS_OFT) |
                      #endif
                      (rc_ss->mcs_max << MCS_INDEX_TX_RCX_OFT);
    }
    else
    {
        rate_config = (rc_ss->format_mod << FORMAT_MOD_TX_RCX_OFT) |
                      (rc_ss->p_type << PRE_TYPE_TX_RCX_OFT) |
                      (rc_ss->r_idx_max << MCS_INDEX_TX_RCX_OFT);
    }

    return rate_config;
}

/**
 ****************************************************************************************
 * @brief Gets the number of samples to be used by the RC algorithm.
 * This function calculates the number of samples to be used for the sample table of the
 * RC algorithm, depending on the number of available rates for the station.
 * The maximum number of samples is limited to RC_MAX_N_SAMPLE.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @return Number of samples to be used
 ****************************************************************************************
 */
static uint16_t rc_get_num_samples(struct rc_sta_stats *rc_ss)
{
    uint32_t i, j;
    uint16_t n_sample = 0;

    switch (rc_ss->format_mod)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            uint16_t r_map = rc_ss->rate_map_l;
            n_sample += (r_map & 1); // HW_RATE_1MBPS: only long preamble
            for (i = HW_RATE_2MBPS; i <= HW_RATE_11MBPS; i++)
            {
                n_sample += (((r_map >> i) & 1) << (1-rc_ss->p_type));
            }
            for (i = HW_RATE_6MBPS; i <= HW_RATE_54MBPS; i++)
            {
                n_sample += ((r_map >> i) & 1);
            }
        } break;

        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        {
            uint32_t mult = 1 << rc_ss->short_gi;
            uint8_t mcs_map = rc_ss->rate_map.ht[0];
            for (j = 0; j < 8; j++)  // MCS 0-7
            {
                n_sample += ((mcs_map & 1) * mult);
                mcs_map = mcs_map >> 1;
            }
            // cck rates
            uint16_t r_map = rc_ss->rate_map_l;
            n_sample += (r_map & 1); // HW_RATE_1MBPS: only long preamble
            for (i = HW_RATE_2MBPS; i <= HW_RATE_11MBPS; i++)
            {
                n_sample += (((r_map >> i) & 1) << (1-rc_ss->p_type));
            }
        } break;

        #if NX_VHT
        case FORMATMOD_VHT:
        {
            // VHT rates
            uint32_t mult = 1 << rc_ss->short_gi;
            uint8_t n_mcs;
            switch (rc_ss->rate_map.vht & MAC_VHT_MCS_MAP_MSK)
            {
                case 0:
                    n_mcs = 8;
                    break;
                case 1:
                case 2:
                    n_mcs = 9;
                    break;
                default:
                    n_mcs = 8;
                    break;
            }
            n_sample = n_mcs * mult;
        } break;
        #endif

        default:
            break;
    }

    if (n_sample > RC_MAX_N_SAMPLE)
    {
        n_sample = RC_MAX_N_SAMPLE;
    }

    return n_sample;
}

/**
 ****************************************************************************************
 * @brief RC algorithm retry chain and sample table initialization.
 * This function initializes the rates of the sample table:
 *  - entry 0 is the lowest rate
 *  - entry 1 is the highest rate
 *  - entry 2-n are chosen randomly between the remaining rates.
 * The retry chain is also initialized:
 *  - step 0: set to the highest rate (entry 1 of sample table)
 *  - step 3: set to the lowest rate (entry 0 of sample table)
 *  - step 1 and 2: set randomly (random entry between 2 and n)
 * @param[in] sta_idx index of the station
 ****************************************************************************************
 */
static void rc_init_rates(uint8_t sta_idx)
{
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct sta_pol_tbl_cntl *pt = &sta_entry->pol_tbl;
    struct rc_sta_stats *rc_ss = pt->sta_stats;
    ASSERT_ERR(rc_ss != NULL);
    uint16_t i;

    // Reset rate configuration
    for (i = 0; i < rc_ss->no_samples; i++)
    {
        rc_ss->rate_stats[i].rate_config = 0xFFFF;
    }

    // Set base rate configuration (lowest allowed rate) as sample number 0
    rc_ss->rate_stats[0].rate_config = rc_get_lowest_rate_config(rc_ss);
    // Set last sample: highest rate index for NON-HT, MCS7 or highest allowed for HT,
    // MCS7 for VHT
    rc_ss->rate_stats[rc_ss->no_samples - 1].rate_config = rc_get_initial_rate_config(rc_ss);
    // Set initial sample rates (do not use duplicated samples) - sample 0 is always the lowest rate
    for (i = 1; i < (rc_ss->no_samples - 1);)
    {
        // Get random rates for the other samples
        uint16_t tmp_rate = rc_new_random_rate(rc_ss, rc_ss->bw_max > BW_20MHZ ? (rc_ss->bw_max - 1) : rc_ss->bw_max);
        // do not use duplicated samples
        if (!rc_check_rate_duplicated(rc_ss, tmp_rate))
        {
            rc_ss->rate_stats[i].rate_config = tmp_rate;
            i++;
        }
    }
    #if NX_DEBUG
    rc_check_stats_rate_config(rc_ss);
    #endif

    // set retry chain steps 0, 1 and 2
    for (i = 0; i < (RATE_CONTROL_STEPS-1); i++)
    {
        rc_ss->retry[i].tp = 0;
        rc_ss->retry[i].idx = (rc_ss->no_samples - 1) - i;
    }
    // Set lowest rate (entry 0 of the sample table) as last step of the retry chain
    rc_ss->retry[RATE_CONTROL_STEPS-1].idx = 0;
    rc_ss->retry[RATE_CONTROL_STEPS-1].tp = 0;
    // Initialize average AMPDU length
    rc_ss->avg_ampdu_len = RC_FRAC(1, 1);
    // Disable fixed rate
    rc_ss->fixed_rate_cfg = RC_FIXED_RATE_NOT_SET;
    // Reset RC flags
    rc_ss->info = 0;
    // Initialize RC algorithm table
    rc_update_stats(rc_ss, 1);
    // Initialize trial period
    rc_ss->sample_wait =  5;
}

/**
 ****************************************************************************************
 * @brief Sets the maximum AMSDU length, depending on the number of spatial streams.
 * @param[in] rc_ss pointer to rate control station statistics structure
 ****************************************************************************************
 */
static void rc_set_max_amsdu_len(struct rc_sta_stats *rc_ss)
{
    uint8_t step_0_idx = rc_ss->retry[rc_ss->sw_retry_step].idx;
    uint16_t rate_config = rc_ss->rate_stats[step_0_idx].rate_config;
    uint16_t max_amsdu_size = 0;

    if (rc_get_mcs_index(rate_config) == rc_ss->mcs_max)
    {
        // set the max size of AMSDU depending on the number of spatial streams:
        // - 1 SS: max size 2
        // - 2 SS: max size 4
        // - 3-8 SS: max size 6
        if (rc_ss->no_ss == 0)
        {
            max_amsdu_size = 2;
        }
        else if (rc_ss->no_ss == 1)
        {
            max_amsdu_size = 4;
        }
        else
        {
            max_amsdu_size = 6;
        }
    }
    else
    {
        max_amsdu_size = 0;
    }
    rc_ss->curr_amsdu_len = co_min(rc_ss->max_amsdu_len, max_amsdu_size*1550);
}

/**
 ****************************************************************************************
 * @brief Sets/resets the aggregation flag
 * This function updates the value og the aggregation flag depending on the rates set in
 * the retry chain. The aggregation is disabled only for legacy rates, since it's not
 * supported. The aggregation is disabled if:
 * - one the steps 0-2 of the retry chain is a legacy rate
 * - first step of the retry chain is HT/VHT, 1SS, with low MCS and low probability (<10%)
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @param[in] tx_ampdu pointer to the aggregation flag
 ****************************************************************************************
 */
static void rc_set_aggregation_flag(struct rc_sta_stats *rc_ss, bool *tx_ampdu)
{
    bool agg_enabled = 1;

    // check if aggregation has to be disabled
    do
    {
        // disable aggregation only for stations allowing legacy rates
        if (rc_ss->r_idx_min > HW_RATE_54MBPS)
        {
            break;
        }
        // disable aggregation if one of the steps 0-2 is a DSSS/CCK rate
        uint16_t rate_0 = rc_ss->rate_stats[rc_ss->retry[0].idx].rate_config;
        uint16_t rate_1 = rc_ss->rate_stats[rc_ss->retry[1].idx].rate_config;
        uint16_t rate_2 = rc_ss->rate_stats[rc_ss->retry[2].idx].rate_config;
        if (rc_get_format_mod(rate_0) < FORMATMOD_HT_MF ||
            rc_get_format_mod(rate_1) < FORMATMOD_HT_MF ||
            rc_get_format_mod(rate_2) < FORMATMOD_HT_MF)
        {
            agg_enabled = 0;
            break;
        }
        // If fixed rate is set, do not disable agregation because of low probability
        if (rc_ss->info & RC_FIX_RATE_EN_MASK)
        {
            break;
        }
        // disable aggregation if first step:
        // 1 SS, probability of success < 10%. MCS0 or MCS1
        uint8_t step_0 = rc_ss->sw_retry_step;
        uint16_t step_0_idx = rc_ss->retry[step_0].idx;
        struct rc_rate_stats *stats_step_0 = &rc_ss->rate_stats[step_0_idx];
        uint8_t step_0_mcs = rc_get_mcs_index(stats_step_0->rate_config);
        uint8_t step_0_nss = rc_get_nss(stats_step_0->rate_config);
        if (((step_0_mcs <= 2) &&
            (step_0_nss == 0) &&
            (stats_step_0->probability < RC_FRAC(10, 100))) ||
            (stats_step_0->probability < RC_FRAC(1, 100)))
        {
            agg_enabled = 0;
        }
    } while (0);

    rc_ss->info &= ~RC_AGG_ALLOWED_MASK;
    rc_ss->info |= (agg_enabled << RC_AGG_ALLOWED_OFT);

    *tx_ampdu = agg_enabled;
}

/**
 ****************************************************************************************
 * @brief Update the retry chain and statistics when a fix rate is set.
 * This function inserts the fix rate in the sample table and updates the retry chain, if
 * a fixed rate request is pending.
 * Otherwise, it updates the statistics for the fixed rate.
 * @param[in] rc_ss pointer to rate control station statistics structure
 * @return Whether the retry chain has been modified.
 ****************************************************************************************
 */
static bool rc_update_stats_fixed_rate(struct rc_sta_stats *rc_ss)
{
    bool update_requested = 0;
    uint16_t i = 0;
    uint8_t step_0_idx = rc_ss->retry[0].idx;

    // Fixed rate request
    if (rc_ss->info & RC_FIX_RATE_REQ_MASK)
    {
        uint8_t idx = 0;
        // Check if selected rate is in the sample table
        while (idx < rc_ss->no_samples)
        {
            if (rc_ss->fixed_rate_cfg == rc_ss->rate_stats[idx].rate_config)
            {
                break;
            }
            idx++;
        }
        // If not present in the sample table, insert it
        if (idx == rc_ss->no_samples)
        {
            idx = rc_ss->no_samples - 1;
            rc_ss->rate_stats[idx].rate_config = rc_ss->fixed_rate_cfg;
            rc_ss->rate_stats[idx].probability = 0;
        }
        // Set retry chain
        for (i = 0; i < 3; i++)
        {
            rc_ss->retry[i].idx = idx;
            rc_ss->retry[i].tp = 0;
        }
        // Reset statistics
        for (i = 0; i < rc_ss->no_samples; i++)
        {
            rc_ss->rate_stats[i].attempts = 0;
            rc_ss->rate_stats[i].success = 0;
        }
        rc_ss->info &= ~RC_FIX_RATE_STATUS_MASK;
        rc_ss->info |= RC_FIX_RATE_EN_MASK;
        update_requested = 1;
    }
    // Fixed rate is set: update statistics and reset counters
    else
    {
        struct rc_rate_stats *rc_rs = &rc_ss->rate_stats[step_0_idx];
        // Calculate success probability
        rc_calc_prob_ewma(rc_rs);
        // Reset statistics
        rc_ss->rate_stats[step_0_idx].attempts = 0;
        rc_ss->rate_stats[step_0_idx].success = 0;
        rc_ss->rate_stats[0].attempts = 0;
        rc_ss->rate_stats[0].success = 0;
    }

    return update_requested;
}

/**
 * PUBLIC FUNCTIONS DEFINITION
 ****************************************************************************************
 */

/**
 ****************************************************************************************
 * @brief RC algorithm counters update.
 * This function updates the number of attempts and success for each rate of the retry
 * chain.
 * @param[in] sta_idx index of the station
 * @param[in] attempts number of attempts with the current retry chain
 * @param[in] failures number of failures with the current retry chain
 * @param[in] isampdu '1' if the transmitted message is an AMPDU, '0' otherwise
 ****************************************************************************************
 */
void rc_update_counters(uint8_t sta_idx, uint32_t attempts, uint32_t failures, bool tx_ampdu, bool retry_required)
{
    if (sta_idx >= NX_REMOTE_STA_MAX)
        return;
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    if (sta_entry->inst_nbr == 0xFF)
        return;
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;
    struct rc_sta_stats *rc_ss = rc->sta_stats;
    ASSERT_ERR(rc_ss != NULL);

    // update the number of AMPDUs transmitted
    rc_ss->ampdu_packets += 1;

    #if NX_AMPDU_TX
    if (tx_ampdu)
    {
        // update the number of MPDUs transmitted (success and failures)
        rc_ss->ampdu_len += attempts;
        // Set number of attempts and success for first rate of the retry chain
        uint16_t step_0_idx = rc_ss->retry[rc_ss->sw_retry_step].idx;
        struct rc_rate_stats *rc_rs = &rc_ss->rate_stats[step_0_idx];
        rc_rs->attempts += attempts;
        rc_rs->success += (attempts - failures);
        ASSERT_ERR(rc_rs->attempts >= rc_rs->success);
        // update number of retry requested with this rate
        rc_rs->n_retry += retry_required;
    }
    else
    #endif
    {
        // update the number of MPDUs transmitted (success and failures)
        rc_ss->ampdu_len += 1;
        // update the number of attempts and success for each rate of the retry chain
        for (uint32_t i = 0; (i < RATE_CONTROL_STEPS) && (attempts > 0); i++)
        {
            uint16_t rate_idx = rc_ss->retry[i].idx;
            struct rc_rate_stats *rc_rs = &rc_ss->rate_stats[rate_idx];
            if (failures >= RC_MAX_NUM_RETRY)
            {
                rc_rs->attempts += RC_MAX_NUM_RETRY;
                attempts -= RC_MAX_NUM_RETRY;
                failures -= RC_MAX_NUM_RETRY;
            }
            else
            {
                rc_rs->attempts += attempts;
                rc_rs->success += (attempts - failures);
                attempts = 0;
                failures = 0;
            }
            ASSERT_ERR(rc_rs->attempts >= rc_rs->success);
        }
    }
    // Update trial TX countdown and check if it's time for a trial tx
    if (rc_ss->trial_status == RC_TRIAL_STATUS_WAIT)
    {
        if (rc_ss->sample_wait > 0)
        {
            rc_ss->sample_wait -= 1;
        }
        else
        {
            rc_ss->trial_status = RC_TRIAL_STATUS_CHECK;
        }
    }
    else if ((rc_ss->trial_status == RC_TRIAL_STATUS_UPD_CHAIN) &&
             ((rc->upd_field & CO_BIT(STA_MGMT_POL_UPD_RATE)) == 0))
    {
        rc_ss->trial_status = RC_TRIAL_STATUS_RVT_CHAIN;
    }
    #if NX_AMPDU_TX
    if (tx_ampdu)
    {
        // check if it's time to select next step of the chain
        struct rc_rate_stats *stats_step_0 = &rc_ss->rate_stats[rc_ss->retry[rc_ss->sw_retry_step].idx];
        uint8_t step_0_mcs = rc_get_mcs_index(stats_step_0->rate_config);
        if ((stats_step_0->n_retry & 0xF) >= (step_0_mcs + 1))
        {
            stats_step_0->n_retry = 0x10;
            rc_ss->info |= RC_SW_RETRY_REQ_MASK;
        }
    }
    #endif
}

/**
 ****************************************************************************************
 * @brief RC algorithm check.
 * This function checks if it's time to update statistics and run RC algorithm
 * (and runs it) and if it's time to transmit using a trial rate.
 * This function sets the pointer to the statics structure for the station, reads the
 * capability informations about the station (format, max bw, max spatial streams, max
 * MCS, guard interval, min and max rate index, preamble type) and sets them into the RC
 * statistics structure, initializes the rate control algorithm.
 * @param[in] sta_idx index of the station
 * @param[in] tx_ampdu whether the packet could be part of an AMPDU
 ****************************************************************************************
 */
uint8_t rc_check(uint8_t sta_idx, bool *tx_ampdu)
{
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;
    struct rc_sta_stats *rc_ss = rc->sta_stats;
    bool update = 0;

    ASSERT_ERR(rc_ss != NULL);
    if (sta_idx >= NX_REMOTE_STA_MAX)
        return 0;
    
    #if NX_AMPDU_TX
    // update ampdu tx
    if (((rc_ss->info & RC_AGG_ALLOWED_MASK) == 0) || (*tx_ampdu == 0))
    {
        rc_ss->info &= ~RC_AGG_TX_MASK;
    }
    else
    {
        rc_ss->info |= RC_AGG_TX_MASK;
    }
    #endif

    // Check if it is time to run the RC algorithm
    if (hal_machw_time_past(rc_ss->last_rc_time + RC_PERIOD_TOUT))
    {
        // Calculate statistics and update retry chain
        update |= rc_update_stats(rc_ss, 0);
        // Reset trial status
        rc_ss->trial_status = RC_TRIAL_STATUS_WAIT;
        // Reset SW retry request flag
        rc_ss->info &= ~RC_SW_RETRY_REQ_MASK;
        // Reset SW retry step
        rc_ss->sw_retry_step = 0;
        // Update RC timer
        rc_ss->last_rc_time = hal_machw_time();
    }

    // check if the retry chain has to be updated due to a trial tx or SW retry
    update |= rc_check_next_retry_chain(rc_ss);

    if (update)
    {
        #if NX_AMSDU_TX
        // Set maximum AMSDU len
        rc_set_max_amsdu_len(rc_ss);
        #endif
        #if NX_AMPDU_TX
        rc_set_aggregation_flag(rc_ss, tx_ampdu);
        #endif
        // Keep in mind we have to update the rate
        rc->upd_field |= CO_BIT(STA_MGMT_POL_UPD_RATE);
    }

    return rc_ss->sw_retry_step;
}

/**
 ****************************************************************************************
 * @brief RC algorithm initialization.
 * This function sets the pointer to the statics structure for the station, reads the
 * capability informations about the station (format, max bw, max spatial streams, max
 * MCS, guard interval, min and max rate index, preamble type) and sets them into the RC
 * statistics structure, initializes the rate control algorithm.
 * @param[in] sta_entry pointer to STA Info Table
 ****************************************************************************************
 */
void rc_init(struct sta_info_tag *sta_entry)
{
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;
    struct tx_policy_tbl *pol = &rc->buf_ctrl[0]->policy_tbl;
    struct tx_policy_tbl *pol1 = &rc->buf_ctrl[1]->policy_tbl;
    uint8_t hw_key_idx = MM_STA_TO_KEY(sta_entry->staid);

    // Set pointer to station RC statistics
    ASSERT_ERR(sta_entry->staid < NX_REMOTE_STA_MAX);
    rc->sta_stats = &sta_stats[sta_entry->staid];
    ASSERT_ERR(rc->sta_stats != NULL);

    struct rc_sta_stats *rc_ss = rc->sta_stats;
    uint32_t phy_cntrl_info1 = phy_get_ntx() << NX_TX_PT_OFT;
    int i;

    memset(rc_ss, 0, sizeof(rc_ss));

    // Check if the peer is a 11n or 11ac device
    if (sta_entry->info.capa_flags & STA_HT_CAPA)
    {
        struct mac_htcapability *htcap = &sta_entry->info.ht_cap;

        // set the bitmap for legacy rates
        struct mac_rates ratefield;
        me_rate_bitfield_legacy_build(&ratefield, &sta_entry->info.rate_set, 0);
        rc_ss->rate_map_l = ratefield.legacy;
        #if NX_VHT
        if (sta_entry->info.capa_flags & STA_VHT_CAPA)
        {
            struct mac_vhtcapability *vhtcap = &sta_entry->info.vht_cap;
            struct mac_vhtcapability *vhtcaploc = &me_env.vht_cap;
            // VHT configurations
            rc_ss->format_mod = FORMATMOD_VHT;
            rc_ss->no_ss = co_min(me_11ac_nss_max(vhtcap->rx_mcs_map),
                                  me_11ac_nss_max(vhtcaploc->tx_mcs_map));
            ASSERT_ERR(rc_ss->no_ss < 8);
            // Set the bitmap for VHT rates
            rc_ss->rate_map.vht = me_rate_bitfield_vht_build(vhtcap->rx_mcs_map,
                                                             vhtcaploc->tx_mcs_map);
            rc_ss->mcs_max = co_min(me_11ac_mcs_max(vhtcap->rx_mcs_map),
                                   me_11ac_mcs_max(vhtcaploc->tx_mcs_map));
            ASSERT_ERR(rc_ss->mcs_max < 10);
            // legacy configurations
            rc_ss->p_type = 0;
            // remove legacy OFDM rates from the map
            rc_ss->rate_map_l = 0;
            rc_ss->r_idx_min = me_legacy_ridx_min(rc_ss->rate_map_l);
            rc_ss->r_idx_max = me_legacy_ridx_max(rc_ss->rate_map_l);
            if ((vhtcaploc->vht_capa_info & MAC_VHTCAPA_RXLDPC) &&
                (vhtcap->vht_capa_info & MAC_VHTCAPA_RXLDPC))
            {
                phy_cntrl_info1 |= FEC_CODING_PT_BIT;
            }

            uint16_t max_mpdu_head_tail = MAC_LONG_QOS_HTC_MAC_HDR_LEN + MAC_FCS_LEN;
            #if RW_WAPI_EN
            max_mpdu_head_tail += WPI_IV_LEN + WPI_MIC_LEN;
            #else
            max_mpdu_head_tail += IV_LEN + EIV_LEN + MIC_LEN + ICV_LEN;
            #endif
            switch (vhtcap->vht_capa_info & MAC_VHTCAPA_MAX_MPDU_LENGTH_MSK)
            {
                case MAC_VHTCAPA_MAX_MPDU_LENGTH_3895:
                    rc_ss->max_amsdu_len = 3895 - max_mpdu_head_tail;
                case MAC_VHTCAPA_MAX_MPDU_LENGTH_7991:
                    rc_ss->max_amsdu_len = 7991 - max_mpdu_head_tail;
                case MAC_VHTCAPA_MAX_MPDU_LENGTH_11454:
                    rc_ss->max_amsdu_len = 11454 - max_mpdu_head_tail;
            }
        }
        else
        #endif
        {
            struct mac_htcapability *htcaploc = &me_env.ht_cap;
            // HT configurations
            rc_ss->format_mod = FORMATMOD_HT_MF;
            rc_ss->no_ss = co_min(me_11n_nss_max(htcap->mcs_rate),
                                  me_11n_nss_max(htcaploc->mcs_rate));
            ASSERT_ERR(rc_ss->no_ss <= 3);
            // Set the bitmap for HT rates
            memcpy(rc_ss->rate_map.ht, htcap->mcs_rate, sizeof(rc_ss->rate_map.ht));
            rc_ss->mcs_max = 7;
            // legacy configurations
            // remove legacy OFDM rates from the map
            rc_ss->rate_map_l = rc_ss->rate_map_l & ~0xFF0;
            rc_ss->r_idx_min = me_legacy_ridx_min(rc_ss->rate_map_l);
            ASSERT_ERR((rc_ss->r_idx_min <= HW_RATE_11MBPS) || (rc_ss->r_idx_min == MAC_RATESET_LEN));
            rc_ss->r_idx_max = me_legacy_ridx_max(rc_ss->rate_map_l);
            ASSERT_ERR((rc_ss->r_idx_max <= HW_RATE_11MBPS) || (rc_ss->r_idx_max == MAC_RATESET_LEN));
            rc_ss->p_type = (rc->ppdu_tx_cfg & PRE_TYPE_TX_RCX_MASK) >> PRE_TYPE_TX_RCX_OFT;

            if ((htcaploc->ht_capa_info & MAC_HTCAPA_LDPC) &&
                (htcap->ht_capa_info & MAC_HTCAPA_LDPC))
            {
                phy_cntrl_info1 |= FEC_CODING_PT_BIT;
            }
            if (htcap->ht_capa_info & MAC_HTCAPA_AMSDU)
            {
                #if NX_AMPDU_TX
                rc_ss->max_amsdu_len = 4095;
                #else
                rc_ss->max_amsdu_len = 7935;
                #endif
            }
            else
            {
                rc_ss->max_amsdu_len = 3839;
            }
        }
        // bandwidth
        rc_ss->bw_max = sta_entry->info.bw_cur;
        ASSERT_ERR(rc_ss->bw_max <= BW_160MHZ);
        // guard interval
        switch(sta_entry->info.bw_cur)
        {
            case BW_20MHZ:
                if (htcap->ht_capa_info & MAC_HTCAPA_SHORTGI_20)
                    rc_ss->short_gi = 1;
                break;
            case BW_40MHZ:
                if (htcap->ht_capa_info & MAC_HTCAPA_SHORTGI_40)
                    rc_ss->short_gi = 1;
                break;
            #if NX_VHT
            case BW_80MHZ:
                if (sta_entry->info.vht_cap.vht_capa_info & MAC_VHTCAPA_SHORT_GI_80)
                    rc_ss->short_gi = 1;
                break;
            case BW_160MHZ:
                if (sta_entry->info.vht_cap.vht_capa_info & MAC_VHTCAPA_SHORT_GI_160)
                    rc_ss->short_gi = 1;
                break;
            #endif
            default:
                break;
        }
    }
    else
    {
        // Set bitmap for legacy rates
        struct mac_rates ratefield;
        me_rate_bitfield_legacy_build(&ratefield, &sta_entry->info.rate_set, 0);
        rc_ss->rate_map_l = ratefield.legacy;
        rc_ss->r_idx_min = me_legacy_ridx_min(rc_ss->rate_map_l);
        ASSERT_ERR(rc_ss->r_idx_min < MAC_RATESET_LEN);
        rc_ss->r_idx_max = me_legacy_ridx_max(rc_ss->rate_map_l);
        ASSERT_ERR(rc_ss->r_idx_max < MAC_RATESET_LEN);
        rc_ss->mcs_max = 0xFF;
        rc_ss->bw_max = sta_entry->info.bw_cur;
        ASSERT_ERR(rc_ss->bw_max == BW_20MHZ);
        // preamble type: 0=short and long, 1=only long
        rc_ss->p_type = (rc->ppdu_tx_cfg & PRE_TYPE_TX_RCX_MASK) >> PRE_TYPE_TX_RCX_OFT;
    }
    // Set number of samples for statistics
    rc_ss->no_samples = rc_get_num_samples(rc_ss);
    ASSERT_ERR(rc_ss->no_samples >= 2);
    ASSERT_ERR(rc_ss->no_samples <= RC_MAX_N_SAMPLE);

    os_printf("%s: station_id=%i format_mod=%i pre_type=%i short_gi=%i max_bw=%i\n",
            __func__, sta_entry->staid, rc_ss->format_mod, rc_ss->p_type, rc_ss->short_gi, rc_ss->bw_max);
    os_printf("%s: nss_max=%i mcs_max=%i r_idx_min=%i r_idx_max=%i no_samples=%i\n",
            __func__, rc_ss->no_ss, rc_ss->mcs_max, rc_ss->r_idx_min, rc_ss->r_idx_max, rc_ss->no_samples);

    // Init RC algorithm
    rc_init_rates(sta_entry->staid);

    // Set current rate configuration
    for (i = 0; i < RATE_CONTROL_STEPS; i++)
    {
        uint8_t idx = rc_ss->retry[i].idx;
        pol->ratecntrlinfo[i] =
            (RC_MAX_NUM_RETRY << N_RETRY_RCX_OFT) | rc_ss->rate_stats[idx].rate_config;
    }

    #if NX_AMSDU_TX
    // Set maximum AMSDU len
    rc_set_max_amsdu_len(rc_ss);
    #endif
    #if NX_AMPDU_TX
    rc_set_aggregation_flag(rc_ss, NULL);
    #endif

    // Init last_rc_time
    rc_ss->last_rc_time = hal_machw_time();

    rc->buf_ctrl_idx = 0;
    pol->maccntrlinfo1 = hw_key_idx << KEYSRAM_INDEX_RA_OFT;
    pol1->maccntrlinfo1 = pol->maccntrlinfo1;

    pol->phycntrlinfo1 = phy_cntrl_info1;
    pol1->phycntrlinfo1 = pol->phycntrlinfo1;

    pol->phycntrlinfo2  = TX_NTX_2_ANTENNA_SET(phy_get_ntx());
    pol1->phycntrlinfo2 = pol->phycntrlinfo2;

    // Set update rate flag
    rc->upd_field |= CO_BIT(STA_MGMT_POL_UPD_RATE);
}


/**
 ****************************************************************************************
 * @brief Gets the TX duration of a packet of 1200 bytes.
 * @param[in] rate_config rate configuration to be updated
 * @return Transmission duration in nsecs.
 ****************************************************************************************
 */
uint32_t rc_get_duration(uint16_t rate_config)
{
    uint32_t duration = 0;
    uint32_t idx;
    uint8_t format_mod = rc_get_format_mod(rate_config);
    uint8_t mcs = rc_get_mcs_index(rate_config);

    switch (format_mod)
    {
        case FORMATMOD_NON_HT:
        case FORMATMOD_NON_HT_DUP_OFDM:
        {
            if (mcs < HW_RATE_6MBPS)
            {
                uint8_t pre_type = rc_get_pre_type(rate_config);
                idx = (uint32_t)(mcs << 1) | pre_type;
                duration = rc_duration_cck[idx];
            }
            else
            {
                idx = (mcs - HW_RATE_6MBPS);
                duration = rc_duration_non_ht[idx];
            }
        } break;

        case FORMATMOD_HT_MF:
        case FORMATMOD_HT_GF:
        #if NX_VHT
        case FORMATMOD_VHT:
        #endif
        {
            uint8_t bw = rc_get_bw(rate_config);
            uint8_t gi = rc_get_gi(rate_config);
            idx = (mcs << 3) | (bw << 1) | gi;
            duration = rc_duration_ht_ampdu[idx] / (rc_get_nss(rate_config) + 1);
        } break;

        default:
            break;
    }

    return duration;
}

/**
 ****************************************************************************************
 * @brief Updates the maximum bandwidth allowed by the station
 * @param[in] sta_idx index of the station
 * @param[in] bw_max maximum bandwith
 ****************************************************************************************
 */
void rc_update_bw_nss_max(uint8_t sta_idx, uint8_t bw_max, uint8_t nss_max)
{
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;
    struct rc_sta_stats *rc_ss = rc->sta_stats;
    uint16_t i = 0;
    uint32_t cur_tp[RC_MAX_N_SAMPLE];
    ASSERT_ERR(rc_ss != NULL);

    if ((bw_max == rc_ss->bw_max) && (nss_max == rc_ss->no_ss))
        return;

    rc_ss->bw_max = bw_max;
    ASSERT_ERR(rc_ss->bw_max <= BW_160MHZ);
    rc_ss->no_ss = nss_max;
    ASSERT_ERR(rc_ss->no_ss < 8);

    // Calculate RC algorithm expected throughput for each sample of the stats table,
    // then fill the retry table
    if (rc_ss->fixed_rate_cfg != RC_FIXED_RATE_NOT_SET)
        return;

    for (i = 0; i < rc_ss->no_samples; i++)
    {
        uint16_t rate_cfg;
        struct rc_rate_stats *rc_rs = &rc_ss->rate_stats[i];

        // Get a new rate to replace
        do
        {
            rate_cfg = rc_new_random_rate(rc_ss, rc_ss->bw_max > BW_20MHZ ? (rc_ss->bw_max - 1) : rc_ss->bw_max);
        } while (rc_check_rate_duplicated(rc_ss, rate_cfg));

        rc_rs->n_retry = 0;
        rc_rs->rate_allowed = 1;
        rc_rs->rate_config = rate_cfg;
        rc_rs->probability = 0;
        rc_rs->old_prob_available = 0;
        cur_tp[i] = 0;
    }

    // Sort sample table by TP
    rc_sort_samples_tp(rc_ss, cur_tp);
    // Update the retry chain with: max TP, 2nd max TP, max probability, lowest rate
    rc_update_retry_chain(rc_ss, cur_tp);

    #if NX_AMSDU_TX
    // Set maximum AMSDU len
    rc_set_max_amsdu_len(rc_ss);
    #endif
    #if NX_AMPDU_TX
    //rc_set_aggregation_flag(rc_ss, tx_ampdu);
    #endif
    // Keep in mind we have to update the rate
    rc->upd_field |= CO_BIT(STA_MGMT_POL_UPD_RATE);
}

/**
 ****************************************************************************************
 * @brief Updates the preamble type allowed by the station
 * @param[in] sta_idx index of the station
 * @param[in] preamble_type preamble type to set
 ****************************************************************************************
 */
void rc_update_preamble_type(uint8_t sta_idx, uint8_t preamble_type)
{
    struct sta_info_tag *sta_entry = &sta_info_tab[sta_idx];
    struct sta_pol_tbl_cntl *pt = &sta_entry->pol_tbl;
    struct rc_sta_stats *rc_ss = pt->sta_stats;
    ASSERT_ERR(rc_ss != NULL);

    rc_ss->p_type = preamble_type;
}

/**
 ****************************************************************************************
 * @brief Initializes the RC algorithm for bc/mc transmissions
 * @param[in] sta_idx index of the station
 * @param[in] basic_rate_idx rate index of the basic rate to be set
 ****************************************************************************************
 */
void rc_init_bcmc_rate(struct sta_info_tag *sta_entry, uint8_t basic_rate_idx)
{
    struct sta_pol_tbl_cntl *rc = &sta_entry->pol_tbl;
    struct tx_policy_tbl *pol = &rc->buf_ctrl[0]->policy_tbl;
    uint8_t i;
    uint16_t rate_config;
    uint8_t p_type = (rc->ppdu_tx_cfg & PRE_TYPE_TX_RCX_MASK) >> PRE_TYPE_TX_RCX_OFT;
    // Set rate configuration
    rate_config = ((basic_rate_idx < HW_RATE_6MBPS ? p_type : 0) << PRE_TYPE_TX_RCX_OFT) |
                  (basic_rate_idx << MCS_INDEX_TX_RCX_OFT);
    // Set the basic rate idx for all the steps of the retry chain
    for (i = 0; i < RATE_CONTROL_STEPS; i++)
    {
        pol->ratecntrlinfo[i] = (1 << N_RETRY_RCX_OFT) | rate_config;
    }
}

bool rc_check_fixed_rate_config(struct rc_sta_stats *rc_ss, uint16_t fixed_rate_config)
{
    uint8_t fixed_rate_ft = rc_get_format_mod(fixed_rate_config);
    bool update = 0;

    do
    {
        // Check format and modulation
        if (fixed_rate_ft > rc_ss->format_mod)
        {
            break;
        }
        if ((rc_ss->format_mod == FORMATMOD_VHT) && (fixed_rate_ft != FORMATMOD_VHT))
        {
            break;
        }
        if (((rc_ss->format_mod == FORMATMOD_HT_MF) || (rc_ss->format_mod == FORMATMOD_HT_GF)) &&
            ((fixed_rate_ft < FORMATMOD_HT_MF) && (rc_ss->r_idx_min > HW_RATE_11MBPS)))
        {
            break;
        }
        if (fixed_rate_ft < FORMATMOD_HT_MF)
        {
            // Check preamble type
            if ((rc_get_pre_type(fixed_rate_config) == 0) && (rc_ss->p_type == 1))
            {
                break;
            }
        }
        else
        {
            // Check guard interval
            if ((rc_get_gi(fixed_rate_config) == 1) && (rc_ss->short_gi == 0))
            {
                break;
            }
            // Check bandwidth
            if (rc_get_bw(fixed_rate_config) > rc_ss->bw_max)
            {
                break;
            }
            // Check number of spatial streams
            if (rc_get_nss(fixed_rate_config) > rc_ss->no_ss)
            {
                break;
            }
        }
        // Check MCS / rate index
        if (rc_check_valid_rate(rc_ss, fixed_rate_config) == 0)
        {
            break;
        }

        update = 1;

    } while (0);

    return update;
}

#endif
/// @}

