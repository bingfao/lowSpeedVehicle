/*
 * @Author: your name
 * @Date: 2025-02-23 10:37:03
 * @LastEditTime: 2025-03-05 14:36:24
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\app\upgrade_unit.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "UPGRADE_UNIT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "upgrade_unit.h"

#include "console.h"
#include "elog.h"
#include "error_code.h"
#include "mcu_ctl.h"
#include "md5.h"
#include "shell.h"
#include "shell_port.h"
#include "user_os.h"
#include "ymodem.h"
/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef struct
{
    uint8_t src;
    uint8_t dest;
    uint8_t status;
    uint32_t flash_area_addr;
    uint32_t flash_area_size;
    uint32_t operation_tick;
    uint32_t flash_write_index;
    uint32_t file_size_addr;  // file size in bin is 0xFFFFFFFF, need tobe changed after download
    uint32_t file_size;
    uint32_t md5_addr;  // md5 sum of bin file is 16byte(0x00 - 0x0F), need to be changed after download
    MD5_CTX md5_ctx;
    uint8_t md5_sum[16];
} UPGRADE_INFO_t;

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
static int32_t cal_app_smd5(uint32_t addr, uint32_t size, uint32_t zero_offset, uint32_t zero_size, uint8_t *md5_sum);
static int32_t exchang_run_bank(void);
static void ymodem_upgrade_task(void const *argument);
static int32_t ymodem_upgrade_task_init(void);

USER_THREAD_OBJ_t g_ymodem_upgrade_thread =
    USER_THREAD_OBJ_INIT(ymodem_upgrade_task, "ymodem_upgrade", 1024, NULL, RTOS_PRIORITY_NORMAL, NULL);
USER_SEM_OBJ_t g_ymodem_upgrade_sem = {0};
YMODEM_PORT_t g_ymodem_port = {0};

UPGRADE_INFO_t g_upgrade_info = {0};
uint8_t g_upgrade_init_flg = 0;
uint8_t g_ymodem_ready_flg = 0;

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */

int32_t upgrade_unit_init(void)
{
    int32_t ret = 0;
    uint32_t temp = 0;

    memset(&g_upgrade_info, 0, sizeof(UPGRADE_INFO_t));
    ret = mcu_ctl_flash_get_upgrade_area(&g_upgrade_info.flash_area_addr, &g_upgrade_info.flash_area_size);
    if (ret < 0) {
        log_e("get flash area failed, ret:%d\r\n", ret);
        return ret;
    }
    ret = mcu_ctl_flash_get_md5_addr_offset(&temp);
    if (ret < 0) {
        return ret;
    }
    g_upgrade_info.md5_addr = g_upgrade_info.flash_area_addr + temp;
    ret = mcu_ctl_flash_get_flile_size_addr_offset(&temp);
    if (ret < 0) {
        return ret;
    }
    g_upgrade_info.file_size_addr = g_upgrade_info.flash_area_addr + temp;

    ret = ymodem_upgrade_task_init();
    if (ret < 0) {
        log_e("ymodem_upgrade_task_init failed, ret:%d\r\n", ret);
        return ret;
    }
    g_ymodem_ready_flg = 1;
    g_upgrade_init_flg = 1;
    return ret;
}

int32_t upgrade_mcu_ymodem_start(uint8_t src, uint8_t dest)
{
    if (g_upgrade_init_flg == 0) {
        log_e("upgrade unit not inited\r\n");
        return -EIO;
    }
    if (g_ymodem_ready_flg == 0) {
        log_e("ymodem upgrade not inited\r\n");
        return -EIO;
    }
    if (g_upgrade_info.status == UPGRADE_STATUS_START || g_upgrade_info.status == UPGRADE_STATUS_ONGOING) {
        log_e("upgrade unit is not idle, status:%d\r\n", g_upgrade_info.status);
        return -EBUSY;
    }
    if (dest != UPGRADE_DEST_MCU) {
        log_e("dest is not MCU, dest:%d\r\n", dest);
        return -EINVAL;
    }
    if ((src != UPGRADE_START_SOURCE_SHELL_SERIAL) && (src != UPGRADE_START_SOURCE_BLE_UART)) {
        log_e("src is not shell or ble uart, src:%d\r\n", src);
        return -EINVAL;
    }
    g_upgrade_info.src = src;
    g_upgrade_info.dest = dest;
    g_upgrade_info.status = UPGRADE_STATUS_START;
    g_upgrade_info.flash_write_index = 0;
    memset(&g_upgrade_info.md5_ctx, 0, sizeof(MD5_CTX));
    MD5Init(&g_upgrade_info.md5_ctx);

    xSemaphoreGive(g_ymodem_upgrade_sem.sem_handle);

    return 0;
}

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

static void ymodem_close_shell_log(void)
{
    elog_stop();
    shell_port_stop();
}

static void ymodem_open_shell_log(void)
{
    elog_start();
    shell_port_restart();
}

