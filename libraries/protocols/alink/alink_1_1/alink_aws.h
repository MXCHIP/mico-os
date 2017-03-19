#include "alink_config.h"
#include "mico.h"

#ifndef _ALINK_AWS_H
#define _ALINK_AWS_H

#define STR_SSID_LEN        (32 + 1)
#define STR_PASSWD_LEN      (64 + 1)

///////////////softap tcp server sample/////////////////////
#define SOFTAP_GATEWAY_IP       "172.31.254.250"
#define SOFTAP_TCP_SERVER_PORT      (65125)

char *vendor_get_model( void );
char *vendor_get_secret( void );
char *vendor_get_mac( void );

OSStatus start_aws_config_mode( void );

OSStatus aws_softap_tcp_server( char ssid[STR_SSID_LEN], char passwd[STR_PASSWD_LEN] );

#endif
