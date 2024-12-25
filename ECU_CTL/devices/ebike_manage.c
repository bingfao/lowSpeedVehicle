/*
 * @Author: your name
 * @Date: 2024-11-07 15:16:23
 * @LastEditTime: 2024-12-25 16:25:24
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\ebike_manage.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "EBICK_MNGR"
#define LOG_LVL ELOG_LVL_DEBUG
#include "ebike_manage.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <error_code.h>
#include <task.h>

#include "elog.h"
#include "error_code.h"
#include "net_agreement.h"
#include "net_port.h"
#include "version.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef struct
{
    uint16_t msg_id;
    uint8_t msg_type;
    MSG_ID_FUNC callback;
} MSG_ID_MAP_t;

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */
#define EBIKE_ACK_QUEUE_OK    0x01
#define EBIKE_ACK_QUEUE_ERROR 0x02

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
EBIKE_MANAGE_OBJ_t g_ebike_manage_obj;
osThreadId g_ebike_rx_handle;
osMessageQId g_ebike_rx_queue_handle;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t ebike_manage_send_data(uint8_t *data, int32_t len);
static uint32_t ebike_manage_get_ticks(void);
static int32_t ebike_manage_register_msg_id(void *obj);
static int32_t ebike_manage_read_start(void);
static int32_t ebike_rx_prepare(void);
static void ebike_rx_task(void const *argument);
static int32_t ebike_msg_send_queue(uint32_t message);
static int32_t ebike_msg_wait_queue(uint32_t *in_message, uint32_t timeout);

// device register callback function
static int32_t ebike_device_register_ack(uint8_t *in_data, int32_t in_len, uint8_t *out_data, int32_t *out_len);
static int32_t ebike_device_state_upload_ack(uint8_t *in_data, int32_t in_len, uint8_t *out_data, int32_t *out_len);

MSG_ID_MAP_t g_msg_id_fun_map[] = {
    {NET_TX_MSG_ID_REGISTER_DEV, NET_MSG_TYPE_RESP, ebike_device_register_ack},
    {NET_TX_MSG_ID_DEV_STATE, NET_MSG_TYPE_RESP, ebike_device_state_upload_ack},
};

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t ebike_manage_init(void)
{
    memset(&g_ebike_manage_obj, 0, sizeof(EBIKE_MANAGE_OBJ_t));
    g_ebike_manage_obj.dev_id = EBIKE_DEV_ID;
    g_ebike_manage_obj.dev_type = EBIKE_DEV_TYPE;
    g_ebike_manage_obj.power_battery_series_counts = EBIKE_POWER_BATTERY_SERIES_COUNT;
    g_ebike_manage_obj.power_battery_parallel_counts = EBIKE_POWER_BATTERY_PARALLEL_COUNT;
    g_ebike_manage_obj.net_agreement_obj =
        net_agreement_create(g_ebike_manage_obj.dev_id, ebike_manage_send_data, ebike_manage_get_ticks);
    if (g_ebike_manage_obj.net_agreement_obj != NULL) {
        ebike_manage_register_msg_id(g_ebike_manage_obj.net_agreement_obj);
        ebike_manage_read_start();
    }

    return 0;
}

bool ebike_is_connected_server(void)
{
    if (g_ebike_manage_obj.connect_flg != 0) {
        return true;
    }

    return false;
}

EBIKE_TYPE_t ebike_get_device_type(void)
{
    return g_ebike_manage_obj.dev_type;
}

int32_t ebike_get_location(double *lng, double *lat)
{
    *lng = 121.47;
    *lat = 31.23;

    return 0;
}

float ebike_get_total_mileage(void)
{
    return 1001.0;  // unit is M
}

bool ebike_is_running(void)
{
    return true;
}

float ebike_get_speed(void)
{
    return 25.0;  // unit is km/h, 10000(mm/s) means 36(km/h)
}

bool ebike_electric_lock_is_lock(void)
{
    return false;
}

bool ebike_seat_lock_is_lock(void)
{
    return true;
}

bool ebike_trunk_lock_is_lock(void)
{
    return true;
}

bool ebike_helmet_lock_is_lock(void)
{
    return true;
}

bool ebike_electric_drive_lock_is_lock(void)
{
    return false;
}

