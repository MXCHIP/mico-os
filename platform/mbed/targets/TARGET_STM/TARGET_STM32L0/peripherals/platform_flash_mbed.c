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
  return err;
//   HAL_FLASH_Lock();
//   require_action( HAL_OK == hal_status, exit, err =  kWriteErr);
}

/*stm32l0xx do not support byte program*/
// OSStatus iflash_write_byte( volatile uint32_t* FlashAddress, uint8_t* Data, uint32_t DataLength )
// {
//     uint32_t i = 0, dataInRam;
//     OSStatus err = kNoErr;
//     HAL_StatusTypeDef hal_status;

//     for ( i = 0; (i < DataLength) && (*FlashAddress <= (FLASH_END_ADDRESS )); i++ ) {
//         dataInRam = *(uint8_t*) (Data + i);
//         HAL_FLASH_Unlock();
//         hal_status = HAL_FLASH_Program( FLASH_TYPEPROGRAM_BYTE, *FlashAddress, dataInRam );
//         HAL_FLASH_Lock();
//         require_action( hal_status == HAL_OK, exit, err = kWriteErr );
//         /* Readout and check */
//         require_action( *(uint8_t* )*FlashAddress == dataInRam, exit, err = kChecksumErr );
//         *FlashAddress += 1;
//     }

// exit:
//     return err;
// }


OSStatus iflash_write(volatile uint32_t* FlashAddress, uint32_t* Data ,uint32_t DataLength)
{
    OSStatus err = kNoErr;
    uint32_t i = 0;
    uint32_t dataInRam;
    uint8_t startNumber;
    uint32_t DataLength32 = DataLength;

    /*First bytes that are not 32bit align*/
    if ( *FlashAddress % 4 ) {
        // startNumber = 4 - (*FlashAddress) % 4;
        // err = iflash_write_byte( FlashAddress, (uint8_t *) Data, startNumber );
        // require_noerr( err, exit );
        // DataLength32 = DataLength - startNumber;
        // Data = (uint32_t *) ((uint32_t) Data + startNumber);
    }

    /*Program flash by words*/
    for ( i = 0; (i < DataLength32 / 4) && (*FlashAddress <= (FLASH_END_ADDRESS - 3)); i++ ) {
        /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
         be done by word */
        dataInRam = *(Data + i);
        HAL_FLASH_Unlock();
        HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, *FlashAddress, dataInRam );
        // HAL_FLASH_Lock();
        // require_action( hal_status == HAL_OK, exit, err = kWriteErr );
        /* Readout and check */
        require_action( *(uint32_t* )*FlashAddress == dataInRam, exit, err = kChecksumErr );
        /* Increment FLASH destination address */
        *FlashAddress += 4;
    }
        //HAL_FLASH_Unlock();
        //HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, *FlashAddress, (uint32_t *) Data + i * 4 );
        // HAL_FLASH_Lock();
        // require_action( hal_status == HAL_OK, exit, err = kWriteErr );

    // /*Last bytes that cannot be write by a 32 bit word*/
    // err = iflash_write_byte( FlashAddress, (uint8_t *) Data + i * 4, DataLength32 - i * 4 );
    // require_noerr( err, exit );

exit:
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
