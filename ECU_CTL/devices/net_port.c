/*
 * @Author: your name
 * @Date: 2024-10-24 14:58:21
 * @LastEditTime: 2024-12-09 09:38:58
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\net_port.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "NET_PORT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "net_port.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <error_code.h>
#include <string.h>
#include <task.h>

#include "elog.h"
#include "error_code.h"
/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */
#define NET_PORT_DRV_NAME "ec800m"

#define NET_RETRY_TO_CONNECT_TCP_CYCLE      (5*60000)        // 5min

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
DRIVER_OBJ_t *g_driver = NULL;

int32_t g_set_tcp_connect_mode = NET_PORT_TCP_CONNECT_MODE_STRAIGHT_OUT;
char g_tcp_host[64] = {0};
char g_tcp_port[16] = {0};
int8_t g_net_driver_init_flag = 0;
int8_t g_net_driver_need_connect_flag = 0;
osThreadId g_net_port_monitor_thread = NULL;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t net_port_keep_tcp_mode(void);
static void net_port_monitor_task(void const *argument);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t net_port_init(void)
{
    int32_t ret = 0;
    g_driver = get_driver(NET_PORT_DRV_NAME);
    if (g_driver == NULL) {
        log_d("driver %s not found \r\n", NET_PORT_DRV_NAME);
        return -ENODEV;
    }
    log_d("driver %s found \r\n", NET_PORT_DRV_NAME);
    ret = driver_init(g_driver);
    if (ret != 0) {
        log_e("driver init failed \r\n");
        return -EIO;
    }
    ret = driver_open(g_driver, 0);
    if (ret != 0) {
        log_e("driver open failed \r\n");
        return -EIO;
    }
    osThreadDef(net_port_monitor, net_port_monitor_task, osPriorityNormal, 0, 1024);
    g_net_port_monitor_thread = osThreadCreate(osThread(net_port_monitor), NULL);
    g_net_driver_init_flag = 1;

    return 0;
}

int32_t net_port_deinit(void)
{
    if (g_driver == NULL) {
        log_d("driver %s not found \r\n", NET_PORT_DRV_NAME);
        return -ENODEV;
    }
    driver_deinit(g_driver);
    return 0;
}

int32_t net_port_tcp_connect(const char *host, const char *port)
{
    int32_t ret = 0;
    bool is_registered = false;
    int32_t time_out = 10000;
    char temp_host[64] = {0};
    char temp_port[16] = {0};

    if (g_driver == NULL) {
        log_d("driver %s not found \r\n", NET_PORT_DRV_NAME);
        return -ENODEV;
    }
    strncpy(temp_host, host, sizeof(temp_host) - 1);
    strncpy(temp_port, port, sizeof(temp_port) - 1);
    memset(g_tcp_host, 0, sizeof(g_tcp_host));
    strncpy(g_tcp_host, temp_host, sizeof(g_tcp_host) - 1);
    memset(g_tcp_port, 0, sizeof(g_tcp_port));
    strncpy(g_tcp_port, temp_port, sizeof(g_tcp_port) - 1);
    while (is_registered == false && time_out > 0) {
        driver_control(g_driver, NET_PORT_CMD_GET_CS_REGISTERED, &is_registered);
        osDelay(100);
        time_out -= 100;
    }
    if (is_registered == false) {
        log_d("cs not registered \r\n");
        return -EAGAIN;
    }
    ret += driver_control(g_driver, NET_PORT_CMD_TCP_SET_HOST, (void *)temp_host);
    ret += driver_control(g_driver, NET_PORT_CMD_TCP_SET_PORT, (void *)temp_port);
    ret += driver_control(g_driver, NET_PORT_CMD_TCP_CONNECT, &g_set_tcp_connect_mode);
    if (ret != 0) {
        log_e("tcp connect failed \r\n");
        return -EIO;
    }
    g_net_driver_need_connect_flag = 1;

    return 0;
}

