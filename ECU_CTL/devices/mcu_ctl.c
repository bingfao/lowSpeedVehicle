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
#elif defined STM32F407xx
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

int32_t mcu_ctl_flash_get_upgrade_area(uint32_t *addr, uint32_t *size)
{
    int32_t ret = 0;
    uint32_t data[2] = {0};

    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }
    ret = driver_control(g_mcu_drv, MCU_CTL_GET_UPGRADE_FLASH_AREA, data, sizeof(data));
    if (ret != 0) {
        log_e("mcu_ctl flash get upgrade area failed \r\n");
        return ret;
    }
    *addr = data[0];
    *size = data[1];

    return ret;
}

int32_t mcu_ctl_flash_get_md5_addr_offset(uint32_t *offset)
{
    int32_t ret = 0;
    uint32_t data = 0;

    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }
    ret = driver_control(g_mcu_drv, MCU_CTL_GET_MD5_ADDR_OFFSET, &data, sizeof(data));
    if (ret != 0) {
        log_e("mcu_ctl flash get md5 addr offset failed \r\n");
        return ret;
    }
    *offset = data;

    return ret;
}

int32_t mcu_ctl_flash_get_flile_size_addr_offset(uint32_t *offset)
{
    int32_t ret = 0;
    uint32_t data = 0;

    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }
    ret = driver_control(g_mcu_drv, MCU_CTL_GET_FSIZE_ADDR_OFFSET, &data, sizeof(data));
    if (ret != 0) {
        log_e("mcu_ctl flash get file size addr offset failed \r\n");
        return ret;
    }
    *offset = data;

    return ret;
}

int32_t mcu_ctl_flash_erase(uint32_t addr, uint32_t len)
{
    int32_t ret = 0;
    uint32_t erase_info[2] = {0};
    uint32_t i, j;
    uint8_t check_buf[64] = {0};
    uint32_t read_len = 0;
    uint32_t read_addr = 0;

    erase_info[0] = addr;
    erase_info[1] = len;
    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }

    ret = driver_control(g_mcu_drv, MCU_CTL_FLASH_ERASE, erase_info, sizeof(erase_info));
    if (ret != 0) {
        log_e("mcu_ctl flash erase failed \r\n");
        return -1;
    }
    read_addr = addr;
    for (i = 0; i < len; i += sizeof(check_buf)) {
        read_len = (len - i) > sizeof(check_buf) ? sizeof(check_buf) : (len - i);
        ret = driver_read(g_mcu_drv, read_addr, &check_buf, read_len);
        if (ret < 0) {
            log_e("mcu_ctl flash read failed \r\n");
            return ret;
        }
        for (j = 0; j < read_len; j++) {
            if (check_buf[j] != 0xFF) {
                log_e("mcu_ctl flash check failed \r\n");
                return -1;
            }
        }
        read_addr += read_len;
    }

    return ret;
}

int32_t mcu_ctl_flash_write(uint32_t addr, uint8_t *data, uint32_t len)
{
    int32_t ret = 0;

    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }
    ret = driver_write(g_mcu_drv, addr, data, len);
    if (ret < 0) {
        log_e("mcu_ctl flash write failed \r\n");
    }

    return ret;
}
int32_t mcu_ctl_flash_read(uint32_t addr, uint8_t *data, uint32_t len)
{
    int32_t ret = 0;

    if (g_mcu_ctl_inited_flg != 1) {
        log_e("mcu_ctl not inited \r\n");
        return -1;
    }
    ret = driver_read(g_mcu_drv, addr, data, len);
    if (ret < 0) {
        log_e("mcu_ctl flash read failed \r\n");
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
