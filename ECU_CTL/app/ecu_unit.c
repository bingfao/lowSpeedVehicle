/*
 * @Author: your name
 * @Date: 2024-10-22 16:37:37
 * @LastEditTime: 2024-10-22 17:32:37
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

    return 0;
}

int32_t ecu_unit_start(void)
{
    osThreadDef(ecu_unit, ecu_unit_task, osPriorityNormal, 0, 128);
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
    return 0;
}

static void ecu_unit_task(void const *argument)
{
    ecu_unit_prepare();

    while (1) {
        printf("ECU_UNIT task running...\r\n");
        osDelay(1000);
    }
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
