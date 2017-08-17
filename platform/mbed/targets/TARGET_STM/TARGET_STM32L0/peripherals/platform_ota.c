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
#include "mico.h"

int switch_active_firmware(void)
{
    OSStatus err = kUnknownErr;
    FLASH_AdvOBProgramInitTypeDef AdvOBInit;
    HAL_StatusTypeDef hal_result;
    /* Set BFB2 bit to enable boot from Flash Bank2 */
    /* Allow Access to Flash control registers and user Flash */
    HAL_FLASH_Unlock();

    /* Clear OPTVERR bit set on virgin samples */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_OPTVERR);

    /* Allow Access to option bytes sector */
    HAL_FLASH_OB_Unlock();

    /* Get the Dual boot configuration status */
    AdvOBInit.OptionType = OPTIONBYTE_BOOTCONFIG;
    HAL_FLASHEx_AdvOBGetConfig(&AdvOBInit);

    /* Switch Boot bank */
    if (AdvOBInit.BootConfig == OB_BOOT_BANK2)
    {
        /* At startup, if boot pin 0 and BOOT1 bit are set in boot from user Flash position
           and this parameter is selected the device will boot from Bank 1 */
        AdvOBInit.BootConfig = OB_BOOT_BANK1;
    }
    else
    {
        /* At startup, if boot pin 0 and BOOT1 bit are set in boot from user Flash position
           and this parameter is selected the device will boot from Bank 2 */
        AdvOBInit.BootConfig = OB_BOOT_BANK2;
    }

    hal_result = HAL_FLASHEx_AdvOBProgram (&AdvOBInit);
    require_noerr_quiet(hal_result,exit);

    hal_result = HAL_FLASH_OB_Launch();
    require_noerr_quiet(hal_result,exit);

    exit:
    return err;
}
