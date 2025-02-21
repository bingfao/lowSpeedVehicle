/*
 * @Author: your name
 * @Date: 2025-02-14 15:13:00
 * @LastEditTime: 2025-02-20 20:17:38
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\app\shell_cmd\t_mcu copy.c
 */
#define LOG_TAG "T_MCU"
#define LOG_LVL ELOG_LVL_DEBUG

#include <FreeRTOS.h>
#include <stdio.h>
#include <string.h>

#include "elog.h"
#include "main.h"
#include "mcu_ctl.h"
#include "shell.h"
#include "shell_port.h"
#include "shell_cmd_group.h"
#include "stdlib.h"
#include "util.h"
#include "version.h"

static int get_mcu_run_bank_handler(int argc, char *argv[])
{
    uint32_t bank = 0;
    int32_t ret = mcu_ctl_get_run_bank(&bank);
    printf("MCU run bank: %d\n\r", bank);

    return 0;
}

static int set_mcu_run_bank_handler(int argc, char *argv[])
{
    uint32_t bank = 0;

    if (argc != 2) {
        printf("Usage: mcu set_bank <bank>\n\r");
        return -1;
    }
    bank = atoi(argv[1]);
    int32_t ret = mcu_ctl_set_run_bank(bank);
    if (ret != 0) {
        printf("set MCU run bank failed\n\r");
    } else {
        printf("set MCU run bank %d success\n\r", bank);
    }

    return 0;
}

static int mcu_start_ymodem_handler(int argc, char *argv[])
{
    log_d("ymodem start");
    elog_stop();
    shell_port_stop();

    return 0;
}

static int mcu_flash_erase_handler(int argc, char *argv[])
{
    int32_t ret = 0;
    uint32_t addr = 0;
    uint32_t len = 0;

    if (argc < 3) {
        log_e("Usage: mcu erase <addr> <len>\r\n");
        return -1;
    }
    addr = (uint32_t)strtoul(argv[1], NULL, 16);
    len = (uint32_t)atoi(argv[2]);

    log_i("mcu flash erase addr = 0x%x len = %d :\r\n", addr, len);

    ret = mcu_ctl_flash_erase(addr, len);
    if (ret < 0) {
        log_e("mcu_ctl_flash_erase failed\r\n");
    }

    return 0;
}

static int mcu_flash_read_handler(int argc, char *argv[])
{
    int32_t ret = 0;
    uint32_t addr = 0;
    uint32_t len = 0;
    uint8_t data[64] = {0};

    if (argc < 3) {
        log_e("Usage: mcu read <addr> <len>\r\n");
        return -1;
    }
    addr = (uint32_t)strtoul(argv[1], NULL, 16);
    len = (uint32_t)atoi(argv[2]);

    log_i("mcu flash read addr = 0x%x len = %d :\r\n", addr, len);

    for (int i = 0; i < len;) {
        if (len - i > 64) {
            ret = mcu_ctl_flash_read(addr + i, data, 64);
        } else {
            ret = mcu_ctl_flash_read(addr + i, data, len - i);
        }
        if (ret < 0) {
            log_e("mcu flash read failed\r\n");
            break;
        }
        for (int j = 0; j < ret; j++) {
            if (j % 16 == 0) {
                printf("\r\n0x%08x: ", addr + i + j);
            }
            printf("%02x ", data[j]);
        }
        i += ret;
    }

    return 0;
}

static int mcu_flash_write_handler(int argc, char *argv[])
{
    int32_t ret = 0;
    uint32_t addr = 0;
    uint32_t len = 0;
    uint8_t data[16] = {0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87, 0x78, 0x69, 0x5a, 0x4b, 0x3c, 0x2d, 0x1e, 0x0f};

    if (argc < 3) {
        log_e("Usage: mcu write <addr> <len>\r\n");
        return -1;
    }
    addr = (uint32_t)strtoul(argv[1], NULL, 16);
    len = (uint32_t)atoi(argv[2]);

    log_i("mcu flash write addr = 0x%x len = %d :\r\n", addr, len);

    for (int i = 0; i < len;) {
        if (len - i > 16) {
            ret = mcu_ctl_flash_write(addr + i, data, 16);
        } else {
            ret = mcu_ctl_flash_write(addr + i, data, len - i);
        }
        if (ret < 0) {
            log_e("mcu_ctl_flash_write failed\r\n");
            break;
        }
        i += ret;
    }

    return 0;
}

ShellCommand mcu_ctl_sub_cmd[] = {
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, get_bank, get_mcu_run_bank_handler, get MCU run bank),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, set_bank, set_mcu_run_bank_handler, set MCU run bank),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, log_ymodem, mcu_start_ymodem_handler, log port start ymoden),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, erase, mcu_flash_erase_handler, erase flash),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, read, mcu_flash_read_handler, read flash),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, write, mcu_flash_write_handler, write flash),
    SHELL_CMD_GROUP_END()};

SHELL_EXPORT_CMD_GROUP(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), mcu, mcu_ctl_sub_cmd, mcu control commands);