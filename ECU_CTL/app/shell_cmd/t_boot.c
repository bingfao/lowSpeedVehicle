#define LOG_TAG "T_BOOT"
#define LOG_LVL ELOG_LVL_DEBUG

#include <FreeRTOS.h>
#include <stdio.h>

#include "elog.h"
#include "main.h"
#include "shell.h"
#include "stdlib.h"
#include "util.h"
#include "version.h"
#include "mcu_ctl.h"

void reboot_handler(int argc, char *argv[])
{
    log_d("do reboot\r\n");
    mcu_ctl_reset();
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, reboot,
                 reboot_handler, restart MCU \r\n);

void version_handler(int argc, char *argv[])
{
    printf("Version\t\t: %d.%d.%d.%d %c\n\r", g_info_head.version.major, g_info_head.version.minor,
           g_info_head.version.sub, g_info_head.version.build, g_info_head.version.release);
    printf("Build time\t: %s, %s\n\r", g_info_head.date, g_info_head.time);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, version,
                 version_handler, read version\r\n);

// static int get_mcu_run_bank_handler(int argc, char *argv[])
// {
//     uint32_t bank = 0;
//     int32_t ret = mcu_ctl_get_run_bank(&bank);
//     printf("MCU run bank: %d\n\r", bank);

//     return 0;
// }

// static int set_mcu_run_bank_handler(int argc, char *argv[])
// {
//     uint32_t bank = 0;

//     if (argc != 2) {
//         printf("Usage: mcu set_bank <bank>\n\r");
//         return -1;
//     }
//     bank = atoi(argv[1]);
//     int32_t ret = mcu_ctl_set_run_bank(bank);
//     if (ret != 0) {
//         printf("set MCU run bank failed\n\r");
//     } else {
//         printf("set MCU run bank %d success\n\r", bank);
//     }

//     return 0;
// }

// ShellCommand mcu_ctl_sub_cmd[] = {
//     SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, get_mcu_run_bank_handler, get_mcu_run_bank_handler, get MCU run bank),
//     SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_set_bank, set_mcu_run_bank_handler, set MCU run bank),
// SHELL_CMD_GROUP_END()};


// SHELL_EXPORT_CMD_GROUP(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), mcu, mcu_ctl_sub_cmd, mcu control commands);