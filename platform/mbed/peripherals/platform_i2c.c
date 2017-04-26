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


#include "platform_peripheral.h"
#include "pinmap.h"
#include "mbed_critical.h"

/******************************************************
*                    Constants
******************************************************/

#define  I2C_Direction_Transmitter      ((uint8_t)0x00)
#define  I2C_Direction_Receiver         ((uint8_t)0x01)
#define  IS_I2C_DIRECTION(DIRECTION) (((DIRECTION) == I2C_Direction_Transmitter) || \
                                     ((DIRECTION) == I2C_Direction_Receiver))

/******************************************************
*                   Enumerations
******************************************************/

/******************************************************
*                 Type Definitions
******************************************************/

/******************************************************
*                    Structures
******************************************************/

/******************************************************
*               Variables Definitions
******************************************************/

/******************************************************
*               Static Function Declarations
******************************************************/

static OSStatus i2c_address_device( platform_i2c_driver_t* driver, const platform_i2c_config_t* config, int retries, uint8_t direction );

/******************************************************
*               Function Declarations
******************************************************/
OSStatus platform_i2c_init( platform_i2c_driver_t* driver, const platform_i2c_t* i2c, const platform_i2c_config_t* config )
{
	OSStatus err = kNoErr;
	int hz = 100000;
  	platform_mcu_powersave_disable( );

  	if(config->address_width == I2C_ADDRESS_WIDTH_10BIT || config->address_width == I2C_ADDRESS_WIDTH_16BIT)
  	{
  		err = kUnsupportedErr;
		goto exit;
  	}

  	switch ( config->speed_mode )
    {
        case I2C_LOW_SPEED_MODE:
            hz = 10000;
            break;
        case I2C_STANDARD_SPEED_MODE:
            hz = 100000;
            break;
        case I2C_HIGH_SPEED_MODE:
            hz = 400000;
            break;
        default:
            err = kUnsupportedErr;
            goto exit;
            break;
    }

	i2c_init((i2c_t *)&driver->i2c, i2c->mbed_sda_pin, i2c->mbed_scl_pin);
	i2c_frequency((i2c_t *)&driver->i2c, hz);

exit:
  	platform_mcu_powersave_enable( );
  	return err;
}


bool platform_i2c_probe_device( platform_i2c_driver_t* driver, const platform_i2c_config_t* config, int retries )
{
  	OSStatus err = kGeneralErr;
	
  	platform_mcu_powersave_disable();

    err = i2c_address_device( driver, config, retries, I2C_Direction_Transmitter );

  	platform_mcu_powersave_enable();
  	return ( err == kNoErr) ? true : false;
}

OSStatus platform_i2c_init_tx_message( platform_i2c_message_t* message, const void* tx_buffer, uint16_t tx_buffer_length, uint16_t retries )
{
  	OSStatus err = kNoErr;

  	require_action_quiet( ( message != NULL ) && ( tx_buffer != NULL ) && ( tx_buffer_length != 0 ), exit, err = kParamErr);

  	memset(message, 0x00, sizeof(platform_i2c_message_t));
  	message->tx_buffer = tx_buffer;
  	message->retries = retries;
  	message->tx_length = tx_buffer_length;
  
exit:  
  	return err;
}

OSStatus platform_i2c_init_rx_message( platform_i2c_message_t* message, void* rx_buffer, uint16_t rx_buffer_length, uint16_t retries )
{
  	OSStatus err = kNoErr;

  	require_action_quiet( ( message != NULL ) && ( rx_buffer != NULL ) && ( rx_buffer_length != 0 ), exit, err = kParamErr);

  	memset(message, 0x00, sizeof(platform_i2c_message_t));
  	message->rx_buffer = rx_buffer;
  	message->retries = retries;
  	message->rx_length = rx_buffer_length;
  
exit:
  	return err;
}

OSStatus platform_i2c_init_combined_message( platform_i2c_message_t* message, const void* tx_buffer, void* rx_buffer, uint16_t tx_buffer_length, uint16_t rx_buffer_length, uint16_t retries )
{
  	OSStatus err = kNoErr;

  	require_action_quiet( ( message != NULL ) && ( tx_buffer != NULL ) && ( tx_buffer_length != 0 ) && ( rx_buffer != NULL ) && ( rx_buffer_length != 0 ), exit, err = kParamErr);

  	memset(message, 0x00, sizeof(platform_i2c_message_t));
  	message->rx_buffer = rx_buffer;
  	message->tx_buffer = tx_buffer;
  	message->retries = retries;
  	message->tx_length = tx_buffer_length;
  	message->rx_length = rx_buffer_length;
  
exit:
  	return err;
}

static OSStatus i2c_transfer_message( platform_i2c_driver_t* driver, const platform_i2c_config_t* config,
                                      platform_i2c_message_t* message )
{
    OSStatus err = kTimeoutErr;
    int retries = message->retries;
    int ret = -1;
    int stop = ( message->tx_buffer != NULL &&  message->rx_buffer != NULL) ? 0 : 1;

    if ( message->tx_buffer != NULL ) {
        for( ; retries > 0; --retries ) {
            ret = i2c_write( &driver->i2c, (int)(config->address << 1), (char *) (message->tx_buffer), message->tx_length, stop );
            if( ret == message->tx_length ) break;
        }
    }

    if ( message->rx_buffer != NULL ) {
        for( ; retries > 0; --retries ) {
            ret = i2c_read( &driver->i2c, (int)(config->address << 1), (char *) (message->rx_buffer), message->rx_length, 1 );
            if( ret == message->rx_length ) break;
        }
    }

    if( retries ) err = kNoErr;
    return err;
}

OSStatus platform_i2c_transfer( platform_i2c_driver_t* driver, const platform_i2c_config_t* config, platform_i2c_message_t* messages, uint16_t number_of_messages )
{
  	OSStatus err = kNoErr;

  	platform_mcu_powersave_disable();

  	for ( int i = 0; i < number_of_messages; i++ )
  	{
  	    err = i2c_transfer_message( driver, config, &messages[ i ] );
  	    require_noerr(err, exit);
  	}

 exit: 
  	platform_mcu_powersave_enable();
  	return err;
}

OSStatus platform_i2c_deinit( platform_i2c_driver_t* driver, const platform_i2c_config_t* config )
{
    UNUSED_PARAMETER( driver );
    UNUSED_PARAMETER( config );

  	return kNoErr;
}

static OSStatus i2c_address_device( platform_i2c_driver_t* driver, const platform_i2c_config_t* config, int retries, uint8_t direction )
{
    OSStatus err = kTimeoutErr;
    int ret = -1;

    /* Some chips( authentication and security related chips ) has to be addressed several times before they acknowledge their address */
    for ( ; retries != 0 ; --retries )
    {
        ret = i2c_write((i2c_t *)&driver->i2c, (int)(config->address << 1), NULL, 0, 1);
        if(0 == ret) {
            err = kNoErr;
            break;
        }
    }

      return err;
}


