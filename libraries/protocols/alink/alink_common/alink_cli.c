#include "command_console/mico_cli.h"
#include "mico.h"

#define cli_user_log(M, ...) custom_log("CLI_USER", M, ##__VA_ARGS__)

extern void PlatformEasyLinkButtonClickedCallback( void );

static void aws_mode( char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv )
{
    PlatformEasyLinkButtonClickedCallback( );
    cli_user_log("enter aws config mode...");
}

static const struct cli_command user_clis[] = {
    { "aws", "enter aws config mode", aws_mode },
};

void alink_cli_user_commands_register( void )
{
    cli_register_commands( user_clis, sizeof(user_clis) / sizeof(struct cli_command) );
}

