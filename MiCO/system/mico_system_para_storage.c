/**
 ******************************************************************************
 * @file    mico_system_para_storage.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide functions to read and write configuration data on
 *          nonvolatile memory.
 ******************************************************************************
 *
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico.h"
#include "mico_system.h"
#include "system_internal.h"
#include "CheckSumUtils.h"

/* Update seed number every time*/
static int32_t seedNum = 0;

#define SYS_CONFIG_SIZE     ( sizeof(system_config_t) - sizeof( boot_table_t ) )
#define CRC_SIZE      ( 2 )

system_context_t* sys_context = NULL;
static mico_mutex_t para_flash_mutex = NULL;
//#define para_log(M, ...) custom_log("MiCO Settting", M, ##__VA_ARGS__)

#define para_log(M, ...)

static OSStatus try_old_para(system_context_t *inContext);

WEAK void appRestoreDefault_callback(void *user_data, uint32_t size)
{

}

#ifndef OFFSETOF
#define OFFSETOF( type, member )  ( (uintptr_t)&((type *)0)->member )
#endif /* OFFSETOF */

#define ENCRYPT_KEY_LEN 16

static bool _encrypt_use;
static uint8_t _encrypt_key[ENCRYPT_KEY_LEN];
static uint8_t _buffered_user_key[maxKeyLen];

static const uint32_t mico_context_section_offsets[ ] =
{
    [PARA_BOOT_TABLE_SECTION]            = OFFSETOF( system_config_t, bootTable ),
    [PARA_MICO_DATA_SECTION]             = OFFSETOF( system_config_t, micoSystemConfig ),
#ifdef MICO_BLUETOOTH_ENABLE
    [PARA_BT_DATA_SECTION]               = OFFSETOF( system_config_t, bt_config ),
#endif
    [PARA_SYS_END_SECTION]               = sizeof( system_config_t ),
    [PARA_APP_DATA_SECTION]              = sizeof( system_config_t ),
#ifdef MICO_EXTRA_AP_NUM
    [PARA_SYS_EXTRA_SECTION]             = PARA_SYS_EXTRA_OFFSET,
#endif

};

void* mico_system_context_init( uint32_t user_config_data_size )
{
  void *user_config_data = NULL;

#ifdef MICO_EXTRA_AP_NUM
  if (user_config_data_size + mico_context_section_offsets[ PARA_APP_DATA_SECTION ] > 
    mico_context_section_offsets[ PARA_SYS_EXTRA_SECTION ]) {
    system_log("!!Please change PARA_SYS_EXTRA_OFFSET in mico_config.h!!");
  }
#endif

  if( sys_context !=  NULL) {
    if( sys_context->user_config_data != NULL )
      free( sys_context->user_config_data );
    free( sys_context );
    sys_context = NULL;
  }

  if( user_config_data_size ){
    user_config_data = calloc( 1, user_config_data_size );
    require( user_config_data, exit );
  }

  sys_context = calloc( 1, sizeof(system_context_t) );
  require( sys_context, exit );

  sys_context->user_config_data = user_config_data;
  sys_context->user_config_data_size = user_config_data_size;

  para_log( "Init context: len=%d", sizeof(system_context_t));

  mico_rtos_init_mutex( &sys_context->flashContentInRam_mutex );
  mico_rtos_init_mutex( &para_flash_mutex);
  MICOReadConfiguration( sys_context );

exit:
  return &sys_context->flashContentInRam;
}

mico_Context_t* mico_system_context_get( void )
{
  return &sys_context->flashContentInRam;
}

void* mico_system_context_get_user_data( mico_Context_t* const in_context )
{
    if( sys_context )
        return sys_context->user_config_data;
    else
        return NULL;
}

static bool is_crc_match( uint16_t crc_1, uint16_t crc_2)
{
  if( crc_1 != crc_2 )
    return false;
      
  return true;
}

