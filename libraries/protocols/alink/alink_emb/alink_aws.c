#include "alink_export.h"
#include "alink_aws.h"
#include "product.h"
#include "system_internal.h"

#define aws_log(format, ...)  custom_log("aws", format, ##__VA_ARGS__)

static void aws_config_thread( uint32_t arg )
{
    OSStatus err = kNoErr;

    mico_system_delegate_config_will_start();
    aws_log("start alink_emb aws mode");

    awss_easylink_start( );
    err = awss_start();

    if( err != kNoErr ){
        mico_system_delegate_config_will_stop();
        MicoSysLed(false);
        aws_log("aws timeout err %d", err);
    }else{
        aws_log("aws success");
        awss_easylink_stop();
    }

    mico_rtos_delete_thread( NULL );
}

OSStatus start_aws_config_mode( void )
{
    return mico_rtos_create_thread( NULL, MICO_APPLICATION_PRIORITY, "aws", aws_config_thread,
                                    0x800, 0 );
}


