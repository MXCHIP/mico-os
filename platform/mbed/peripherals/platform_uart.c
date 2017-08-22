/**
 ******************************************************************************
 * @file    platform_uart.c
 * @author  William Xu
 * @version V1.0.0
 * @date    05-May-2014
 * @brief   This file provide UART driver functions.
 ******************************************************************************
 *  UNPUBLISHED PROPRIETARY SOURCE CODE
 *  Copyright (c) 2016 MXCHIP Inc.
 *
 *  The contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of MXCHIP Corporation.
 ******************************************************************************
 */

#include "mico_board.h"
#include "platform_peripheral.h"

/******************************************************
*                    Constants
******************************************************/

#define DMA_INTERRUPT_FLAGS  ( DMA_IT_TC | DMA_IT_TE | DMA_IT_DME | DMA_IT_FE )

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
*        Static Function Declarations
******************************************************/
/* Interrupt service functions - called from interrupt vector table */
void platform_uart_irq( uint32_t id, SerialIrq event);
OSStatus serial_send_stream(serial_t* serial_obj, char* data_out, uint32_t size);

/******************************************************
*               Function Definitions
******************************************************/

OSStatus platform_uart_init( platform_uart_driver_t* driver, const platform_uart_t* peripheral, const platform_uart_config_t* config, ring_buffer_t* optional_ring_buffer )
{
  	OSStatus err = kNoErr;
	int wordlen;
	SerialParity parity;
	int stopbit;
	FlowControl flowcontrol;

  	platform_mcu_powersave_disable();

  	require_action_quiet( ( driver != NULL ) && ( peripheral != NULL ) && ( config != NULL ), exit, err = kParamErr);
  	require_action_quiet( (optional_ring_buffer == NULL) || ((optional_ring_buffer->buffer != NULL ) && (optional_ring_buffer->size != 0)), exit, err = kParamErr);
  	driver->rx_size              = 0;
  	driver->tx_size              = 0;
  	driver->last_transmit_result = kNoErr;
  	driver->last_receive_result  = kNoErr;
  	driver->is_recv_over_flow    = false;
  	driver->FlowControl          = FlowControlNone;

  	mico_rtos_init_semaphore( &driver->tx_complete, 1 );
  	mico_rtos_init_semaphore( &driver->rx_complete, 1 );
  	mico_rtos_init_mutex( &driver->tx_mutex );

  	serial_init(&(driver->serial_obj), peripheral->mbed_tx_pin, peripheral->mbed_rx_pin);

	switch ( config->data_width)
  	{
    		case DATA_WIDTH_7BIT  :
      			wordlen = 7;
      			break;
    		case DATA_WIDTH_8BIT:
      			wordlen = 8;
      			break;
    		default:
      			err = kParamErr;
      			goto exit;
  	}

  	switch ( config->parity )
  	{
    		case NO_PARITY:
      			parity = ParityNone;
      			break;

    		case EVEN_PARITY:
      			parity = ParityEven;
      			break;

    		case ODD_PARITY:
      			parity = ParityOdd;
      			break;
    		default:
      			err = kParamErr;
      			goto exit;
  	}

  	switch ( config->stop_bits )
  	{
    		case STOP_BITS_1:
      			stopbit = 1;
      			break;
    		case STOP_BITS_2:
      			stopbit = 2;
      			break;
    		default:
      			err = kParamErr;
      			goto exit;
  	}

  	switch ( config->flow_control )
    {
        case FLOW_CONTROL_DISABLED:
            driver->FlowControl = FlowControlNone;
            break;
        case FLOW_CONTROL_RTS:
            driver->FlowControl = FlowControlRTS;
            break;
        case FLOW_CONTROL_CTS:
            driver->FlowControl = FlowControlCTS;
            break;
        case FLOW_CONTROL_CTS_RTS:
            driver->FlowControl = FlowControlRTSCTS;
            break;
        default:
            err = kParamErr;
            goto exit;
    }
	serial_baud((serial_t*)&driver->serial_obj,config->baud_rate);
	serial_format((serial_t*)&driver->serial_obj, wordlen, parity, stopbit);
#if DEVICE_SERIAL_FC
	if (driver->FlowControl != FlowControlNone)
	    serial_set_flow_control((serial_t*)&driver->serial_obj, driver->FlowControl, peripheral->mbed_rts_pin, peripheral->mbed_cts_pin);
#endif

	if (optional_ring_buffer != NULL){
  		/* Note that the ring_buffer should've been initialised first */
  		driver->rx_buffer = optional_ring_buffer;
  		driver->rx_size   = 0;
  		//platform_uart_receive_bytes( uart, optional_rx_buffer->buffer, optional_rx_buffer->size, 0 );
	}else{
      	return kOptionErr;
	}
	serial_irq_handler(&driver->serial_obj, platform_uart_irq, (uint32_t)driver);
	serial_irq_set(&driver->serial_obj, RxIrq, 1);
	serial_irq_set(&driver->serial_obj, TxIrq, 1);
exit:
    	platform_mcu_powersave_enable();
  	return err;
}

