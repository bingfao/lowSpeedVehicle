/*
 * @Author: your name
 * @Date: 2025-02-04 10:43:59
 * @LastEditTime: 2025-02-20 10:43:01
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\U575\driver_bk_sram.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "driver_bk_sram.h"

#include "driver_com.h"
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
#define BK_SRAM_START_ADDR BKPSRAM_BASE
#define BK_SRAM_SIZE       0x800
#define BK_SRAM_END_ADDR   (BKPSRAM_BASE + BK_SRAM_SIZE - 1)

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
static uint8_t g_bk_sram_open_flag = 0;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t bk_sram_drv_init(DRIVER_OBJ_t *p_driver);
static int32_t bk_sram_drv_deinit(DRIVER_OBJ_t *p_driver);
static int32_t bk_sram_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag);
static int32_t bk_sram_drv_close(DRIVER_OBJ_t *p_driver);
static int32_t bk_sram_drv_read(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);
static int32_t bk_sram_drv_write(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);

DRIVER_CTL_t g_driver_bk_sram = {
    .init = bk_sram_drv_init,
    .deinit = bk_sram_drv_deinit,
    .open = bk_sram_drv_open,
    .close = bk_sram_drv_close,
    .read = bk_sram_drv_read,
    .write = bk_sram_drv_write,
};
DRIVER_REGISTER(&g_driver_bk_sram, bk_sram)

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
static int32_t bk_sram_drv_init(DRIVER_OBJ_t *p_driver)
{
    return 0;
}

static int32_t bk_sram_drv_deinit(DRIVER_OBJ_t *p_driver)
{
    return 0;
}
static int32_t bk_sram_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag)
{
    uint32_t write_addr = BK_SRAM_END_ADDR;
    uint8_t data = 0;
    uint8_t read_data = 0;

    data = *(volatile uint8_t *)(write_addr);
    data++;
    *(volatile uint8_t *)(write_addr) = data;
    read_data = *(volatile uint8_t *)(write_addr);
    if (data != read_data) {
        g_bk_sram_open_flag = 0;
        return -EIO;
    }
    g_bk_sram_open_flag = 1;

    return 0;
}

static int32_t bk_sram_drv_close(DRIVER_OBJ_t *p_driver)
{
    g_bk_sram_open_flag = 0;

    return 0;
}

static int32_t bk_sram_drv_read(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    uint32_t read_addr = BK_SRAM_START_ADDR + pos;
    uint8_t *p_read_buffer = (uint8_t *)buffer;

    if (pos + size > BK_SRAM_SIZE) {
        return -ENOMEM;
    }
    if (g_bk_sram_open_flag == 0) {
        return -EPERM;
    }

    for (uint32_t i = 0; i < size; i++) {
        p_read_buffer[i] = *(volatile uint8_t *)(read_addr + i);
    }

    return 0;
}

static int32_t bk_sram_drv_write(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    uint32_t write_addr = BK_SRAM_START_ADDR + pos;
    uint8_t *p_write_buffer = (uint8_t *)buffer;

    if (pos + size > BK_SRAM_SIZE) {
        return -ENOMEM;
    }
    if (g_bk_sram_open_flag == 0) {
        return -EPERM;
    }

    for (uint32_t i = 0; i < size; i++) {
        *(volatile uint8_t *)(write_addr + i) = p_write_buffer[i];
    }

    return 0;
}
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
