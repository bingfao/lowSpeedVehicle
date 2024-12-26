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

#include "ebike_manage.h"
#include "elog.h"
#include "main.h"
#include "net_port.h"
#include "shell.h"
#include "shell_cmd_group.h"
#include "stdlib.h"
#include "util.h"
#include "version.h"

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

static int t_ebike_register(int argc, char *argv[])
{
    int32_t ret = 0;

    ret = ebike_device_register_to_server();
    if (ret == 0) {
        log_i("register to server success\r\n");
    } else {
        log_e("register to server failed\r\n");
    }

    return 0;
}

static int t_ebike_state_upload(int argc, char *argv[])
{
    int32_t ret = 0;

    ret = ebike_device_state_upload_to_server();
    if (ret == 0) {
        log_i("state upload to server success\r\n");
    } else {
        log_e("state upload to server failed\r\n");
    }

    return 0;
}

static int t_socket_state(int argc, char *argv[])
{
    int32_t ret = 0;

    ret = net_port_socket_refresh();
    log_i("socket state = %d (0: connected, 1: straight out mode, 2: transparent mode)\r\n", ret);

    return 0;
}

ShellCommand ebike_ctl[] = {
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_ebike_register, t_ebike_register, ebike register to server),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_ebike_state_upload, t_ebike_state_upload, ebike state upload to server),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_socket_state, t_socket_state, ebike socket state),
    SHELL_CMD_GROUP_END()};

SHELL_EXPORT_CMD_GROUP(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), ebike_cmd, ebike_ctl, ebike_cmd);

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */