/*
 * @Author: your name
 * @Date: 2024-11-07 15:47:34
 * @LastEditTime: 2024-12-16 19:53:06
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\middlewares\net_agreement\net_agreement.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "net_agreement.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "error_code.h"
#include "fault.h"
#include "user_crc.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef struct
{
    uint16_t msg_id;
    uint8_t msg_type;
    MSG_ID_FUNC msg_func;
    uint8_t msg_register;
} MSG_ID_FUNC_MAP_t;

typedef struct
{
    uint32_t dev_id;
    uint32_t session_id;
    uint16_t msg_num;
    uint16_t msg_resp_num;
    NET_AG_TX_FUNC tx_func;
    NET_GET_TICK get_tick;
    NET_COMM_MSG_t comm_msg[NET_COMM_MSG_NUM_MAX];
    MSG_ID_FUNC_MAP_t msg_id_func_map[NET_TX_MSG_ID_MAX + NET_RX_MSG_ID_MAX];
    uint8_t rx_msg[NET_RX_MSG_HEAD_BODY_LEN_MAX];
    uint32_t rx_msg_index;
    uint32_t msg_rx_state;
} NET_AGREEMENT_OBJ_t;

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */
#define NET_MSG_RX_STATE_IDLE              0
#define NET_MSG_RX_STATE_WAIT_MSG_ID_1     1
#define NET_MSG_RX_STATE_WAIT_MSG_ID_2     2
#define NET_MSG_RX_STATE_WAIT_MSG_HEADER   3
#define NET_MSG_RX_STATE_WAIT_TOTAL_HEADER 4
#define NET_MSG_RX_STATE_WAIT_BODY_END     5
#define NET_MSG_RX_STATE_PROGRESS          6

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
NET_AGREEMENT_OBJ_t g_net_agreement_obj;

uint16_t g_net_msg_id_map[NET_TX_MSG_ID_MAX + NET_RX_MSG_ID_MAX] = {
    NET_TX_MSG_ID_REGISTER_DEV,
    NET_TX_MSG_ID_DEV_STATE,
    NET_TX_MSG_ID_EBIKE_JOURNEY,
    NET_TX_MSG_ID_EBIKE_CHARGE_END,
    NET_TX_MSG_ID_EBIKE_EVENT_REPORT,
    NET_RX_MSG_ID_UNLOCK,
    NET_RX_MSG_ID_LOCK,
    NET_RX_MSG_ID_RESTRICT,
    NET_RX_MSG_ID_FILE_DOWNLOAD,
    NET_RX_MSG_ID_ENABLE_UNLOCK_DEVICE,
    NET_RX_MSG_ID_DISABLE_UNLOCK_DEVICE,
};

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static bool msg_body_need_aes(uint32_t msg_id);
static int32_t msg_aes_encoder(uint8_t *in_msg, uint32_t in_len, uint8_t *out_msg, uint32_t *out_len);
static int32_t msg_aes_decoder(uint8_t *in_msg, uint32_t in_len, uint8_t *out_msg, uint32_t *out_len);
static bool net_msg_id_is_registered(NET_AGREEMENT_OBJ_t *net_obj, uint8_t *data);
static MSG_ID_FUNC_MAP_t *net_msg_id_get_register_obj(NET_AGREEMENT_OBJ_t *net_obj, uint8_t *data);
static bool net_msg_crc_check_ok(uint8_t *data);
static bool net_msg_type_is_resp(uint8_t *data);
static uint32_t net_msg_head_get_body_len(uint8_t *data);
static int32_t net_msg_body_aes_decoder(uint8_t *data);
static int32_t net_msg_body_process(NET_AGREEMENT_OBJ_t *net_obj, uint8_t *data);
static int32_t net_agreement_byte_in(NET_AGREEMENT_OBJ_t *net_obj, uint8_t byte, uint32_t tick);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
void *net_agreement_create(uint32_t dev_id, NET_AG_TX_FUNC tx_func, NET_GET_TICK get_tick_func)
{
    memset(&g_net_agreement_obj, 0, sizeof(NET_AGREEMENT_OBJ_t));
    g_net_agreement_obj.dev_id = dev_id;
    g_net_agreement_obj.tx_func = tx_func;
    g_net_agreement_obj.get_tick = get_tick_func;

    return &g_net_agreement_obj;
}

