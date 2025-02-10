/*
 * @Author: your name
 * @Date: 2025-02-08 17:35:54
 * @LastEditTime: 2025-02-10 09:39:42
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\app\shell_cmd\t_file.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "T_FILE"
#define LOG_LVL ELOG_LVL_DEBUG

#include <FreeRTOS.h>
#include <stdio.h>
#include <string.h>

#include "elog.h"
#include "lfs_port.h"
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

// static int t_onefile_info(char *path, uint8_t *buf, uint32_t size)
// {
//     int32_t ret = 0;

//     ret = lfs_port_scan_file(path, buf, size);
//     return ret;
// }



static int t_file_read(int argc, char *argv[])
{
    int32_t ret = 0;
    int32_t len = 0;
    int32_t file_size = 0;
    uint32_t addr = 0;
    uint8_t data[64] = {0};

    if (argc != 2 && argc != 3) {
        log_e("Usage: file cat <path> (<len>)\r\n");
        return -1;
    }
    file_size = lfs_port_get_file_size(argv[1]);
    if (argc == 3) {
        len = (uint32_t)atoi(argv[2]);
        len = MIN(len, file_size);
    } else {
        len = file_size;
    }
    log_i("file: %s size: %d r_size: %d:\r\n", argv[1], file_size, len);

    for (int i = 0; i < len;) {
        if (len - i > 64) {
            ret = lfs_port_read(argv[1], addr + i, data, 64);
        } else {
            ret = lfs_port_read(argv[1], addr + i, data, len - i);
        }
        if (ret < 0) {
            log_e("file read failed\r\n");
            break;
        }
        for (int j = 0; j < ret; j++) {
            printf("%c", data[j]);
        }
        i += ret;
    }
    printf("\r\n");

    return 0;
}

static int t_file_erase(int argc, char *argv[])
{
    int32_t ret = 0;

    if (argc != 2) {
        log_e("Usage: file rm <path>\r\n");
        return -1;
    }
    log_i("file delete: %s\r\n", argv[1]);
    ret = lfs_port_delete(argv[1]);
    if (ret < 0) {
        log_e("file delete failed\r\n");
    }

    return 0;
}

static int t_file_write(int argc, char *argv[])
{
    int32_t ret = 0;
    int32_t len = 0;

    if (argc != 3) {
        log_e("Usage: file echo <char> <path>\r\n");
        return -1;
    }
    len = strlen(argv[1]);
    log_i("file write %s\r\n", argv[2]);

    ret = lfs_port_write(argv[2], 0xFFFFFFFF, (uint8_t *)argv[1], len);
    if (ret < 0) {
        log_e("file write failed\r\n");
    }

    return 0;
}

static int t_file_write_continue(int argc, char *argv[])
{
    int32_t ret = 0;
    int32_t len = 0;

    if (argc != 3) {
        log_e("Usage: file echo_c <char> <path>\r\n");
        return -1;
    }
    len = strlen(argv[1]);
    log_i("file write %s\r\n", argv[2]);

    ret = lfs_port_write_continue(argv[2], (uint8_t *)argv[1], len);
    if (ret < 0) {
        log_e("file write failed\r\n");
    }

    return 0;
}

static int t_file_info(int argc, char *argv[])
{
    int32_t ret = 0;
    uint8_t buf[1024] = {0};

    if (argc == 2) {
        ret = lfs_port_scan_file(argv[1], buf, 1024);
        if (ret < 0) {
            log_e("file info failed\r\n");
            return -1;
        }
        log_i("file <%s> info:\r\n", argv[1]);
    } else if (argc == 1) {
        ret = lfs_port_get_files(buf, 1024);
        if (ret < 0) {
            log_e("file info failed\r\n");
            return -1;
        }
        log_i("file info:\r\n");
    } else {
        log_e("Usage: file ls / ls <path>\r\n");
        return -1;
    }
    printf("%s", buf);

    return 0;
}

static int t_file_close(int argc, char *argv[])
{
    int32_t ret = 0;

    if (argc != 1) {
        log_e("Usage: file close\r\n");
        return -1;
    }
    log_i("file close\r\n");
    ret = lfs_port_close();
    if (ret < 0) {
        log_e("file close failed\r\n");
    }

    return 0;
}

static int t_lfs_size_info(int argc, char *argv[])
{
    int32_t ret = 0;
    int32_t total_sz = 0;
    int32_t avail_sz = 0;

    if (argc != 1) {
        log_e("Usage: file df\r\n");
        return -1;
    }
    ret = lfs_port_get_avail_size(&total_sz, &avail_sz);
    if (ret < 0) {
        log_e("get avail size failed\r\n");
    }
    log_i("total size: %d, avail size: %d, used size: %d%%\r\n", total_sz, avail_sz, (int)(avail_sz * 100 / total_sz));

    return 0;
}

ShellCommand file_ctl[] = {
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, cat, t_file_read, file cat <path> <len>),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, rm, t_file_erase, file rm <path>),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, echo, t_file_write, file echo <char> <path> ),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, echo_c, t_file_write_continue, file echo_c <char> <path>),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, ls, t_file_info, file ls / ls <path>),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, close, t_file_close, file close),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, df, t_lfs_size_info, file df),
    SHELL_CMD_GROUP_END()};

SHELL_EXPORT_CMD_GROUP(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), file, file_ctl, extern_flash operation);

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */