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

#include "ebike_manage.h"
#include "elog.h"
#include "net_port.h"
#include "user_os.h"

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

    return 0;
}

static void net_unit_task(void const *argument)
{
    net_unit_prepare();

    log_d("NET_UNIT task running...\r\n");
    while (1) {
        net_upload_data(1000);
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

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
