/**
 * @author  snow yang
 * @email   snowyang.iot@gmail.com
 * @date    2020-04-01 11:41:03
 */

#ifndef _IMC_H_
#define _IMC_H_

#include <stdint.h>
#include <string.h>

#define IMC_MAX_N (0xFF)

typedef void (*imc_func_t)(uint8_t *data, int n);

typedef struct
{
    imc_func_t handler[IMC_MAX_N];
    imc_func_t output;
} imc_t;

imc_t *imc_new(imc_func_t output);
void imc_send(imc_t *imc, uint8_t type, uint8_t *data, int n);
void imc_input(imc_t *imc, uint8_t *data, int n);
void imc_reg(imc_t *imc, uint8_t type, imc_func_t handler);
void imc_delete(imc_t *imc);

#endif
