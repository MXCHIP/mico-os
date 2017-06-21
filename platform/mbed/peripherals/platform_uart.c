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
#ifndef NO_MICO_RTOS
static void thread_wakeup(void *arg);
static void RX_PIN_WAKEUP_handler(void *arg);
#endif

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
  	driver->is_flow_control      = false;

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
			flowcontrol = FlowControlNone;
			break;
    		case FLOW_CONTROL_RTS:
      			flowcontrol = FlowControlRTS;
      			break;
    		case FLOW_CONTROL_CTS:
      			flowcontrol = FlowControlCTS;
      			break;
    		case FLOW_CONTROL_CTS_RTS:
      			flowcontrol = FlowControlRTSCTS;
      			break;
    		default:
      			err = kParamErr;
      			goto exit;
  	}
	serial_baud((serial_t*)&driver->serial_obj,config->baud_rate);
	serial_format((serial_t*)&driver->serial_obj, wordlen, parity, stopbit);
        /* NOTE: flow control must be inited at last. - by swyang */
        /*only UART1 support flow control*/
	if(config->flow_control != FLOW_CONTROL_DISABLED &&  driver->serial_obj.serial.uart!=UART_1 )
	{
		err = kUnsupportedErr;
		goto exit;
	}else{
	    if(driver->serial_obj.serial.uart == UART_1){
	         serial_set_flow_control((serial_t*)&driver->serial_obj, flowcontrol, peripheral->mbed_rts_pin, peripheral->mbed_cts_pin);
	         driver->is_flow_control = true;
	    }
	}

	if (optional_ring_buffer != NULL){
  		/* Note that the ring_buffer should've been initialised first */
  		driver->rx_buffer = optional_ring_buffer;
  		driver->rx_size   = 0;
  		//platform_uart_receive_bytes( uart, optional_rx_buffer->buffer, optional_rx_buffer->size, 0 );
	}else{
      	return kOptionErr;
	}
	serial_irq_handler((serial_t*)&driver->serial_obj, platform_uart_irq, (uint32_t)driver);
	serial_irq_set((serial_t*)&driver->serial_obj, RxIrq, 1);
	serial_irq_set((serial_t*)&driver->serial_obj, TxIrq, 1);
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
  	driver->is_flow_control      = false;
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
	mico_rtos_get_semaphore( &driver->tx_complete, MICO_NEVER_TIMEOUT );

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
    if( ( driver->is_flow_control == true  ) &&  ( driver->is_recv_over_flow == true ) )
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



#if 0
uint8_t platform_uart_get_port_number( USART_TypeDef* uart )
{
    if ( uart == USART1 )
    {
        return 0;
    }
    else if ( uart == USART2 )
    {
        return 1;
    }
    else if ( uart == USART3 )
    {
        return 2;
    }
    else if ( uart == UART4 )
    {
        return 3;
    }
    else if ( uart == UART5 )
    {
        return 4;
    }
    else if ( uart == USART6 )
    {
        return 5;
    }
    else
    {
        return 0xff;
    }
}
#endif

#ifndef NO_MICO_RTOS
static void thread_wakeup(void *arg)
{
#if 0
  platform_uart_driver_t* driver = arg;

  while(1){
    if( mico_rtos_get_semaphore( driver->sem_wakeup, 1000) != kNoErr )
    {
      platform_gpio_irq_enable( driver->peripheral->pin_rx, IRQ_TRIGGER_FALLING_EDGE, RX_PIN_WAKEUP_handler, driver );
      platform_mcu_powersave_enable( );
    }
  }
#endif
}
#endif

/******************************************************
*            Interrupt Service Routines
******************************************************/
#ifndef NO_MICO_RTOS
void RX_PIN_WAKEUP_handler(void *arg)
{
#if 0
  (void)arg;
  platform_uart_driver_t* driver = arg;
  uint32_t uart_number;

  platform_gpio_enable_clock( driver->peripheral->pin_rx );

  uart_number = platform_uart_get_port_number( driver->peripheral->port );

  uart_peripheral_clock_functions[ uart_number ]( uart_peripheral_clocks[ uart_number ], ENABLE );

  /* Enable DMA peripheral clock */
  if ( driver->peripheral->tx_dma_config.controller == DMA1 )
  {
      RCC->AHB1ENR |= RCC_AHB1Periph_DMA1;
  }
  else
  {
      RCC->AHB1ENR |= RCC_AHB1Periph_DMA2;
  }

  platform_gpio_irq_disable( driver->peripheral->pin_rx );
  platform_mcu_powersave_disable( );
  mico_rtos_set_semaphore( &driver->sem_wakeup );
#endif
}
#endif


void platform_uart_irq( uint32_t id, SerialIrq event)
{
  	uint8_t recv_byte;
	platform_uart_driver_t* driver = (platform_uart_driver_t*)id;

	if(event == RxIrq){
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
             if( driver->is_flow_control == true ){
                  serial_irq_set((serial_t*)&driver->serial_obj, RxIrq, 0);
                  driver->is_recv_over_flow = true;
             }else{
                 serial_getc((serial_t*)&driver->serial_obj);
             }
         }
	}else if ((event == TxIrq)) {
		mico_rtos_set_semaphore( &driver->tx_complete );
	}
}

OSStatus serial_send_stream(serial_t* serial_obj, char* data_out, uint32_t size)
{
	OSStatus err = kNoErr;
	for(int i=0;i<size;i++){
		serial_putc(serial_obj,*(data_out+i));
	}

	return err;
}
