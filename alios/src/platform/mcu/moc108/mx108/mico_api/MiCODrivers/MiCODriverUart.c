#include "include.h"
#include "rtos_pub.h"
#include "MiCODriverUart.h"

const mico_uart_config_t test_uart_config[] =
{
    [0] =
    {
        .baud_rate     = 115200,
        .data_width   =    DATA_WIDTH_8BIT,
        .parity  = NO_PARITY,
        .stop_bits = STOP_BITS_1,
        .flow_control = FLOW_CONTROL_DISABLED,
        .flags   = 0,
    },
    [1] =
    {
        .baud_rate     = 19200,
        .data_width   =    DATA_WIDTH_8BIT,
        .parity  = NO_PARITY,
        .stop_bits = STOP_BITS_1,
        .flow_control = FLOW_CONTROL_DISABLED,
        .flags   = 0,
    },
    [2] =
    {
        .baud_rate     = 115200,
        .data_width   =    DATA_WIDTH_8BIT,
        .parity  = EVEN_PARITY,
        .stop_bits = STOP_BITS_1,
        .flow_control = FLOW_CONTROL_DISABLED,
        .flags   = 0,
    },
};

OSStatus MicoUartInitialize_test( mico_uart_t uart, uint8_t config, ring_buffer_t *optional_rx_buffer )
{
    return bk_uart_initialize(uart, &test_uart_config[config], optional_rx_buffer);
}

OSStatus MicoUartInitialize( mico_uart_t uart, const mico_uart_config_t *config, ring_buffer_t *optional_rx_buffer )
{
    return bk_uart_initialize(uart, config, optional_rx_buffer);
}

OSStatus MicoStdioUartInitialize( const mico_uart_config_t *config, ring_buffer_t *optional_rx_buffer )
{
#ifdef STDIO_UART
    return bk_stdio_uart_initialize(config, optional_rx_buffer);
#else
    return 0;
#endif
}

OSStatus MicoUartFinalize( mico_uart_t uart )
{
    return bk_uart_finalize(uart);
}

OSStatus MicoUartSend( mico_uart_t uart, const void *data, uint32_t size )
{
    return bk_uart_send(uart, data, size);
}

OSStatus MicoUartRecv( mico_uart_t uart, void *data, uint32_t size, uint32_t timeout )
{
    return bk_uart_recv(uart, data, size, timeout);
}

OSStatus MicoUartRecvPrefetch ( mico_uart_t uart, void* data, uint32_t size, uint32_t timeout )
{
    return bk_uart_recv_prefetch(uart,data,size,timeout);
}

uint32_t MicoUartGetLengthInBuffer( mico_uart_t uart )
{
    return bk_uart_get_length_in_buffer(uart);
}
// eof

