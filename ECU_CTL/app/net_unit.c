/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */

#define LOG_TAG "NET_UNIT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "net_unit.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <error_code.h>
#include <task.h>

#include "elog.h"
#include "net_port.h"

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
osThreadId g_net_unit_handle;

uint32_t g_net_connected = 0;


/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */

static int32_t net_unit_prepare(void);
static void net_unit_task(void const *argument);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t net_unit_start(void)
{
    osThreadDef(net_unit, net_unit_task, osPriorityNormal, 0, 1024);
    g_net_unit_handle = osThreadCreate(osThread(net_unit), NULL);

    return 0;
}
/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

static int32_t net_unit_prepare(void)
{
    int32_t ret = 0;

    ret = net_port_init();
    if (ret!= 0) {
        log_e("net_port_init failed\r\n");
        g_net_connected = 0;
        return ret;
    }
    ret = net_port_tcp_connect(NET_PORT_TCP_TEST_HOST, NET_PORT_TCP_TEST_PORT);
    if (ret!= 0) {
        log_e("net_port_tcp_connect failed\r\n");
        g_net_connected = 0;
        return ret;
    }
    g_net_connected = 1;

    return 0;
}

static void net_unit_task(void const *argument)
{
    net_unit_prepare();

    log_d("NET_UNIT task running...\r\n");
    if (g_net_connected == 1) {
        net_port_send("Hello, world!\r\n", strlen("Hello, world!\r\n"));
    }
    while (1) {
        osDelay(1000);
    }
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */