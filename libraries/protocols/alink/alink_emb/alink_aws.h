#include "alink_config.h"
#include "mico.h"

#ifndef _ALINK_AWS_H
#define _ALINK_AWS_H

#define FTC_PORT 8000

OSStatus awss_easylink_start( void );

OSStatus awss_easylink_stop( void );

OSStatus start_aws_config_mode( void );

#endif
