/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "EC800M_DRV_AT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "ec800m_drv_at.h"

#include "at_com.h"
#include "cmsis_os.h"
#include "elog.h"
#include "error_code.h"
#include "usart.h"
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
#define EC800M_TRANS_DRV_NAME "usart2"
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
AT_COM_t g_ec800m_at_com;
DRIVER_OBJ_t *g_ec800m_trans_driver = NULL;
char g_ec800m_tx_buf[EC800M_DRV_TX_BUF_SIZE];
char g_ec800m_rx_buf[EC800M_DRV_RX_BUF_SIZE];

osSemaphoreDef(g_ec800m_tx_sem);
osSemaphoreId g_ec800m_tx_sem_handler = NULL;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t ec800m_send_direct(char *p_buf, uint32_t len);
static int32_t ec800m_delay_ms(uint32_t ms);

static int32_t ec800m_drv_init(DRIVER_OBJ_t *p_driver);
static int32_t ec800m_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag);
static int32_t ec800m_drv_close(DRIVER_OBJ_t *p_driver);
static int32_t ec800m_drv_send(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);

DRIVER_CTL_t g_ec800m_driver = {
    .init = ec800m_drv_init,
    .open = ec800m_drv_open,
    .close = ec800m_drv_close,
    .write = ec800m_drv_send,
};

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

static int32_t ec800m_drv_init(DRIVER_OBJ_t *p_driver)
{
    int32_t ret = 0;

    at_com_init(&g_ec800m_at_com, g_ec800m_tx_buf, EC800M_DRV_TX_BUF_SIZE, g_ec800m_rx_buf, EC800M_DRV_RX_BUF_SIZE);
    at_com_set_send_func(&g_ec800m_at_com, ec800m_send_direct);
    at_com_set_delay_func(&g_ec800m_at_com, ec800m_delay_ms);
    g_ec800m_trans_driver = get_driver(EC800M_TRANS_DRV_NAME);
    if (g_ec800m_trans_driver != NULL) {
        ret = driver_init(g_ec800m_trans_driver);
        if (ret != 0) {
            log_d("g_ec800m_trans_driver init failed, ret:%d\r\n", ret);
            return ret;
        }
    }
    driver_set_inted(p_driver);

    return 0;
}

static void ec800m_it_tx_callback(void *arg)
{
    osSemaphoreRelease(g_ec800m_tx_sem_handler);
    log_d("ec800m_tx_done\r\n");
}

static int32_t ec800m_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag)
{
    int32_t ret = 0;

    if (g_ec800m_trans_driver == NULL) {
        log_e("g_ec800m_trans_driver is NULL\r\n");
        return -EINVAL;
    }
    g_ec800m_tx_sem_handler = osSemaphoreCreate(osSemaphore(g_ec800m_tx_sem), 1);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK, (void *)ec800m_it_tx_callback);
    ret = driver_open(g_ec800m_trans_driver, 0);
    if (ret != 0) {
        log_d("g_ec800m_trans_driver open failed, ret:%d\r\n", ret);
        return ret;
    }
    driver_set_opened(p_driver);

    return 0;
}

static int32_t ec800m_drv_close(DRIVER_OBJ_t *p_driver)
{
    int32_t ret = 0;

    ret = driver_close(g_ec800m_trans_driver);
    if (ret != 0) {
        log_d("g_ec800m_trans_driver close failed, ret:%d\r\n", ret);
        return ret;
    }
    driver_clear_opened(p_driver);

    return 0;
}

static int32_t ec800m_drv_send(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    int32_t ret = 0;

    if (driver_is_opened(p_driver) != true) {
        log_e("ec800m_drv_ is not opened\r\n");
        return -EINVAL;
    }

    ec800m_send_direct(buffer, size);
    ret = osSemaphoreWait(g_ec800m_tx_sem_handler, 1000);
    if (ret != 0) {
        log_e("osSemaphoreWait failed, xReturn:%d\r\n", ret);
        return -ETIMEDOUT;
    }

    return 0;
}

static int32_t ec800m_send_direct(char *p_buf, uint32_t len)
{
    int32_t ret = 0;

    ret = driver_write(g_ec800m_trans_driver, 0, p_buf, len);
    if (ret < 0) {
        log_e("g_ec800m_trans_driver_write failed, ret:%d\r\n", ret);
    }

    return ret;
}
static int32_t ec800m_delay_ms(uint32_t ms)
{
    osDelay(ms);
    return 0;
}

DRIVER_REGISTER(&g_ec800m_driver, ec800m)
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
