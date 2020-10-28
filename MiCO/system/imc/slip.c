/**
 * @author  snow yang
 * @email   snowyang.iot@gmail.com
 * @date    2020-04-07 10:33:43
 */

#include <stdlib.h>
#include "slip.h"

enum
{
    SLIP_START = 0xC0,
    SLIP_END = 0xD0,
    SLIP_ESCAPE = 0xDB,
    SLIP_ESCAPE_START = 0xDC,
    SLIP_ESCAPE_ES = 0xDD,
    SLIP_ESCAPE_END = 0xDE,
};

enum
{
    SLIP_STATE_IDLE,       // NOT START
    SLIP_STATE_CONTINUE,   // receive data[i] to buffer
    SLIP_STATE_GOT_ESCAPE, // last byte is escape
};

slip_t *slip_new(slip_output_t output)
{
    slip_t *slip = malloc(sizeof(slip_t));
    memset(slip, 0, sizeof(slip_t));
    slip->output = output;
    return slip;
}

void slip_delete(slip_t *slip)
{
    free(slip);
}

void slip_reg(slip_t *slip, slip_handler_t handler)
{
    slip->handler = handler;
}

void slip_send_start(slip_t *slip)
{
    slip->output(SLIP_START);
}

void slip_send_end(slip_t *slip)
{
    slip->output(SLIP_END);
}

void slip_send_byte(slip_t *slip, uint8_t data)
{
    switch (data)
    {
    case SLIP_START:
        slip->output(SLIP_ESCAPE);
        slip->output(SLIP_ESCAPE_START);
        break;
    case SLIP_END:
        slip->output(SLIP_ESCAPE);
        slip->output(SLIP_ESCAPE_END);
        break;
    case SLIP_ESCAPE:
        slip->output(SLIP_ESCAPE);
        slip->output(SLIP_ESCAPE_ES);
        break;
    default:
        slip->output(data);
        break;
    }
}

void slip_send_data(slip_t *slip, uint8_t *data, int n)
{
    slip_send_start(slip);
    while (n--)
    {
        slip_send_byte(slip, *data++);
    }
    slip_send_end(slip);
}

void slip_input(slip_t *slip, uint8_t data)
{
    static int i = 0;
    static int state = SLIP_STATE_IDLE;

    if (i >= SLIP_MTU)
    {
        goto RESET;
    }

    switch (state)
    {
    case SLIP_STATE_GOT_ESCAPE:
        if (data == SLIP_START)
        {
            i = 0;
        }
        else if (data == SLIP_ESCAPE_START)
        {
            slip->buffer[i++] = SLIP_START;
        }
        else if (data == SLIP_ESCAPE_ES)
        {
            slip->buffer[i++] = SLIP_ESCAPE;
        }
        else if (data == SLIP_ESCAPE_END)
        {
            slip->buffer[i++] = SLIP_END;
        }
        else
        {
            goto RESET;
        }
        state = SLIP_STATE_CONTINUE;
        break;

    case SLIP_STATE_IDLE:
        if (data == SLIP_START)
        {
            i = 0;
            state = SLIP_STATE_CONTINUE;
        }
        break;

    case SLIP_STATE_CONTINUE:
        if (data == SLIP_START)
        {
            i = 0;
            state = SLIP_STATE_CONTINUE;
        }
        else if (data == SLIP_END)
        {
            slip->handler(slip->buffer, i);
            goto RESET;
        }
        else if (data == SLIP_ESCAPE)
        {
            state = SLIP_STATE_GOT_ESCAPE;
        }
        else
        {
            slip->buffer[i++] = data;
        }
        break;

    default: // imposible
        break;
    }

    return;

RESET:
    i = 0;
    state = SLIP_STATE_IDLE;
}