// 0b00:OFF, 0b01:OPEN, 0b11:ERR
uint16_t ebike_get_state_short_range_headlight(void)
{
    return 0x01;
}

// 0b00:OFF, 0b01:OPEN, 0b11:ERR
uint16_t ebike_get_state_long_range_headlight(void)
{
    return 0x03;
}

// 0b00:OFF, 0b01:OPEN, 0b11:ERR
uint16_t ebike_get_state_clearance_lamp(void)
{
    return 0x01;
}

// 0b00:OFF, 0b01:turn left, 0b10:turn right, 0b11:ERR
uint16_t ebike_get_state_turn_signal(void)
{
    return 0x00;
}

// 0b00:OFF, 0b01:OPEN, 0b11:ERR
uint16_t ebike_get_state_emergency_flashers(void)
{
    return 0x01;
}

// 0b00:OFF, 0b01:OPEN, 0b11:ERR
uint16_t ebike_get_state_brake_light(void)
{
    return 0x00;
}

bool ebike_sensor_seat_is_tirgger(void)
{
    return true;
}

bool ebike_sensor_kick_stand_is_tirgger(void)
{
    return false;
}

bool ebike_sensor_child_seat_is_tirgger(void)
{
    return false;
}

bool ebike_sensor_rollover_is_tirgger(void)
{
    return false;
}

bool ebike_rear_brake_is_tirgger(void)
{
    return false;
}

bool ebike_front_brake_is_tirgger(void)
{
    return false;
}

bool ebike_abs_is_tirgger(void)
{
    return false;
}

bool ebike_tcs_tirgger(void)
{
    return false;
}

bool ebike_mini_battery_is_exit(void)
{
    return true;
}

int32_t ebike_get_mini_battery_id(uint8_t *id, uint32_t *len)
{
    if (*len < EBIKE_SMALL_BATTERY_ID_SIZE) {
        return -EINVAL;
    }
    memcpy(id, EBIKE_SMALL_BATTERY_ID, EBIKE_SMALL_BATTERY_ID_SIZE);
    *len = EBIKE_SMALL_BATTERY_ID_SIZE;

    return 0;
}

int32_t ebike_get_mini_battery_soc(uint8_t *soc)
{
    *soc = 100;
    return 0;
}

int32_t ebike_get_mini_battery_voltage(float *voltage)
{
    *voltage = 12.0;

    return 0;
}

int32_t ebike_get_mini_battery_temp(float *temper)
{
    *temper = 25.0;

    return 0;
}

bool ebike_power_battery_is_exit(void)
{
    return true;
}

bool ebike_power_battery_is_charging(void)
{
    return true;
}

int32_t ebike_get_power_battery_id(uint8_t *id, uint32_t *len)
{
    if (*len < EBIKE_POWER_BATTERY_ID_SIZE) {
        return -EINVAL;
    }
    memcpy(id, EBIKE_POWER_BATTERY_ID, EBIKE_POWER_BATTERY_ID_SIZE);
    *len = EBIKE_POWER_BATTERY_ID_SIZE;

    return 0;
}

int32_t ebike_get_power_battery_soc(uint8_t *soc)
{
    *soc = 100;
    return 0;
}

int32_t ebike_get_power_battery_voltage(float *voltage)
{
    *voltage = 48.123;
    return 0;
}

int32_t ebike_get_power_battery_current(float *current)
{
    *current = 3.21;
    return 0;
}

int32_t ebike_get_power_battery_temp(float *temper)
{
    *temper = 25.0;
    return 0;
}

int32_t ebike_get_power_battery_charge_cycle(uint16_t *cycle)
{
    *cycle = 123;
    return 0;
}

int32_t ebike_get_power_battery_soc_health(uint8_t *soc_health)
{
    *soc_health = 98;
    return 0;
}

int32_t ebike_get_power_battery_series_count(uint8_t *count)
{
    *count = g_ebike_manage_obj.power_battery_series_counts;

    return 0;
}

int32_t ebike_get_power_battery_parallel_count(uint8_t *count)
{
    *count = g_ebike_manage_obj.power_battery_parallel_counts;

    return 0;
}

int32_t ebike_get_power_battery_series_voltages(float *voltages, uint8_t series_count, uint8_t parallel_count)
{
    if (series_count >= g_ebike_manage_obj.power_battery_series_counts) {
        return -EINVAL;
    }
    *voltages = 3.2012;

    return 0;
}

