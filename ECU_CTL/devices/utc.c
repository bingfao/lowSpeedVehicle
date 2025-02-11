/*
 * @Author: your name
 * @Date: 2025-01-31 19:31:02
 * @LastEditTime: 2025-02-11 13:40:55
 * @LastEditors: DESKTOP-SPAS98O
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

#include <stdlib.h>

#include "bk_config.h"
#include "elog.h"
#include "error_code.h"
#include "net_port.h"
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
static time_t g_last_sync_utc_sec = 0;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static void read_utc_save_data(void);
static void write_utc_save_data(uint32_t utc_sec, uint32_t utc_nsec);

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
    read_utc_save_data();
    g_utc_init_flag = 1;

    return 0;
}

bool utc_is_init(void)
{
    return g_utc_init_flag != 0;
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
    write_utc_save_data(time_spec.tv_sec, time_spec.tv_nsec);
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
    write_utc_save_data(time_spec.tv_sec, time_spec.tv_nsec);

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
    write_utc_save_data(time_spec.tv_sec, time_spec.tv_nsec);
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
    write_utc_save_data(time_spec.tv_sec, time_spec.tv_nsec);

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

int32_t utc_time_store_bk_sram(void)
{
    struct timespec time_spec = {0, 0};
    int32_t ret = 0;

    if (g_utc_init_flag == 0) {
        // log_e("utc not init");
        return -1;
    }

    ret = driver_read(g_driver, 0, &time_spec, sizeof(struct timespec));
    if (ret != 0) {
        log_e("driver %s read failed", UTC_DRIVER_NAME);
        return ret;
    }
    write_utc_save_data(time_spec.tv_sec, time_spec.tv_nsec);

    return 0;
}

int32_t utc_time_sync_from_net(uint8_t is_force)
{
    struct tm temp_tm;
    int32_t ret = 0;
    long nsec = 0;
    struct tm local_time = {0};
    int64_t difftime = 0;

    ret = net_port_get_utc(&temp_tm);
    if (ret != 0) {
        log_e("net_port_get_utc failed");
        return ret;
    }
    if (is_force == 0) {
        // if not force, check the time diff between local and net time, if less than 1 minute, no need to sync
        ret = utc_get_time(&local_time, &nsec);
        if (ret != 0) {
            log_e("utc_get_time failed");
            return ret;
        }
        difftime = abs(mktime(&temp_tm) - mktime(&local_time));
        if (difftime < UTC_TIME_NEED_SYNC_GAP_S) {
            return 0;
        }
    }
    ret = utc_set_time(&temp_tm, 0);
    if (ret != 0) {
        log_e("utc_set_time failed");
    }
    g_last_sync_utc_sec = mktime(&temp_tm);

    return ret;
}

/**
 * @brief this function is used to check if the time needs to be synchronized from the network, synchronize period is be
 * defined by UTC_NEED_TIME_SYNC_PERIOD_S
 *
 * @return true
 * @return false
 */
bool utc_need_time_sync(void)
{
    int32_t ret = 0;
    struct tm local_time = {0};
    long nsec = 0;
    int64_t difftime = 0;

    ret = utc_get_time(&local_time, &nsec);
    if (ret != 0) {
        log_e("utc_get_time failed");
        return true;
    }
    difftime = abs(mktime(&local_time) - g_last_sync_utc_sec);
    if (difftime < UTC_NEED_TIME_SYNC_PERIOD_S) {
        return false;
    }

    return true;
}

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */
static void read_utc_save_data(void)
{
    int32_t ret = 0;
    uint32_t utc_data[2] = {0, 0};
    struct timespec time_spec = {0, 0};

    ret = bk_config_utc_sec_nsec_read((uint8_t *)utc_data);
    if (ret != 0) {
        log_e("bk_config_utc_sec_nsec_read failed");
        return;
    }
    log_d("bk_config UTC sec %d nsec %d", utc_data[0], utc_data[1]);
    time_spec.tv_sec = utc_data[0];
    time_spec.tv_nsec = utc_data[1];
    ret = driver_write(g_driver, 0, &time_spec, sizeof(struct timespec));
    if (ret != 0) {
        log_e("driver %s write failed", UTC_DRIVER_NAME);
        return;
    }
}

static void write_utc_save_data(uint32_t utc_sec, uint32_t utc_nsec)
{
    int32_t ret = 0;
    uint32_t utc_data[2] = {utc_sec, utc_nsec};

    if (bk_config_is_init() != true) {
        return;
    }
    ret = bk_config_utc_sec_nsec_write((uint8_t *)utc_data);
    if (ret != 0) {
        log_e("bk_config_utc_sec_nsec_write failed");
        return;
    }
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
