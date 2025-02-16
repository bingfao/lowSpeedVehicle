/*
 * @Author: your name
 * @Date: 2025-02-14 21:34:33
 * @LastEditTime: 2025-02-16 22:08:05
 * @LastEditors: stone_honor
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\gnss.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __GNSS_PORT_H
#define __GNSS_PORT_H
/*
 * ============================================================================
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ============================================================================
 */
#ifdef __cplusplus
extern "C" {
#endif
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "stdint.h"
#include "stdio.h"
#include "time.h"

/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */
typedef struct
{
    float latitude;
    float longitude;
    float altitude;
    float hdop;  // Horizontal Dilution of Precision
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day;
    uint8_t month;
    uint16_t year;
    time_t timestamp;
    uint8_t time_type;  // 0: UTC, 1: LOCAL
} GNSS_LOCATION_t;

/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported macro                                             ********
 * ****************************************************************************
 */
#define GNSS_LOCATION_READ_INTERVAL_IN_STOP_MS (5 * 60 * 1000)  //  5 minutes
#define GNSS_LOCATION_READ_INTERVAL_IN_MOVE_MS (1 * 60 * 1000)  //  1 minutes
#define GNSS_LOCATION_READ_INTERVAL_IN_RUN_MS  (1 * 1000)       //  1 second
#define GNSS_LOCATION_READ_INTERVAL_RETRY_MS   (1 * 60 * 1000)  //  1 minutes

#define GNSS_ERR_CODE_GPS_GET_ERROR            (1 << 0)  // GPS get location error

/*
 * ****************************************************************************
 * ******** Exported variables                                         ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported Function                                          ********
 * ****************************************************************************
 */

int32_t gnss_init(void);
int32_t gnss_get_location(GNSS_LOCATION_t *location);
int32_t gnss_get_latitude_longitude(float *latitude, float *longitude);
void gnss_set_location_read_interval(int32_t interval_ms);
int32_t gnss_do_get_location(GNSS_LOCATION_t *location);  // for test only, get location without cache

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__GNSS_PORT_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
