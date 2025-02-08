/*
 * @Author: your name
 * @Date: 2025-02-06 17:48:00
 * @LastEditTime: 2025-02-08 10:58:56
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\ex_flash.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "EX_FLASH"
#define LOG_LVL ELOG_LVL_DEBUG
#include "ex_flash.h"

#include "driver_com.h"
#include "elog.h"
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
#define EX_FLASH_DRV_NAME         "w25q128"

#define EX_FLASH_QSPI_MODE        0
#define EX_FLASH_SPI_MODE         1

#define EX_FLASH_DEFAULT_SPI_MODE EX_FLASH_SPI_MODE
/*
 * ****************************************************************************
 * ******** Private global variables                                   ********
 * ****************************************************************************
 */
uint8_t g_ex_flash_inited = 0;
uint32_t g_ex_flash_size = 0;
uint32_t g_ex_sector_size = 0;
DRIVER_OBJ_t *g_ex_flash_drv = NULL;

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
int32_t ex_flash_init(void)
{
    int32_t ret = 0;
    uint32_t ex_flash_mode = EX_FLASH_DEFAULT_SPI_MODE;

    g_ex_flash_drv = get_driver(EX_FLASH_DRV_NAME);
    if (g_ex_flash_drv == NULL) {
        log_e("driver %s not found\r\n", EX_FLASH_DRV_NAME);
        return -ENODEV;
    }
    driver_init(g_ex_flash_drv);
    ret = driver_open(g_ex_flash_drv, 0);
    if (ret != 0) {
        log_e("driver %s open failed\r\n", EX_FLASH_DRV_NAME);
        return ret;
    }
    ret = driver_control(g_ex_flash_drv, DRV_CMD_GET_BUFFER_TOTAL_SIZE, &g_ex_flash_size);
    if (ret != 0 || g_ex_flash_size == 0) {
        log_e("driver %s get buffer size failed, ret:%d, size:%d\r\n", EX_FLASH_DRV_NAME, ret, g_ex_flash_size);
        return ret;
    }
    log_d("ex_flash size: %d bytes\r\n", g_ex_flash_size);
    ret = driver_control(g_ex_flash_drv, DRV_CMD_GET_BUFFER_BLOCK_SIZE, &g_ex_sector_size);
    if (ret != 0 || g_ex_sector_size == 0) {
        log_e("driver %s get sector size failed, ret:%d, size:%d\r\n", EX_FLASH_DRV_NAME, ret, g_ex_sector_size);
        return ret;
    }
    log_d("ex_flash sector size: %d bytes\r\n", g_ex_sector_size);
    ret = driver_control(g_ex_flash_drv, DRV_CMD_SET_DATA_MODE, &ex_flash_mode);
    if (ret != 0) {
        log_e("driver %s set data mode :%d failed\r\n", EX_FLASH_DRV_NAME, ex_flash_mode);
        return ret;
    }
    g_ex_flash_inited = 1;

    return 0;
}

int32_t ex_flash_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    int32_t ret = 0;

    if (g_ex_flash_inited == 0) {
        log_e("ex_flash not inited\r\n");
        return -EIO;
    }
    if (data == NULL || len == 0 || addr + len > g_ex_flash_size) {
        log_e("invalid parameters\r\n");
        return -EINVAL;
    }

    ret = driver_read(g_ex_flash_drv, addr, data, len);
    if (ret < 0) {
        log_e("ex flash read failed\r\n");
        return -EIO;
    }

    return len;
}

int32_t ex_flash_write(uint32_t addr, uint8_t *data, uint32_t len)
{
    int32_t ret = 0;

    if (g_ex_flash_inited == 0) {
        log_e("ex_flash not inited\r\n");
        return -EIO;
    }
    if (data == NULL || len == 0 || addr + len > g_ex_flash_size) {
        log_e("invalid parameters\r\n");
        return -EINVAL;
    }

    ret = driver_write(g_ex_flash_drv, addr, data, len);
    if (ret < 0) {
        log_e("ex flash write failed ");
        return -EIO;
    }

    return len;
}

int32_t ex_flash_erase(uint32_t addr, uint32_t len)
{
    int32_t ret = 0;
    uint32_t erase_info[2] = {0};

    if (g_ex_flash_inited == 0) {
        log_e("ex_flash not inited\r\n");
        return -EIO;
    }
    if (addr + len > g_ex_flash_size) {
        log_e("invalid parameters\r\n");
        return -EINVAL;
    }
    erase_info[0] = addr;
    erase_info[1] = len;
    ret = driver_control(g_ex_flash_drv, DRV_CMD_ERASE_BUFFER, &erase_info);
    if (ret != 0) {
        log_e("driver %s erase buffer failed\r\n", EX_FLASH_DRV_NAME);
    }

    return ret;
}

int32_t ex_flash_get_sector_size(uint32_t *size)
{
    if (g_ex_flash_inited == 0) {
        log_e("ex_flash not inited\r\n");
        return -EIO;
    }
    *size = g_ex_sector_size;

    return 0;
}

int32_t ex_flash_get_total_size(uint32_t *size)
{
    if (g_ex_flash_inited == 0) {
        log_e("ex_flash not inited\r\n");
        return -EIO;
    }
    *size = g_ex_flash_size;

    return 0;
}

bool ex_flash_is_inited(void)
{
    return g_ex_flash_inited != 0;
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
