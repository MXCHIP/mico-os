/*
 * Copyright (C) 2015-2017 Alibaba Group Holding Limited
 */

#include "hal/soc/soc.h"
#include <aos/kernel.h>
#include <aos/aos.h>

/* Logic partition on flash devices */
const hal_logic_partition_t hal_partitions[] =
{
	[HAL_PARTITION_BOOTLOADER] =
	{
	    .partition_owner            = HAL_FLASH_EMBEDDED,
	    .partition_description      = "Bootloader",
	    .partition_start_addr       = 0x0,
	    .partition_length           = 0x8000,    //32k bytes
	    .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_DIS,
	},
	[HAL_PARTITION_APPLICATION] =
	{
	    .partition_owner            = HAL_FLASH_EMBEDDED,
	    .partition_description      = "Application",
	    .partition_start_addr       = 0xB000,
	    .partition_length           = 0xF2000, //568k bytes
	    .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
	},
        
	[HAL_PARTITION_PARAMETER_1] =
        {
        .partition_owner            = HAL_FLASH_EMBEDDED,
        .partition_description      = "PARAMETER1",
        .partition_start_addr       = 0xFD000,
        .partition_length           = 0x1000, // 4k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
        },
        
        [HAL_PARTITION_PARAMETER_2] =
        {
        .partition_owner            = HAL_FLASH_EMBEDDED,
        .partition_description      = "PARAMETER2",
        .partition_start_addr       = 0xFE000,
        .partition_length           = 0x2000, // 8k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
        },

        [HAL_PARTITION_OTA_TEMP] =
        {
        .partition_owner           = HAL_FLASH_EMBEDDED,
        .partition_description     = "OTA Storage",
        .partition_start_addr      = 0x100000,
        .partition_length          = 0x8E000, //568k bytes
        .partition_options         = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
        },
    [HAL_PARTITION_PARAMETER_3] =
    {
        .partition_owner            = HAL_FLASH_EMBEDDED,
        .partition_description      = "PARAMETER3",
        .partition_start_addr       = 0x1FD000,
        .partition_length           = 0x1000, // 4k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
    [HAL_PARTITION_PARAMETER_4] =
    {
        .partition_owner            = HAL_FLASH_EMBEDDED,
        .partition_description      = "PARAMETER4",
        .partition_start_addr       = 0x1FE000,
        .partition_length           = 0x2000,// 8k bytes
        .partition_options          = PAR_OPT_READ_EN | PAR_OPT_WRITE_EN,
    },
};

#define KEY_AWSS   12

static uint64_t   awss_time = 0;
static gpio_dev_t gpio_key_awss;

static void key_poll_func(void *arg)
{
    uint32_t level;
    uint64_t diff;

    hal_gpio_input_get(&gpio_key_awss, &level);

    if (level == 0) {
        aos_post_delayed_action(10, key_poll_func, NULL);
    } else {
        diff = aos_now_ms() - awss_time;
        if (diff > 6000) { /*long long press */
            awss_time = 0;
            aos_post_event(EV_KEY, CODE_BOOT, VALUE_KEY_LLTCLICK);
        } else if (diff > 2000) { /* long press */
            awss_time = 0;
            aos_post_event(EV_KEY, CODE_BOOT, VALUE_KEY_LTCLICK);
        } else if (diff > 40) { /* short press */
            awss_time = 0;
            aos_post_event(EV_KEY, CODE_BOOT, VALUE_KEY_CLICK);
        } else {
            aos_post_delayed_action(10, key_poll_func, NULL);
        }
    }
}

static void key_proc_work(void *arg)
{
    aos_schedule_call(key_poll_func, NULL);
}

static void handle_awss_key(void *arg)
{
    uint32_t gpio_value;

    hal_gpio_input_get(&gpio_key_awss, &gpio_value);
    if (gpio_value == 0 && awss_time == 0) {
        awss_time = aos_now_ms();
        aos_loop_schedule_work(0, key_proc_work, NULL, NULL, NULL);
    }
}

void board_init(void)
{
    gpio_key_awss.port = KEY_AWSS;
    gpio_key_awss.config = INPUT_PULL_UP;

    hal_gpio_init(&gpio_key_awss);
    hal_gpio_enable_irq(&gpio_key_awss, IRQ_TRIGGER_FALLING_EDGE, handle_awss_key, NULL);
}

#include <stdio.h>
#include <string.h>
#include <CheckSumUtils.h>

#define PRODUCT_KEY_MAXLEN          (20)
#define DEVICE_NAME_MAXLEN          (32)
#define DEVICE_SECRET_MAXLEN        (64)

static char pk[PRODUCT_KEY_MAXLEN + 1];
static char ps[DEVICE_SECRET_MAXLEN + 1];
static char dn[DEVICE_NAME_MAXLEN + 1];
static char ds[DEVICE_SECRET_MAXLEN + 1];

void Board_SecrectInit(void)
{
    static bool init = false;
    if(init)
        return;

    init = true;

    uint32_t offset = 0x00;
    char magic[2];
    hal_flash_read(HAL_PARTITION_LINK_KEY, &offset, magic, 2);
    if(magic[0] != 'I' || magic[1] != 'D')
    {
        printf("\r\n******** [ERROR] Magic number of link key isn't match ********\r\n");
        return;
    }

    uint8_t len;
    hal_flash_read(HAL_PARTITION_LINK_KEY, &offset, &len, 1);

    uint8_t *data = malloc(len);
    hal_flash_read(HAL_PARTITION_LINK_KEY, &offset, data, len);

    uint16_t crc;
    hal_flash_read(HAL_PARTITION_LINK_KEY, &offset, &crc, 2);

    uint16_t crc_tmp;
    CRC16_Context contex;
    CRC16_Init( &contex );
    CRC16_Update( &contex, data, len);
    CRC16_Final( &contex, &crc_tmp );
    if(crc != crc_tmp)
    {
        printf("\r\n******** [ERROR] CRC of link key isn't match ********\r\n");
        free(data);
        return;
    }

    uint8_t *tmp = data;
    // Product key
    len = strlen(tmp);
    strcpy(pk, tmp);
    tmp += len + 1;

    // Product secrect
    len = strlen(tmp);
    strcpy(ps, tmp);
    tmp += len + 1;

    // Device secrect
    len = strlen(tmp);
    strcpy(ds, tmp);
    tmp += len + 1;

    // Device name
    len = strlen(tmp);
    strcpy(dn, tmp);
    tmp += len + 1;

    printf("\r\n");
    printf("Secrect Init\r\n");
    printf("================================\r\n");
    printf("Product key: %s\r\n", pk);
    printf("Product secrect: %s\r\n", ps);
    printf("Device name: %s\r\n", dn);
    printf("Device secrect: %s\r\n", ds);
    printf("================================\r\n");

    free(data);
}

int Platform_GetProductKey(char product_key[PRODUCT_KEY_MAXLEN])
{
    int len = strlen(pk);
    strncpy(product_key, pk, len);
    return len;
}

int Platform_GetDeviceName(char device_name[DEVICE_NAME_MAXLEN])
{
    int len = strlen(dn);
    strncpy(device_name, dn, len);
    return len;
}

int Platform_GetDeviceSecret(char device_secret[DEVICE_SECRET_MAXLEN])
{
    int len = strlen(ds);
    strncpy(device_secret, ds, len);
    return len;
}

int Platform_GetProductSecret(char product_secret[DEVICE_SECRET_MAXLEN])
{
    int len = strlen(ps);
    strncpy(product_secret, ps, len);
    return len;
}

#if 1
void Board_SecrectUpdate(int argc, char **argv)
{
    uint32_t offset = 0x00;
    char magic[2] = {'I','D'};
    hal_flash_write(HAL_PARTITION_LINK_KEY, &offset, magic, 2);

    uint8_t len = strlen(argv[1])+1+strlen(argv[2])+1+strlen(argv[3])+1+strlen(argv[4])+1;
    hal_flash_write(HAL_PARTITION_LINK_KEY, &offset, &len, 1);

    hal_flash_write(HAL_PARTITION_LINK_KEY, &offset, argv[1], strlen(argv[1])+1);
    hal_flash_write(HAL_PARTITION_LINK_KEY, &offset, argv[2], strlen(argv[2])+1);
    hal_flash_write(HAL_PARTITION_LINK_KEY, &offset, argv[3], strlen(argv[3])+1);
    hal_flash_write(HAL_PARTITION_LINK_KEY, &offset, argv[4], strlen(argv[4])+1);

    uint16_t crc;
    CRC16_Context contex;
    CRC16_Init( &contex );
    CRC16_Update( &contex, argv[1], strlen(argv[1])+1);
    CRC16_Update( &contex, argv[2], strlen(argv[2])+1);
    CRC16_Update( &contex, argv[3], strlen(argv[3])+1);
    CRC16_Update( &contex, argv[4], strlen(argv[4])+1);
    CRC16_Final( &contex, &crc );
    hal_flash_write(HAL_PARTITION_LINK_KEY, &offset, &crc, 2);
}
#endif

