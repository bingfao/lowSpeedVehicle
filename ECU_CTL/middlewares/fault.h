/**
 * Impella Actuator
 * Copyright (c) 2024 JN. All rights reserved.
 * @file   : fault.h
 * @brief  : 系统异常处理
 * @author :
 * @date   : 2024/2/1
 */

#ifndef _FAULT_H
#define _FAULT_H

#include <stdbool.h>

enum {
    FAULT_CODE_NORMAL,
    FAULT_CODE_LED,
    FAULT_CODE_BATTERY,
    FAULT_CODE_BUZZER,
    FAULT_CODE_E2P,
    FAULT_CODE_KEY,
    FAULT_CODE_SETTINGS,
    FAULT_CODE_CONSOLE,
    FAULT_CODE_EXT_ADC,
    FAULT_CODE_IN_ADC,
    FAULT_CODE_MOTOR,
    FAULT_CODE_DMA,
    FAULT_CODE_I2C,
    FAULT_CODE_TIMER,
    FAULT_CODE_MEMPRY,

    FAULT_CODE_INVALID
};

/*!
 * 根据条件x执行异常处理
 * fault_assert使用在产品阶段可能出现的异常，例如E2P脱焊，外部电压异常等
 * CY_ASSERT使用在开发阶段，处理开发阶段因为编码原因造成的异常情况，例如数组访问超界等
 *
 * @param x - 检测条件
 * @param code - 异常代码
 */
void fault_assert(bool x, int code);

#endif  // _FAULT_H
