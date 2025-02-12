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

void reboot_handler(int argc, char *argv[])
{
    log_d("do reboot\r\n");
    HAL_NVIC_SystemReset();
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
