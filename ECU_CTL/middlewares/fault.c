/**
 * Impella Actuator
 * Copyright (c) 2024 JN. All rights reserved.
 * @file   : fault.c
 * @brief  : 系统异常处理
 * @author :
 * @date   : 2024/2/1
 */

#include "fault.h"

#include <FreeRTOS.h>
#include <elog.h>
#include <task.h>

#include "util.h"

static char *fault_msg[] = {
    [FAULT_CODE_NORMAL] = "Normal",      // 正常
    [FAULT_CODE_BATTERY] = "Battery",    // 电池故障
    [FAULT_CODE_BUZZER] = "BUZZER",      // 蜂鸣器故障
    [FAULT_CODE_E2P] = "E2P",            // E2P故障
    [FAULT_CODE_SETTINGS] = "Settings",  // 设置错误
    [FAULT_CODE_CONSOLE] = "Console",    // 控制台故障
    [FAULT_CODE_EXT_ADC] = "EXT_ADC",    // 外部ADC故障
    [FAULT_CODE_DMA] = "DMA",            // DMA故障
    [FAULT_CODE_I2C] = "I2C",            // I2C故障
};

void fault_assert(bool x, int code)
{
    /* 条件检测通过 */
    if (x) {
        return;
    }

    /* 条件检测不通过，程序输出异常信息 */
    if (code < FAULT_CODE_INVALID) {
        log_e("FAULT!!! %d, %s", code, fault_msg[code]);
    } else {
        log_e("Invalid fault code %d", code);
    }

    /* 等待信息输出完成 */
    vTaskDelay(OS_SECOND(1));

    /** 通过蜂鸣器提示错误 **/
    /* 关闭系统任务调度 */

    int timeout = 0;
    /* 闪灯 */
    while (true) {
        //        Cy_GPIO_Set(PIN_PWM_LAMP_RED_PORT, PIN_PWM_LAMP_RED_PIN);
        vTaskDelay(OS_MS(200));
        timeout += 200;
        //        Cy_GPIO_Clr(PIN_PWM_LAMP_RED_PORT, PIN_PWM_LAMP_RED_PIN);
        vTaskDelay(OS_MS(1000));
        timeout += 1000;

        if (timeout > (15 * 1000)) {
            log_e("Board power off");
            vTaskDelay(OS_MS(5));

            /* 关机 */
            while (true);
        }
    }
}
