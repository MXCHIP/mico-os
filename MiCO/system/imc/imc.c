/**
 * @author  snow yang
 * @email   snowyang.iot@gmail.com
 * @date    2020-04-01 11:47:17
 */

#include <stdlib.h>
#include <string.h>
#include "imc.h"

imc_t *imc_new(imc_func_t output)
{
    imc_t *imc = malloc(sizeof(imc_t));
    memset(imc, 0, sizeof(imc_t));
    imc->output = output;
    return imc;
}

void imc_delete(imc_t *imc)
{
    free(imc);
}

void imc_send(imc_t *imc, uint8_t type, uint8_t *buf, int n)
{
    uint8_t *data = malloc(1 + n);
    data[0] = type;
    memcpy(data + 1, buf, n);
    imc->output(data, 1 + n);
    free(data);
}

void imc_input(imc_t *imc, uint8_t *data, int n)
{
    uint8_t type = data[0];
    imc_func_t handler = imc->handler[type];
    if (handler != NULL)
    {
        handler(&data[1], n - 1);
    }
}

void imc_reg(imc_t *imc, uint8_t type, imc_func_t handler)
{
    imc->handler[type] = handler;
}
