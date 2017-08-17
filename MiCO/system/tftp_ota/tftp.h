/**********************************************************
Date: 		OCT 28, 2006
Project :	TFTP Server
Programer:	Reza Rahmanian, Craig Holmes
File:		Header Files
Purpose:	This file contains all the needed header files 
		for the implementation of the TFTP client and servers.
***********************************************************/
#ifndef __MICO_TFTP_H__
#define __MICO_TFTP_H__

#include <stdio.h>		/*for input and output */
#include <string.h>		/*for string functions */
#include <stdlib.h> 		/**/
#include "mico.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char     filename[32];
    uint32_t filelen;
    uint32_t flashaddr; // the flash address of this file
    int      flashtype; // SPI flash or Internal flash.
} tftp_file_info_t;


int tsend (tftp_file_info_t *fileinfo, uint32_t ipaddr);
/*a function to get a file from the server*/
int tget (tftp_file_info_t *fileinfo, uint32_t ipaddr);

#ifdef __cplusplus
}
#endif

#endif

