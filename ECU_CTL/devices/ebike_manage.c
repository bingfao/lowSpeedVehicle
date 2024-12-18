/*
 * @Author: your name
 * @Date: 2024-11-07 15:16:23
 * @LastEditTime: 2024-12-18 10:14:34
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
} EBIKE_REGISTER_OBJ_t;
#pragma pack(pop)

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

MSG_ID_MAP_t g_msg_id_fun_map[] = {
    {NET_TX_MSG_ID_REGISTER_DEV, NET_MSG_TYPE_RESP, ebike_device_register_ack},
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

int32_t ebike_device_register_to_server(void)
{
    EBIKE_REGISTER_OBJ_t data = {0};
    int32_t ret = 0;
    uint8_t times = 3;
    uint32_t message = 0;

    if (g_ebike_manage_obj.connect_flg == 0) {
        return -ENOTCONN;
    }
    memset(&data, 0, sizeof(EBIKE_REGISTER_OBJ_t));
    data.time.year = 2024;
    data.time.month = 11;
    data.time.day = 7;
    data.time.hour = 15;
    data.time.minute = 16;
    data.time.second = 23;
    data.dev_sw_version[0] = VERSION_MAJOR;
    data.dev_sw_version[1] = VERSION_MINOR;
    data.dev_sw_version[2] = VERSION_SUB;
    data.dev_sw_version[3] = VERSION_BUILD;

    do {
        ret = net_agreement_send_msg(g_ebike_manage_obj.net_agreement_obj, NET_TX_MSG_ID_REGISTER_DEV,
                                     NET_MSG_TYPE_SEND, (uint8_t *)&data, sizeof(EBIKE_REGISTER_OBJ_t));
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

    printf("ebike_device_register_ack :\r\n");
    for (i = 0; i < in_len; i++) {
        printf("0x%x ", in_data[i]);
    }
    printf("\r\n");
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
