#ifndef _LOTA_HEADER_H_
#define _LOTA_HEADER_H_


#include "mico.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOTA_VERSION "1.0"

#define lota_log(fmt, ...)  custom_log("lota", fmt, ##__VA_ARGS__)

typedef enum
{
    LOTA_EVT_FOUND,
    LOTA_EVT_CONNECT,
    LOTA_EVT_OTA_DONE,
    LOTA_EVT_SUCCESS,
    
    LOTA_EVT_FAIL,
}LOTA_EVENT_E;


enum {
    LOTA_FLASH_ALL,
    LOTA_FLASH_KVRO,
    LOTA_FLASH_OTA,
};



typedef void (*lota_handler_t)(uint8_t event, uint8_t *data, int n);
typedef void (*emitter_callback_t)(uint8_t *src_mac, char *ssid, uint8_t *data, uint8_t size);

void lota_event(uint8_t event, void *data, int n);

int lota_check(void);

int lota_flash_erase(int8_t type, uint32_t offset, uint32_t len);

int lota_flash_write(int8_t type, uint32_t *poffset, uint8_t *buffer, uint32_t len);

int lota_flash_read(int8_t type, uint32_t *poffset, uint8_t *buffer, uint32_t len);

void lota_monitor_init(uint8_t channel, emitter_callback_t beacon_input);

void lota_monitor_start(monitor_cb_t func, uint8_t channel);

/* stop wifi monitor*/
void lota_monitor_stop(void);
void lota_monitor_deinit(void);

/* wifi initilize */
void lota_wifi_init(void);

/* wifi connect, max wait 4 seconds
 * retval: 0=connected, other=fail.
 */
int lota_wifi_connect(char *ssid, char *psk, char*pass, mwifi_connect_attr_t *attr, mwifi_ip_attr_t *ip);

int lota_wifi_disconnect(void);

void save_lota_info(void);

/* user callback function 
 *  event: LOTA_EVENT_E
 *  data:  message
 *  n:     message length
 */
void lota_event(uint8_t event, void *data, int n);

/* extra user message in lota hi message .
 *  return: user want to send extra message to LOTA host. This message is freed by lota.
 *         return NULL for nothing.
 * 
 */
char* lota_extra_message(void);

/* received lota user message 
 *  data: received user message, received len
 *  txlen: user tx message length
 *  return: user tx message, tx message is freed by lota.
 */
void* lota_rx_user_msg(void *data, int len, int *txlen);

char *lota_mxid_read(void);
#ifdef __cplusplus
} /*extern "C" */
#endif

#endif
