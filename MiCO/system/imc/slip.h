/**
 * @author  snow yang
 * @email   snowyang.iot@gmail.com
 * @date    2020-04-07 10:33:37
 */

#ifndef _SLIP_H_
#define _SLIP_H_

#include <stdint.h>
#include <string.h>

#define SLIP_MTU 1024

typedef void (*slip_handler_t)(uint8_t *data, int n);
typedef void (*slip_output_t)(uint8_t data);

typedef struct
{
    slip_handler_t handler;
    slip_output_t output;
    uint8_t buffer[SLIP_MTU];
} slip_t;

slip_t *slip_new(slip_output_t output);
void slip_delete(slip_t *slip);
void slip_reg(slip_t *slip, slip_handler_t handler);
void slip_send_start(slip_t *slip);
void slip_send_end(slip_t *slip);
void slip_send_byte(slip_t *slip, uint8_t data);
void slip_send_data(slip_t *slip, uint8_t *data, int n);
void slip_input(slip_t *slip, uint8_t data);

#endif
