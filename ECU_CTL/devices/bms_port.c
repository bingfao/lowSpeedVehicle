/*
 * @Author: your name
 * @Date: 2025-01-08 14:09:30
 * @LastEditTime: 2025-01-15 17:27:02
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\bms_port.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "BMS_PORT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "bms_port.h"

#include "elog.h"
#include "error_code.h"
/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
#define BMS_PORT_DRV_NAME              "f407_usart3"

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
static DRIVER_OBJ_t *g_driver = NULL;
static int8_t g_driver_init_flag = 0;
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
int32_t bms_port_init(void)
{
    int32_t ret = 0;

    g_driver = get_driver(BMS_PORT_DRV_NAME);
    if (g_driver == NULL)
    {
        log_e("driver %s not found", BMS_PORT_DRV_NAME);
        ret = -ENODEV;
    }
    ret = driver_init(g_driver);
    if (ret != 0) {
        log_e("driver %s init failed", BMS_PORT_DRV_NAME);
        return ret;
    }
    ret = driver_open(g_driver, 0);
    if (ret != 0) {
        log_e("driver %s open failed", BMS_PORT_DRV_NAME);
        return ret;
    }
    g_driver_init_flag = 1;

    return 0;
}

int32_t bms_port_deinit(void)
{
    if (g_driver == NULL) {
        log_d("driver %s not found \r\n", BMS_PORT_DRV_NAME);
        return -ENODEV;
    }
    if (driver_deinit(g_driver) != 0) {
        log_e("driver %s deinit failed", BMS_PORT_DRV_NAME);
        return -EIO;
    }
    g_driver_init_flag = 0;
    return 0;
}

int32_t bms_port_send(const uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;

    if (g_driver == NULL || g_driver_init_flag == 0) {
        log_d("driver %s not ready \r\n", BMS_PORT_DRV_NAME);
        return -ENODEV;
    }
    ret = driver_write(g_driver, DEV_POS_NONE, (void *)buf, len);
    if (ret < 0) {
        log_e("send failed ");
        return -EIO;
    }

    return ret;
}

int32_t bms_port_recv(uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;

    if (g_driver == NULL || g_driver_init_flag == 0) {
        log_d("driver %s not ready \r\n", BMS_PORT_DRV_NAME);
        return -ENODEV;
    }
    ret = driver_read(g_driver, DEV_POS_NONE, buf, len);
    if (ret < 0) {
        log_e("recv failed ");
        return -EIO;
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
