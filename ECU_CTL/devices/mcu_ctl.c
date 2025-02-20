/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "MCU_CTL"
#define LOG_LVL ELOG_LVL_DEBUG
#include "mcu_ctl.h"

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
uint8_t g_mcu_ctl_inited_flg = 0;
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
    int32_t ret = 0;

    g_mcu_drv = get_driver(MCU_DRV_NAME);
    if (g_mcu_drv == NULL) {
        log_d("driver %s not found \r\n", MCU_DRV_NAME);
        return -1;
    }
    ret = driver_init(g_mcu_drv);
    if (ret != 0) {
        log_e("driver %s init failed \r\n", MCU_DRV_NAME);
        return -1;
    }
    ret = driver_open(g_mcu_drv, 0);
    if (ret != 0) {
        log_e("driver %s open failed \r\n", MCU_DRV_NAME);
    }
    g_mcu_ctl_inited_flg = 1;

    return ret;
}

int32_t mcu_ctl_get_id(uint8_t *id, uint8_t len)
{
    int32_t ret = 0;

    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }
    ret = driver_control(g_mcu_drv, DRV_CMD_GET_ID, id, len);

    return ret;
}

int32_t mcu_ctl_reset(void)
{
    int32_t ret = 0;

    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }
    log_d("mcu reset\r\n");
    driver_control(g_mcu_drv, MCU_CTL_CMD_RESET, NULL, 0);

    return ret;
}

int32_t mcu_ctl_get_run_bank(uint32_t *bank)
{
    int32_t ret = 0;

    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }
    ret = driver_control(g_mcu_drv, MCU_CTL_GET_RUN_BANK, bank, sizeof(uint32_t));
    if (ret != 0) {
        log_e("mcu_ctl get run bank failed \r\n");
    }

    return ret;
}

int32_t mcu_ctl_set_run_bank(uint32_t bank)
{
    int32_t ret = 0;

    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }
    ret = driver_control(g_mcu_drv, MCU_CTL_SET_RUN_BANK, &bank, sizeof(uint32_t));
    if (ret != 0) {
        log_e("mcu_ctl set run bank failed \r\n");
    }

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
