/*
 * @Author: your name
 * @Date: 2024-10-22 16:37:37
 * @LastEditTime: 2025-02-20 10:30:33
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

#include "bk_config.h"
#include "bms_port.h"
#include "console.h"
#include "driver_com.h"
#include "elog.h"
#include "ex_flash.h"
#include "gnss_port.h"
#include "lfs_port.h"
#include "mcu_ctl.h"
#include "net_unit.h"
#include "shell_port.h"
#include "user_os.h"
#include "utc.h"
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
static void ecu_unit_utc_check(uint32_t used_tick);
static void ecu_unit_task(void const *argument);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t ecu_unit_init(void)
{
    driver_register_fun_doing();
    bk_config_init();
    utc_init();
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
    if (ex_flash_init() == 0) {
        lfs_port_init();
    }
    gnss_init();

    return 0;
}

#define NEED_TIME_SYNC_PERIOD_S     (1000 * 60 * 60 * 24)  // 1 day
#define NEED_TIME_SYNC_PERIOD_S_MIN (1000 * 60 * 10)       // 10 min
static void ecu_unit_utc_check(uint32_t used_tick)
{
    static int32_t time_out = 0;
    int32_t ret = 0;

    if (time_out <= 0) {
        ret = utc_time_sync_from_net(0);
        if (ret != 0) {
            log_e("Sync time from net failed\r\n");
            time_out = NEED_TIME_SYNC_PERIOD_S_MIN;
        } else {
            log_d("Sync time from net success\r\n");
            time_out = NEED_TIME_SYNC_PERIOD_S;
        }
    } else {
        time_out -= used_tick;
    }
}

static void ecu_unit_task(void const *argument)
{
    ecu_unit_prepare();
    uint8_t mcuID[18];
    int32_t size = 0;
    // uint32_t tick = 0;

    size = mcu_ctl_get_id(mcuID, sizeof(mcuID));
    if (size < 0) {
        log_e("Get MCU ID failed\r\n");
    } else {
        log_raw("CPU ID: 0x");
        for (int i = 0; i < size; i++) {
            log_raw("%02X ", mcuID[i]);
        }
        log_raw("\r\n");
    }

    log_d("ECU_UNIT task running...\r\n");
    while (1) {
        // tick = xTaskGetTickCount();
        // log_d("tick: %d\r\n", tick);
        utc_time_store_bk_sram();
        ecu_unit_utc_check(1000);
        vTaskDelay(1000);
    }
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
