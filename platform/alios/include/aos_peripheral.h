/* MiCO Team
 * Copyright (c) 2017 MXCHIP Information Tech. Co.,Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __PLATFORM_PERIPHERAL_H__
#define __PLATFORM_PERIPHERAL_H__

#ifdef __cplusplus
extern "C" {
#endif


#ifdef ALIOS_DEV_GPIO
#include "aos_gpio.h"
#else
#include "platform_gpio.h"
#endif

#ifdef ALIOS_DEV_UART
#include "aos_uart.h"
#else
#include "platform_uart.h"
#endif

#ifdef ALIOS_DEV_I2C
#include "aos_i2c.h"
#else
#include "platform_i2c.h"
#endif

#ifdef ALIOS_DEV_SPI
#include "aos_spi.h"
#else
#include "platform_spi.h"
#endif

#include "platform_flash.h"
#include "platform_iis.h"

#endif


#ifdef __cplusplus
} /*"C" */
#endif



