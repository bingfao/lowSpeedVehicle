/*
 * @Author: your name
 * @Date: 2024-10-22 16:37:37
 * @LastEditTime: 2025-01-22 14:02:17
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ECU_CTL\app\ecu_unit.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "ECU_UNIT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "ecu_unit.h"

#include <error_code.h>

#include "bms_port.h"
#include "console.h"
#include "driver_com.h"
#include "elog.h"
#include "mcu_ctl.h"
#include "net_unit.h"
#include "shell_port.h"
#include "user_os.h"
#include "version.h"
/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */
USER_THREAD_OBJ_t g_ecu_unit_thread = {0};

/*
 * ****************************************************************************
 * ******** Private macro                                              ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Private global variables                                   ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t ecu_unit_prepare(void);
static void ecu_unit_task(void const *argument);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t ecu_unit_init(void)
{
    driver_register_fun_doing();
    console_init();
    print_system_inf();

    return 0;
}

int32_t ecu_unit_start(void)
{
    BaseType_t ret;

    memset(&g_ecu_unit_thread, 0, sizeof(USER_THREAD_OBJ_t));
    g_ecu_unit_thread.thread = ecu_unit_task;
    g_ecu_unit_thread.name = "ECU_UNIT";
    g_ecu_unit_thread.stack_size = 1024;
    g_ecu_unit_thread.parameter = NULL;
    g_ecu_unit_thread.priority = RTOS_PRIORITY_NORMAL;
    ret = xTaskCreate((TaskFunction_t)g_ecu_unit_thread.thread, g_ecu_unit_thread.name, g_ecu_unit_thread.stack_size,
                      g_ecu_unit_thread.parameter, g_ecu_unit_thread.priority, &g_ecu_unit_thread.thread_handle);
    if (ret != pdPASS) {
        log_e("Create ECU_UNIT task failed\r\n");
        return -1;
    }

    return 0;
}
/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */
static int32_t ecu_unit_prepare(void)
{
    shell_port_init();
    mcu_ctl_init();
    net_unit_start();
    bms_port_init();

    return 0;
}

static void ecu_unit_task(void const *argument)
{
    ecu_unit_prepare();
    uint8_t mcuID[18];
    uint8_t size = 0;
    // uint32_t tick = 0;

    size = mcu_ctl_get_id(mcuID);
    printf("CPU ID: 0x");
    for (int i = 0; i < size; i++) {
        printf("%02X ", mcuID[i]);
    }
    printf("\r\n");

    log_d("ECU_UNIT task running...\r\n");
    while (1) {
        // tick = osKernelSysTick();
        // log_d("tick: %d\r\n", tick);
        vTaskDelay(1000);
    }
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
