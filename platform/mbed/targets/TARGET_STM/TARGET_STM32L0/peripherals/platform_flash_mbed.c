/* MiCO Team
 * Copyright (c) 2017 MXCHIP Information Tech. Co.,Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


/* Includes ------------------------------------------------------------------*/
#include "platform_logging.h"
#include "platform_peripheral.h"

#include "mico_board.h"
#include "mico_board_conf.h"


/* Private constants --------------------------------------------------------*/

/* End of the Flash address */
#define FLASH_START_ADDRESS     (uint32_t)0x08000000  
#define FLASH_END_ADDRESS       (uint32_t)0x08030000
// #define FLASH_SIZE              (FLASH_END_ADDRESS -  FLASH_START_ADDRESS + 1)
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static uint32_t _GetPage( uint32_t Address );

int iflash_init( void )
{
    HAL_FLASH_Unlock( );
    /* Clear pending flags (if any) */
    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_BSY | FLASH_FLAG_ENDHV );
    return 0;
}

OSStatus iflash_erase( uint32_t StartAddress, uint32_t EndAddress )
{
  OSStatus err = kNoErr;
  uint32_t sector_err = 0;
  FLASH_EraseInitTypeDef pEraseInit;

  pEraseInit.TypeErase = FLASH_TYPEERASE_PAGES;
  pEraseInit.NbPages = _GetPage(EndAddress) - _GetPage(StartAddress) + 1;
  pEraseInit.PageAddress = StartAddress / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;

  HAL_FLASH_Unlock();
  HAL_FLASHEx_Erase(&pEraseInit, &sector_err);
  HAL_FLASH_Lock();
  return err;

}

/*stm32l0xx do not support byte program*/

static OSStatus iflash_write_word(volatile uint32_t* FlashAddress, uint32_t* Data ,uint32_t DataLength)
{
    OSStatus err = kNoErr;
    uint32_t i = 0;
    HAL_StatusTypeDef hal_status;

    uint32_t DataLength32 = DataLength;

    /*Program flash by words*/
    for ( i = 0; (i < DataLength32 / 4) && (*FlashAddress <= (FLASH_END_ADDRESS - 3)); i++ ) {
        /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
         be done by word */
        hal_status = HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, *FlashAddress, *(Data + i) );
        require_action( hal_status == HAL_OK, exit, err = kWriteErr );
        *FlashAddress += 4;
    }

exit:
    return err;
}

OSStatus iflash_write(volatile uint32_t* FlashAddress, uint32_t* Data ,uint32_t DataLength)
{
    OSStatus err = kNoErr;
    uint32_t i = 0;
    uint32_t temp_data;
    uint8_t last_words_len, last_bytes_len;
    uint8_t existed_bytes_len, existed_words_len;
    uint32_t temp_data_len = DataLength;
    uint32_t start_addr = *FlashAddress;
    uint32_t actual_start_addr = start_addr & 0xFFFFFFFC;

    HAL_FLASH_Unlock();

    /*First bytes that are not word align*/
    existed_bytes_len = start_addr & 0x3;
    if ( existed_bytes_len ) {
        temp_data = * ((uint32_t *)actual_start_addr);
        memcpy( (uint8_t *)(&temp_data) + existed_bytes_len , (uint8_t *)Data, 4 - existed_bytes_len );

        err = iflash_write_word(&actual_start_addr, &temp_data, 4);
        require_noerr( err, exit );

        Data = (uint32_t *)((uint32_t)Data + 4 - existed_bytes_len);
        temp_data_len -= (4 - existed_bytes_len);
        start_addr = actual_start_addr;
    }

    /*First bytes that are not 16 words align*/
    existed_words_len = start_addr & 0x3F;
    if ( existed_words_len ) {
        err = iflash_write_word( &start_addr, Data, 64 - existed_words_len );
        require_noerr( err, exit );

        Data = (uint32_t *)((uint32_t)Data + 64 - existed_words_len);
        temp_data_len -= (64 - existed_words_len);
    }

    /*Program flash by half page*/
    for ( i = 0; (i < temp_data_len / 64) && (actual_start_addr <= (FLASH_END_ADDRESS - 63)); i++ ) {

        err = HAL_FLASHEx_HalfPageProgram(start_addr, Data + i*16);
        require_noerr( err, exit );
        start_addr += 64;
    }
    Data = (uint32_t *)((uint32_t)Data + i * 64);
    temp_data_len -= i * 64;

    /* Program last words not in a half page */
    last_words_len =  temp_data_len&0xFFFFFFFC;
    if (last_words_len) {
        err = iflash_write_word( &start_addr, Data, last_words_len );
        require_noerr( err, exit );

        Data = (uint32_t *)((uint32_t)Data + last_words_len);
        temp_data_len -= last_words_len;
    }

    /* Program last uncompleted word */
    last_bytes_len =  temp_data_len;
    if (last_bytes_len) {
        temp_data = * ((uint32_t *)actual_start_addr);
        memcpy( (uint8_t *)(&temp_data) , (uint8_t *)Data, last_bytes_len );

        err = iflash_write_word( &start_addr, &temp_data, 4 );
        require_noerr( err, exit );

        Data = (uint32_t *)((uint32_t)Data + last_bytes_len);
        temp_data_len -= last_bytes_len;
    }

exit:
    *FlashAddress = start_addr;
    HAL_FLASH_Lock();
    return err;
}

/**
* @brief  Gets the page of a given address
* @param  Address: Flash address
* @retval The sector of a given address
*/
static uint32_t _GetPage(uint32_t Address)
{
  uint32_t page = 0;

  /* FLASH_PAGE_SIZE            ((uint32_t)128U) */
  page = (Address - 0x8000000) / FLASH_PAGE_SIZE;
  return page;
}
