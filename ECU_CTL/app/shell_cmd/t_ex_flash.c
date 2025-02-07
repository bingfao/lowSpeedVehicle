/*
 * @Author: your name
 * @Date: 2025-02-06 21:21:16
 * @LastEditTime: 2025-02-07 13:48:10
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\app\shell_cmd\t_ex_flash.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "T_BOOT"
#define LOG_LVL ELOG_LVL_DEBUG

#include <FreeRTOS.h>
#include <stdio.h>
#include <string.h>

#include "elog.h"
#include "ex_flash.h"
#include "main.h"
#include "shell.h"
#include "shell_cmd_group.h"
#include "stdlib.h"
#include "util.h"


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

static int t_ex_flash_read(int argc, char *argv[])
{
    int32_t ret = 0;
    uint32_t addr = 0;
    uint32_t len = 0;
    uint8_t data[64] = {0};

    if (argc < 3) {
        log_e("Usage: ex_flash_read <addr> <len>\r\n");
        return -1;
    }
    addr = (uint32_t)strtoul(argv[1], NULL, 16);
    len = (uint32_t)atoi(argv[2]);

    log_i("ex_flash_read addr = 0x%x len = %d :\r\n", addr, len);

    for (int i = 0; i < len; ) {
        if (len - i > 64) {
            ret = ex_flash_read(addr + i, data, 64);
        } else {
            ret = ex_flash_read(addr + i, data, len - i);
        }
        if (ret < 0) {
            log_e("ex_flash_read failed\r\n");
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

static int t_ex_flash_erase(int argc, char *argv[])
{
    int32_t ret = 0;
    uint32_t addr = 0;
    uint32_t len = 0;

    if (argc < 3) {
        log_e("Usage: ex_flash_erase <addr> <len>\r\n");
        return -1;
    }
    addr = (uint32_t)strtoul(argv[1], NULL, 16);
    len = (uint32_t)atoi(argv[2]);

    log_i("ex_flash_erase addr = 0x%x len = %d :\r\n", addr, len);

    ret = ex_flash_erase(addr, len);
    if (ret < 0) {
        log_e("ex_flash_erase failed\r\n");
    }

    return 0;
}

static int t_ex_flash_write(int argc, char *argv[])
{
    int32_t ret = 0;
    uint32_t addr = 0;
    uint32_t len = 0;
    uint8_t data[16] = {0xf0, 0xe1, 0xd2, 0xc3, 0xb4, 0xa5, 0x96, 0x87, 0x78, 0x69, 0x5a, 0x4b, 0x3c, 0x2d, 0x1e, 0x0f};

    if (argc < 3) {
        log_e("Usage: ex_flash_write <addr> <len>\r\n");
        return -1;
    }
    addr = (uint32_t)strtoul(argv[1], NULL, 16);
    len = (uint32_t)atoi(argv[2]);

    log_i("ex_flash_write addr = 0x%x len = %d :\r\n", addr, len);

    for (int i = 0; i < len; ) {
        if (len - i > 16) {
            ret = ex_flash_write(addr + i, data, 16);
        } else {
            ret = ex_flash_write(addr + i, data, len - i);
        }
        if (ret < 0) {
            log_e("ex_flash_write failed\r\n");
            break;
        }
        i += ret;
    }

    return 0;
}

ShellCommand ex_flash_ctl[] = {
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, read, t_ex_flash_read, ex_flash_read <addr> <len>),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, erase, t_ex_flash_erase, ex_flash_erase <addr> <len>),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, write, t_ex_flash_write, ex_flash_write <addr> <len>),
    SHELL_CMD_GROUP_END()};

SHELL_EXPORT_CMD_GROUP(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), ex_flash, ex_flash_ctl, extern_flash operation);

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */