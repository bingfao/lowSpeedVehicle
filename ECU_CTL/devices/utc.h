/*
 * @Author: your name
 * @Date: 2025-01-31 19:31:11
 * @LastEditTime: 2025-02-11 13:39:20
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\utc.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __UTC_H
#define __UTC_H
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
#include <stdbool.h>
#include <stdint.h>

#include "driver_com.h"
#include "time.h"
/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */
#define UTC_DRIVER_NAME             "rtc"
/*
 * ****************************************************************************
 * ******** Exported macro                                             ********
 * ****************************************************************************
 */

#define UTC_NEED_TIME_SYNC_PERIOD_S (10 * 60)  // every 10 minutes to sync time from net
#define UTC_TIME_NEED_SYNC_GAP_S    (60)       // if 1 minute diff between local and net time, need to sync

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
int32_t utc_init(void);
bool utc_is_init(void);
int32_t utc_get_local_time(struct tm *local_time, long *nsec);
int32_t utc_set_local_time(struct tm *local_time, long nsec);
int32_t utc_get_time(struct tm *local_time, long *nsec);
int32_t utc_set_time(struct tm *local_time, long nsec);
int32_t utc_set_time_zone(int8_t time_zone);
int8_t utc_get_time_zone(void);
int32_t utc_time_store_bk_sram(void);
int32_t utc_time_sync_from_net(uint8_t is_force);
bool utc_need_time_sync(void);

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__UTC_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
