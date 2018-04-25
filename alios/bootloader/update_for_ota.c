/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <stdio.h>
#include "stdbool.h"
#include "hal/soc/soc.h"
#include "aos/debug.h"
#include "aos/kernel.h"
#include "board.h"    
#include "ymodem.h"
#include "bootloader.h" 
#include "CheckSumUtils.h" 
#include <hal/ota.h>

typedef int Log_Status;					
#define Log_NotExist		        (1)
#define Log_NeedUpdate			    (2)
#define Log_UpdateTagNotExist		(3)
#define Log_contentTypeNotExist     (4)
#define Log_dataLengthOverFlow      (5)
#define Log_StartAddressERROR		(6)
#define Log_UnkonwnERROR            (7)
#define Log_CRCERROR                (8)

#define SizePerRW 4096   /* Bootloader need 2xSizePerRW RAM heap size to operate, 
                            but it can boost the setup. */

static uint8_t data[SizePerRW];
static uint8_t newData[SizePerRW];
uint8_t paraSaveInRam[16*1024];

static int32_t checkcrc(uint16_t crc_in, int partition_type, int total_len)
{
    uint16_t crc = 0;
    hal_logic_partition_t* part;
    int len;
    int32_t err = 0;
    uint32_t update_data_offset = 0x0;
    CRC16_Context contex;

    CRC16_Init( &contex );
    
    if (crc_in == 0xFFFF)
        goto exit;

    part = hal_flash_get_info((hal_partition_t)partition_type);
    if (part == NULL)
        goto exit;

    while(total_len > 0){
      if( SizePerRW < total_len ){
        len = SizePerRW;
      } else {
        len = total_len;
      }
      err = hal_flash_read( HAL_PARTITION_OTA_TEMP, &update_data_offset, data , len);
      require_noerr(err, exit);

      total_len -= len;

      CRC16_Update( &contex, data, len );
    }

  CRC16_Final( &contex, &crc );
    if (crc != crc_in)
        err = -1;
exit:
    printf("CRC check return %d, got crc %x, calcuated crc %x\r\n", err, crc_in, crc);
    return err;
}

Log_Status updateLogCheck( boot_table_t *updateLog, hal_partition_t *dest_partition_type )
{
    uint32_t i;

    for ( i = 0; i < sizeof(boot_table_t); i++ )
    {
        if ( *((uint8_t *) updateLog + i) != 0xff )
            break;
    }
    if ( i == sizeof(boot_table_t) )
        return Log_NotExist;

    if ( updateLog->upgrade_type != 'U' )
        return Log_UpdateTagNotExist;

    if ( updateLog->start_address
        != hal_flash_get_info( HAL_PARTITION_OTA_TEMP )->partition_start_addr )
        return Log_StartAddressERROR;

    if ( updateLog->type == 'B' )
        *dest_partition_type = HAL_PARTITION_BOOTLOADER;
    else if ( updateLog->type == 'A' )
        *dest_partition_type = HAL_PARTITION_APPLICATION;
    else if ( updateLog->type == 'D' )
        *dest_partition_type = HAL_PARTITION_RF_FIRMWARE;
    else
        return Log_contentTypeNotExist;

    if ( updateLog->length > hal_flash_get_info( *dest_partition_type )->partition_length )
        return Log_dataLengthOverFlow;

    if ( checkcrc( updateLog->crc, *dest_partition_type, updateLog->length ) != 0 )
        return Log_CRCERROR;

    return Log_NeedUpdate;
}


