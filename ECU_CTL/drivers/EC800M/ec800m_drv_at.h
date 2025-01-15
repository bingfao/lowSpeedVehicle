
/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __EC800M_DRV_AT_H
#define __EC800M_DRV_AT_H
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
#include <main.h>

#include "driver_com.h"
#include "net_port.h"

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
#define EC800M_DRV_AT_TX_BUF_SIZE        128
#define EC800M_DRV_AT_RX_BUF_SIZE        1024
#define EC800M_DRV_RX_BUF_SIZE           5120

#define EC800M_DRV_POS_BLOCKING          DEV_RXTX_POS_BLOCKING       // driver R/W position blocking
#define EC800M_DRV_POS_BLOCKING_1000     DEV_RXTX_POS_BLOCKING_1000  // driver R/W position blocking 1000ms
#define EC800M_DRV_POS_NONBLOCKING       DEV_RXTX_POS_NONBLOCKING    // driver R/W position non-blocking

#define DRV_EC800M_CMD_TCP_SET_HOST      NET_PORT_CMD_TCP_SET_HOST
#define DRV_EC800M_CMD_TCP_SET_PORT      NET_PORT_CMD_TCP_SET_PORT
#define DRV_EC800M_CMD_TCP_CONNECT       NET_PORT_CMD_TCP_CONNECT
#define DRV_EC800M_CMD_TCP_DISCONNECT    NET_PORT_CMD_TCP_DISCONNECT
#define DRV_EC800M_CMD_TCP_GET_MODE      NET_PORT_CMD_TCP_GET_MODE
#define DRV_EC800M_CMD_GET_CS_REGISTERED NET_PORT_CMD_GET_CS_REGISTERED
#define DRV_EC800M_CMD_RESET             NET_PORT_CMD_RESET
#define DRV_EC800M_CMD_TCP_REF_STATE     NET_PORT_CMD_TCP_REFRESH_STATE
#define DRV_EC800M_CMD_SET_DIS_STATE     NET_PORT_CMD_SET_DIS_STATE

#define EC800M_CONNECT_MODE_DISCONNECT   NET_PORT_TCP_CONNECT_MODE_DISCONNECT
#define EC800M_CONNECT_MODE_STRAIGHT_OUT NET_PORT_TCP_CONNECT_MODE_STRAIGHT_OUT
#define EC800M_CONNECT_MODE_DIRECT       NET_PORT_TCP_CONNECT_MODE_TRANSPARENT

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

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__EC800M_DRV_AT_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