OSStatus platform_uart_deinit( platform_uart_driver_t* driver )
{
  	uint8_t          uart_number;
  	OSStatus          err = kNoErr;

  	platform_mcu_powersave_disable();

  	require_action_quiet( ( driver != NULL ), exit, err = kParamErr);

  	serial_free((serial_t*)&driver->serial_obj);

  	mico_rtos_deinit_semaphore( &driver->rx_complete );
  	mico_rtos_deinit_semaphore( &driver->tx_complete );
  	mico_rtos_deinit_mutex( &driver->tx_mutex );

  	driver->rx_size              = 0;
  	driver->tx_size              = 0;
  	driver->last_transmit_result = kNoErr;
  	driver->last_receive_result  = kNoErr;
  	driver->FlowControl          = FlowControlNone;
  	driver->is_recv_over_flow    = false;

exit:
    	platform_mcu_powersave_enable();
    	return err;
}

OSStatus platform_uart_transmit_bytes( platform_uart_driver_t* driver, const uint8_t* data_out, uint32_t size )
{
  	OSStatus err = kNoErr;
    	int32_t ret;

  	platform_mcu_powersave_disable();

	mico_rtos_lock_mutex( &driver->tx_mutex );

	ret = serial_send_stream((serial_t*)&driver->serial_obj, (char*)data_out, size);
	if(ret != 0){
		err = kGeneralErr;
		goto exit;
	}
	/* Wait for transmission complete */
	//mico_rtos_get_semaphore( &driver->tx_complete, MICO_NEVER_TIMEOUT );

  	driver->tx_size = 0;
  	err = driver->last_transmit_result;

exit:
	mico_rtos_unlock_mutex( &driver->tx_mutex );
  	platform_mcu_powersave_enable();
  	return err;
}

OSStatus platform_uart_receive_bytes( platform_uart_driver_t* driver, uint8_t* data_in, uint32_t expected_data_size, uint32_t timeout_ms )
{
    OSStatus err = kNoErr;
    //platform_mcu_powersave_disable();

    require_action_quiet( ( driver != NULL ) && ( data_in != NULL ) && ( expected_data_size != 0 ), exit, err = kParamErr);

    mico_rtos_get_semaphore( &driver->rx_complete, 0 );

    /* Check if ring buffer already contains the required amount of data. */
    if( ( driver->FlowControl == FlowControlRTSCTS || driver->FlowControl == FlowControlRTS ) &&  ( driver->is_recv_over_flow == true ) )
     {
         driver->is_recv_over_flow = false;
         serial_irq_set((serial_t*)&driver->serial_obj, RxIrq, 1);
     }

   while ( expected_data_size != 0 )
   {
     uint32_t transfer_size = MIN( driver->rx_buffer->size / 2, expected_data_size );

       /* Set rx_size and wait in rx_complete semaphore until data reaches rx_size or timeout occurs */
       driver->last_receive_result = kNoErr;
       driver->rx_size             = transfer_size;

     /* Check if ring buffer already contains the required amount of data. */
     if ( transfer_size > ring_buffer_used_space( driver->rx_buffer ) )
     {
       err = mico_rtos_get_semaphore( &driver->rx_complete, timeout_ms );

       /* Reset rx_size to prevent semaphore being set while nothing waits for the data */
       driver->rx_size = 0;

       if( err != kNoErr )
         goto exit;
     }else {
       driver->rx_size = 0;
     }
     err = driver->last_receive_result;
     expected_data_size -= transfer_size;

     /* Grab data from the buffer */
     do
     {
       uint8_t* available_data;
       uint32_t bytes_available;

       ring_buffer_get_data( driver->rx_buffer, &available_data, &bytes_available );
       bytes_available = MIN( bytes_available, transfer_size );
       memcpy( data_in, available_data, bytes_available );
       transfer_size -= bytes_available;
       data_in = ( (uint8_t*) data_in + bytes_available );
       ring_buffer_consume( driver->rx_buffer, bytes_available );
     } while ( transfer_size != 0 );
   }

 exit:
   platform_mcu_powersave_enable();
   return err;
}

uint32_t platform_uart_get_length_in_buffer( platform_uart_driver_t* driver )
{  
  return ring_buffer_used_space( driver->rx_buffer );
}

/******************************************************
*            Interrupt Service Routines
******************************************************/

void platform_uart_irq( uint32_t id, SerialIrq event)
{
  	uint8_t recv_byte;
	platform_uart_driver_t* driver = (platform_uart_driver_t*)id;

	if (event == RxIrq){
        if ( ring_buffer_is_full( driver->rx_buffer ) == 0 )
        {
             recv_byte = serial_getc((serial_t*)&driver->serial_obj);
             ring_buffer_write( driver->rx_buffer, &recv_byte, 1 );
             if ( ( driver->rx_size > 0 ) && ( ring_buffer_used_space( driver->rx_buffer ) >= driver->rx_size ) )
             {
                  mico_rtos_set_semaphore( &driver->rx_complete );
                  driver->rx_size = 0;
             }
         } else{
             if( driver->FlowControl == FlowControlRTSCTS || driver->FlowControl == FlowControlRTS ){
                  serial_irq_set((serial_t*)&driver->serial_obj, RxIrq, 0);
                  driver->is_recv_over_flow = true;
             }else{
                 serial_getc((serial_t*)&driver->serial_obj);
             }
         }
	}
//	else if ((event == TxIrq)) {
//		mico_rtos_set_semaphore( &driver->tx_complete );
//	}
}

OSStatus serial_send_stream(serial_t* serial_obj, char* data_out, uint32_t size)
{
	OSStatus err = kNoErr;
	for(int i=0;i<size;i++){
		serial_putc(serial_obj,*(data_out+i));
	}

	return err;
}