static int32_t cal_app_smd5(uint32_t addr, uint32_t size, uint32_t zero_offset, uint32_t zero_size, uint8_t *md5_sum)
{
    int32_t ret = 0;
    uint32_t i = 0;
    uint8_t app_buf[16] = {0};
    MD5_CTX cal_md5_ctx;
    uint32_t read_size;

    memset(&cal_md5_ctx, 0, sizeof(MD5_CTX));
    MD5Init(&cal_md5_ctx);
    if (zero_offset > size) {
        return -EINVAL;
    }

    for (i = 0; i < zero_offset; i += 1) {
        ret = mcu_ctl_flash_read(addr + i, app_buf, 1);
        if (ret < 0) {
            log_e("mcu_ctl_flash_read failed, ret:%d\r\n", ret);
            return ret;
        }
        MD5Update(&cal_md5_ctx, app_buf, 1);
    }
    app_buf[0] = 0;
    for (i = 0; i < zero_size; i += 1) {
        MD5Update(&cal_md5_ctx, app_buf, 1);
    }
    for (i = zero_offset + zero_size; i < size; i += 16) {
        read_size = (size - i) > 16 ? 16 : (size - i);
        ret = mcu_ctl_flash_read(addr + i, app_buf, read_size);
        if (ret < 0) {
            log_e("mcu_ctl_flash_read failed, ret:%d\r\n", ret);
            return ret;
        }
        MD5Update(&cal_md5_ctx, app_buf, read_size);
    }
    MD5Final(&cal_md5_ctx, md5_sum);

    return ret;
}

static int32_t exchang_run_bank(void)
{
    int32_t ret = 0;
    uint32_t run_bank = 0;

    ret = mcu_ctl_get_run_bank(&run_bank);
    if (ret < 0) {
        log_e("mcu_ctl_get_run_bank failed, ret:%d\r\n", ret);
    }
    if (run_bank == 1) {
        run_bank = 2;
    } else {
        run_bank = 1;
    }
    ret = mcu_ctl_set_run_bank(run_bank);
    if (ret < 0) {
        log_e("mcu_ctl_set_run_bank failed, ret:%d\r\n", ret);
    }

    return ret;
}

static void ymodem_upgrade_task(void const *argument)
{
    BaseType_t res = pdFALSE;
    int32_t ret = 0;
    char file_name[64] = {0};
    uint32_t file_size = 0;
    uint8_t cal_md5[16] = {0};

    log_d("ymodem_upgrade_task run\r\n");
    while (1) {
        res = xSemaphoreTake(g_ymodem_upgrade_sem.sem_handle, OS_WAIT_FOREVER);
        if (res != pdPASS) {
            log_e("osSemaphoreWait failed, xReturn:%d\r\n", ret);
        }
        ymodem_close_shell_log();
        g_upgrade_info.status = UPGRADE_STATUS_ONGOING;
        ret = ymodem_port_receive_start(&g_ymodem_port, file_name, &file_size, 20000);
        ymodem_open_shell_log();
        MD5Final(&g_upgrade_info.md5_ctx, g_upgrade_info.md5_sum);
        g_upgrade_info.file_size = file_size;
        if (ret < 0) {
            log_e("ymodem_port_receive_start failed, ret:%d\r\n", ret);
        } else {
            log_i("ymodem_port_receive_start success, file_name:%s, file_size:%d\r\n", file_name, file_size);
            log_i("md5_sum:");
            for (uint32_t i = 0; i < 16; i++) {
                log_raw("%02X ", g_upgrade_info.md5_sum[i]);
            }
            log_raw("\r\n");
            ret = cal_app_smd5(g_upgrade_info.flash_area_addr, g_upgrade_info.file_size,
                               g_upgrade_info.md5_addr - g_upgrade_info.flash_area_addr, 32, cal_md5);
            if (ret >= 0) {
                if (memcmp(g_upgrade_info.md5_sum, cal_md5, 16) == 0) {
                    log_i("md5_sum is same, upgrade start\r\n");
                    ret = mcu_ctl_flash_write(g_upgrade_info.md5_addr, cal_md5, sizeof(cal_md5));
                    if (ret < 0) {
                        log_e("APP md5 write failed, ret:%d\r\n", ret);
                    }
                    ret = mcu_ctl_flash_write(g_upgrade_info.file_size_addr, (uint8_t *)&g_upgrade_info.file_size, 4);
                    if (ret < 0) {
                        log_e("APP file size write failed, ret:%d\r\n", ret);
                    }
                    for (int32_t i = 5; i > 0; i -= 1) {
                        log_d("%d s\r\n", i);
                        vTaskDelay(OS_MS(1000));
                    }
                    exchang_run_bank();
                } else {
                    log_e("md5_sum is not same, upgrade failed\r\n");
                }
            }
        }
        g_upgrade_info.status = UPGRADE_STATUS_IDLE;
        vTaskDelay(OS_MS(1000));
    }
}

