/*
 * @Author: your name
 * @Date: 2024-11-07 15:47:43
 * @LastEditTime: 2025-01-16 14:44:36
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\middlewares\net_agreement\net_agreement.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __NET_AGREEMENT_H
#define __NET_AGREEMENT_H
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
#define NET_TX_MSG_BODY_LEN_MAX      256
#define NET_TX_MSG_HEAD_BODY_LEN_MAX (NET_TX_MSG_BODY_LEN_MAX + 23)  // require header + body
#define NET_RX_MSG_BODY_LEN_MAX      0x5400                          // 21504 bytes
#define NET_RX_MSG_HEAD_BODY_LEN_MAX (NET_RX_MSG_BODY_LEN_MAX + 19)  // response header + body

#define NET_COMM_MSG_NUM_MAX         5

#pragma pack(push, 1)
typedef struct
{
    uint16_t msg_id;
    uint8_t type_flg;  // 0 Send, 1: Resp
    uint16_t seq_num;
    uint32_t msg_body_len;
    uint8_t crypt_flg;  // 0: No crypt, 1: AES
    uint8_t reserved[3];
    uint16_t crc16;
} NET_MSG_HEADER_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    NET_MSG_HEADER_t head;
    uint32_t dev_id;
    uint32_t session_id;
} NET_REQUIRE_HEADER_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    NET_MSG_HEADER_t head;
    uint32_t resp_code;
} NET_RESPONSE_HEADER_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    NET_REQUIRE_HEADER_t req_head;
    uint8_t msg_body[NET_TX_MSG_BODY_LEN_MAX];
} NET_SEND_MSG_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    NET_RESPONSE_HEADER_t resp_head;
    uint8_t resp_body[NET_RX_MSG_BODY_LEN_MAX];
} NET_RESP_MSG_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint8_t tx_msg[NET_TX_MSG_HEAD_BODY_LEN_MAX];
    uint8_t *rx_msg;
    uint8_t msg_state;
    uint32_t tx_msg_ticks;
} NET_COMM_MSG_t;
#pragma pack(pop)

typedef int32_t (*NET_AG_TX_FUNC)(uint8_t *buf, int32_t len);
typedef uint32_t (*NET_GET_TICK)(void);
typedef int32_t (*MSG_ID_FUNC)(uint8_t *in_data, int32_t in_len, uint8_t *out_data, int32_t *out_len);

/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */
#define NET_COMM_MSG_STATE_IDLE             0
#define NET_COMM_MSG_STATE_TX               1
#define NET_COMM_MSG_STATE_TX_ACK           2
#define NET_COMM_MSG_STATE_RX               3

#define NET_TX_MSG_ID_REGISTER_DEV          (1001)
#define NET_TX_MSG_ID_DEV_STATE             (1002)
#define NET_TX_MSG_ID_EBIKE_JOURNEY         (1008)
#define NET_TX_MSG_ID_EBIKE_CHARGE_END      (1010)
#define NET_TX_MSG_ID_EBIKE_EVENT_REPORT    (1011)
#define NET_TX_MSG_ID_MAX                   (5)

#define NET_RX_MSG_ID_UNLOCK                (2001)
#define NET_RX_MSG_ID_LOCK                  (2002)
#define NET_RX_MSG_ID_RESTRICT              (2003)
#define NET_RX_MSG_ID_FILE_DOWNLOAD         (2020)
#define NET_RX_MSG_ID_ENABLE_UNLOCK_DEVICE  (2021)
#define NET_RX_MSG_ID_DISABLE_UNLOCK_DEVICE (2022)
#define NET_RX_MSG_ID_MAX                   (6)

#define NET_MSG_RESPCODE_OK                 (0)
#define NET_MSG_RESPCODE_REDEFINE           (1)
#define NET_MSG_RESPCODE_REFUSE             (2)

#define ASSERT_MSG_ID(msg_id)                                                                                          \
    ((msg_id == NET_TX_MSG_ID_REGISTER_DEV) || (msg_id == NET_TX_MSG_ID_DEV_STATE) ||                                  \
     (msg_id == NET_TX_MSG_ID_EBIKE_JOURNEY) || (msg_id == NET_TX_MSG_ID_EBIKE_CHARGE_END) ||                          \
     (msg_id == NET_TX_MSG_ID_EBIKE_EVENT_REPORT) || (msg_id == NET_RX_MSG_ID_UNLOCK) ||                               \
     (msg_id == NET_RX_MSG_ID_LOCK) || (msg_id == NET_RX_MSG_ID_RESTRICT) ||                                           \
     (msg_id == NET_RX_MSG_ID_FILE_DOWNLOAD) || (msg_id == NET_RX_MSG_ID_ENABLE_UNLOCK_DEVICE) ||                      \
     (msg_id == NET_RX_MSG_ID_DISABLE_UNLOCK_DEVICE))

#define NET_MSG_TYPE_SEND          0
#define NET_MSG_TYPE_RESP          1

#define NET_MSG_CRYPT_FLG_NO_CRYPT 0
#define NET_MSG_CRYPT_FLG_AES      1

#define NET_MSG_TX_WAITE_TICKS_MAX 10000  // 10S for wait ack from server
#define NET_MSG_RX_WAITE_TIMEOUT   1000   // 1S for wait one package timeout

#define NET_MAS_LEN_REGISTER_ACK   24  // 24 bytes for register ack
#define NET_MAS_LEN_UPLOAD_ACK      4  // 4 bytes for upload ack
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
void *net_agreement_create(uint32_t dev_id, NET_AG_TX_FUNC tx_func, NET_GET_TICK get_tick_func);
void net_agreement_set_session_id(void *obj, uint32_t session_id);
uint32_t net_agreement_get_session_id(void *obj);
int32_t net_agreement_set_aes_key(void *obj, uint8_t *key, uint32_t len);
int32_t net_agreement_get_aes_key(void *obj, uint8_t *key, uint32_t *len);
int32_t net_agreement_set_aes_iv(void *obj, uint8_t *iv, uint32_t len);
int32_t net_agreement_get_aes_iv(void *obj, uint8_t *iv, uint32_t *len);
void net_agreement_destroy(void *obj);
int32_t register_agreement_tx_func(void *obj, NET_AG_TX_FUNC tx_func);
int32_t net_agreement_send_msg(void *obj, uint16_t msg_id, uint8_t msg_type, uint8_t *msg, uint32_t len);
bool net_agreement_msg_id_legal(uint16_t msg_id);
int32_t net_agreement_msg_regsiter(void *obj, uint16_t msg_id, uint8_t msg_type, MSG_ID_FUNC msg_func);
int32_t net_agreement_msg_unregsiter(void *obj, uint16_t msg_id);
int32_t net_agreement_data_in(void *obj, uint8_t *data, uint32_t len);
/* send package and ack data progress */
int32_t net_agreement_device_register_package(void *obj, uint8_t *data, uint32_t *len);
int32_t net_agreement_device_register_ack(void *obj, uint8_t *data, uint32_t len, uint32_t *session_id,
                                          uint8_t *aes_iv);
int32_t net_agreement_device_state_upload_package(void *obj, uint8_t *data, uint32_t *len);
int32_t net_agreement_device_state_upload_ack(void *obj, uint8_t *data, uint32_t len);

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__NET_AGREEMENT_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
