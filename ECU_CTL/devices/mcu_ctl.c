/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "MCU_CTL"
#define LOG_LVL ELOG_LVL_DEBUG
#include "mcu_ctl.h"

#include "driver_com.h"
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
#ifdef STM32U575xx
#define MCU_DRV_NAME "mcu_u575"
#elif defined  STM32F407xx
#define MCU_DRV_NAME "mcu_407"
#endif

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
DRIVER_OBJ_t *g_mcu_drv = NULL;
/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t mcu_ctl_init(void)
{
    g_mcu_drv = get_driver(MCU_DRV_NAME);
    if (g_mcu_drv == NULL) {
        log_d("driver %s not found \r\n", MCU_DRV_NAME);
        return -1;
    }
    driver_init(g_mcu_drv);

    return 0;
}

int32_t mcu_ctl_get_id(uint8_t *id)
{
    int32_t ret = 0;
    if (g_mcu_drv == NULL) {
        log_e("driver %s not found \r\n", MCU_DRV_NAME);
        return -1;
    }
    driver_open(g_mcu_drv, 0);
    ret = driver_control(g_mcu_drv, DRV_CMD_GET_ID, id);
    driver_close(g_mcu_drv);

    return ret;
}

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
