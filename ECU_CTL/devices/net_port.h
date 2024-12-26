/*
 * @Author: your name
 * @Date: 2024-10-24 15:09:26
 * @LastEditTime: 2024-12-26 11:31:36
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\net_port.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __NET_PORT_H
#define __NET_PORT_H
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
#include <stdint.h>

#include "driver_com.h"

/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */
typedef enum {
    NET_PORT_CMD_NONE = DRV_CMD_NET_PORT_OPERATION_BASE,
    NET_PORT_CMD_TCP_SET_HOST,
    NET_PORT_CMD_TCP_SET_PORT,
    NET_PORT_CMD_TCP_CONNECT,
    NET_PORT_CMD_TCP_DISCONNECT,
    NET_PORT_CMD_TCP_GET_MODE,
    NET_PORT_CMD_GET_CS_REGISTERED,  // Check Internet access (0: not registered, 1: registered)
    NET_PORT_CMD_RESET,
    NET_PORT_CMD_TCP_REFRESH_STATE,   // refresh socket state (0: not connected, 1: connected)
    NET_PORT_CMD_SET_DIS_STATE,
    NET_PORT_CMD_TCP_MAX
} NET_PORT_CMD_t;

/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */
#define NET_PORT_TCP_CONNECT_MODE_DISCONNECT   0  // tcp is disconnect
#define NET_PORT_TCP_CONNECT_MODE_STRAIGHT_OUT 1  // tcp connect with straight out mode
#define NET_PORT_TCP_CONNECT_MODE_TRANSPARENT  2  // tcp connect with transparent  mode

// #define NET_PORT_TCP_TEST_HOST "s352373a61.vicp.fun"
// #define NET_PORT_TCP_TEST_PORT "37494"
#define NET_PORT_TCP_TEST_HOST "kingxun.site"
#define NET_PORT_TCP_TEST_PORT "10086"

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

int32_t net_port_init(void);
int32_t net_port_deinit(void);
int32_t net_port_tcp_connect(const char *host, const char *port);
int32_t net_port_tcp_disconnect(void);
bool net_port_is_connected(void);
int32_t net_port_send(const uint8_t *buf, uint32_t len);
int32_t net_port_recv(uint8_t *buf, uint32_t len);
int32_t net_port_socket_refresh(void);
int32_t net_port_tcp_reconnect(void);


/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__NET_PORT_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
