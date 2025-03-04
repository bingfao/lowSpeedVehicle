/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */

#define LOG_TAG "NET_UNIT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "net_unit.h"

#include <FreeRTOS.h>
#include <error_code.h>
#include <task.h>

#include "bk_config.h"
#include "ebike_manage.h"
#include "elog.h"
#include "net_port.h"
#include "ota_file_manage.h"
#include "user_os.h"
#include "utc.h"

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
USER_THREAD_OBJ_t g_net_unit_thread;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */

static int32_t net_unit_prepare(void);
static void net_unit_task(void const *argument);
static void net_upload_data(int32_t ticks_used);
static void net_unit_flow_prepare(void);
static void net_unit_set_flow_read_period(uint32_t period_s);
static void net_unit_flow_need_clear_check(uint32_t ticks_used);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t net_unit_start(void)
{
    BaseType_t ret;

    memset(&g_net_unit_thread, 0, sizeof(USER_THREAD_OBJ_t));
    g_net_unit_thread.thread = net_unit_task;
    g_net_unit_thread.name = "net_unit";
    g_net_unit_thread.stack_size = 1024;
    g_net_unit_thread.parameter = NULL;
    g_net_unit_thread.priority = RTOS_PRIORITY_NORMAL;
    ret = xTaskCreate((TaskFunction_t)g_net_unit_thread.thread, g_net_unit_thread.name, g_net_unit_thread.stack_size,
                      g_net_unit_thread.parameter, g_net_unit_thread.priority, &g_net_unit_thread.thread_handle);
    if (ret != pdPASS) {
        log_e("net_unit_start failed\r\n");
        return -1;
    }

    return 0;
}
/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

static int32_t net_unit_prepare(void)
{
    // int32_t ret = 0;
    ebike_manage_init();
    ota_file_thread_init();
    net_unit_flow_prepare();
    return 0;
}

static void net_unit_task(void const *argument)
{
    net_unit_prepare();

    log_d("NET_UNIT task running...\r\n");
    while (1) {
        net_upload_data(1000);
        net_unit_flow_need_clear_check(1000);
        net_unit_set_flow_read_period(60);
        vTaskDelay(1000);
    }
}

#define NET_UPLOAD_INTERVAL_MS 60000  // 60s
static void net_upload_data(int32_t ticks_used)
{
    static int32_t timeout = 10000;  // 10s when connected wait 10S to register to server
    int32_t ret = 0;

    if (ebike_is_connected_server() == false) {
        return;
    }
    if (ebike_is_register() != true) {
        utc_time_sync_from_net(0);
        ret = ebike_device_register_to_server();
        if (ret == 0) {
            ebike_device_state_upload_to_server();
        }
        timeout = NET_UPLOAD_INTERVAL_MS;
    }
    if (timeout <= 0) {
        if (ebike_is_register() == true) {
            ebike_device_state_upload_to_server();
        } else {
            ret = ebike_device_register_to_server();
            if (ret == 0) {
                ebike_device_state_upload_to_server();
            }
        }
        timeout = NET_UPLOAD_INTERVAL_MS;
    } else {
        timeout -= ticks_used;
    }
}

static void net_unit_set_flow_read_period(uint32_t period_s)
{
    static uint8_t set_flow_read_period_flag = 0;
    int32_t ret = 0;

    if (set_flow_read_period_flag == 0) {
        ret = net_port_set_traffic_statistics_recoder_period(period_s);
        if (ret == 0) {
            log_i("net_port_set_traffic_statistics_recoder_period: %d, success\r\n", period_s);
            set_flow_read_period_flag = 1;
        }
    }
}

static void net_unit_flow_prepare(void)
{
    int32_t ret = 0;
    uint32_t tx_bytes = 0;
    uint32_t rx_bytes = 0;

    ret = bk_config_network_flow_read(&tx_bytes, &rx_bytes);
    if (ret < 0) {
        log_w("bk_config_network_flow_read failed\r\n");
        return;
    }
    log_d("last flow: tx_bytes: %d, rx_bytes: %d\r\n", tx_bytes, rx_bytes);
    net_port_set_flow(tx_bytes, rx_bytes);

    return;
}

#define NET_FLOW_NEED_CLEAR_CHECK_INTERVAL_MS 60000  // 60s
static void net_unit_flow_need_clear_check(uint32_t ticks_used)
{
    static uint8_t flow_has_clear_flg = 1;
    static int32_t timeout = 10000;
    struct tm local_time = {0};
    long nsec = 0;
    int32_t ret = 0;
    uint32_t tx_bytes = 0;
    uint32_t rx_bytes = 0;

    if (timeout > 0) {
        timeout -= ticks_used;
        return;
    }
    utc_get_local_time(&local_time, &nsec);
    ret = net_port_get_flow(&tx_bytes, &rx_bytes);
    if (ret == 0) {
        bk_config_network_flow_write(tx_bytes, rx_bytes);
    }
    log_d("net_port_get_flow_total: %d\r\n", tx_bytes + rx_bytes);
    if (flow_has_clear_flg == 0 && local_time.tm_mday == 1) {  // 1st day of month the flow need clear
        ret = net_port_clr_flow();
        if (ret == 0) {
            log_i("net_port_clr_flow [SUCCESS]\r\n");
            flow_has_clear_flg = 1;
        } else {
            log_e("net_port_clr_flow [FAILED]\r\n");
        }

    } else {
        if (local_time.tm_mday > 15) {  // 15th day of month the flow can be cleared
            flow_has_clear_flg = 0;
        }
    }
    timeout = NET_FLOW_NEED_CLEAR_CHECK_INTERVAL_MS;
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