int32_t check_ota(void)
{
  boot_table_t updateLog;
  uint32_t i, j, size;
  uint32_t update_data_offset = 0x0;
  uint32_t dest_offset;
  uint32_t boot_table_offset = 0x0;
  uint32_t para_offset = 0x0;
  uint32_t copyLength;
  //uint8_t *paraSaveInRam = NULL;
  hal_logic_partition_t *ota_partition_info, *dest_partition_info, *para_partition_info;
  hal_partition_t dest_partition;
  int32_t err = 0;

  ota_partition_info = hal_flash_get_info(HAL_PARTITION_OTA_TEMP);
  require_action( ota_partition_info->partition_owner != HAL_FLASH_NONE, exit, err = -1 );
  
  para_partition_info = hal_flash_get_info(HAL_PARTITION_PARAMETER_1);
  require_action( para_partition_info->partition_owner != HAL_FLASH_NONE, exit, err = -1 );
  
  memset(data, 0xFF, SizePerRW);
  memset(newData, 0xFF, SizePerRW);

  //paraSaveInRam = malloc( para_partition_info->partition_length );
  //require_action( paraSaveInRam, exit, err = kNoMemoryErr );
  memset(paraSaveInRam, 0xFF, para_partition_info->partition_length);
    
  err = hal_flash_read( HAL_PARTITION_PARAMETER_1, &boot_table_offset, (uint8_t *)&updateLog, sizeof(boot_table_t));
  require_noerr(err, exit);

  /*Not a correct record*/
  if(updateLogCheck( &updateLog, &dest_partition) != Log_NeedUpdate){
    size = ( ota_partition_info->partition_length )/SizePerRW;
    for(i = 0; i <= size; i++){
      if( i==size ){
        err = hal_flash_read( HAL_PARTITION_OTA_TEMP , &update_data_offset, data , ( ota_partition_info->partition_length )%SizePerRW );
        require_noerr(err, exit);
      }
      else{
        err = hal_flash_read( HAL_PARTITION_OTA_TEMP, &update_data_offset, data , SizePerRW);
        require_noerr(err, exit);
      }
      
      for(j=0; j<SizePerRW; j++){
        if(data[j] != 0xFF){
          printf("Update data need to be erased\r\n");
          err = hal_flash_dis_secure( HAL_PARTITION_OTA_TEMP, 0x0, ota_partition_info->partition_length );
          require_noerr(err, exit);
          err = hal_flash_erase( HAL_PARTITION_OTA_TEMP, 0x0, ota_partition_info->partition_length );
          require_noerr(err, exit);
          goto exit;
        }
      }
    }
    goto exit;
  }

  dest_partition_info = hal_flash_get_info( dest_partition );
  require_action( dest_partition_info->partition_owner != HAL_FLASH_NONE, exit, err = -1 );
  
  printf("Write OTA data to partition: %s, length %ld\r\n",
    dest_partition_info->partition_description, updateLog.length);
  
  #ifdef CONFIG_MX108
  dest_offset = dest_partition_info->partition_start_addr / 16;
  #else
  dest_offset = 0x0;
  #endif
  update_data_offset = 0x0;
  
  err = hal_flash_dis_secure( dest_partition, 0x0, dest_partition_info->partition_length );
  require_noerr(err, exit);
  err = hal_flash_erase( dest_partition, 0x0, dest_partition_info->partition_length );
  require_noerr(err, exit);
  size = (updateLog.length)/SizePerRW;
  
  for(i = 0; i <= size; i++){
    if( i == size ){
      if( (updateLog.length)%SizePerRW )
        copyLength = (updateLog.length)%SizePerRW;
      else
        break;
    }else{
      copyLength = SizePerRW;
    }
    err = hal_flash_read( HAL_PARTITION_OTA_TEMP, &update_data_offset, data , copyLength);
    require_noerr(err, exit);
    err = hal_flash_write( dest_partition, &dest_offset, data, copyLength);
    require_noerr(err, exit);
    dest_offset -= copyLength;
    err = hal_flash_read( dest_partition, &dest_offset, newData , copyLength);
    require_noerr(err, exit);
    err = memcmp(data, newData, copyLength);
    require_noerr_action(err, exit, err = -1); 
 }

  printf("Update start to clear data...\r\n");
    
  para_offset = 0x0;
  err = hal_flash_dis_secure( HAL_PARTITION_PARAMETER_1, 0x0, para_partition_info->partition_length );
  require_noerr(err, exit);
  err = hal_flash_read( HAL_PARTITION_PARAMETER_1, &para_offset, paraSaveInRam, para_partition_info->partition_length );
  require_noerr(err, exit);
  memset(paraSaveInRam, 0xff, sizeof(boot_table_t));
  err = hal_flash_erase( HAL_PARTITION_PARAMETER_1, 0x0, para_partition_info->partition_length );
  require_noerr(err, exit);
  para_offset = 0x0;
  err = hal_flash_write( HAL_PARTITION_PARAMETER_1, &para_offset, paraSaveInRam, para_partition_info->partition_length );
  require_noerr(err, exit);
  

  err = hal_flash_dis_secure( HAL_PARTITION_OTA_TEMP, 0x0, ota_partition_info->partition_length );
  require_noerr(err, exit);  
  err = hal_flash_erase( HAL_PARTITION_OTA_TEMP, 0x0, ota_partition_info->partition_length );
  require_noerr(err, exit);
  printf("Update success\r\n");
  
exit:
  if(err != 0) printf("Update exit with err = %d\r\n", err);
  return err;
}



