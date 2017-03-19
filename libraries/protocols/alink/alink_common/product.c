/*
 * Copyright (c) 2014-2015 Alibaba Group. All rights reserved.
 *
 * Alibaba Group retains all right, title and interest (including all
 * intellectual property rights) in and to this computer program, which is
 * protected by applicable intellectual property laws.  Unless you have
 * obtained a separate written license from Alibaba Group., you are not
 * authorized to utilize all or a part of this computer program for any
 * purpose (including reproduction, distribution, modification, and
 * compilation into object code), and you must immediately destroy or
 * return to Alibaba Group all copies of this computer program.  If you
 * are licensed by Alibaba Group, your rights to utilize this computer
 * program are limited by the terms of that license.  To obtain a license,
 * please contact Alibaba Group.
 *
 * This computer program contains trade secrets owned by Alibaba Group.
 * and, unless unauthorized by Alibaba Group in writing, you agree to
 * maintain the confidentiality of this computer program and related
 * information and to not disclose this computer program and related
 * information to any other person or entity.
 *
 * THIS COMPUTER PROGRAM IS PROVIDED AS IS WITHOUT ANY WARRANTIES, AND
 * Alibaba Group EXPRESSLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * INCLUDING THE WARRANTIES OF MERCHANTIBILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, TITLE, AND NONINFRINGEMENT.
 */
//#include <stdio.h>
//#include <string.h>
#include "product.h"
#include "mico_app_define.h"

char *product_get_name( char name_str[PRODUCT_NAME_LEN] )
{
    return strncpy( name_str, product_dev_name, PRODUCT_NAME_LEN );
}

char *product_get_version( char ver_str[PRODUCT_VERSION_LEN] )
{
    return strncpy( ver_str, product_dev_version, PRODUCT_VERSION_LEN );
}

char *product_get_model( char model_str[PRODUCT_MODEL_LEN] )
{
    return strncpy( model_str, product_model, PRODUCT_MODEL_LEN );
}

char *product_get_key( char key_str[PRODUCT_KEY_LEN] )
{
    return strncpy( key_str, product_key, PRODUCT_KEY_LEN );
}

char *product_get_secret( char secret_str[PRODUCT_SECRET_LEN] )
{
    return strncpy( secret_str, product_secret, PRODUCT_SECRET_LEN );
}

char *product_get_debug_key( char key_str[PRODUCT_KEY_LEN] )
{
    return strncpy( key_str, product_debug_key, PRODUCT_KEY_LEN );
}

char *product_get_debug_secret( char secret_str[PRODUCT_SECRET_LEN] )
{
    return strncpy( secret_str, product_debug_secret, PRODUCT_SECRET_LEN );
}

char *product_get_sn( char sn_str[PRODUCT_SN_LEN] )
{
    return strncpy( sn_str, product_dev_sn, PRODUCT_SN_LEN );
}

char *product_get_type( char type_str[STR_NAME_LEN] )
{
    return strncpy( type_str, product_dev_type, STR_NAME_LEN );
}

char *product_get_category( char category_str[STR_NAME_LEN] )
{
    return strncpy( category_str, product_dev_category, STR_NAME_LEN );
}

char *product_get_manufacturer( char manufacturer_str[STR_NAME_LEN] )
{
    return strncpy( manufacturer_str, product_dev_manufacturer, STR_NAME_LEN );
}

char *product_get_cid( char cid_str[STR_CID_LEN] )
{
    return strncpy( cid_str, product_dev_cid, STR_CID_LEN );
}

char *alink_get_firmware_version( char version[30] )
{
    char wifi_model[13];
    char *pos;

    sprintf( wifi_model, "%s", MODEL );
    pos = wifi_model;
    pos += (strlen( pos ) - 4);
    sprintf( version, "ALINK_%s_%s_%s_%s_V%s", MANUFACTURER_NAME, pos, PRODUCT_TYPE, PRODUCT_MODEL,
             product_dev_version );
    return version;
}