void net_agreement_set_session_id(void *obj, uint32_t session_id)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    net_obj->session_id = session_id;
}

uint32_t net_agreement_get_session_id(void *obj)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    return net_obj->session_id;
}

void net_agreement_destroy(void *obj)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    memset(net_obj, 0, sizeof(NET_AGREEMENT_OBJ_t));
}

int32_t register_agreement_tx_func(void *obj, NET_AG_TX_FUNC tx_func)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    net_obj->tx_func = tx_func;
    return 0;
}

/**
 * @brief net agreement send msg
 * @param obj net argument
 * @param msg_id message id
 * @param msg_type 0:send 1:resp
 * @param msg msg body
 * @param len msg body length
 * @return int32_t <0:error, =0:success, >0:send msg length
 */
int32_t net_agreement_send_msg(void *obj, uint16_t msg_id, uint8_t msg_type, uint8_t *msg, uint32_t len)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    int32_t i = 0;
    NET_COMM_MSG_t *comm_msg = NULL;
    NET_SEND_MSG_t *tx_msg = NULL;
    int32_t ret = 0;
    uint32_t ticks = 0;
    uint32_t out_len = 0;
    uint32_t *start_addr = NULL;

    fault_assert(ASSERT_MSG_ID(msg_id), FAULT_CODE_NET_AGMT);
    if (net_obj->tx_func == NULL || net_obj->dev_id == 0) {
        return -EACCES;
    }
    if (net_obj->get_tick) {
        ticks = net_obj->get_tick();
    }
    /* find idle and timeout comm_msg  */
    for (i = 0; i < NET_COMM_MSG_NUM_MAX; i++) {
        if ((net_obj->comm_msg[i].msg_state == NET_COMM_MSG_STATE_IDLE) ||
            ((net_obj->comm_msg[i].tx_msg_ticks + NET_MSG_TX_WAITE_TICKS_MAX) < ticks)) {
            net_obj->comm_msg[i].msg_state = NET_COMM_MSG_STATE_IDLE;
            break;
        }
    }
    if (i >= NET_COMM_MSG_NUM_MAX) {
        return -ENOMEM;
    }
    // net_obj->msg_num++;
    comm_msg = &net_obj->comm_msg[i];
    tx_msg = (NET_SEND_MSG_t *)&comm_msg->tx_msg;
    if (len > sizeof(tx_msg->msg_body)) {
        return -ENOMEM;
    }
    if (len > 0) {
        if (msg_body_need_aes(msg_id)) {
            ret = msg_aes_encoder(msg, len, tx_msg->msg_body, &out_len);
            tx_msg->req_head.head.msg_body_len = out_len;
            tx_msg->req_head.head.crypt_flg = NET_MSG_CRYPT_FLG_AES;
        } else {
            memcpy(tx_msg->msg_body, msg, len);
            tx_msg->req_head.head.msg_body_len = len;
            tx_msg->req_head.head.crypt_flg = NET_MSG_CRYPT_FLG_NO_CRYPT;
        }
    }

    tx_msg->req_head.head.msg_id = msg_id;
    tx_msg->req_head.head.type_flg = msg_type;
    if (msg_type == NET_MSG_TYPE_SEND) {
        tx_msg->req_head.head.seq_num = net_obj->msg_num++;
    } else {
        tx_msg->req_head.head.seq_num = net_obj->msg_resp_num;
    }
    memset(tx_msg->req_head.head.reserved, 0, sizeof(tx_msg->req_head.head.reserved));
    start_addr = (uint32_t *)&(tx_msg->req_head);
    tx_msg->req_head.head.crc16 = crc16_ccitt((unsigned char *)start_addr, sizeof(tx_msg->req_head.head) - 2, 0);
    tx_msg->req_head.dev_id = net_obj->dev_id;
    tx_msg->req_head.session_id = net_obj->session_id;
    if (net_obj->tx_func) {
        ret = net_obj->tx_func((uint8_t *)tx_msg, sizeof(NET_REQUIRE_HEADER_t) + tx_msg->req_head.head.msg_body_len);
        if (ret >= 0) {
            if (net_obj->get_tick) {
                comm_msg->tx_msg_ticks = net_obj->get_tick();
            } else {
                comm_msg->tx_msg_ticks = 0;
            }
            if (msg_type == NET_MSG_TYPE_SEND) {
                comm_msg->msg_state = NET_COMM_MSG_STATE_TX;
            } else {
                comm_msg->msg_state = NET_COMM_MSG_STATE_IDLE;
            }
        }
    }

    return ret;
}

