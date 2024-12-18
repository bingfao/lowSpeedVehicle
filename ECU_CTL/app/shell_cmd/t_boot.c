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

typedef struct
{
    uint8_t major;
    uint8_t minor;
    uint8_t sub;
    uint8_t build;
    uint8_t release;
} SOFT_VERSION_t;

#pragma location = ".user_version"
const SOFT_VERSION_t g_soft_version = {VERSION_MAJOR, VERSION_MINOR, VERSION_SUB, VERSION_BUILD, VERSION_RELEASE};

#pragma location = ".user_compiler_data"
const char g_compiler_date[] = "Date: "__DATE__;
#pragma location = ".user_compiler_time"
const char g_compiler_time[] = "Time: "__TIME__;

void reboot_handler(int argc, char *argv[])
{
    log_d("do reboot\r\n");
    HAL_NVIC_SystemReset();
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, reboot,
                 reboot_handler, restart MCU \r\n);

void version_handler(int argc, char *argv[])
{
    printf("Version\t\t: %d.%d.%d.%d %c\n\r", g_soft_version.major, g_soft_version.minor, g_soft_version.sub,
           g_soft_version.build, g_soft_version.release);
    printf("Build time\t: %s, %s\n\r", g_compiler_date, g_compiler_time);
}

SHELL_EXPORT_CMD(SHELL_CMD_PERMISSION(0) | SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN) | SHELL_CMD_DISABLE_RETURN, version,
                 version_handler, read version\r\n);