/* Calculate CRC value for parameter1/parameter2. exclude boottable and the last 2 bytes(crc16 result) */
static uint16_t para_crc16(mico_partition_t part)
{
    uint16_t crc_result;
    CRC16_Context crc_context;
    uint32_t offset, len = 1024, end;
    uint8_t *tmp;
    mico_logic_partition_t *partition; 
    
    if ((part != MICO_PARTITION_PARAMETER_1) && (part != MICO_PARTITION_PARAMETER_2))
        return 0;

    tmp = (uint8_t*)malloc(1024);
    if (tmp == NULL)
        return 0;

    offset = mico_context_section_offsets[ PARA_MICO_DATA_SECTION ];
    partition = MicoFlashGetInfo( part );
    /* Calculate CRC value */
    CRC16_Init( &crc_context );
    end = partition->partition_length - CRC_SIZE;
    while(offset < end) {
        if (offset + len > end)
            len = end - offset;
        MicoFlashRead( part, &offset, tmp, len);
        CRC16_Update( &crc_context, tmp, len );
    }
    CRC16_Final( &crc_context, &crc_result );

    free(tmp);
    return crc_result;
}

static OSStatus internal_update_config( system_context_t * const inContext )
{
  OSStatus err = kNoErr;
  uint32_t para_offset;
  uint16_t crc_result;
  uint16_t crc_readback;;
  mico_logic_partition_t *partition; 

  require_action(inContext, exit, err = kNotPreparedErr);

  if (_encrypt_use)
  {
    if (memcmp(_buffered_user_key, inContext->flashContentInRam.micoSystemConfig.user_key, maxKeyLen) != 0)
    {
      Aes aes;

      AesSetKey(&aes, _encrypt_key, AES_BLOCK_SIZE, _encrypt_key, AES_ENCRYPTION);
      AesCbcEncrypt(&aes, (byte *)inContext->flashContentInRam.micoSystemConfig.user_key, (const byte *)inContext->flashContentInRam.micoSystemConfig.user_key, maxKeyLen);
      memcpy(_buffered_user_key, inContext->flashContentInRam.micoSystemConfig.user_key, maxKeyLen);
    }
  }

  para_log("Flash write!");
  mico_rtos_lock_mutex( &para_flash_mutex);
  partition = MicoFlashGetInfo( MICO_PARTITION_PARAMETER_1 );
  err = MicoFlashErase( MICO_PARTITION_PARAMETER_1, 0x0, partition->partition_length);
  require_noerr(err, exit);

  para_offset = 0x0;
  err = MicoFlashWrite( MICO_PARTITION_PARAMETER_1, &para_offset, (uint8_t *)&inContext->flashContentInRam, sizeof(system_config_t));
  require_noerr(err, exit);

  para_offset = mico_context_section_offsets[ PARA_APP_DATA_SECTION ];
  err = MicoFlashWrite( MICO_PARTITION_PARAMETER_1, &para_offset, inContext->user_config_data, inContext->user_config_data_size );
  require_noerr(err, exit);
#ifdef MICO_EXTRA_AP_NUM
  para_offset = mico_context_section_offsets[ PARA_SYS_EXTRA_SECTION ];
  err = MicoFlashWrite( MICO_PARTITION_PARAMETER_1, &para_offset, (uint8_t *)inContext->extra_ap, sizeof(inContext->extra_ap) );
  require_noerr(err, exit);
#endif

  crc_result = para_crc16(MICO_PARTITION_PARAMETER_1);
  para_offset = partition->partition_length - CRC_SIZE;
  err = MicoFlashWrite( MICO_PARTITION_PARAMETER_1, &para_offset, (uint8_t *)&crc_result, CRC_SIZE );
  require_noerr(err, exit);
  
  /* Read back*/
  para_offset = partition->partition_length - CRC_SIZE;
  err = MicoFlashRead( MICO_PARTITION_PARAMETER_1, &para_offset, (uint8_t *)&crc_readback, CRC_SIZE );
  if( crc_readback != crc_result) {
    mico_rtos_unlock_mutex( &para_flash_mutex);
    para_log( "crc_readback = %d, crc_result %d", crc_readback, crc_result);
    return kWriteErr;
  }

  partition = MicoFlashGetInfo( MICO_PARTITION_PARAMETER_2 );
  /* Write backup data*/
  err = MicoFlashErase( MICO_PARTITION_PARAMETER_2, 0x0, partition->partition_length );
  require_noerr(err, exit);

  para_offset = 0x0;
  err = MicoFlashWrite( MICO_PARTITION_PARAMETER_2, &para_offset, (uint8_t *)&inContext->flashContentInRam, sizeof(system_config_t));
  require_noerr(err, exit);

  para_offset = mico_context_section_offsets[ PARA_APP_DATA_SECTION ];
  err = MicoFlashWrite( MICO_PARTITION_PARAMETER_2, &para_offset, inContext->user_config_data, inContext->user_config_data_size );
  require_noerr(err, exit);
#ifdef MICO_EXTRA_AP_NUM
  para_offset = mico_context_section_offsets[ PARA_SYS_EXTRA_SECTION ];
  err = MicoFlashWrite( MICO_PARTITION_PARAMETER_2, &para_offset, (uint8_t *)inContext->extra_ap, sizeof(inContext->extra_ap) );
  require_noerr(err, exit);
#endif

  para_offset = partition->partition_length - CRC_SIZE;
  err = MicoFlashWrite( MICO_PARTITION_PARAMETER_2, &para_offset, (uint8_t *)&crc_result, CRC_SIZE );
  require_noerr(err, exit);

exit:
  mico_rtos_unlock_mutex( &para_flash_mutex);
  return err;
}

OSStatus mico_system_context_restore( mico_Context_t * const inContext )
{ 
  OSStatus err = kNoErr;
  require_action( inContext, exit, err = kNotPreparedErr );

  /*wlan configration is not need to change to a default state, use easylink to do that*/
  memset(&sys_context->flashContentInRam, 0x0, sizeof(system_config_t));
  sprintf(sys_context->flashContentInRam.micoSystemConfig.name, DEFAULT_NAME);
  sys_context->flashContentInRam.micoSystemConfig.configured = unConfigured;
  sys_context->flashContentInRam.micoSystemConfig.easyLinkByPass = EASYLINK_BYPASS_NO;
  sys_context->flashContentInRam.micoSystemConfig.rfPowerSaveEnable = false;
  sys_context->flashContentInRam.micoSystemConfig.mcuPowerSaveEnable = false;
  sys_context->flashContentInRam.micoSystemConfig.magic_number = SYS_MAGIC_NUMBR;
  sys_context->flashContentInRam.micoSystemConfig.seed = seedNum;
#ifdef MICO_BLUETOOTH_ENABLE
  memset(&sys_context->flashContentInRam.bt_config, 0xFF, sizeof(mico_bt_config_t));
#endif
#ifdef MICO_EXTRA_AP_NUM
  memset(&sys_context->extra_ap, 0, sizeof(sys_context->extra_ap));
#endif

  /*Application's default configuration*/
  appRestoreDefault_callback(sys_context->user_config_data, sys_context->user_config_data_size);

  para_log("Restore to default");

exit:
  return err;
}

#ifdef MFG_MODE_AUTO
OSStatus MICORestoreMFG( void )
{ 
  OSStatus err = kNoErr;
  require_action( sys_context, exit, err = kNotPreparedErr );

  /*wlan configration is not need to change to a default state, use easylink to do that*/
  sprintf(sys_context->flashContentInRam.micoSystemConfig.name, DEFAULT_NAME);
  sys_context->flashContentInRam.micoSystemConfig.configured = mfgConfigured;
  sys_context->flashContentInRam.micoSystemConfig.magic_number = SYS_MAGIC_NUMBR;

  /*Application's default configuration*/
  appRestoreDefault_callback(sys_context->user_config_data, sys_context->user_config_data_size);

  err = internal_update_config( sys_context );
  require_noerr(err, exit);

exit:
  return err;
}
#endif

static int para_copy(mico_partition_t dst, mico_partition_t src)
{
    uint32_t offset, len = 1024, end, dst_offset;
    uint8_t *tmp;
    mico_logic_partition_t *partition;
    
    tmp = (uint8_t*)malloc(1024);
    if (tmp == NULL)
        return -1;
    
    partition = MicoFlashGetInfo( dst );

    MicoFlashErase( dst ,0x0, partition->partition_length );

    offset = mico_context_section_offsets[ PARA_MICO_DATA_SECTION ]; // ignore boottable
    dst_offset = offset;
    end = partition->partition_length;
    while(offset < end) {
        if (offset + len > end)
            len = end - offset;
        MicoFlashRead( src, &offset, tmp, len);
        MicoFlashWrite( dst, &dst_offset, tmp, len);
    }

    free(tmp);
    
    return 0;
}

