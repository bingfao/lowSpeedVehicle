/*
 * @Author: your name
 * @Date: 2025-01-08 14:09:44
 * @LastEditTime: 2025-01-08 14:11:53
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\bms_port.h
 */


/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __BMS_PORT_H
#define __BMS_PORT_H
/*
 * ============================================================================
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ============================================================================
 */
#ifdef __cplusplus
extern "C"
{
#endif
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include <stdint.h>

#include "driver_com.h"

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

int32_t bms_port_init(void);
int32_t bms_port_deinit(void);
int32_t bms_port_send(const uint8_t *buf, uint32_t len);
int32_t bms_port_recv(uint8_t *buf, uint32_t len);


/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__BMS_PORT_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
