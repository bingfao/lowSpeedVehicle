/*
 * @Author: your name
 * @Date: 2024-11-07 15:16:41
 * @LastEditTime: 2024-12-25 14:15:08
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\ebike_manage.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __EBIKE_MANAGE_H
#define __EBIKE_MANAGE_H
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

/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */
typedef enum {
    EBIKE_TYPE_NULL = 0,
    EBIKE_TYPE_E_BIKE = 1,
    EBIKE_TYPE_E_MOTORBIKE = 2,
    EBIKE_TYPE_E_TRICYCLE = 4,
    EBIKE_TYPE_E_QUADRICYCLE = 6,
    EBIKE_TYPE_E_REPLACE_BATTERY_CUPBOARD = 8,
    EBIKE_TYPE_E_CHARGER_BATTERY_CUPBOARD = 9,
} EBIKE_TYPE_t;

typedef struct
{
    uint32_t dev_id;
    uint32_t session_id;
    EBIKE_TYPE_t dev_type;
    uint8_t power_battery_series_counts;
    uint8_t power_battery_parallel_counts;
    uint8_t aes_iv[16];
    int8_t register_flg;
    int8_t connect_flg;
    void *net_agreement_obj;
} EBIKE_MANAGE_OBJ_t;

/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */
#define EBIKE_DEV_ID                           (10009)              // default device id
#define EBIKE_DEV_TYPE                         (EBIKE_TYPE_E_BIKE)  // default device type

#define EBIKE_SMALL_BATTERY_ID_SIZE            (30)
#define EBIKE_SMALL_BATTERY_ID                 ("12345678901234567890123456789\0")

#define EBIKE_POWER_BATTERY_ID_SIZE            (32)
#define EBIKE_POWER_BATTERY_ID                 ("1234567890123456789012345678901\0")

#define EBIKE_POWER_BATTERY_SERIES_COUNT_MAX   (18)
#define EBIKE_POWER_BATTERY_PARALLEL_COUNT_MAX (6)
#define EBIKE_POWER_BATTERY_SERIES_COUNT       (16)  // default series count
#define EBIKE_POWER_BATTERY_PARALLEL_COUNT     (1)   // default parallel count

/*
 * ****************************************************************************
 * ******** Exported macro                                             ********
 * ****************************************************************************
 */

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
int32_t ebike_manage_init(void);
bool ebike_is_connected_server(void);
int32_t ebike_device_register_to_server(void);
int32_t ebike_device_state_upload_to_server(void);
// get ebike info
EBIKE_TYPE_t ebike_get_device_type(void);
int32_t ebike_get_location(double *lng, double *lat);
float ebike_get_total_mileage(void);
bool ebike_is_running(void);
float ebike_get_speed(void);
// get ebike state info
bool ebike_electric_lock_is_lock(void);
bool ebike_seat_lock_is_lock(void);
bool ebike_trunk_lock_is_lock(void);
bool ebike_helmet_lock_is_lock(void);
bool ebike_electric_drive_lock_is_lock(void);
uint16_t ebike_get_state_short_range_headlight(void);
uint16_t ebike_get_state_long_range_headlight(void);
uint16_t ebike_get_state_clearance_lamp(void);
uint16_t ebike_get_state_turn_signal(void);
uint16_t ebike_get_state_emergency_flashers(void);
uint16_t ebike_get_state_brake_light(void);
bool ebike_sensor_seat_is_tirgger(void);
bool ebike_sensor_kick_stand_is_tirgger(void);
bool ebike_sensor_child_seat_is_tirgger(void);
bool ebike_sensor_rollover_is_tirgger(void);
bool ebike_rear_brake_is_tirgger(void);
bool ebike_front_brake_is_tirgger(void);
bool ebike_abs_is_tirgger(void);
bool ebike_tcs_tirgger(void);
// get mini battery info
bool ebike_mini_battery_is_exit(void);
int32_t ebike_get_mini_battery_id(uint8_t *id, uint32_t *len);
int32_t ebike_get_mini_battery_soc(uint8_t *soc);
int32_t ebike_get_mini_battery_voltage(float *voltage);
int32_t ebike_get_mini_battery_temp(float *temper);
// get power battery info
bool ebike_power_battery_is_exit(void);
bool ebike_power_battery_is_charging(void);
int32_t ebike_get_power_battery_id(uint8_t *id, uint32_t *len);
int32_t ebike_get_power_battery_soc(uint8_t *soc);
int32_t ebike_get_power_battery_voltage(float *voltage);
int32_t ebike_get_power_battery_current(float *current);
int32_t ebike_get_power_battery_temp(float *temper);
int32_t ebike_get_power_battery_charge_cycle(uint16_t *cycle);
int32_t ebike_get_power_battery_soc_health(uint8_t *soc_health);
int32_t ebike_get_power_battery_series_count(uint8_t *count);
int32_t ebike_get_power_battery_parallel_count(uint8_t *count);
int32_t ebike_get_power_battery_series_voltages(float *voltages, uint8_t series_count, uint8_t parallel__count);
int32_t ebike_get_power_battery_series_temp(float *temp, uint8_t series_count, uint8_t parallel__count);

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__EBIKE_MANAGE_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
