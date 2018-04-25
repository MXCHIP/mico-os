/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include <ctype.h>
#include "hal/soc/soc.h"
#include "board.h"
#include "bootloader.h"

extern int check_ota(void);
extern void boot(void);
#if STDIO_BREAK_TO_MENU
extern int stdio_break_in(void);
#endif
extern void menu_loop(void);

const char menu[] =
"\r\n"
"AOS bootloader for %s, %s, HARDWARE_REVISION: %s\r\n"
"+ command -------------------------+ function ------------+\r\n"
"| 0:BOOTUPDATE    <-r>             | Update bootloader    |\r\n"
"| 1:FWUPDATE      <-r>             | Update application   |\r\n"
"| 2:DRIVERUPDATE  <-r>             | Update RF driver     |\r\n"
"| 3:PARUPDATE     <-id n><-r><-e>  | Update MICO partition|\r\n"
"| 4:FLASHUPDATE   <-dev device>    |                      |\r\n"
"|  <-e><-r><-start addr><-end addr>| Update flash content |\r\n"
"| 5:MEMORYMAP                      | List flash memory map|\r\n"
"| 6:BOOT                           | Excute application   |\r\n"
"| 7:REBOOT                         | Reboot               |\r\n"
"+----------------------------------+----------------------+\r\n";

int main(void)
{
  check_ota();

#if STDIO_BREAK_TO_MENU
  if (stdio_break_in())
    goto MENU;
#endif

  boot();

#if STDIO_BREAK_TO_MENU
  MENU:
#endif

  printf ( menu, MODEL, Bootloader_REVISION, HARDWARE_REVISION );

  for(;;)
  {                             
    menu_loop();
  }
}


