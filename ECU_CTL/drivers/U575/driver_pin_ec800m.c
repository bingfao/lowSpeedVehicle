/*
 * @Author: your name
 * @Date: 2025-01-23 11:10:42
 * @LastEditTime: 2025-01-23 11:29:12
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\F407\driver_ec800m.c
 */


/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "driver_pin_ec800m.h"

#include "error_code.h"
#include "main.h"

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
static int32_t ec800m_pin_drv_init(DRIVER_OBJ_t *p_driver);
static int32_t ec800m_pin_open(DRIVER_OBJ_t *p_driver, uint32_t oflag);
static int32_t ec800m_pin_control(DRIVER_OBJ_t *drv, uint32_t cmd, void *args);
static int32_t ec800m_rst_pin_write(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size);

DRIVER_CTL_t g_reset_pin_ec800m_driver = {
    .init = ec800m_pin_drv_init,
    .open = ec800m_pin_open,
    .write = ec800m_rst_pin_write,
    .control = ec800m_pin_control,
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
static int32_t ec800m_pin_drv_init(DRIVER_OBJ_t *p_driver)
{
    return 0;
}

static int32_t ec800m_pin_open(DRIVER_OBJ_t *p_driver, uint32_t oflag)
{
    return 0;
}

static int32_t ec800m_pin_control(DRIVER_OBJ_t *drv, uint32_t cmd, void *args)
{
    return 0;
}

static int32_t ec800m_rst_pin_write(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size)
{
    if (*(uint32_t *)buffer == 0) {
        HAL_GPIO_WritePin(EC800M_RESET_GPIO_Port, EC800M_RESET_Pin, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(EC800M_RESET_GPIO_Port, EC800M_RESET_Pin, GPIO_PIN_SET);
    }

    return 0;
}


DRIVER_REGISTER(&g_reset_pin_ec800m_driver, ec800m_rst_pin)
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