bool net_agreement_msg_id_legal(uint16_t msg_id)
{
    int i = 0;

    for (i = 0; i < sizeof(g_net_msg_id_map) / sizeof(uint16_t); i++) {
        if (g_net_msg_id_map[i] == msg_id) {
            return true;
        }
    }

    return false;
}

int32_t net_agreement_msg_regsiter(void *obj, uint16_t msg_id, uint8_t msg_type, MSG_ID_FUNC msg_func)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    int32_t i = 0;

    if (net_agreement_msg_id_legal(msg_id) == false) {
        return -EINVAL;
    }

    for (i = 0; i < sizeof(net_obj->msg_id_func_map) / sizeof(MSG_ID_FUNC_MAP_t); i++) {
        if (net_obj->msg_id_func_map[i].msg_register == 0) {
            net_obj->msg_id_func_map[i].msg_id = msg_id;
            net_obj->msg_id_func_map[i].msg_type = msg_type;
            net_obj->msg_id_func_map[i].msg_func = msg_func;
            net_obj->msg_id_func_map[i].msg_register = 1;
            return 0;
        }
    }
    return -ENOMEM;
}

int32_t net_agreement_msg_unregsiter(void *obj, uint16_t msg_id)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    int32_t i = 0;

    if (net_agreement_msg_id_legal(msg_id) == false) {
        return -EINVAL;
    }

    for (i = 0; i < sizeof(net_obj->msg_id_func_map) / sizeof(MSG_ID_FUNC_MAP_t); i++) {
        if (net_obj->msg_id_func_map[i].msg_register == 1 && net_obj->msg_id_func_map[i].msg_id == msg_id) {
            memset(&net_obj->msg_id_func_map[i], 0, sizeof(MSG_ID_FUNC_MAP_t));
            return 0;
        }
    }

    return -ENOMEM;
}

int32_t net_agreement_data_in(void *obj, uint8_t *data, uint32_t len)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    int32_t i = 0;
    int32_t ret = 0;
    uint32_t ticks = 0;

    if (net_obj->get_tick) {
        ticks = net_obj->get_tick();
    }
    for (i = 0; i < len; i++) {
        net_agreement_byte_in(obj, data[i], ticks);
    }

    return ret;
}

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */
static bool msg_body_need_aes(uint32_t msg_id)
{
    if (msg_id == NET_TX_MSG_ID_REGISTER_DEV || msg_id == NET_TX_MSG_ID_DEV_STATE) {
        return false;
    }
    return true;
}

static int32_t msg_aes_encoder(uint8_t *in_msg, uint32_t in_len, uint8_t *out_msg, uint32_t *out_len)
{
    return *out_len;
}

static int32_t msg_aes_decoder(uint8_t *in_msg, uint32_t in_len, uint8_t *out_msg, uint32_t *out_len)
{
    return *out_len;
}

static bool net_msg_id_is_registered(NET_AGREEMENT_OBJ_t *net_obj, uint8_t *data)
{
    int32_t i = 0;
    MSG_ID_FUNC_MAP_t *register_obj = NULL;
    NET_MSG_HEADER_t *msg_header = (NET_MSG_HEADER_t *)data;

    for (i = 0; i < sizeof(net_obj->msg_id_func_map) / sizeof(MSG_ID_FUNC_MAP_t); i++) {
        register_obj = &net_obj->msg_id_func_map[i];
        if (register_obj->msg_register == 1 && register_obj->msg_id == msg_header->msg_id &&
            register_obj->msg_type == msg_header->type_flg) {
            return true;
        }
    }

    return false;
}

static MSG_ID_FUNC_MAP_t *net_msg_id_get_register_obj(NET_AGREEMENT_OBJ_t *net_obj, uint8_t *data)
{
    int32_t i = 0;
    MSG_ID_FUNC_MAP_t *register_obj = NULL;
    NET_MSG_HEADER_t *msg_header = (NET_MSG_HEADER_t *)data;

    for (i = 0; i < sizeof(net_obj->msg_id_func_map) / sizeof(MSG_ID_FUNC_MAP_t); i++) {
        register_obj = &net_obj->msg_id_func_map[i];
        if (register_obj->msg_register == 1 && register_obj->msg_id == msg_header->msg_id &&
            register_obj->msg_type == msg_header->type_flg) {
            return register_obj;
        }
    }

    return NULL;
}

