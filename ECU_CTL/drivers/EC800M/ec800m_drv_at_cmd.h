
/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __MP32GM51_HAL_XXXX_H
#define __MP32GM51_HAL_XXXX_H
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
#define EC800M_AT_ACK_OK                     "OK"
#define EC800M_AT_ACK_ERROR                  "ERROR"
#define EC800M_AT_ACK_REQUIRE_DATA           ">"        // Require data flg from the modem
#define EC800M_AT_ACK_SEND_OK                "SEND OK"  // Send data flg from the modem
#define EC800M_AT_CMD_READY                  "RDY"      // Modem ready to receive data
#define EC800M_AT_CMD_END                    "\r\n"
#define EC800M_AT_CMD_TIMEOUT                1000

#define EC800M_AT_CMD_ATI                    "ATI\r\n"
#define EC800M_AT_CMD_ATI_ACK_DEVICE         "EC800M"

#define EC800M_AT_CMD_CREG_ASK               "AT+CREG?\r\n"
#define EC800M_AT_CMD_CREG_ASK_STAT          "+CREG:"

#define EC800M_AT_CMD_QICLOSE                "AT+QICLOSE=0\r\n"

#define EC800M_AT_CMD_QIOPEN                 "AT+QIOPEN"
#define EC800M_AT_CMD_QIOPEN_TRANSPARENT_ACK "CONNECT"
#define EC800M_AT_CMD_QIOPEN_NO_CARRIER_ACK  "NO CARRIER"
#define EC800M_AT_CMD_QIOPEN_STRAIGHT_ACK    "+QIOPEN"

#define EC800M_AT_CMD_QUIT_TCP_TRANSPARENT_1 "+++"
#define EC800M_AT_CMD_QUIT_TCP_TRANSPARENT_2 "+++\r\n"

#define EC800M_AT_CMD_QISEND                 "AT+QISEND"

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
#endif /*__EC800M_DRV_AT_CMD_h */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
