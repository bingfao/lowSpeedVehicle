/*
 * @Author: your name
 * @Date: 2024-11-07 15:47:34
 * @LastEditTime: 2025-01-09 16:48:12
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\middlewares\net_agreement\net_agreement.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "net_agreement"
#define LOG_LVL ELOG_LVL_DEBUG
#include "net_agreement.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ebike_manage.h"
#include "elog.h"
#include "error_code.h"
#include "fault.h"
#include "user_crc.h"
#include "version.h"
#include "aes.h"

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
    uint8_t aes_key[16];
    uint8_t aes_iv[16];
} NET_AGREEMENT_OBJ_t;

#pragma pack(push, 1)
typedef struct
{
    uint16_t year;
    uint8_t month;
    uint8_t day;
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} EBIKE_TIME_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    EBIKE_TIME_t time;
    uint8_t dev_hw_version[4];
    uint8_t dev_sw_version[4];
    uint8_t motor_hw_version[4];
    uint8_t motor_sw_version[4];
    uint8_t dash_board_hw_version[4];
    uint8_t dash_board_sw_version[4];
} NET_EBIKE_REGISTER_OBJ_t;
#pragma pack(pop)

typedef struct
{
    uint8_t electric_lock       : 1;
    uint8_t seat_lock           : 1;
    uint8_t trunk_lock          : 1;
    uint8_t helmet_lock         : 1;
    uint8_t electric_drive_lock : 1;
    uint8_t resvrved            : 3;
} LOCK_STATUS_t;

typedef struct
{
    uint16_t short_range_headlight : 2;  // 00:OFF, 01:OPEN, 11:ERR
    uint16_t long_range_headlight  : 2;  // 00:OFF, 01:OPEN, 11:ERR
    uint16_t clearance_lamp        : 2;  // 00:OFF, 01:ON, 11:ERR
    uint16_t turn_signal           : 2;  // 00:OFF, 01:turn left, 10:turn right, 11:ERR
    uint16_t emergency_flashers    : 2;  // 00:OFF, 01:ON, 11:ERR
    uint16_t brake_light           : 2;  // 00:OFF, 01:ON, 11:ERR
    uint16_t reserved              : 4;
} LIGHT_STATE_t;

typedef struct
{
    uint8_t seat       : 1;
    uint8_t kick_stand : 1;
    uint8_t child_seat : 1;
    uint8_t rollover   : 1;
    uint8_t reserved   : 4;
} SENSOR_STATE_t;

typedef struct
{
    uint8_t rear_brake     : 1;
    uint8_t front_brake    : 1;
    uint8_t reserved_b2_b3 : 2;
    uint8_t abs            : 1;
    uint8_t tcs            : 1;
    uint8_t reserved_b6_b7 : 2;
} BRAKE_STATE_t;

#pragma pack(push, 1)
typedef struct
{
    LOCK_STATUS_t lock_status;
    LIGHT_STATE_t light_state;
    SENSOR_STATE_t sensor_state;
    BRAKE_STATE_t brake_state;
    uint8_t reserved;
} EBIKE_WORK_STATE_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint8_t exit_flg;
    uint8_t id[EBIKE_SMALL_BATTERY_ID_SIZE];
    uint8_t soc;
    uint16_t vol;  //  voltage vlaue (V) * 100
    int16_t temp;  //  temperature value (℃) * 100
} EBIKE_MINI_BATTERT_STATE_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint16_t vol;  //  voltage vlaue (V) * 100
    int16_t temp;  //  temperature value (℃) * 100 , 0xFFFF means no temperature sensor
} POWER_BATTERY_SERIES_STATE_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint8_t exit_flg;
    uint8_t charge_flg;
    uint8_t id[EBIKE_POWER_BATTERY_ID_SIZE];
    uint8_t soc;
    uint16_t vol;  //  voltage vlaue * 100
    int16_t temp;  //  temperature value * 100
    uint16_t charge_cycle;
    uint8_t soc_health;
    uint8_t curr_direction;  // 2:charge, 1:discharge
    uint16_t curr;           // current value (A) * 100
} EBIKE_POWER_BATTERT_STATE_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint8_t series_counts;
    POWER_BATTERY_SERIES_STATE_t series_state[EBIKE_POWER_BATTERY_SERIES_COUNT_MAX];
} EBIKE_POWER_BATTERT_SERIES_STATE_t;
#pragma pack(pop)

#pragma pack(push, 1)
typedef struct
{
    uint8_t protocol_flag;
    double lng_pos;
    double lat_pos;
    uint32_t total_mileage;
    bool is_running;
    uint16_t speed;
    EBIKE_WORK_STATE_t work_state;
    EBIKE_MINI_BATTERT_STATE_t mini_battery;
    EBIKE_POWER_BATTERT_STATE_t power_battery;
    EBIKE_POWER_BATTERT_SERIES_STATE_t power_battery_series;
} EBIKE_STATE_UPLOAD_t;
#pragma pack(pop)

typedef union
{
    EBIKE_STATE_UPLOAD_t ebike;
} UPLOAD_STATE_UNION_t;

#pragma pack(push, 1)
typedef struct
{
    uint8_t dev_type;
    UPLOAD_STATE_UNION_t upload_state;
} NET_EBIKE_STATE_UPLOAD_OBJ_t;
#pragma pack(pop)

typedef struct
{
    uint32_t section_id;
    uint8_t aes_iv[16];
} REGISTER_ACK_DATA_t;
typedef union
{
    REGISTER_ACK_DATA_t register_ack_data;
} ACK_DATA_t;

typedef struct
{
    uint32_t respcode;
    ACK_DATA_t ack_data;
} NET_MSG_ACK;

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

int32_t net_agreement_set_aes_key(void *obj, uint8_t *key, uint32_t len)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    if (len > sizeof(net_obj->aes_key)) {
        return -EINVAL;
    }
    memcpy(net_obj->aes_key, key, len);
    return 0;
}

int32_t net_agreement_get_aes_key(void *obj, uint8_t *key, uint32_t *len)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    if (*len < sizeof(net_obj->aes_key)) {
        return -EINVAL;
    }
    *len = sizeof(net_obj->aes_key);
    memcpy(key, net_obj->aes_key, *len);

    return 0;
}

int32_t net_agreement_set_aes_iv(void *obj, uint8_t *iv, uint32_t len)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    if (len > sizeof(net_obj->aes_iv)) {
        return -EINVAL;
    }
    memcpy(net_obj->aes_iv, iv, len);
    return 0;
}

int32_t net_agreement_get_aes_iv(void *obj, uint8_t *iv, uint32_t *len)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    if (*len < sizeof(net_obj->aes_iv)) {
        return -EINVAL;
    }
    *len = sizeof(net_obj->aes_iv);
    memcpy(iv, net_obj->aes_iv, *len);

    return 0;
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
            out_len = sizeof(tx_msg->msg_body);
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

int32_t net_agreement_device_register_package(void *obj, uint8_t *data, uint32_t *len)
{
    NET_EBIKE_REGISTER_OBJ_t *p_data = (NET_EBIKE_REGISTER_OBJ_t *)data;
    if (*len < sizeof(NET_EBIKE_REGISTER_OBJ_t)) {
        return -EINVAL;
    }
    memset(p_data, 0, sizeof(NET_EBIKE_REGISTER_OBJ_t));
    p_data->time.year = 2024;
    p_data->time.month = 11;
    p_data->time.day = 7;
    p_data->time.hour = 15;
    p_data->time.minute = 16;
    p_data->time.second = 23;
    p_data->dev_sw_version[0] = VERSION_MAJOR;
    p_data->dev_sw_version[1] = VERSION_MINOR;
    p_data->dev_sw_version[2] = VERSION_SUB;
    p_data->dev_sw_version[3] = VERSION_BUILD;
    *len = sizeof(NET_EBIKE_REGISTER_OBJ_t);

    return 0;
}

int32_t net_agreement_device_register_ack(void *obj, uint8_t *data, uint32_t len, uint32_t *session_id, uint8_t *aes_iv)
{
    NET_AGREEMENT_OBJ_t *net_obj = (NET_AGREEMENT_OBJ_t *)obj;
    NET_MSG_ACK *ack_msg = (NET_MSG_ACK *)data;

    if (len != NET_MAS_LEN_REGISTER_ACK) {
        return -EINVAL;
    }
    if (ack_msg->respcode != NET_MSG_RESPCODE_OK) {
        log_e("register device fail, respcode:%d", ack_msg->respcode);
        return -EPERM;
    }
    *session_id = ack_msg->ack_data.register_ack_data.section_id;
    memcpy(aes_iv, ack_msg->ack_data.register_ack_data.aes_iv, sizeof(ack_msg->ack_data.register_ack_data.aes_iv));
    memcpy(net_obj->aes_iv, ack_msg->ack_data.register_ack_data.aes_iv, sizeof(net_obj->aes_iv));
    net_obj->session_id = *session_id;
    log_i("register device success, session_id:0x%08x", *session_id);

    return 0;
}

int32_t net_agreement_device_state_upload_package(void *obj, uint8_t *data, uint32_t *len)
{
    NET_EBIKE_STATE_UPLOAD_OBJ_t *p_data = (NET_EBIKE_STATE_UPLOAD_OBJ_t *)data;
    uint32_t need_size = sizeof(NET_EBIKE_STATE_UPLOAD_OBJ_t) - sizeof(EBIKE_POWER_BATTERT_SERIES_STATE_t);
    uint8_t bat_series = 0;
    uint8_t bat_parallel = 0;
    EBIKE_STATE_UPLOAD_t *ebike_state = NULL;
    EBIKE_POWER_BATTERT_SERIES_STATE_t *series_state = NULL;
    int32_t ret = 0;
    int32_t i = 0;
    uint32_t id_len = 0;
    uint32_t data_len = 0;
    float temp = 0.0f;
    double temp_double1 = 0.0;
    double temp_double2 = 0.0;
    uint16_t temp_u16 = 0;

    ebike_get_power_battery_series_count(&bat_series);
    ebike_get_power_battery_parallel_count(&bat_parallel);
    need_size += sizeof(POWER_BATTERY_SERIES_STATE_t) * (bat_series * bat_parallel) + 1;
    if (*len < need_size) {
        return -EINVAL;  // need more data
    }
    ebike_state = &p_data->upload_state.ebike;
    memset(p_data, 0, need_size);
    p_data->dev_type = (uint8_t)ebike_get_device_type();
    ebike_state->protocol_flag = 1;
    ret += ebike_get_location(&temp_double1, &temp_double2);
    ebike_state->lng_pos = temp_double1;
    ebike_state->lat_pos = temp_double2;
    temp = ebike_get_total_mileage();                              // get total mileage unit is x.x m
    ebike_state->total_mileage = (uint32_t)((temp + 5.0f) / 10.0f);  // mileage unit is 10m
    ebike_state->is_running = ebike_is_running();
    temp = ebike_get_speed();                        // get speed unit is x.x km/h
    ebike_state->speed = (uint16_t)(temp / 0.0036f);  // update speed unit is  mm/s
    // ebike state data packet
    ebike_state->work_state.lock_status.electric_lock = ebike_electric_lock_is_lock() ? 0 : 1;
    ebike_state->work_state.lock_status.seat_lock = ebike_seat_lock_is_lock() ? 0 : 1;
    ebike_state->work_state.lock_status.trunk_lock = ebike_trunk_lock_is_lock() ? 0 : 1;
    ebike_state->work_state.lock_status.helmet_lock = ebike_helmet_lock_is_lock() ? 0 : 1;
    ebike_state->work_state.lock_status.electric_drive_lock = ebike_electric_drive_lock_is_lock() ? 0 : 1;
    ebike_state->work_state.light_state.short_range_headlight = ebike_get_state_short_range_headlight();
    ebike_state->work_state.light_state.long_range_headlight = ebike_get_state_long_range_headlight();
    ebike_state->work_state.light_state.clearance_lamp = ebike_get_state_clearance_lamp();
    ebike_state->work_state.light_state.turn_signal = ebike_get_state_turn_signal();
    ebike_state->work_state.light_state.emergency_flashers = ebike_get_state_emergency_flashers();
    ebike_state->work_state.light_state.brake_light = ebike_get_state_brake_light();
    ebike_state->work_state.sensor_state.seat = ebike_sensor_seat_is_tirgger() ? 1 : 0;
    ebike_state->work_state.sensor_state.kick_stand = ebike_sensor_kick_stand_is_tirgger() ? 1 : 0;
    ebike_state->work_state.sensor_state.child_seat = ebike_sensor_child_seat_is_tirgger() ? 1 : 0;
    ebike_state->work_state.sensor_state.rollover = ebike_sensor_rollover_is_tirgger() ? 1 : 0;
    ebike_state->work_state.brake_state.rear_brake = ebike_rear_brake_is_tirgger() ? 1 : 0;
    ebike_state->work_state.brake_state.front_brake = ebike_front_brake_is_tirgger() ? 1 : 0;
    ebike_state->work_state.brake_state.abs = ebike_abs_is_tirgger() ? 1 : 0;
    ebike_state->work_state.brake_state.tcs = ebike_tcs_tirgger() ? 1 : 0;
    // get mini battery state
    ebike_state->mini_battery.exit_flg = ebike_mini_battery_is_exit() ? 1 : 0;
    id_len = sizeof(ebike_state->mini_battery.id);
    ret += ebike_get_mini_battery_id(ebike_state->mini_battery.id, &id_len);
    ret += ebike_get_mini_battery_soc(&ebike_state->mini_battery.soc);
    ret += ebike_get_mini_battery_voltage(&temp);              // get voltage unit is x.x V
    ebike_state->mini_battery.vol = (uint16_t)(temp * 100.0f);  // update voltage unit is 10mV
    ret += ebike_get_mini_battery_temp(&temp);                 // get temperature unit is x.x ℃
    ebike_state->mini_battery.temp = (int16_t)(temp * 100.0f);  // update temperature unit is 0.01C
    // get power battery state
    ebike_state->power_battery.exit_flg = ebike_power_battery_is_exit() ? 1 : 0;
    ebike_state->power_battery.charge_flg = ebike_power_battery_is_charging() ? 1 : 0;
    id_len = sizeof(ebike_state->power_battery.id);
    ret += ebike_get_power_battery_id(ebike_state->power_battery.id, &id_len);
    ret += ebike_get_power_battery_soc(&ebike_state->power_battery.soc);
    ret += ebike_get_power_battery_voltage(&temp);              // get voltage unit is x.x V
    ebike_state->power_battery.vol = (uint16_t)(temp * 100.0f);  // update voltage unit is 10mV
    ret += ebike_get_power_battery_temp(&temp);                 // get temperature unit is x.x ℃
    ebike_state->power_battery.temp = (int16_t)(temp * 100.0f);  // update temperature unit is 0.01C
    ret += ebike_get_power_battery_charge_cycle(&temp_u16);
    ebike_state->power_battery.charge_cycle = temp_u16;
    ret += ebike_get_power_battery_soc_health(&ebike_state->power_battery.soc_health);
    ret += ebike_get_power_battery_current(&temp);  // get current unit is x.x A
    ebike_state->power_battery.curr_direction = temp < 0 ? 2 : 1;
    ebike_state->power_battery.curr = (uint16_t)(temp * 100.0f);  // update current unit is 10mA
    data_len = sizeof(NET_EBIKE_STATE_UPLOAD_OBJ_t) - sizeof(EBIKE_POWER_BATTERT_SERIES_STATE_t);
    // get power battery series state
    ret += ebike_get_power_battery_series_count(&ebike_state->power_battery_series.series_counts);
    data_len++;
    series_state = &ebike_state->power_battery_series;
    for (i = 0; i < ebike_state->power_battery_series.series_counts; i++) {
        ret += ebike_get_power_battery_series_voltages(&temp, i, 0);   // get voltage unit is x.x V
        series_state->series_state[i].vol = (uint16_t)(temp * 100.0f);  // update voltage unit is 10mV
        ret += ebike_get_power_battery_series_temp(&temp, i, 0);       // get temperature unit is x.x ℃
        series_state->series_state[i].temp = (int16_t)(temp * 100.0f);  // update temperature unit is 0.01C
        data_len += sizeof(POWER_BATTERY_SERIES_STATE_t);
    }
    if (data_len != need_size) {
        log_e("package error, data_len:%d, need_size:%d \r\n", data_len, need_size);
        return -EINVAL;
    }
    *len = need_size;

    return 0;
}

int32_t net_agreement_device_state_upload_ack(void *obj, uint8_t *data, uint32_t len)
{
    NET_MSG_ACK *ack_msg = (NET_MSG_ACK *)data;

    if (len != NET_MAS_LEN_UPLOAD_ACK) {
        return -EINVAL;
    }
    if (ack_msg->respcode != NET_MSG_RESPCODE_OK) {
        log_e("upload device state fail, respcode:%d", ack_msg->respcode);
        return -EPERM;
    }
    log_i("upload device state success");

    return 0;
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
    struct AES_ctx ctx;
    uint32_t len = in_len;
    int32_t ret = 0;

    AES_init_ctx_iv(&ctx, g_net_agreement_obj.aes_key, g_net_agreement_obj.aes_iv);
    ret = AES_CBC_encrypt_buffer_fill(&ctx, in_msg, &len, *out_len);
    if (ret < 0) {
        log_e("msg_aes_encoder overflow, ret:%d", ret);
        return ret;
    }
    *out_len = len;
    memcpy(out_msg, in_msg, len);

    return 0;
}

static int32_t msg_aes_decoder(uint8_t *in_msg, uint32_t in_len, uint8_t *out_msg, uint32_t *out_len)
{
    struct AES_ctx ctx;
    uint32_t len = 0;
    int32_t ret = 0;

    AES_init_ctx_iv(&ctx, g_net_agreement_obj.aes_key, g_net_agreement_obj.aes_iv);
    ret = AES_CBC_decrypt_buffer_fill(&ctx, in_msg, &len);
    if (ret < 0) {
        log_e("msg_aes_decoder fail, ret:%d", ret);
        return ret;
    }
    *out_len = len;
    memcpy(out_msg, in_msg, len);

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
    uint32_t aes_len = 0;

    if (msg_header->crypt_flg == NET_MSG_CRYPT_FLG_NO_CRYPT) {
        return 0;  // no need to decode
    }
    if (net_msg_type_is_resp(data) == true) {
        msg_body = data + sizeof(NET_RESPONSE_HEADER_t);
    } else {
        msg_body = data + sizeof(NET_REQUIRE_HEADER_t);
        aes_len = msg_header->msg_body_len - 6;
    }

    ret = msg_aes_decoder(msg_body, aes_len, msg_body, &out_len);
    if (ret < 0) {
        return ret;
    }
    memcpy(&msg_body[out_len], &msg_body[aes_len], 6);
    msg_header->msg_body_len = out_len + 6;

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
    static int32_t temp_index = 0;
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
            temp_index = (int32_t)net_msg_head_get_body_len(rx_data);
            if (temp_index > NET_RX_MSG_BODY_LEN_MAX) {
                net_obj->msg_rx_state = NET_MSG_RX_STATE_IDLE;
            }
            if (temp_index == 0) {
                net_obj->msg_rx_state = NET_MSG_RX_STATE_PROGRESS;
            } else {
                break;
            }
        case NET_MSG_RX_STATE_WAIT_BODY_END:
            if (temp_index > 0) {
                rx_data[*rx_data_index] = byte;
                *rx_data_index += 1;
                temp_index--;
            }
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
