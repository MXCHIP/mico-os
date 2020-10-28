#include "mico.h"
#include "lota.h"

int lota_flash_erase(int8_t lota_part, uint32_t offset, uint32_t len)
{
    if (lota_part == LOTA_FLASH_ALL)
        lota_part = LOTA_FLASH_PART_ALL;
    else if (lota_part == LOTA_FLASH_KVRO)
        lota_part = LOTA_FLASH_PART_KVRO;
    else if (lota_part == LOTA_FLASH_OTA)
        lota_part = LOTA_FLASH_PART_OTA;
    return MicoFlashErase(lota_part, offset, len);
}

int lota_flash_write(int8_t lota_part, uint32_t *poffset, uint8_t *buffer, uint32_t len)
{
    if (lota_part == LOTA_FLASH_ALL)
        lota_part = LOTA_FLASH_PART_ALL;
    else if (lota_part == LOTA_FLASH_KVRO)
        lota_part = LOTA_FLASH_PART_KVRO;
    else if (lota_part == LOTA_FLASH_OTA)
        lota_part = LOTA_FLASH_PART_OTA;
    return MicoFlashWrite(lota_part, poffset, buffer, len);
}

int lota_flash_read(int8_t lota_part, uint32_t *poffset, uint8_t *buffer, uint32_t len)
{
    if (lota_part == LOTA_FLASH_ALL)
        lota_part = LOTA_FLASH_PART_ALL;
    else if (lota_part == LOTA_FLASH_KVRO)
        lota_part = LOTA_FLASH_PART_KVRO;
    else if (lota_part == LOTA_FLASH_OTA)
        lota_part = LOTA_FLASH_PART_OTA;
    return MicoFlashRead(lota_part, poffset, buffer, len);
}

char *lota_mxid_read(void)
{
    return NULL;
}