int32_t ebike_get_power_battery_series_temp(float *temp, uint8_t series_count, uint8_t parallel__count)
{
    if (series_count >= g_ebike_manage_obj.power_battery_series_counts) {
        return -EINVAL;
    }
    *temp = 25.1;

    return 0;
}

int32_t ebike_device_register_to_server(void)
{
    uint8_t data[100] = {0};
    uint32_t len;
    int32_t ret = 0;
    uint8_t times = 3;
    uint32_t message = 0;

    if (g_ebike_manage_obj.connect_flg == 0) {
        return -ENOTCONN;
    }
    len = sizeof(data);
    ret = net_agreement_device_register_package(g_ebike_manage_obj.net_agreement_obj, data, &len);
    if (ret != 0) {
        log_e("net_agreement_device_register_package failed\r\n");
        return ret;
    }
    do {
        ret = net_agreement_send_msg(g_ebike_manage_obj.net_agreement_obj, NET_TX_MSG_ID_REGISTER_DEV,
                                     NET_MSG_TYPE_SEND, data, len);
        if (ret < 0) {
            log_e("net_agreement_send_msg failed\r\n");
            osDelay(1000);
            continue;
        }
        ret = ebike_msg_wait_queue(&message, 5000);
        if (ret == 0) {
            if (message == EBIKE_ACK_QUEUE_OK) {
                log_d("ebike_device_register success\r\n");
                return 0;
            } else if (message == EBIKE_ACK_QUEUE_ERROR) {
                log_e("ebike_device_register failed\r\n");
                return -EIO;
            }
        }
    } while (times-- > 0 && ret < 0);
    log_e("ebike_device_register timeout\r\n");

    return -ETIMEDOUT;
}

int32_t ebike_device_state_upload_to_server(void)
{
    uint8_t data[256] = {0};
    uint32_t len;
    int32_t ret = 0;
    uint8_t times = 3;
    uint32_t message = 0;

    if (g_ebike_manage_obj.connect_flg == 0) {
        return -ENOTCONN;
    }
    len = sizeof(data);
    ret = net_agreement_device_state_upload_package(g_ebike_manage_obj.net_agreement_obj, data, &len);
    if (ret != 0) {
        log_e("net_agreement_device_state_upload_package failed\r\n");
        return ret;
    }
    do {
        ret = net_agreement_send_msg(g_ebike_manage_obj.net_agreement_obj, NET_TX_MSG_ID_DEV_STATE,
                                     NET_MSG_TYPE_SEND, data, len);
        if (ret < 0) {
            log_e("net_agreement_send_msg failed\r\n");
            osDelay(1000);
            continue;
        }
        ret = ebike_msg_wait_queue(&message, 5000);
        if (ret == 0) {
            if (message == EBIKE_ACK_QUEUE_OK) {
                log_d("ebike_device_register success\r\n");
                return 0;
            } else if (message == EBIKE_ACK_QUEUE_ERROR) {
                log_e("ebike_device_register failed\r\n");
                return -EIO;
            }
        }
    } while (times-- > 0 && ret < 0);
    log_e("ebike_device_register timeout\r\n");

    return -ETIMEDOUT;
}
/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */
static int32_t ebike_manage_send_data(uint8_t *data, int32_t len)
{
    int32_t ret = 0;

    ret = net_port_send(data, len);
    if (ret < 0) {
        return -ret;
    }

    return len;
}

static uint32_t ebike_manage_get_ticks(void)
{
    return (uint32_t)osKernelSysTick();
}

static int32_t ebike_manage_register_msg_id(void *obj)
{
    int32_t ret = 0;
    int32_t register_num = 0;
    int32_t i = 0;

    for (i = 0; i < sizeof(g_msg_id_fun_map) / sizeof(MSG_ID_MAP_t); i++) {
        ret = net_agreement_msg_regsiter(obj, g_msg_id_fun_map[i].msg_id, g_msg_id_fun_map[i].msg_type,
                                         g_msg_id_fun_map[i].callback);
        if (ret < 0) {
            log_w("register msg id 0x%x failed", g_msg_id_fun_map[i].msg_id);
            continue;
        }
        register_num++;
    }
    log_d("register msg id num %d", register_num);

    return register_num;
}

