/*
 * @Author: your name
 * @Date: 2025-01-31 19:31:02
 * @LastEditTime: 2025-01-31 20:08:40
 * @LastEditors: stone_honor
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\utc.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "UTC"
#define LOG_LVL ELOG_LVL_DEBUG
#include "utc.h"

#include "elog.h"
#include "error_code.h"
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
static DRIVER_OBJ_t *g_driver = NULL;
static int8_t g_utc_init_flag = 0;
static int8_t g_time_zone = 8;  // default time zone is 8 hours

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
int32_t utc_init(void)
{
    int32_t ret = 0;

    g_driver = get_driver(UTC_DRIVER_NAME);
    if (g_driver == NULL) {
        log_e("driver %s not found", UTC_DRIVER_NAME);
        ret = -ENODEV;
    }
    ret = driver_init(g_driver);
    if (ret != 0) {
        log_e("driver %s init failed", UTC_DRIVER_NAME);
        return ret;
    }
    ret = driver_open(g_driver, 0);
    if (ret != 0) {
        log_e("driver %s open failed", UTC_DRIVER_NAME);
        return ret;
    }
    g_utc_init_flag = 1;

    return 0;
}

int32_t utc_get_local_time(struct tm *local_time, long *nsec)
{
    struct timespec time_spec = {0, 0};
    int32_t ret = 0;
    struct tm *read_time = NULL;

    if (g_utc_init_flag == 0) {
        log_e("utc not init");
        return -1;
    }

    memset(local_time, 0, sizeof(struct tm));
    ret = driver_read(g_driver, 0, &time_spec, sizeof(struct timespec));
    if (ret != 0) {
        log_e("driver %s read failed", UTC_DRIVER_NAME);
        return ret;
    }
    time_spec.tv_sec += g_time_zone * 3600;  // add time zone
    read_time = localtime(&time_spec.tv_sec);
    memcpy(local_time, read_time, sizeof(struct tm));
    *nsec = time_spec.tv_nsec;

    return 0;
}

int32_t utc_set_local_time(struct tm *local_time, long nsec)
{
    struct timespec time_spec = {0, 0};
    int32_t ret = 0;

    if (g_utc_init_flag == 0) {
        log_e("utc not init");
        return -1;
    }
    time_spec.tv_sec = mktime(local_time);
    time_spec.tv_sec -= g_time_zone * 3600;  // sub time zone
    time_spec.tv_nsec = nsec;

    ret = driver_write(g_driver, 0, &time_spec, sizeof(struct timespec));

    return ret;
}

int32_t utc_get_time(struct tm *local_time, long *nsec)
{
    struct timespec time_spec = {0, 0};
    int32_t ret = 0;
    struct tm *read_time = NULL;

    if (g_utc_init_flag == 0) {
        log_e("utc not init");
        return -1;
    }

    memset(local_time, 0, sizeof(struct tm));
    ret = driver_read(g_driver, 0, &time_spec, sizeof(struct timespec));
    if (ret != 0) {
        log_e("driver %s read failed", UTC_DRIVER_NAME);
        return ret;
    }
    read_time = localtime(&time_spec.tv_sec);
    memcpy(local_time, read_time, sizeof(struct tm));
    *nsec = time_spec.tv_nsec;

    return 0;
}

int32_t utc_set_time(struct tm *local_time, long nsec)
{
    struct timespec time_spec = {0, 0};
    int32_t ret = 0;

    if (g_utc_init_flag == 0) {
        log_e("utc not init");
        return -1;
    }
    time_spec.tv_sec = mktime(local_time);
    time_spec.tv_nsec = nsec;

    ret = driver_write(g_driver, 0, &time_spec, sizeof(struct timespec));

    return ret;
}

int32_t utc_set_time_zone(int8_t time_zone)
{
    g_time_zone = time_zone;
    return 0;
}

int8_t utc_get_time_zone(void)
{
    return g_time_zone;
}

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
