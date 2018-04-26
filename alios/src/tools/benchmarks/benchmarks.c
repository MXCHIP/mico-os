#include <string.h>
#include "aos/aos.h"

static void benchmarks_command(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    char         *method;
    char          number[256]={"41757646344124860145938033"};
    char         *cfrac_argv[2]={0};
    unsigned int  begintime,endtime;

    if(argc >= 2) {
        method = argv[1];
        if(strcmp(method,"cfrac")==0){
            if(argc > 2) {
                strncpy(number,argv[2],255);
            }
            cfrac_argv[0] = "cfrac";
            cfrac_argv[1] = number;
            begintime = aos_now_ms();
            cfrac_main(2, cfrac_argv);
            endtime = aos_now_ms();
            cli_printf("\r\ncfrac duration time=%ldms!\r\n",endtime > begintime ? endtime-begintime : begintime-endtime);
        }
        else{
            cli_printf("%s not support!\r\n",argv[1]);
            return;
        }
    }
    else
        cli_printf("benchmarks cfrac <number>\r\n");

}

static const struct cli_command user_clis[] =
{
    {"benchmarks", "benchmarks cfrac <number>", benchmarks_command},
};


void benchmark_cli_init()
{
    cli_register_commands(&user_clis[0],
                              sizeof(user_clis) /
                              sizeof(struct cli_command));

}

