/*
 * @Author: your name
 * @Date: 2025-02-14 15:13:00
 * @LastEditTime: 2025-02-18 13:35:11
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

ShellCommand mcu_ctl_sub_cmd[] = {
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, get_bank, get_mcu_run_bank_handler, get MCU run bank),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, set_bank, set_mcu_run_bank_handler, set MCU run bank),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, log_ymodem, mcu_start_ymodem_handler, log port start ymoden),
    SHELL_CMD_GROUP_END()};

SHELL_EXPORT_CMD_GROUP(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), mcu, mcu_ctl_sub_cmd, mcu control commands);