/*
 * @Author: your name
 * @Date: 2025-02-14 21:34:15
 * @LastEditTime: 2025-03-11 10:40:56
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\gnss_port.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "GNSS_PORT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "gnss_port.h"

#include "elog.h"
#include "error_code.h"
#include "net_port.h"
#include "user_os.h"
#include "utc.h"

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
#define GNSS_LOCATION_POINT_MAX (10)

/*
 * ****************************************************************************
 * ******** Private global variables                                   ********
 * ****************************************************************************
 */
static uint8_t g_gnss_init_flg = 0;
static GNSS_LOCATION_t g_gnss_location_point[GNSS_LOCATION_POINT_MAX] = {0};
static uint8_t g_gnss_location_point_index = 0;
static int32_t g_gnss_location_read_interval_ms = GNSS_LOCATION_READ_INTERVAL_IN_STOP_MS;
static int32_t g_gnss_location_read_interval_ms_save = GNSS_LOCATION_READ_INTERVAL_IN_STOP_MS;

static uint32_t g_gnss_err_code = 0;
/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static void gnss_port_monitor_task(void const *argument);
static int32_t gnss_location_point_get(GNSS_LOCATION_t *location);
static void gnss_location_point_store(uint32_t ticks);

USER_THREAD_OBJ_t g_gnss_port_monitor_thread =
    USER_THREAD_OBJ_INIT(gnss_port_monitor_task, "gnss_port_monitor", 1024, NULL, RTOS_PRIORITY_NORMAL, NULL);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t gnss_init(void)
{
    BaseType_t res;

    USER_THREAD_OBJ_t *thread = &g_gnss_port_monitor_thread;
    res = xTaskCreate((TaskFunction_t)thread->thread, thread->name, thread->stack_size, thread->parameter,
                      thread->priority, &thread->thread_handle);
    if (res != pdPASS) {
        log_e("create net_port_monitor_task failed \r\n");
        return -EIO;
    }
    g_gnss_init_flg = 1;

    return 0;
}

int32_t gnss_get_location(GNSS_LOCATION_t *location)
{
    if (g_gnss_init_flg == 0) {
        log_e("gnss not init");
        return -EIO;
    }
    if ((g_gnss_err_code & GNSS_ERR_CODE_GPS_GET_ERROR) == GNSS_ERR_CODE_GPS_GET_ERROR) {
        log_e("gnss get location failed");
        if (g_gnss_location_point[g_gnss_location_point_index].altitude != 0) {
            memcpy(location, &g_gnss_location_point[g_gnss_location_point_index], sizeof(GNSS_LOCATION_t));
            return -ENXIO;
        }
        return -EIO;
    }
    memcpy(location, &g_gnss_location_point[g_gnss_location_point_index], sizeof(GNSS_LOCATION_t));

    return 0;
}

int32_t gnss_get_latitude_longitude(float *latitude, float *longitude)
{
    GNSS_LOCATION_t location = {0};
    int32_t ret = 0;

    ret = gnss_get_location(&location);
    if (ret != 0) {
        log_e("gnss_get_location failed");
        return ret;
    }
    *latitude = location.latitude;
    *longitude = location.longitude;

    return 0;
}

void gnss_set_location_read_interval(int32_t interval_ms)
{
    g_gnss_location_read_interval_ms = interval_ms;
    g_gnss_location_read_interval_ms_save = interval_ms;
}

int32_t gnss_do_get_location(GNSS_LOCATION_t *location)
{
    return gnss_location_point_get(location);
}
/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */
static void gnss_port_monitor_task(void const *argument)
{
    while (1) {
        vTaskDelay(1000);
        gnss_location_point_store(1000);
    }
}

static int32_t gnss_location_point_get(GNSS_LOCATION_t *location)
{
    NET_PORT_GNSS_t gnss_info = {0};
    int32_t ret = 0;
    int8_t time_zone = 0;
    struct tm *local_time = NULL;

    ret = net_port_get_gnss(&gnss_info);
    if (ret != 0) {
        log_e("net_port_get_gnss failed");
        return ret;
    }
    memcpy(location, &gnss_info, sizeof(NET_PORT_GNSS_t));
    time_zone = utc_get_time_zone();
    log_d("location->timestamp: %d, time_zone: %d", location->timestamp, time_zone);
    location->timestamp += (int32_t)time_zone * 3600;
    local_time = localtime(&location->timestamp);
    location->year = local_time->tm_year + 1900;
    location->month = local_time->tm_mon + 1;
    location->day = local_time->tm_mday;
    location->hour = local_time->tm_hour;
    location->minute = local_time->tm_min;
    location->second = local_time->tm_sec;
    location->time_type = 1;

    return 0;
}

static void gnss_location_point_store(uint32_t ticks)
{
    GNSS_LOCATION_t location = {0};
    GNSS_LOCATION_t *location_point = NULL;
    int32_t ret = 0;
    static int32_t time_out = GNSS_LOCATION_READ_INTERVAL_IN_STOP_MS;

    if (time_out < g_gnss_location_read_interval_ms) {
        time_out += ticks;
        return;
    }
    time_out = 0;

    ret = gnss_location_point_get(&location);
    if (ret != 0) {
        g_gnss_err_code |= GNSS_ERR_CODE_GPS_GET_ERROR;
        log_e("gnss_location_point_get failed");
        // if get location failed, retry after 1 min
        g_gnss_location_read_interval_ms = GNSS_LOCATION_READ_INTERVAL_RETRY_MS;
        return;
    }
    if (g_gnss_location_read_interval_ms == GNSS_LOCATION_READ_INTERVAL_RETRY_MS &&
        (g_gnss_err_code & GNSS_ERR_CODE_GPS_GET_ERROR) == GNSS_ERR_CODE_GPS_GET_ERROR) {
        g_gnss_location_read_interval_ms = g_gnss_location_read_interval_ms_save;
    }
    g_gnss_err_code &= ~GNSS_ERR_CODE_GPS_GET_ERROR;
    g_gnss_location_point_index++;
    if (g_gnss_location_point_index >= GNSS_LOCATION_POINT_MAX) {
        g_gnss_location_point_index = 0;
    }
    location_point = &g_gnss_location_point[g_gnss_location_point_index];
    memset(location_point, 0, sizeof(GNSS_LOCATION_t));
    memcpy(location_point, &location, sizeof(GNSS_LOCATION_t));
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
