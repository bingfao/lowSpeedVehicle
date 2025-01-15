/**
 * Impella Actuator
 * Copyright (c) 2024 JN. All rights reserved.
 * @file   : console.h
 * @brief  : 串口控制台接口
 * @author :
 * @date   : 2024/4/1
 */

#ifndef IMPELLAACTUATOR_CONSOLE_H
#define IMPELLAACTUATOR_CONSOLE_H

#include <FreeRTOS.h>

/*!
 * 初始化串口，并启动控制台服务
 * @return
 */
void console_init(void);

/*!
 * 向控制台写入数据，并等待发送完成
 * @param data 数据
 * @param size 数据量
 * @return 写入情况
 *        -EBUSY: 控制台忙，并且未获取到控制台使用权限，写入失败
 *          -EIO: 串口启动发送错误，写入失败
 *     -ETIMEOUT: 超时，写入失败
 *            >0: 成功写入的数据量
 */
size_t console_write(const uint8_t *data, size_t size);

/*!
 * 从控制台读取数据，并等待接收完成
 * @param data 数据
 * @param size 数据量
 * @return 写入情况
 *        -EBUSY: 控制台忙，并且未获取到控制台使用权限，读取失败
 *          -EIO: 串口启动接收错误，读取失败
 *     -ETIMEOUT: 超时，读取失败
 *            >0: 成功读取到的数据量
 */
size_t console_read(uint8_t *data, size_t size);

#endif //IMPELLAACTUATOR_CONSOLE_H
