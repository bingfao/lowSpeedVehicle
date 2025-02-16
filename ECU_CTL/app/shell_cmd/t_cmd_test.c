/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "T_CMD_TEST"
#define LOG_LVL ELOG_LVL_DEBUG

#include <FreeRTOS.h>
#include <stdio.h>
#include <string.h>

#include "ebike_manage.h"
#include "elog.h"
#include "gnss_port.h"
#include "main.h"
#include "net_port.h"
#include "shell.h"
#include "shell_cmd_group.h"
#include "stdlib.h"
#include "utc.h"
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

static int t_traffic_report(int argc, char *argv[])
{
    int32_t ret = 0;

    ret = ebike_device_traffic_report();
    log_i("device_traffic start = %d \r\n", ret);

    return 0;
}

static int t_write_utc(int argc, char *argv[])
{
    int year, month, day, hour, minute, second;
    struct tm local_time = {0};

    if (argc < 7) {
        log_e("Usage: t_write_utc <year> <month> <day> <hour> <minute> <second>\r\n");
        return 0;
    }
    year = atoi(argv[1]);
    month = atoi(argv[2]);
    day = atoi(argv[3]);
    hour = atoi(argv[4]);
    minute = atoi(argv[5]);
    second = atoi(argv[6]);

    log_d("UTC time set: %04d-%02d-%02d %02d:%02d:%02d", year, month, day, hour, minute, second);
    local_time.tm_year = year - 1900;
    local_time.tm_mon = month - 1;
    local_time.tm_mday = day;
    local_time.tm_hour = hour;
    local_time.tm_min = minute;
    local_time.tm_sec = second;
    local_time.tm_isdst = -1;
    if (utc_set_local_time(&local_time, 0) == 0) {
        log_i("UTC time set success\r\n");
    } else {
        log_e("UTC time set failed\r\n");
    }

    return 0;
}

static int t_sync_utc(int argc, char *argv[])
{
    int32_t ret = 0;

    ret = utc_time_sync_from_net(1);
    if (ret == 0) {
        log_i("UTC time sync success\r\n");
    } else {
        log_e("UTC time sync failed\r\n");
    }

    return 0;
}

static int t_gnss_info(int argc, char *argv[])
{
    int32_t ret = 0;
    GNSS_LOCATION_t location = {0};

    ret = gnss_do_get_location(&location);
    if (ret == 0) {
        log_i("GNSS get location success\r\n");
        log_i("latitude = %f, longitude = %f, altitude = %f, hdop = %f\r\n", (double)location.latitude,
              (double)location.longitude, (double)location.altitude, (double)location.hdop);
    } else {
        log_e("GNSS get location failed\r\n");
    }

    return 0;
}

ShellCommand ebike_ctl[] = {
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_ebike_register, t_ebike_register, ebike register to server),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_ebike_state_upload, t_ebike_state_upload, ebike state upload to server),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_socket_state, t_socket_state, ebike socket state),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_traffic_report, t_traffic_report, ebike traffic report start file load),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_write_utc, t_write_utc,
                         write utc time : t_write_utc<year><month><day><hour><minute><second>),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_sync_utc, t_sync_utc, sync UTC : t_sync_utc),
    SHELL_CMD_GROUP_ITEM(SHELL_TYPE_CMD_MAIN, t_gnss_info, t_gnss_info, get gnss : t_gnss_info),
    SHELL_CMD_GROUP_END()};

SHELL_EXPORT_CMD_GROUP(SHELL_CMD_TYPE(SHELL_TYPE_CMD_MAIN), ebike_cmd, ebike_ctl, ebike_cmd);

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */