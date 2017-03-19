/**
 ******************************************************************************
 * @file    airkiss_discovery.c
 * @author  William Xu
 * @version V1.0.0
 * @date    28-10-2015
 * @brief   WECHAT device discovery protocol for airkiss extension
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico.h"
#include "SocketUtils.h"
#include "airkiss.h"

//#define airkiss_log(M, ...) custom_log("AIRKISS", M, ##__VA_ARGS__)
#define airkiss_log(M, ...)

#define LOCAL_UDP_PORT  12476
#define REMOTE_UDP_PORT 12476

#define Major_Version                   (1)
#define Minor_Version                   (0)
#define Revision                        (0)

const airkiss_config_t akconf =
{
  (airkiss_memset_fn)&memset, 
  (airkiss_memcpy_fn)&memcpy, 
  (airkiss_memcmp_fn)&memcmp, 0
};

static void ak_discovery_thread( uint32_t arg);

static char *_appid = NULL;
static char *_deviceid = NULL;
static bool _started = false;

void airkiss_discovery_lib_version( uint8_t *major, uint8_t *minor, uint8_t *revision )
{
  if( major ) *major = Major_Version;
  if( minor ) *minor = Minor_Version;
  if( revision ) *revision = Revision;
}

OSStatus airkiss_discovery_start( char *appid, char *deviceid )
{
  OSStatus err = kNoErr;

  _appid = appid;
  _deviceid = deviceid;

  err = mico_rtos_create_thread(NULL, MICO_APPLICATION_PRIORITY, "ak discovery", ak_discovery_thread, 0x800, NULL );
  require_noerr_string( err, exit, "ERROR: Unable to start the airkiss discovery thread." );
  _started = true;
  
exit:
  return err;
}


OSStatus airkiss_discovery_stop( void )
{
  _started = false;
  return kNoErr;
}

static void ak_discovery_thread( uint32_t arg )
{
  UNUSED_PARAMETER( arg );

  OSStatus err;
  struct sockaddr_in addr;
  socklen_t addr_len = sizeof(addr);
  struct timeval t;
  int udp_fd = -1 ;
  fd_set readfds;
  char *buf = NULL;
  ssize_t len = 0;
  int ret;

  buf = (char*)malloc( 1024 );
  require_action( buf, exit, err = kNoMemoryErr );
  
  /*Establish a UDP port to receive any data sent to this port*/
  udp_fd = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
  require_action( IsValidSocket( udp_fd ), exit, err = kNoResourcesErr );
  
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = INADDR_ANY;
  addr.sin_port = htons(LOCAL_UDP_PORT);
  err = bind(udp_fd, (struct sockaddr *)&addr, sizeof(addr));
  require_noerr( err, exit );

  t.tv_sec = 5;
  t.tv_usec = 0;

  while(1)
  {
    //udp_broadcast_log( "broadcast now!" );
    FD_ZERO( &readfds );
    FD_SET( udp_fd, &readfds );
    
    require_action( select( udp_fd + 1, &readfds, NULL, NULL, &t) >= 0, exit, err = kConnectionErr );
    
    /* recv wlan data, and send back */
    if( FD_ISSET( udp_fd, &readfds ) )
    {
      memset(buf, 0x0, 1024);
      len = recvfrom( udp_fd, buf, 1024, 0, (struct sockaddr *)&addr, &addr_len);
      require_action( len >= 0, exit, err = kConnectionErr );
      airkiss_log("Airkiss discover request received, length=%d", len);

      ret = airkiss_lan_recv(buf, len, &akconf);
      switch (ret){
        case AIRKISS_LAN_SSDP_REQ:
          len = 1024;
          ret = airkiss_lan_pack(AIRKISS_LAN_SSDP_RESP_CMD, _appid, _deviceid, 0, 0, buf, (unsigned short *)&len, &akconf); 
          require_action( ret == AIRKISS_LAN_PAKE_READY, exit, err = kMalformedErr );

          len = sendto( udp_fd, buf, len, 0, (struct sockaddr *)&addr, sizeof(addr) );
          require_action( len >= 0, exit, err = kConnectionErr );
          break;
        default:
          airkiss_log("Pack is not ssdq req!"); 
          break;
        }
    }
    if(_started == false)
      goto exit;
  }
  
exit:
  if( err != kNoErr )
    airkiss_log("Airkiss discover thread exit with err: %d", err);
  if(buf) free(buf);
  SocketClose( &udp_fd );
  mico_rtos_delete_thread(NULL);
}


