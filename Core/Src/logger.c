#include "logger.h"

#include "usart.h"

static volatile bool uart_busy = false;

bool logger_is_busy(void)
{
    return uart_busy;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart1)
    {
        uart_busy = false;
    }
}

void logger_log_serial(const uint8_t *frame, uint16_t size)
{
    if (!uart_busy)
    {
        uart_busy = true;
        if (HAL_UART_Transmit_DMA(&huart1, frame, size) != HAL_OK)
            uart_busy = false;
    }
}
