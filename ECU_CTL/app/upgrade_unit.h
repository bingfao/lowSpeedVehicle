/*
 * @Author: your name
 * @Date: 2025-02-23 10:38:13
 * @LastEditTime: 2025-02-24 09:26:19
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\app\upgrade_unit.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __UPGRADE_UNIT_H
#define __UPGRADE_UNIT_H
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
#define UPGRADE_START_SOURCE_SHELL_SERIAL 0x01  // use shell port to start upgrade
#define UPGRADE_START_SOURCE_BLE_UART     0x02  // use ble uart port to start upgrade
#define UPGRADE_START_SOURCE_OTA_MODE     0x03  // use ota mode to start upgrade

#define UPGRADE_DEST_MCU                  0x00  // upgrade MCU, ECU upgrade
#define UPGRADE_DEST_BMS                  0x01  // upgrade BMS
#define UPGRADE_DEST_MOTOR_CONTROL        0x02  // upgrade Motor Control

#define UPGRADE_STATUS_IDLE               0x00  // upgrade idle
#define UPGRADE_STATUS_START              0x01  // upgrade start
#define UPGRADE_STATUS_ONGOING            0x02  // upgrade ongoing
#define UPGRADE_STATUS_STOP               0x03  // upgrade stop
#define UPGRADE_STATUS_TIMEOUT            0x04  // upgrade success
#define UPGRADE_STATUS_SUCCESS            0x05  // upgrade success
#define UPGRADE_STATUS_FAIL               0x06  // upgrade fail

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
int32_t upgrade_unit_init(void);
int32_t upgrade_mcu_ymodem_start(uint8_t src, uint8_t dest);

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__UPGRADE_UNIT_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
