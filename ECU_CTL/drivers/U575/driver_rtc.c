/*
 * @Author: your name
 * @Date: 2025-01-31 15:02:11
 * @LastEditTime: 2025-01-31 22:11:05
 * @LastEditors: stone_honor
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\U575\driver_rtc.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "driver_rtc.h"

#include <time.h>

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
uint8_t g_rtc_open_flag = 0;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t rtc_drv_init(DRIVER_OBJ_t *p_driver);
static int32_t rtc_drv_deinit(DRIVER_OBJ_t *p_driver);
static int32_t rtc_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag);
static int32_t rtc_drv_close(DRIVER_OBJ_t *p_driver);
static int32_t rtc_drv_read(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);
static int32_t rtc_drv_write(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);

DRIVER_CTL_t g_driver_rtc = {
    .init = rtc_drv_init,
    .deinit = rtc_drv_deinit,
    .open = rtc_drv_open,
    .close = rtc_drv_close,
    .read = rtc_drv_read,
    .write = rtc_drv_write,
};
DRIVER_REGISTER(&g_driver_rtc, rtc)

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
static int32_t rtc_drv_init(DRIVER_OBJ_t *p_driver)
{
    return 0;
}

static int32_t rtc_drv_deinit(DRIVER_OBJ_t *p_driver)
{
    return 0;
}
static int32_t rtc_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag)
{
    g_rtc_open_flag = 1;

    return 0;
}

static int32_t rtc_drv_close(DRIVER_OBJ_t *p_driver)
{
    g_rtc_open_flag = 0;

    return 0;
}

static int32_t rtc_drv_read(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    struct tm time_info = {0};
    struct timespec *p_time = NULL;
    RTC_DateTypeDef data;
    RTC_TimeTypeDef data_time;
    time_t timestamp = 0;

    if (g_rtc_open_flag == 0 || buffer == NULL || size != sizeof(struct timespec)) {
        return -1;
    }
    p_time = (struct timespec *)buffer;
    HAL_RTC_GetTime(&hrtc, &data_time, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &data, RTC_FORMAT_BIN);
    // printf("RTC_DateTypeDef: %d-%d-%d, %d\r\n", data.Year, data.Month, data.Date, data.WeekDay);
    // printf("RTC_TimeTypeDef: %d:%d:%d, %d\r\n", data_time.Hours, data_time.Minutes, data_time.Seconds, data_time.SubSeconds);

    time_info.tm_year = (int)data.Year + 2000 - 1900;  // tm_year 是从1900年开始的年数
    time_info.tm_mon = data.Month - 1;                 // tm_mon 是从0开始的月份（0表示1月）
    time_info.tm_mday = data.Date;                     // tm_mday 是日
    time_info.tm_wday = data.WeekDay % 7;              // tm_wday 是星期几（0表示星期日）
    time_info.tm_hour = data_time.Hours;               // tm_hour 是时
    time_info.tm_min = data_time.Minutes;              // tm_min 是分
    time_info.tm_sec = data_time.Seconds;              // tm_sec 是秒
    timestamp = mktime(&time_info);

    p_time->tv_sec = timestamp;
    p_time->tv_nsec = (data_time.SubSeconds % 0x100) * 1000 / 0x100 * 1000000;

    return 0;
}

static int32_t rtc_drv_write(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    struct tm *time_info = NULL;
    struct timespec *p_time = NULL;
    RTC_DateTypeDef data = {0};
    RTC_TimeTypeDef data_time = {0};
    time_t timestamp = 0;

    if (g_rtc_open_flag == 0 || buffer == NULL || size != sizeof(struct timespec)) {
        return -1;
    }
    p_time = (struct timespec *)buffer;
    timestamp = p_time->tv_sec;
    time_info = localtime(&timestamp);

    data.Year = (uint8_t)(time_info->tm_year + 1900 - 2000);  // tm_year 是从1900年开始的年数
    data.Month = (uint8_t)(time_info->tm_mon + 1);            // tm_mon 是从0开始的月份（0表示1月）
    data.Date = (uint8_t)time_info->tm_mday;                  // tm_mday 是日
    data.WeekDay = (uint8_t)time_info->tm_wday;
    data.WeekDay = (data.WeekDay == 0) ? 7 : data.WeekDay;  // tm_wday 是星期几（0表示星期日）
    data_time.Hours = (uint8_t)time_info->tm_hour;              // tm_hour 是时
    data_time.Minutes = (uint8_t)time_info->tm_min;             // tm_min 是分
    data_time.Seconds = (uint8_t)time_info->tm_sec;             // tm_sec 是秒
    data_time.SubSeconds = (uint32_t)p_time->tv_nsec / 1000000 * 0x100 / 1000;
    HAL_RTC_SetDate(&hrtc, &data, RTC_FORMAT_BIN);
    HAL_RTC_SetTime(&hrtc, &data_time, RTC_FORMAT_BIN);

    return 0;
}
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
