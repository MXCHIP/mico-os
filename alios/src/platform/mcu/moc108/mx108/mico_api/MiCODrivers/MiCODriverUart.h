
#ifndef __MICODRIVERUART_H__
#define __MICODRIVERUART_H__

#pragma once
#include "BkDriverUart.h"

/** @addtogroup MICO_PLATFORM
* @{
*/

/** @defgroup MICO_UART MICO UART Driver
* @brief  Universal Asynchronous Receiver Transmitter (UART) Functions
* @{
*/

/******************************************************
 *                   Enumerations
 ******************************************************/

/******************************************************
 *                    Structures
 ******************************************************/

/******************************************************
 *                 Type Definitions
 ******************************************************/
typedef bk_uart_t   mico_uart_t;
typedef bk_uart_config_t mico_uart_config_t;

/******************************************************
 *                 Function Declarations
 ******************************************************/
/**@brief Initialises a UART interface
 *
 * @note Prepares an UART hardware interface for communications
 *
 * @param  uart     : the interface which should be initialised
 * @param  config   : UART configuration structure
 * @param  optional_rx_buffer : Pointer to an optional RX ring buffer
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoUartInitialize( mico_uart_t uart, const mico_uart_config_t *config, ring_buffer_t *optional_rx_buffer );


/**@brief Initialises a STDIO UART interface, internal use only
 *
 * @note Prepares an UART hardware interface for stdio communications
 *
 * @param  config   : UART configuration structure
 * @param  optional_rx_buffer : Pointer to an optional RX ring buffer
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoStdioUartInitialize( const mico_uart_config_t *config, ring_buffer_t *optional_rx_buffer );


/**@brief Deinitialises a UART interface
 *
 * @param  uart : the interface which should be deinitialised
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoUartFinalize( mico_uart_t uart );


/**@brief Transmit data on a UART interface
 *
 * @param  uart     : the UART interface
 * @param  data     : pointer to the start of data
 * @param  size     : number of bytes to transmit
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoUartSend( mico_uart_t uart, const void *data, uint32_t size );


/**@brief Receive data on a UART interface
 *
 * @param  uart     : the UART interface
 * @param  data     : pointer to the buffer which will store incoming data
 * @param  size     : number of bytes to receive
 * @param  timeout  : timeout in milisecond
 *
 * @return    kNoErr        : on success.
 * @return    kGeneralErr   : if an error occurred with any step
 */
OSStatus MicoUartRecv( mico_uart_t uart, void *data, uint32_t size, uint32_t timeout );

/**@brief Read the length of the data that is already recived by uart driver and stored in buffer
 *
 * @param uart     : the UART interface
 *
 * @return    Data length
 */
uint32_t MicoUartGetLengthInBuffer( mico_uart_t uart );

/** @} */
/** @} */
OSStatus MicoUartInitialize_test( mico_uart_t uart, uint8_t config, ring_buffer_t *optional_rx_buffer );
#endif