OSStatus MICOReadConfiguration(system_context_t *inContext)
{
  uint32_t para_offset = 0x0;
  uint32_t crc_offset;
  uint16_t crc_result, crc_target;
  uint16_t crc_backup_result, crc_backup_target;
  mico_logic_partition_t *partition; 

  mico_Context_t *mico_context = mico_system_context_get();
  
  OSStatus err = kNoErr;

  require_action(inContext, exit, err = kNotPreparedErr);

  partition = MicoFlashGetInfo( MICO_PARTITION_PARAMETER_1 );
  crc_result = para_crc16(MICO_PARTITION_PARAMETER_1);
  para_log( "crc_result = %d", crc_result);
  crc_offset = partition->partition_length - CRC_SIZE;
  err = MicoFlashRead( MICO_PARTITION_PARAMETER_1, &crc_offset, (uint8_t *)&crc_target, CRC_SIZE );
  para_log( "crc_target = %d", crc_target);

  crc_backup_result = para_crc16(MICO_PARTITION_PARAMETER_2);
  para_log( "crc_backup_result = %d", crc_backup_result);
  crc_offset = partition->partition_length - CRC_SIZE;
  err = MicoFlashRead( MICO_PARTITION_PARAMETER_2, &crc_offset, (uint8_t *)&crc_backup_target, CRC_SIZE );
  para_log( "crc_backup_target = %d", crc_backup_target);
  
  /* Data collapsed at main partition */
  if( is_crc_match( crc_result, crc_target ) == false ){
    /* Data collapsed at main partition and backup partition both, restore to default */
    if( is_crc_match( crc_backup_result, crc_backup_target ) == false ){
      para_log("Config failed on both partition, try old partition!");
      err = try_old_para( inContext );
      require_noerr(err, exit);
    }
    /* main collapsed, backup correct, copy data from back up to main */
    else {
      para_log("Config failed on main, recover!");

      para_copy(MICO_PARTITION_PARAMETER_1, MICO_PARTITION_PARAMETER_2);
    }
  }   
  /* main correct */
  else { 
      /* main correct , backup collapsed, or main!=backup, copy data from main to back up */
    if( is_crc_match( crc_result, crc_backup_result ) == false || is_crc_match( crc_backup_result, crc_backup_target ) == false ){
      para_log("Config failed on backup, recover!");

      para_copy(MICO_PARTITION_PARAMETER_2, MICO_PARTITION_PARAMETER_1);
    }
  }

  partition = MicoFlashGetInfo( MICO_PARTITION_PARAMETER_1 );
  /* Load data and crc from main partition */
  para_offset = 0x0;
  err = MicoFlashRead( MICO_PARTITION_PARAMETER_1, &para_offset, (uint8_t *)&inContext->flashContentInRam, sizeof( system_config_t ) );
  para_offset = mico_context_section_offsets[ PARA_APP_DATA_SECTION ];
  err = MicoFlashRead( MICO_PARTITION_PARAMETER_1, &para_offset, (uint8_t *)inContext->user_config_data, inContext->user_config_data_size );
#ifdef MICO_EXTRA_AP_NUM
  para_offset = mico_context_section_offsets[ PARA_SYS_EXTRA_SECTION ];
  err = MicoFlashRead( MICO_PARTITION_PARAMETER_1, &para_offset, (uint8_t *)inContext->extra_ap, sizeof(inContext->extra_ap) );
#endif
  

  if(inContext->flashContentInRam.micoSystemConfig.magic_number != SYS_MAGIC_NUMBR){
    para_log("Magic number error, restore to default");
#ifdef MFG_MODE_AUTO
    err = MICORestoreMFG( );
#else
    err = mico_system_context_restore( mico_context );
#endif
    require_noerr(err, exit);
  }
  para_log(" Config read, seed = %d!", inContext->flashContentInRam.micoSystemConfig.seed);
  
  seedNum = inContext->flashContentInRam.micoSystemConfig.seed;
  if(seedNum == -1) seedNum = 0;

  if(inContext->flashContentInRam.micoSystemConfig.dhcpEnable == DHCP_Disable){
    strcpy((char *)inContext->micoStatus.localIp, inContext->flashContentInRam.micoSystemConfig.localIp);
    strcpy((char *)inContext->micoStatus.netMask, inContext->flashContentInRam.micoSystemConfig.netMask);
    strcpy((char *)inContext->micoStatus.gateWay, inContext->flashContentInRam.micoSystemConfig.gateWay);
    strcpy((char *)inContext->micoStatus.dnsServer, inContext->flashContentInRam.micoSystemConfig.dnsServer);
  }

  if (_encrypt_use)
  {
    Aes aes;

    AesSetKey(&aes, _encrypt_key, AES_BLOCK_SIZE, _encrypt_key, AES_DECRYPTION);
    AesCbcDecrypt(&aes, (byte *)sys_context->flashContentInRam.micoSystemConfig.user_key, (const byte *)sys_context->flashContentInRam.micoSystemConfig.user_key, maxKeyLen);
  }

exit: 

  return err;
}

