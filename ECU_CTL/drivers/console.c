/**
 * Impella Actuator
 * Copyright (c) 2024 JN. All rights reserved.
 * @file   : console.c
 * @brief  : 串口控制台接口
 * @author :
 * @date   : 2024/4/1
 */

#include "console.h"

#include <FreeRTOS.h>
#include <LowLevelIOInterface.h>
#include <error_code.h>
#include <main.h>
#include <semphr.h>

#include "fault.h"
#include "usart.h"
#include "util.h"

static bool active = false;
static UART_HandleTypeDef *serial_dbg = &huart1;
static SemaphoreHandle_t sem_tx = NULL;
static SemaphoreHandle_t sem_rx = NULL;

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sem_tx, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(sem_rx, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

size_t console_write(const uint8_t *data, size_t size)
{
    HAL_UART_Transmit_IT(serial_dbg, data, (uint16_t)size);
    xSemaphoreTake(sem_tx, OS_MS(1000));
    return size;
}

size_t console_read(uint8_t *data, size_t size)
{
    HAL_UART_Receive_IT(serial_dbg, data, (uint16_t)size);
    xSemaphoreTake(sem_rx, OS_WAIT_FOREVER);
    return (int)size;
}

void console_init(void)
{
    if (active) {
        return;
    }

    static StaticSemaphore_t sem_rx_container;
    sem_rx = xSemaphoreCreateBinaryStatic(&sem_rx_container);
    fault_assert(sem_rx != NULL, FAULT_CODE_CONSOLE);
    static StaticSemaphore_t sem_tx_container;
    sem_tx = xSemaphoreCreateBinaryStatic(&sem_tx_container);
    fault_assert(sem_tx != NULL, FAULT_CODE_CONSOLE);

    MX_USART2_UART_Init();
    active = true;
}

__weak size_t __write(int handle, const unsigned char *buffer, size_t size)
{
    int nChars = console_write(buffer, size);

    if (nChars < 0) {
        nChars = 0;
    }

    return (nChars);
}

__weak size_t __read(int handle, unsigned char *buffer, size_t size)
{
    int nChars = 0;
    if (buffer != NULL) {
        console_read((uint8_t *)buffer, size);
        nChars = size;
    }
    return (nChars);
}
