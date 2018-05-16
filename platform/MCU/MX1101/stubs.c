#include "mico.h"

void memory_show_Command( char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv )
{
    micoMemInfo_t *p = MicoGetMemoryInfo();
    printf("number of chunks %d\r\n", p->num_of_chunks);
    printf("total memory %d\r\n", p->total_memory);
    printf("free memory %d\r\n", p->free_memory);
}


void memory_dump_Command( char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv )
{
    int i;
    uint8_t *pstart;
    uint32_t start, length;
    
    if (argc != 3) {
        printf("Usage: memdump <addr> <length>.\r\n");
        return;
    }

    start = strtoul(argv[1], NULL, 0);
    length = strtoul(argv[2], NULL, 0);
    pstart = (uint8_t*)start;
    
    for(i=0; i<length;i++) {
        printf("%02x ", pstart[i]);
        if (i % 0x10 == 0x0F) {
            printf("\r\n");
            
        }
    }
}

void memory_set_Command( char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv )
{
    uint8_t *pstart, value;
    uint32_t start;
    int i;
    
    if (argc < 3) {
        printf("Usage: memset <addr> <value 1> [<value 2> ... <value n>].\r\n");
        return;
    }
    start = strtoul(argv[1], NULL, 0);
    value = strtoul(argv[2], NULL, 0);
    pstart = (uint8_t*)start;
    *pstart = value;
    printf("Set 0x%08x %d bytes\r\n", start, argc-2);
    for(i=2;i<argc;i++) {
        value = strtoul(argv[i], NULL, 0);
        pstart[i-2] = value;
    }
}

void socket_show_Command(char *pcWriteBuffer, int xWriteBufferLen, int argc, char **argv)
{
    socket_show(pcWriteBuffer, xWriteBufferLen);
}

#define LIBRARY_VER "EMG5062.001"

char* system_lib_version(void)
{
    return LIBRARY_VER;
}

extern char* ip4addr_ntoa(const void *addr);

char * ipaddr_ntoa(const void *addr)
{
	return ip4addr_ntoa(addr);
}