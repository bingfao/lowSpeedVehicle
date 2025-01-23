/*
 * @Author: your name
 * @Date: 2025-01-23 10:36:17
 * @LastEditTime: 2025-01-23 13:56:07
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\U575\driver_mcu.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "driver_mcu.h"

#include "error_code.h"

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
static int32_t mcu_drv_init(DRIVER_OBJ_t *p_driver);
static int32_t mcu_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag);
static int32_t get_mcu_id(uint8_t *id);
static int32_t drv_mcu_control(DRIVER_OBJ_t *drv, uint32_t cmd, void *args);

DRIVER_CTL_t g_mcu_driver = {
    .init = mcu_drv_init,
    .open = mcu_drv_open,
    .control = drv_mcu_control,
};
/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */
static int32_t mcu_drv_init(DRIVER_OBJ_t *p_driver)
{
    return 0;
}

static int32_t mcu_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag)
{
    return 0;
}

static int32_t get_mcu_id(uint8_t *id)
{
    int8_t i, j;
    uint32_t id_read[3] = {0};

    if (id == NULL) {
        return -EINVAL;
    }

    id_read[0] = *(__IO uint32_t *)(0x0BFA0700);
    id_read[1] = *(__IO uint32_t *)(0x0BFA0704);
    id_read[2] = *(__IO uint32_t *)(0x0BFA0708);

    for (i = 2; i >= 0; i--) {
        for (j = 0; j < 4; j++) {
            *id++ = id_read[i];
            id_read[i] >>= 8;
        }
    }

    return 12;
}

static int32_t drv_mcu_control(DRIVER_OBJ_t *drv, uint32_t cmd, void *args)
{
    int32_t ret = 0;

    if (!driver_is_opened(drv)) {
        return -EACCES;
    }
    switch (cmd) {
        case DRV_CMD_GET_ID:
            ret = get_mcu_id((uint8_t *)args);
            break;

        default:
            return -EINVAL;
    }

    return ret;
}

DRIVER_REGISTER(&g_mcu_driver, mcu_u575)
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