OSStatus mico_system_context_update( mico_Context_t *in_context )
{
  OSStatus err = kNoErr;
  require_action( in_context, exit, err = kNotPreparedErr );

  sys_context->flashContentInRam.micoSystemConfig.seed = ++seedNum;

  err = internal_update_config( sys_context );
  require_noerr(err, exit); 

exit:
  return err;
}

void mico_system_context_set_passwd_encrypt(uint8_t key[ENCRYPT_KEY_LEN])
{
  _encrypt_use = true;
  memcpy(_encrypt_key, key, ENCRYPT_KEY_LEN);
}

void mico_system_context_set_ssid_passwd(char *ssid, char *passwd)
{
  strncpy(sys_context->flashContentInRam.micoSystemConfig.ssid, ssid, maxSsidLen);
  if (passwd != NULL)
  {
    strncpy(sys_context->flashContentInRam.micoSystemConfig.user_key, passwd, maxKeyLen);
    sys_context->flashContentInRam.micoSystemConfig.user_keyLength = strlen(passwd);
  }
}

//static mico_Context_t * inContext=0;
/******************************************************
 *               Function Definitions
 ******************************************************/
static uint32_t system_context_get_para_data( para_section_t section )
{
    uint32_t data_ptr = 0;
    require( sys_context, exit );
    require( section <= PARA_END_SECTION, exit );

    /* para_data stored in RAM, PARA_APP_DATA_SECTION is a seperate section */
    if ( section == PARA_APP_DATA_SECTION )
        data_ptr = (uint32_t) sys_context->user_config_data;
    else if ( section == PARA_END_SECTION )
        data_ptr = (uint32_t) sys_context->user_config_data + sys_context->user_config_data_size + 1;
    else
        data_ptr = (uint32_t) sys_context + mico_context_section_offsets[section];

exit:
    return data_ptr;
}


OSStatus mico_system_para_read(void** info_ptr, int section, uint32_t offset, uint32_t size)
{
  OSStatus err = kNoErr;
  uint32_t addr_sec = system_context_get_para_data( (para_section_t)section );
  mico_Context_t *mico_context = mico_system_context_get();

  require_action( mico_context, exit, err = kNotPreparedErr );
  require_action( (addr_sec + offset + size) < system_context_get_para_data( (para_section_t)(section + 1) ), exit, err = kSizeErr);

  *info_ptr = (void *)(addr_sec + offset);

exit:
  return err;
}

OSStatus mico_system_para_read_release( void* info_ptr )
{
  UNUSED_PARAMETER( info_ptr );
  return true;
}

OSStatus mico_system_para_write(const void* info_ptr, int section, uint32_t offset, uint32_t size)
{
  OSStatus err = kNoErr;
  uint32_t addr_sec = system_context_get_para_data( (para_section_t)section );
  mico_Context_t *mico_context = mico_system_context_get();

  require_action( mico_context, exit, err = kNotPreparedErr );
  require_action( (addr_sec + offset + size) < system_context_get_para_data( (para_section_t)(section + 1) ), exit, err = kSizeErr );

  memcpy( (void *)(addr_sec + offset), info_ptr, size);
  mico_system_context_update(mico_context);
  
exit:
  return err;
}