int32_t net_port_tcp_disconnect(void)
{
    int32_t ret = 0;

    if (g_driver == NULL) {
        log_d("driver %s not found \r\n", NET_PORT_DRV_NAME);
        return -ENODEV;
    }
    ret += driver_control(g_driver, NET_PORT_CMD_TCP_DISCONNECT, &g_set_tcp_connect_mode);
    if (ret != 0) {
        log_e("tcp disconnect failed \r\n");
        return -EIO;
    }
    g_net_driver_need_connect_flag = 0;

    return 0;
}

bool net_port_is_connected(void)
{
    int32_t connect_mode = 0;

    if (g_driver == NULL) {
        log_d("driver %s not found \r\n", NET_PORT_DRV_NAME);
        return -ENODEV;
    }
    driver_control(g_driver, NET_PORT_CMD_TCP_GET_MODE, &connect_mode);
    if (connect_mode == NET_PORT_TCP_CONNECT_MODE_STRAIGHT_OUT ||
        connect_mode == NET_PORT_TCP_CONNECT_MODE_TRANSPARENT) {
        return true;
    }

    return false;
}

int32_t net_port_send(const uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;

    if (g_driver == NULL) {
        log_d("driver %s not found \r\n", NET_PORT_DRV_NAME);
        return -ENODEV;
    }
    if (net_port_is_connected() == false) {
        log_e("net port not connected \r\n");
        return -ENOTCONN;
    }
    ret = driver_write(g_driver, DEV_RXTX_POS_BLOCKING_1000, (void *)buf, len);
    if (ret < 0) {
        log_e("send failed \r\n");
        return -EIO;
    }

    return ret;
}

int32_t net_port_recv(uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;

    if (g_driver == NULL) {
        log_d("driver %s not found \r\n", NET_PORT_DRV_NAME);
        return -ENODEV;
    }
    if (net_port_is_connected() == false) {
        // log_e("net port not connected \r\n");
        return -ENOTCONN;
    }
    ret = driver_read(g_driver, DEV_RXTX_POS_BLOCKING_1000, buf, len);
    if (ret < 0) {
        log_e("recv failed \r\n");
        return -EIO;
    }

    return ret;
}

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

/**
 * @brief keep tcp mode, connect to server
 * @return int32_t 0:success, others:failed(retrun disconnected state after try reconnect 20 times)
 */
static int32_t net_port_keep_tcp_mode(void)
{
    static uint8_t times = 20;
    static bool old_is_connected = false;
    bool now_is_connected = net_port_is_connected();

    if (old_is_connected == true && now_is_connected == true) {
        times = 20;
    }

    if (g_set_tcp_connect_mode != NET_PORT_TCP_CONNECT_MODE_DISCONNECT) {
        if (now_is_connected == false) {
            times--;
            driver_control(g_driver, NET_PORT_CMD_TCP_DISCONNECT, &g_set_tcp_connect_mode);
            net_port_tcp_connect(g_tcp_host, g_tcp_port);
        }
    }
    old_is_connected = net_port_is_connected();
    if (times == 10) {
        log_d("net port keep tcp mode failed, restart the device \r\n");
        driver_control(g_driver, NET_PORT_CMD_RESET, NULL);
    }
    if (times == 0) {
        log_d("net port keep tcp mode failed, times out \r\n");
        times = 20;
        return -ENETUNREACH;
    }

    return 0;
}

static void net_port_monitor_task(void const *argument)
{
    int32_t times = NET_RETRY_TO_CONNECT_TCP_CYCLE;
    int32_t ret = 0;

    while (1) {
        if (g_net_driver_init_flag == 1) {
            if (g_net_driver_need_connect_flag == 1) {
                // keep tcp mode, connect to server
                ret = net_port_keep_tcp_mode();
                if (ret != 0) {
                    g_net_driver_need_connect_flag = 0;
                    times = NET_RETRY_TO_CONNECT_TCP_CYCLE;
                }
            } else {
                // if tcp disconnect, and after NET_RETRY_TO_CONNECT_TCP_CYCLE to retry connect
                if (times <= 0) {
                    driver_control(g_driver, NET_PORT_CMD_RESET, NULL);
                    g_net_driver_need_connect_flag = 1;
                    times = NET_RETRY_TO_CONNECT_TCP_CYCLE;
                }
                times -= 5000;
            }
        }

        osDelay(5000);
    }
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
