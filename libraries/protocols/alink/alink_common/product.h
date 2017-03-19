

#ifndef __HV_PLATFORM_H__
#define __HV_PLATFORM_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "alink_platform.h"
#include "alink_export.h"

/** @defgroup group_product group_product
 *  @{
 */

/** @defgroup group_product_macro product_macro
 *  @{
 */


#define PRODUCT_SN_LEN          (64 + 1)
#define PRODUCT_MODEL_LEN       (80 + 1)
#define PRODUCT_KEY_LEN         (20 + 1)
#define PRODUCT_SECRET_LEN      (40 + 1)
#define PRODUCT_UUID_LEN        (32 + 1)
#define PRODUCT_VERSION_LEN     (16 + 1)
#define PRODUCT_NAME_LEN        (32 + 1)
#define STR_NAME_LEN            (32 + 1)
#define STR_CID_LEN             (64 + 1)

#define PRODUCT_ASR_APP_KEY_LEN (64)

/** @} */ // end of product_macro



/** @defgroup group_product_all product_all
 *  @{
 */


/**
 * @brief Get the product version string.
 *
 * @param[in] version_str @n Buffer for using to store version string.
 * @return The version string.
 * @see None.
 * @note
 */
char *product_get_version(char version_str[PRODUCT_VERSION_LEN]);

/**
 * @brief Get product name string.
 *
 * @param[out] name_str @n Buffer for using to store name string.
 * @return A pointer to the start address of name_str.
 * @see None.
 * @note None.
 */
char *product_get_name(char name_str[PRODUCT_NAME_LEN]);

/**
 * @brief Get product SN string.
 *
 * @param[out] sn_str @n Buffer for using to store SN string.
 * @return A pointer to the start address of sn_str.
 * @see None.
 * @note None.
 */
char *product_get_sn(char sn_str[PRODUCT_SN_LEN]);

/**
 * @brief Get product model string.
 *
 * @param[out] model_str @n Buffer for using to store model string.
 * @return A pointer to the start address of model_str.
 * @see None.
 * @note None.
 */
char *product_get_model(char model_str[PRODUCT_MODEL_LEN]);

/**
 * @brief Get product key string.
 *
 * @param[out] key_str @n Buffer for using to store key string.
 * @return A pointer to the start address of key_str.
 * @see None.
 * @note None.
 */
char *product_get_key(char key_str[PRODUCT_KEY_LEN]);

/**
 * @brief Get product secret string.
 *
 * @param[out] secret_str @n Buffer for using to store secret string.
 * @return A pointer to the start address of secret_str.
 * @see None.
 * @note None.
 */
char *product_get_secret(char secret_str[PRODUCT_SECRET_LEN]);

/**
 * @brief Get product debug key string.
 *
 * @param[out] key_str @n Buffer for using to store debug key string.
 * @return A pointer to the start address of key_str.
 * @see None.
 * @note None.
 */
char *product_get_debug_key(char key_str[PRODUCT_KEY_LEN]);

/**
 * @brief Get product debug secret string.
 *
 * @param[out] secret_str @n Buffer for using to store debug secret string.
 * @return A pointer to the start address of secret_str.
 * @see None.
 * @note None.
 */
char *product_get_debug_secret(char secret_str[PRODUCT_SECRET_LEN]);
char *product_get_type(char type_str[STR_NAME_LEN]);
char *product_get_category(char category_str[STR_NAME_LEN]);
char *product_get_manufacturer(char manufacturer_str[STR_NAME_LEN]);
char *product_get_cid(char cid_str[STR_CID_LEN]);

char *alink_get_firmware_version( char version[30] );

/** @} */ // end of product_all

	char *product_get_asr_appkey(char app_key[PRODUCT_ASR_APP_KEY_LEN]);


	char *product_get_audio_format();


	/** @} */// end of group_product

#ifdef __cplusplus
}
#endif

#endif


