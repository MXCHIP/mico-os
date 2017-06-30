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


/* Private constants --------------------------------------------------*------*/
#define EEPROM_END_ADDRESS    0x080817FF    
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/
static uint32_t _GetPage( uint32_t Address );

int ieeprom_init( void )
{
    HAL_FLASHEx_DATAEEPROM_Unlock( );
    /* Clear pending flags (if any) */
    __HAL_FLASH_CLEAR_FLAG(
        FLASH_FLAG_EOP | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_BSY | FLASH_FLAG_ENDHV );
    HAL_FLASHEx_DATAEEPROM_Lock( );
    return 0;
}

OSStatus ieeprom_erase( uint32_t StartAddress, uint32_t EndAddress )
{
  OSStatus err = kNoErr;

  HAL_FLASHEx_DATAEEPROM_Unlock();
  for( uint32_t i = StartAddress; i < EndAddress; i+=4 ){
      HAL_FLASHEx_DATAEEPROM_Erase(i);
  }
  HAL_FLASHEx_DATAEEPROM_Lock();
//   require_action( HAL_OK == hal_status, exit, err =  kWriteErr);
  return err;
}

OSStatus ieeprom_write(volatile uint32_t* FlashAddress, uint32_t* Data ,uint32_t DataLength)
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
    for ( i = 0; (i < DataLength32 / 4) && (*FlashAddress <= (EEPROM_END_ADDRESS - 3)); i++ ) {
        /* Device voltage range supposed to be [2.7V to 3.6V], the operation will
         be done by word */
        dataInRam = *(Data + i);
        HAL_FLASHEx_DATAEEPROM_Unlock();
        HAL_FLASHEx_DATAEEPROM_Program( FLASH_TYPEPROGRAMDATA_WORD, *FlashAddress, dataInRam );
        HAL_FLASHEx_DATAEEPROM_Lock();
        // require_action( hal_status == HAL_OK, exit, err = kWriteErr );
        /* Readout and check */
        require_action( *(uint32_t* )*FlashAddress == dataInRam, exit, err = kChecksumErr );
        /* Increment FLASH destination address */
        *FlashAddress += 4;
    }
exit:
    return err;
}