static int32_t ebike_device_register_ack(uint8_t *in_data, int32_t in_len, uint8_t *out_data, int32_t *out_len)
{
    *out_len = 0;
    int32_t i = 0;
    int32_t ret = 0;

    printf("ebike_device_register_ack :\r\n");
    for (i = 0; i < in_len; i++) {
        printf("0x%x ", in_data[i]);
    }
    printf("\r\n");
    ret = net_agreement_device_register_ack(g_ebike_manage_obj.net_agreement_obj, in_data, in_len,
                                            &g_ebike_manage_obj.session_id, g_ebike_manage_obj.aes_iv);
    if (ret != 0) {
        ebike_msg_send_queue(EBIKE_ACK_QUEUE_ERROR);
        return -1;
    }
    ebike_msg_send_queue(EBIKE_ACK_QUEUE_OK);

    return 0;
}

static int32_t ebike_device_state_upload_ack(uint8_t *in_data, int32_t in_len, uint8_t *out_data, int32_t *out_len)
{
    *out_len = 0;
    int32_t i = 0;
    int32_t ret = 0;

    printf("ebike_device_state_upload_ack :\r\n");
    for (i = 0; i < in_len; i++) {
        printf("0x%x ", in_data[i]);
    }
    printf("\r\n");
    ret = net_agreement_device_state_upload_ack(g_ebike_manage_obj.net_agreement_obj, in_data, in_len);
    if (ret != 0) {
        ebike_msg_send_queue(EBIKE_ACK_QUEUE_ERROR);
        return -1;
    }
    ebike_msg_send_queue(EBIKE_ACK_QUEUE_OK);
    return 0;
}

static int32_t ebike_manage_read_start(void)
{
    osThreadDef(ebike_rx, ebike_rx_task, osPriorityNormal, 0, 1024);
    g_ebike_rx_handle = osThreadCreate(osThread(ebike_rx), NULL);

    osMessageQDef(ebike_rx_queue, 10, uint8_t);
    g_ebike_rx_queue_handle = osMessageCreate(osMessageQ(ebike_rx_queue), NULL);

    return 0;
}

static int32_t ebike_rx_prepare(void)
{
    int32_t ret = 0;

    ret = net_port_init();
    if (ret != 0) {
        log_e("net_port_init failed\r\n");
        g_ebike_manage_obj.connect_flg = 0;
        return ret;
    }
    ret = net_port_tcp_connect(NET_PORT_TCP_TEST_HOST, NET_PORT_TCP_TEST_PORT);
    if (ret != 0) {
        log_e("net_port_tcp_connect failed\r\n");
        g_ebike_manage_obj.connect_flg = 0;
        return ret;
    }
    g_ebike_manage_obj.connect_flg = 1;

    return 0;
}

static void ebike_rx_task(void const *argument)
{
    int32_t ret = 0;
    uint8_t data = 0;

    ebike_rx_prepare();
    log_d("ebike_rx task running...\r\n");
    // if (g_ebike_manage_obj.connect_flg == 1) {
    //     log_d("ebike_rx task running...\r\n");
    //     net_port_send("Hello, world!\r\n", strlen("Hello, world!\r\n"));
    // }
    while (1) {
        if (g_ebike_manage_obj.connect_flg == 1) {
            do {
                ret = net_port_recv(&data, 1);
                if (ret > 0) {
                    net_agreement_data_in(g_ebike_manage_obj.net_agreement_obj, &data, 1);
                }
            } while (ret > 0);
        }
        osDelay(100);
    }
}

static int32_t ebike_msg_send_queue(uint32_t message)
{
    osEvent xReturn;
    uint32_t send_data = message;

    xReturn.status = osMessagePut(g_ebike_rx_queue_handle, send_data, 100);
    if (osOK != xReturn.status) {
        log_d("ebike_msg_send_queue osMessage send:%d fail\n", message);
        return -1;
    }

    return 0;
}

static int32_t ebike_msg_wait_queue(uint32_t *in_message, uint32_t timeout)
{
    osEvent xEvent;

    xEvent = osMessageGet(g_ebike_rx_queue_handle, timeout);
    if (osEventMessage == xEvent.status) {
        *in_message = xEvent.value.v;
        return 0;
    } else {
        return -1;
    }
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