OSStatus mico_ota_switch_to_new_fw( int ota_data_len, uint16_t ota_data_crc )
{
    mico_Context_t *mico_context = mico_system_context_get();
#ifdef MICO_ENABLE_SECONDARY_APPLICATION
    UNUSED_PARAMETER( ota_data_len );
    UNUSED_PARAMETER( ota_data_crc );
    extern int switch_active_firmware(void);
    switch_active_firmware();
#else
    mico_logic_partition_t* ota_partition = MicoFlashGetInfo( MICO_PARTITION_OTA_TEMP );

    memset( &sys_context->flashContentInRam.bootTable, 0, sizeof(boot_table_t) );
#ifdef CONFIG_MX108
    sys_context->flashContentInRam.bootTable.dst_adr = 0x13200;
    sys_context->flashContentInRam.bootTable.src_adr = ota_partition->partition_start_addr;
    sys_context->flashContentInRam.bootTable.siz = ota_data_len;
    sys_context->flashContentInRam.bootTable.crc = ota_data_crc;
#else
    sys_context->flashContentInRam.bootTable.length = ota_data_len;
    sys_context->flashContentInRam.bootTable.start_address = ota_partition->partition_start_addr;
    sys_context->flashContentInRam.bootTable.type = 'A';
    sys_context->flashContentInRam.bootTable.upgrade_type = 'U';
    sys_context->flashContentInRam.bootTable.crc = ota_data_crc;
#endif
    mico_system_context_update( mico_context );
#endif
    return kNoErr;
}

static int is_old_part_crc_match(system_context_t *inContext, mico_partition_t part)
{
    uint32_t para_offset = 0x0;
    //uint32_t config_offset = CONFIG_OFFSET;
    uint32_t crc_offset = mico_context_section_offsets[ PARA_APP_DATA_SECTION ] + inContext->user_config_data_size;;
    CRC16_Context crc_context;
    uint16_t crc_result, crc_target;
    
    para_offset = 0x0;
    MicoFlashRead( part, &para_offset, (uint8_t *)&inContext->flashContentInRam, sizeof( system_config_t ) );
    para_offset = mico_context_section_offsets[ PARA_APP_DATA_SECTION ];
    MicoFlashRead( part, &para_offset, (uint8_t *)inContext->user_config_data, inContext->user_config_data_size );

    CRC16_Init( &crc_context );
    CRC16_Update( &crc_context, (uint8_t *)&inContext->flashContentInRam.micoSystemConfig, SYS_CONFIG_SIZE );
    CRC16_Update( &crc_context, inContext->user_config_data, inContext->user_config_data_size );
    CRC16_Final( &crc_context, &crc_result );
    para_log( "crc_result = %d", crc_result);

    crc_offset = mico_context_section_offsets[ PARA_APP_DATA_SECTION ] + inContext->user_config_data_size;;
    MicoFlashRead( part, &crc_offset, (uint8_t *)&crc_target, CRC_SIZE );
    para_log( "crc_target = %d", crc_target);

    if( is_crc_match( crc_result, crc_target ) == true ) {
        return true;
    } else {
        return false;
    }
}

/* Try to use the OLD mico para save method */
static OSStatus try_old_para(system_context_t *inContext)
{
    OSStatus err = kNoErr;
    mico_Context_t *mico_context = mico_system_context_get();
    
    /* Load data and crc from main partition */
    if (is_old_part_crc_match(inContext, MICO_PARTITION_PARAMETER_1) == true) {
        para_log("Main partition CRC correct");
        mico_system_context_update(mico_context);
        return kNoErr;
    }
    /* Load data and crc from backup partition */
    if (is_old_part_crc_match(inContext, MICO_PARTITION_PARAMETER_2) == true) {
        para_log("Backup partition CRC correct");
        mico_system_context_update(mico_context);
        return kNoErr;
    }
    para_log("Config failed on both partition, restore to default settings!");
    err = mico_system_context_restore( mico_context );
    
    return err;
} 


