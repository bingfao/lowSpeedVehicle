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
#include <semphr.h>

#include "driver_com.h"
#include "fault.h"
#include "util.h"

#define CONSOLE_TRANS_DRV_NAME "f407_usart1"

DRIVER_OBJ_t *g_console_trans_driver = NULL;

static bool active = false;

size_t console_write(const uint8_t *data, size_t size)
{
    int32_t ret = 0;
    if (active == false) {
        return -1;
    }
    ret = driver_write(g_console_trans_driver, DEV_RXTX_POS_BLOCKING_1000, (uint8_t *)data, size);
    if (ret < 0) {
        return -1;
    }

    return size;
}

size_t console_read(uint8_t *data, size_t size)
{
    int32_t ret = 0;
    if (active == false) {
        return -1;
    }
    ret = driver_read(g_console_trans_driver, DEV_RXTX_POS_BLOCKING, data, size);
    if (ret <= 0) {
        return 0;
    }

    return (int)size;
}

void console_init(void)
{
    int32_t ret = 0;

    if (active) {
        return;
    }
    g_console_trans_driver = get_driver(CONSOLE_TRANS_DRV_NAME);
    if (g_console_trans_driver == NULL) {
        return;
    }
    ret = driver_init(g_console_trans_driver);
    if (ret != 0) {
        return;
    }
    ret = driver_open(g_console_trans_driver, 0);
    if (ret != 0) {
        return;
    }
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