static bool net_msg_crc_check_ok(uint8_t *data)
{
    uint16_t crc16 = 0;
    NET_MSG_HEADER_t *msg_header = (NET_MSG_HEADER_t *)data;

    crc16 = crc16_ccitt(data, sizeof(NET_MSG_HEADER_t) - 2, 0);
    if (crc16 != msg_header->crc16) {
        return false;
    }

    return true;
}

static bool net_msg_type_is_resp(uint8_t *data)
{
    NET_MSG_HEADER_t *msg_header = (NET_MSG_HEADER_t *)data;

    if (msg_header->type_flg == NET_MSG_TYPE_RESP) {
        return true;
    }

    return false;
}

static uint32_t net_msg_head_get_body_len(uint8_t *data)
{
    NET_MSG_HEADER_t *msg_header = (NET_MSG_HEADER_t *)data;

    return msg_header->msg_body_len;
}

static int32_t net_msg_body_aes_decoder(uint8_t *data)
{
    uint8_t *msg_body = 0;
    int32_t ret = 0;
    NET_MSG_HEADER_t *msg_header = (NET_MSG_HEADER_t *)data;
    uint32_t out_len = 0;

    if (msg_header->crypt_flg == NET_MSG_CRYPT_FLG_NO_CRYPT) {
        return 0;  // no need to decode
    }
    if (net_msg_type_is_resp(data) == true) {
        msg_body = data + sizeof(NET_RESPONSE_HEADER_t);
    } else {
        msg_body = data + sizeof(NET_REQUIRE_HEADER_t);
    }

    ret = msg_aes_decoder(msg_body, msg_header->msg_body_len, msg_body, &out_len);
    if (ret < 0) {
        return ret;
    }
    msg_header->msg_body_len = out_len;

    return 0;
}

static int32_t net_msg_body_process(NET_AGREEMENT_OBJ_t *net_obj, uint8_t *data)
{
    NET_MSG_HEADER_t *msg_header_rx = (NET_MSG_HEADER_t *)data;
    NET_MSG_HEADER_t *msg_header_tx = NULL;
    NET_REQUIRE_HEADER_t *req_head = (NET_REQUIRE_HEADER_t *)data;
    // NET_RESPONSE_HEADER_t *resp_head = (NET_RESPONSE_HEADER_t *)data;
    MSG_ID_FUNC_MAP_t *register_obj = NULL;
    int32_t i = 0;
    int32_t ret = 0;
    uint8_t *buf = NULL;
    int32_t len = 0;
    uint8_t send_buf[64] = {0};
    int32_t send_buf_len = 0;

    if (net_msg_type_is_resp(data) == true) {
        for (i = 0; i < NET_COMM_MSG_NUM_MAX; i++) {
            msg_header_tx = (NET_MSG_HEADER_t *)&net_obj->comm_msg[i].tx_msg;
            if (msg_header_tx->msg_id == msg_header_rx->msg_id && msg_header_tx->seq_num == msg_header_rx->seq_num) {
                net_obj->comm_msg[i].msg_state = NET_COMM_MSG_STATE_IDLE;
                net_obj->comm_msg[i].rx_msg = data;
                break;
            }
        }
        if (i >= NET_COMM_MSG_NUM_MAX) {
            return -EINVAL;
        }
        buf = data + sizeof(NET_MSG_HEADER_t);
        len = net_msg_head_get_body_len(data) + sizeof(NET_RESPONSE_HEADER_t) - sizeof(NET_MSG_HEADER_t);

    } else {
        if (req_head->dev_id != net_obj->dev_id || req_head->session_id != net_obj->session_id) {
            return -EINVAL;
        }
        buf = data + sizeof(NET_REQUIRE_HEADER_t);
        len = net_msg_head_get_body_len(data);
    }
    register_obj = net_msg_id_get_register_obj(net_obj, data);
    if (register_obj == NULL || register_obj->msg_func == NULL) {
        return -EINVAL;
    }
    ret = register_obj->msg_func(buf, len, send_buf, &send_buf_len);
    if (ret < 0) {
        return ret;
    }
    if (send_buf_len > 0) {
        ret = net_agreement_send_msg(net_obj, req_head->head.msg_id, NET_MSG_TYPE_RESP, send_buf, send_buf_len);
        if (ret < 0) {
            return ret;
        }
    }

    return 0;
}

