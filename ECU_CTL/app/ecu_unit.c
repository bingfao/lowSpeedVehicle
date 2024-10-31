/*
 * @Author: your name
 * @Date: 2024-10-22 16:37:37
 * @LastEditTime: 2024-10-31 10:21:58
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

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <error_code.h>
#include <task.h>

#include "console.h"
#include "elog.h"
#include "shell_port.h"
#include "version.h"
#include "net_port.h"
#include "driver_com.h"
#include "mcu_ctl.h"
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
osThreadId g_ecu_unit_handle;

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
    console_init();
    print_system_inf();
    driver_register_fun_doing();

    return 0;
}

int32_t ecu_unit_start(void)
{
    osThreadDef(ecu_unit, ecu_unit_task, osPriorityNormal, 0, 1024);
    g_ecu_unit_handle = osThreadCreate(osThread(ecu_unit), NULL);

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
    net_port_init();
    mcu_ctl_init();
    return 0;
}


static void ecu_unit_task(void const *argument)
{
    ecu_unit_prepare();
    uint8_t mcuID[18];
    uint8_t size = 0;

    size = mcu_ctl_get_id(mcuID);
    printf("CPU ID: 0x");
    for (int i = 0; i < size; i++) {
        printf("%02X ", mcuID[i]);
    }
    printf("\r\n");

    log_d("ECU_UNIT task running...\r\n");
    while (1) {
        osDelay(1000);
    }
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
