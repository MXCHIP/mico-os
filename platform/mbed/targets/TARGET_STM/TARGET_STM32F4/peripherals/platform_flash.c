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
#include "platform_mcu_peripheral.h"
#include "mico_board.h"
#include "mico_board_conf.h"

#include "platform_flash.h"

/* Private constants --------------------------------------------------------*/

#if defined USE_MICO_SPI_FLASH || defined USE_QUAD_SPI_FLASH
//TODO Only one handle is supported here
static sflash_handle_t sflash_handle = {0x0, 0x0, SFLASH_WRITE_NOT_ALLOWED};
#endif

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/



OSStatus platform_flash_init( const platform_flash_t *peripheral )
{
  OSStatus err = kNoErr;

  require_action_quiet( peripheral != NULL, exit, err = kParamErr);

  if( peripheral->flash_type == FLASH_TYPE_EMBEDDED ){
    err = iflash_init();
    require_noerr(err, exit);
  }
#if defined USE_MICO_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_SPI ){
    err = init_sflash( &sflash_handle, 0, SFLASH_WRITE_ALLOWED );
    require_noerr(err, exit);
  }
#else
#if defined USE_QUAD_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_QSPI ){
    err = qsflash_init( &platform_qspi_peripherals[MICO_QSPI_1], peripheral, &sflash_handle, SFLASH_WRITE_ALLOWED );
    require_noerr(err, exit);
  }
#endif
#endif
  else{
    err = kTypeErr;
    goto exit;
  }
exit:
  return err;
}

OSStatus platform_flash_erase( const platform_flash_t *peripheral, uint32_t start_address, uint32_t end_address )
{
  OSStatus err = kNoErr;

  require_action_quiet( peripheral != NULL, exit, err = kParamErr);
  require_action( start_address >= peripheral->flash_start_addr 
               && end_address   <= peripheral->flash_start_addr + peripheral->flash_length - 1, exit, err = kParamErr);

  if( peripheral->flash_type == FLASH_TYPE_EMBEDDED ){
    err = iflash_erase( start_address, end_address );
    require_noerr(err, exit);
  }
#ifdef USE_MICO_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_SPI ){
    err = spiFlashErase( start_address, end_address );
    require_noerr(err, exit);
  }
#else
  #ifdef USE_QUAD_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_QSPI ){
    err = qsflash_erase( start_address, end_address );
    require_noerr(err, exit);
  }
  #endif
#endif
  else{
    err = kTypeErr;
    goto exit;
  }

exit:
  return err;
}

OSStatus platform_flash_write( const platform_flash_t *peripheral, volatile uint32_t* start_address, uint8_t* data ,uint32_t length  )
{
  OSStatus err = kNoErr;

  require_action_quiet( peripheral != NULL, exit, err = kParamErr);
  require_action( *start_address >= peripheral->flash_start_addr 
               && *start_address + length <= peripheral->flash_start_addr + peripheral->flash_length, exit, err = kParamErr);
    
  if( peripheral->flash_type == FLASH_TYPE_EMBEDDED ){
    err = iflash_write( start_address, (uint32_t *)data, length);
    require_noerr(err, exit);
  }
#ifdef USE_MICO_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_SPI ){
    err = sflash_write( &sflash_handle, *start_address, data, length );
    require_noerr(err, exit);
    *start_address += length;
  }
#else
  #ifdef USE_QUAD_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_QSPI ){
    err = qsflash_write( &sflash_handle, *start_address, data, length );
    require_noerr(err, exit);
    *start_address += length;
  }
  #endif
#endif
  else{
    err = kTypeErr;
    goto exit;
  }

exit:
  return err;
}

OSStatus platform_flash_read( const platform_flash_t *peripheral, volatile uint32_t* start_address, uint8_t* data ,uint32_t length  )
{
  OSStatus err = kNoErr;

  require_action_quiet( peripheral != NULL, exit, err = kParamErr);
  require_action( (*start_address >= peripheral->flash_start_addr) 
               && (*start_address + length) <= ( peripheral->flash_start_addr + peripheral->flash_length), exit, err = kParamErr);

  if( peripheral->flash_type == FLASH_TYPE_EMBEDDED ){
    memcpy(data, (void *)(*start_address), length);
    *start_address += length;
  }
#ifdef USE_MICO_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_SPI ){
    err = sflash_read( &sflash_handle, *start_address, data, length );
    require_noerr(err, exit);
    *start_address += length;
  }
#else
  #ifdef USE_QUAD_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_QSPI ){
    err = qsflash_read( &sflash_handle, *start_address, data, length );
    require_noerr(err, exit);
    *start_address += length;
  }
  #endif
#endif
  else{
    err = kTypeErr;
    goto exit;
  }

exit:
  return err;
}

OSStatus platform_flash_enable_protect( const platform_flash_t *peripheral, uint32_t start_address, uint32_t end_address )
{
  OSStatus err = kNoErr;

  require_action_quiet( peripheral != NULL, exit, err = kParamErr);
  require_action( start_address >= peripheral->flash_start_addr 
               && end_address   <= peripheral->flash_start_addr + peripheral->flash_length - 1, exit, err = kParamErr);

  if( peripheral->flash_type == FLASH_TYPE_EMBEDDED ){
#ifdef MCU_ENABLE_FLASH_PROTECT
    err = iflash_protect( start_address, end_address, true );
#endif  
    require_noerr(err, exit);
  }
#ifdef USE_MICO_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_SPI ){
    err = kNoErr;
    goto exit;
  }
#else
  #ifdef USE_QUAD_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_QSPI ){
    err = kNoErr;
    goto exit;
  }
  #endif
#endif
  else{
    err = kTypeErr;
    goto exit;
  }

exit:
  return err;  
}

OSStatus platform_flash_disable_protect( const platform_flash_t *peripheral, uint32_t start_address, uint32_t end_address )
{
  OSStatus err = kNoErr;

  require_action_quiet( peripheral != NULL, exit, err = kParamErr);
  require_action( start_address >= peripheral->flash_start_addr 
               && end_address   <= peripheral->flash_start_addr + peripheral->flash_length - 1, exit, err = kParamErr);

  if( peripheral->flash_type == FLASH_TYPE_EMBEDDED ){
#ifdef MCU_EBANLE_FLASH_PROTECT
    err = internalFlashProtect( start_address, end_address, false );   
#endif 
    require_noerr(err, exit);
  }
#ifdef USE_MICO_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_SPI ){
    err = kNoErr;
    goto exit;
  }
#else
  #ifdef USE_QUAD_SPI_FLASH
  else if( peripheral->flash_type == FLASH_TYPE_QSPI ){
    err = kNoErr;
    goto exit;
  }
  #endif
#endif
  else{
    err = kTypeErr;
    goto exit;
  }

exit:
  return err;  
}

