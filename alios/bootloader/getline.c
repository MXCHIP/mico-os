/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "hal/soc/soc.h"
#include "aos/kernel.h"
#include "board.h"
#include "bootloader.h"

#define CNTLQ      0x11
#define CNTLS      0x13
#define DEL        0x7F
#define BACKSPACE  0x08
#define CR         0x0D
#define LF         0x0A
#define SPACE      0x20

static void uart_putchar( int c )
{
  hal_uart_send( STDIO_UART, &c, 1 );
}

/***************/
/* Line Editor */
/***************/
void getline (char *line, int n)  {
  int  cnt = 0;
  char c;

  do  {
    hal_uart_recv_II( STDIO_UART, &c, 1, NULL, AOS_WAIT_FOREVER);
    if (c == CR)  c = LF;     /* read character                 */
    if (c == BACKSPACE  ||  c == DEL)  {    /* process backspace              */
      if (cnt != 0)  {
        cnt--;                              /* decrement count                */
        line--;                             /* and line pointer               */
        uart_putchar (BACKSPACE);                /* echo backspace                 */
        uart_putchar (' ');
        uart_putchar (BACKSPACE);
      }
    }
    else if (c != CNTLQ && c != CNTLS)  {   /* ignore Control S/Q             */
      uart_putchar (*line = c);             /* echo and store character       */
      line++;                               /* increment line pointer         */
      cnt++;                                /* and count                      */
    }
  }  while (cnt < n - 1  &&  c != LF);      /* check limit and line feed      */
  *(line - 1) = 0;                          /* mark end of string             */
}

#ifdef STDIO_BREAK_TO_MENU
int stdio_break_in(void)
{
    uint8_t c;
    int i, j;
    
    for(i=0, j=0; i<5; i++)
    {
      if (0 != hal_uart_recv_II( STDIO_UART, &c, 1, NULL, 100)) 
        continue;

      if (c == SPACE) 
      {
        if (j++ > 3)
          return 1; 
      } 
      else 
      {
        j = 0;
      }
    }

    return 0;
}
#endif