static int32_t ymodem_port_rx_callback(uint8_t *data, uint32_t len, uint32_t timeout)
{
    int32_t ret = 0;
    uint8_t data_buf[64] = {0};
    uint32_t index = 0;
    uint32_t read_len = 0;

    g_upgrade_info.operation_tick = xTaskGetTickCount();
    read_len = (len - index) > sizeof(data_buf) ? sizeof(data_buf) : (len - index);
    do {
        ret = console_read_timeout_1000(data_buf, read_len);
        if (ret > 0) {
            memcpy(data + index, data_buf, ret);
            index += ret;
            if (len == 132) {
                read_len = 132;
            }
            read_len = (len - index) > sizeof(data_buf) ? sizeof(data_buf) : (len - index);
        }
    } while (index < len && (xTaskGetTickCount() - g_upgrade_info.operation_tick) < timeout);
    if (index < len) {
        return -ETIMEDOUT;
    }
    if (len == 132) {
        read_len = 132;
    }

    return len;
}

static int32_t ymodem_port_tx_callback(uint8_t *data, uint32_t len, uint32_t timeout)
{
    int32_t ret = 0;

    ret = console_write(data, len);
    if (ret < 0) {
        return -EIO;
    }

    return len;
}

static int32_t ymodem_delay_ms(uint32_t ms)
{
    vTaskDelay(OS_MS(ms));
    return 0;
}

static int32_t ymodem_flash_write_callback(uint32_t addr, uint8_t *data, uint32_t len)
{
    int32_t ret = 0;
    uint32_t w_addr_start = addr;
    uint32_t w_addr_end = addr + len - 1;
    uint32_t protect_addr_start = g_upgrade_info.md5_addr;
    uint32_t protect_addr_end = g_upgrade_info.md5_addr + 32 - 1;
    uint32_t w_addr = 0;
    uint32_t w_len = 0;
    int32_t md5_len = 0;
    uint32_t data_index = 0;
    // uint8_t temp_0[32] = {0};

    if ((w_addr_end < protect_addr_start) || (w_addr_start > protect_addr_end)) {
        w_addr = addr;  // w_addr_start;
        w_len = len;    // w_addr_end - w_addr_start + 1;
        ret = mcu_ctl_flash_write(addr, data, w_len);
        if (ret < 0) {
            return -EIO;
        }
    } else if (w_addr_end > protect_addr_start && w_addr_start < protect_addr_end) {
        if (w_addr_start < protect_addr_start) {
            w_addr = w_addr_start;
            w_len = protect_addr_start - w_addr_start;
            ret = mcu_ctl_flash_write(w_addr, data, w_len);
            if (ret < 0) {
                return -EIO;
            }
        }
        if (w_addr_end > protect_addr_end) {
            w_addr = protect_addr_end + 1;
            w_len = w_addr_end - protect_addr_end;
            data_index = len - w_len;
            ret = mcu_ctl_flash_write(w_addr, &data[data_index], w_len);
            if (ret < 0) {
                return -EIO;
            }
        }
    }
    if (g_upgrade_info.flash_write_index + len > g_ymodem_port.file_size) {
        md5_len = g_ymodem_port.file_size - g_upgrade_info.flash_write_index;
    } else {
        md5_len = len;
    }
    if (md5_len > 0) {
        MD5Update(&g_upgrade_info.md5_ctx, data, md5_len);
    }
    g_upgrade_info.flash_write_index += len;

    return len;
}

static int32_t ymodem_flash_erase_callback(uint32_t addr, uint32_t len)
{
    g_upgrade_info.flash_write_index = 0;

    mcu_ctl_flash_erase(addr, len);

    return 0;
}

static int32_t ymodem_upgrade_task_init(void)
{
    int32_t ret = 0;
    BaseType_t res;
    USER_THREAD_OBJ_t *thread = &g_ymodem_upgrade_thread;

    ret += ymodem_port_init(&g_ymodem_port, ymodem_port_rx_callback, ymodem_port_tx_callback);
    ret += ymodem_port_set_delay_func(&g_ymodem_port, ymodem_delay_ms);
    ret += ymodem_port_set_flash_write_func(&g_ymodem_port, ymodem_flash_write_callback);
    ret += ymodem_port_set_flash_erase_func(&g_ymodem_port, ymodem_flash_erase_callback);
    ret += ymodem_port_set_flash_area(&g_ymodem_port, g_upgrade_info.flash_area_addr, g_upgrade_info.flash_area_size);
    ret += ymodem_port_set_flash_write_start(&g_ymodem_port, g_upgrade_info.flash_area_addr);
    if (ret < 0) {
        log_e("ymodem_port_init failed, ret:%d\r\n", ret);
        return -EIO;
    }
    g_ymodem_upgrade_sem.sem_handle = xSemaphoreCreateBinaryStatic(&g_ymodem_upgrade_sem.sem_buf);
    if (g_ymodem_upgrade_sem.sem_handle == NULL) {
        log_e("create ymodem_upgrade_sem failed \r\n");
        return -EIO;
    }
    res = xTaskCreate((TaskFunction_t)thread->thread, thread->name, thread->stack_size, thread->parameter,
                      thread->priority, &thread->thread_handle);
    if (res != pdPASS) {
        log_e("create net_port_monitor_task failed \r\n");
        return -EIO;
    }

    return 0;
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