static int32_t net_agreement_byte_in(NET_AGREEMENT_OBJ_t *net_obj, uint8_t byte, uint32_t tick)
{
    int32_t ret = 0;
    static uint16_t temp_msg_id = 0;
    static uint32_t temp_index = 0;
    uint8_t *rx_data = net_obj->rx_msg;
    uint32_t *rx_data_index = &net_obj->rx_msg_index;
    static uint32_t old_tick = 0;

    if (tick - old_tick > NET_MSG_RX_WAITE_TIMEOUT) {
        net_obj->msg_rx_state = NET_MSG_RX_STATE_IDLE;
    }
    old_tick = tick;

    switch (net_obj->msg_rx_state) {
        case NET_MSG_RX_STATE_IDLE:
            temp_msg_id = 0;
            net_obj->msg_rx_state = NET_MSG_RX_STATE_WAIT_MSG_ID_1;
            temp_index = 0;
            *rx_data_index = 0;
        case NET_MSG_RX_STATE_WAIT_MSG_ID_1:
        case NET_MSG_RX_STATE_WAIT_MSG_ID_2:
            temp_msg_id = (temp_msg_id >> 8) + ((uint16_t)byte << 8);
            if (net_agreement_msg_id_legal(temp_msg_id) == false) {
                net_obj->msg_rx_state = NET_MSG_RX_STATE_WAIT_MSG_ID_2;
            } else {
                net_obj->msg_rx_state = NET_MSG_RX_STATE_WAIT_MSG_HEADER;
                *(uint16_t *)&rx_data[0] = temp_msg_id;
                *rx_data_index = 2;
                temp_index = sizeof(NET_MSG_HEADER_t) - 2;
            }
            break;
        case NET_MSG_RX_STATE_WAIT_MSG_HEADER:
            rx_data[*rx_data_index] = byte;
            *rx_data_index += 1;
            temp_index--;
            if (temp_index > 0) {
                break;
            }
            if (net_msg_id_is_registered(net_obj, rx_data) == false) {
                net_obj->msg_rx_state = NET_MSG_RX_STATE_IDLE;
                break;
            }
            if (net_msg_crc_check_ok(rx_data) == false) {
                net_obj->msg_rx_state = NET_MSG_RX_STATE_IDLE;
            } else {
                if (net_msg_type_is_resp(rx_data) == true) {
                    temp_index = sizeof(NET_RESPONSE_HEADER_t) - sizeof(NET_MSG_HEADER_t);
                } else {
                    temp_index = sizeof(NET_REQUIRE_HEADER_t) - sizeof(NET_MSG_HEADER_t);
                }
                net_obj->msg_rx_state = NET_MSG_RX_STATE_WAIT_TOTAL_HEADER;
            }
            break;
        case NET_MSG_RX_STATE_WAIT_TOTAL_HEADER:
            rx_data[*rx_data_index] = byte;
            *rx_data_index += 1;
            temp_index--;
            if (temp_index > 0) {
                break;
            }
            net_obj->msg_rx_state = NET_MSG_RX_STATE_WAIT_BODY_END;
            temp_index = net_msg_head_get_body_len(rx_data);
            if (temp_index > NET_RX_MSG_BODY_LEN_MAX) {
                net_obj->msg_rx_state = NET_MSG_RX_STATE_IDLE;
            }
            break;
        case NET_MSG_RX_STATE_WAIT_BODY_END:
            rx_data[*rx_data_index] = byte;
            *rx_data_index += 1;
            temp_index--;
            if (temp_index > 0) {
                break;
            }
            ret = net_msg_body_aes_decoder(rx_data);
            if (ret < 0) {
                net_obj->msg_rx_state = NET_MSG_RX_STATE_IDLE;
                break;
            }
            net_obj->msg_rx_state = NET_MSG_RX_STATE_PROGRESS;
        case NET_MSG_RX_STATE_PROGRESS:
            net_msg_body_process(net_obj, rx_data);
            net_obj->msg_rx_state = NET_MSG_RX_STATE_IDLE;
            break;
        default:
            break;
    }

    return 0;
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
