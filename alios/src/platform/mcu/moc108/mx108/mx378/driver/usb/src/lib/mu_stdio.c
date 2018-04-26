/******************************************************************
*                                                                *
*        Copyright Mentor Graphics Corporation 2004              *
*                                                                *
*                All Rights Reserved.                            *
*                                                                *
*    THIS WORK CONTAINS TRADE SECRET AND PROPRIETARY INFORMATION *
*  WHICH IS THE PROPERTY OF MENTOR GRAPHICS CORPORATION OR ITS   *
*  LICENSORS AND IS SUBJECT TO LICENSE TERMS.                    *
*                                                                *
******************************************************************/

/*
* Micro-stdio library
* $Revision: 1.4 $
*/

#include "mu_dsi.h"
#include "mu_stdio.h"

/*
* implementation
*/
uint16_t MUSB_GetLine(char *pBuffer, uint16_t wBufferSize)
{
    char bChar;
    uint16_t wIndex = 0;

    bChar = MUSB_ReadConsole();
    while((wIndex < wBufferSize) &&
            ('\r' != bChar) && ('\n' != bChar) )
    {
        if(bChar == 8)		/* Back space */
        {
            if (wIndex > 0)
            {
                /*
                * Remove the last character of the command from the console
                */
                wIndex--;
                MUSB_PrintString(" \010");
            }
        }
        else
        {
            pBuffer[wIndex++] = bChar;
        }
        bChar = MUSB_ReadConsole();
    }
    if(wIndex < wBufferSize)
    {
        pBuffer[wIndex] = (char)0;
    }
    else
    {
        wIndex = 0;
    }

    return wIndex;
}

/*
* implementation
*/
uint8_t MUSB_PrintString(const char *pBuffer)
{
    const char *pChar = pBuffer;
    while(*pChar)
    {
        MUSB_WriteConsole(*pChar);
        pChar++;
    }
    return TRUE;
}

/*
* implementation
*/
uint8_t MUSB_PrintLine(const char *pBuffer)
{
    MUSB_PrintString(pBuffer);
    MUSB_WriteConsole('\r');
    MUSB_WriteConsole('\n');
    return TRUE;
}

