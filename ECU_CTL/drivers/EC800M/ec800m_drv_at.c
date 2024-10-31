/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "EC800M_DRV_AT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "ec800m_drv_at.h"

#include <FreeRTOS.h>
#include <cmsis_os.h>
#include <error_code.h>
#include <task.h>

#include "at_com.h"
#include "cmsis_os.h"
#include "ec800m_drv_at_cmd.h"
#include "elog.h"
#include "error_code.h"
#include "ring_buffer.h"
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
#define EC800M_TRANS_DRV_NAME      "usart2"

#define EC800M_AT_ACK_QUEUE_OK     0x01
#define EC800M_AT_ACK_QUEUE_ERROR  0x02
#define EC800M_AT_ACK_QUEUE_DEV_OK 0x03

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
char g_ec800m_at_tx_buf[EC800M_DRV_TX_BUF_SIZE];
char g_ec800m_at_rx_buf[EC800M_DRV_RX_BUF_SIZE];

char g_ec800m_rx_ring_buf_data[EC800M_DRV_RX_BUF_SIZE];
RINGBUFF_T g_ec800m_rx_ring_buf_handle;

osSemaphoreDef(g_ec800m_tx_sem);
osSemaphoreId g_ec800m_tx_sem_handler = NULL;
osSemaphoreDef(g_ec800m_rx_sem);
osSemaphoreId g_ec800m_rx_sem_handler = NULL;

osThreadId g_ec800m_rx_thread;
osMessageQId g_ec800m_rx_queue_handle;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t ec800m_send_direct(char *p_buf, uint32_t len);
static int32_t ec800m_delay_ms(uint32_t ms);

static int32_t ec800m_drv_init(DRIVER_OBJ_t *p_driver);
static int32_t ec800m_drv_deinit(DRIVER_OBJ_t *p_driver);
static int32_t ec800m_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag);
static int32_t ec800m_drv_close(DRIVER_OBJ_t *p_driver);
static int32_t ec800m_drv_send(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);
static int32_t ec800m_drv_read(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);

static void ec800m_it_tx_callback(void *arg);
static void ec800m_it_rx_callback(void *arg);
static void ec800m_rx_prepare(void);
static void ec800m_rx_task(void const *argument);
static int32_t ec800m_device_in_callback(void *args);
static int32_t ec800m_device_ack_ok(void *args);
static int32_t ec800m_device_ack_error(void *args);
static bool ec800m_is_ready(void);

DRIVER_CTL_t g_ec800m_driver = {
    .init = ec800m_drv_init,
    .deinit = ec800m_drv_deinit,
    .open = ec800m_drv_open,
    .close = ec800m_drv_close,
    .write = ec800m_drv_send,
    .read = ec800m_drv_read,
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

    at_com_init(&g_ec800m_at_com, g_ec800m_at_tx_buf, EC800M_DRV_TX_BUF_SIZE, g_ec800m_at_rx_buf,
                EC800M_DRV_RX_BUF_SIZE);
    at_com_set_send_func(&g_ec800m_at_com, ec800m_send_direct);
    at_com_set_delay_func(&g_ec800m_at_com, ec800m_delay_ms);
    g_ec800m_trans_driver = get_driver(EC800M_TRANS_DRV_NAME);
    if (g_ec800m_trans_driver != NULL) {
        ret = driver_init(g_ec800m_trans_driver);
        if (ret != 0) {
            log_d("g_ec800m_trans_driver init failed, ret:%d\r\n", ret);
            return ret;
        }
        driver_set_inted(p_driver);
        ret = ec800m_drv_open(p_driver, 0);
        if (ret != 0) {
            log_d("g_ec800m_trans_driver open failed, ret:%d\r\n", ret);
            return ret;
        }
    }
    g_ec800m_tx_sem_handler = osSemaphoreCreate(osSemaphore(g_ec800m_tx_sem), 1);
    g_ec800m_rx_sem_handler = osSemaphoreCreate(osSemaphore(g_ec800m_rx_sem), 1);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK_ARG, NULL);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK, (void *)ec800m_it_tx_callback);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK_ARG, NULL);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK, (void *)ec800m_it_rx_callback);
    RingBuffer_Init(&g_ec800m_rx_ring_buf_handle, g_ec800m_rx_ring_buf_data, sizeof(uint8_t), EC800M_DRV_RX_BUF_SIZE);
    osThreadDef(ec800m_rx, ec800m_rx_task, osPriorityNormal, 0, 256);
    g_ec800m_rx_thread = osThreadCreate(osThread(ec800m_rx), NULL);

    osMessageQDef(ec800m_rx_queue, 10, uint8_t);
    g_ec800m_rx_queue_handle = osMessageCreate(osMessageQ(ec800m_rx_queue), NULL);
    if (ec800m_is_ready() == false) {
        ec800m_drv_close(p_driver);
        ec800m_drv_deinit(p_driver);
        log_d("g_ec800m_drv inited / opened failed\r\n");
        ret = -EBUSY;
    }
    log_d("g_ec800m_drv inited / opened\r\n");

    return 0;
}

static int32_t ec800m_drv_deinit(DRIVER_OBJ_t *p_driver)
{
    osMessageDelete(g_ec800m_rx_queue_handle);
    osThreadTerminate(g_ec800m_rx_thread);
    driver_deinit(g_ec800m_trans_driver);

    return 0;
}

static void ec800m_it_tx_callback(void *arg)
{
    osSemaphoreRelease(g_ec800m_tx_sem_handler);
    // log_d("ec800m_tx_done\r\n");
}

static void ec800m_it_rx_callback(void *arg)
{
    osSemaphoreRelease(g_ec800m_rx_sem_handler);
}

static int32_t ec800m_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag)
{
    int32_t ret = 0;

    if (g_ec800m_trans_driver == NULL) {
        log_e("g_ec800m_trans_driver is NULL\r\n");
        return -EINVAL;
    }
    ret = driver_open(g_ec800m_trans_driver, 0);
    if (ret != 0) {
        log_d("g_ec800m_trans_driver open failed, ret:%d\r\n", ret);
        return ret;
    }
    driver_set_opened(p_driver);

    return 0;
}

static void ec800m_rx_prepare(void)
{
    log_d("ec800m_rx_task running...\r\n");
}

static void ec800m_rx_task(void const *argument)
{
    int32_t size = 0;
    int32_t ret_size = 0;
    int32_t timeout = 100;
    char data[64] = {0};

    ec800m_rx_prepare();
    while (1) {
        osSemaphoreWait(g_ec800m_rx_sem_handler, osWaitForever);
        if (driver_is_opened(g_ec800m_driver.drv_obj) != true) {
            log_e("ec800m_drv is not opened\r\n");
            continue;
        }
        while (timeout > 0) {
            size = driver_control(g_ec800m_trans_driver, DRV_CMD_GET_SIZE, NULL);
            size = size > 64 ? 64 : size;
            if (size > 0) {
                timeout = 100;
                ret_size = driver_read(g_ec800m_trans_driver, 0, data, size);
                if (RingBuffer_IsFull(&g_ec800m_rx_ring_buf_handle)) {
                    RingBuffer_Flush(&g_ec800m_rx_ring_buf_handle);
                }
                RingBuffer_InsertMult(&g_ec800m_rx_ring_buf_handle, data, ret_size);
                at_com_data_process(&g_ec800m_at_com, data, ret_size, 0);
            } else {
                timeout -= 10;
                ec800m_delay_ms(10);
            }
        }
        if (timeout <= 0) {
            data[0] = '\0';
            at_com_data_process(&g_ec800m_at_com, data, 1, 1);
        }
    }
}

static int32_t ec800m_drv_close(DRIVER_OBJ_t *p_driver)
{
    int32_t ret = 0;

    ret = driver_close(g_ec800m_trans_driver);
    if (ret != 0) {
        log_d("g_ec800m_trans_driver close failed, ret:%d\r\n", ret);
        return ret;
    }
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK_ARG, NULL);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK, NULL);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK_ARG, NULL);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK, NULL);
    osSemaphoreDelete(g_ec800m_tx_sem_handler);
    osSemaphoreDelete(g_ec800m_rx_sem_handler);
    driver_clear_opened(p_driver);

    return 0;
}

static int32_t ec800m_drv_send(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    int32_t ret = 0;

    if (driver_is_opened(p_driver) != true) {
        log_e("ec800m_drv is not opened\r\n");
        return -EINVAL;
    }

    ec800m_send_direct(buffer, size);
    if (pos == EC800M_DRV_POS_BLOCKING) {
        osSemaphoreWait(g_ec800m_tx_sem_handler, osWaitForever);
    } else if (pos == EC800M_DRV_POS_BLOCKING_1000) {
        ret = osSemaphoreWait(g_ec800m_tx_sem_handler, 1000);
    } else {
        ret = 0;
    }
    if (ret != 0) {
        log_e("osSemaphoreWait failed, xReturn:%d\r\n", ret);
        return -ETIMEDOUT;
    }

    return size;
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

static int32_t ec800m_drv_read(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    int32_t ret = 0;
    int32_t size_read = 0;
    int32_t time_out = 1000;

    if (driver_is_opened(p_driver) != true) {
        log_e("ec800m_drv is not opened\r\n");
        return -EINVAL;
    }
    if (pos != EC800M_DRV_POS_BLOCKING && pos != EC800M_DRV_POS_BLOCKING_1000 && pos != EC800M_DRV_POS_NONBLOCKING) {
        log_e("ec800m_drv_read pos error\r\n");
        return -EINVAL;
    }

    if (pos == EC800M_DRV_POS_BLOCKING_1000) {
        time_out = 1000;
    }
    while (size_read < size && time_out > 0) {
        if (pos == EC800M_DRV_POS_NONBLOCKING) {
            time_out = 0;
        }
        if (RingBuffer_IsEmpty(&g_ec800m_rx_ring_buf_handle)) {
            if (pos == EC800M_DRV_POS_BLOCKING_1000) {
                time_out -= 100;
            }
            ec800m_delay_ms(100);
            continue;
        }
        if (pos == EC800M_DRV_POS_BLOCKING_1000) {
            time_out = 1000;
        }
        ret = RingBuffer_PopMult(&g_ec800m_rx_ring_buf_handle, &((uint8_t *)buffer)[size_read], size - size_read);
        size_read += ret;
    }

    return size_read;
}

static int32_t ec800m_device_in_callback(void *args)
{
    osEvent xReturn;
    uint32_t send_data = EC800M_AT_ACK_QUEUE_DEV_OK;

    xReturn.status = osMessagePut(g_ec800m_rx_queue_handle, send_data, 100);
    if (osOK != xReturn.status) {
        log_d("osMessage send fail\n");
    }

    return 0;
}

static int32_t ec800m_device_ack_ok(void *args)
{
    osEvent xReturn;
    uint32_t send_data = EC800M_AT_ACK_QUEUE_OK;

    xReturn.status = osMessagePut(g_ec800m_rx_queue_handle, send_data, 100);
    if (osOK != xReturn.status) {
        log_d("osMessage send fail\n");
    }

    return 0;
}

static int32_t ec800m_device_ack_error(void *args)
{
    osEvent xReturn;
    uint32_t send_data = EC800M_AT_ACK_QUEUE_ERROR;

    xReturn.status = osMessagePut(g_ec800m_rx_queue_handle, send_data, 100);
    if (osOK != xReturn.status) {
        log_d("osMessage send fail\n");
    }

    return 0;
}

/**
 * @brief This function is used to check if the device is ready.
 * @note  send "ATI\r\n" ack "EC800M\r\n"
 * @return true
 * @return false
 */
static bool ec800m_is_ready(void)
{
    AT_CMP_STR_NODE_t at_str_ack[3] = {0};
    int32_t ret = 0;
    int32_t has_ec800m = 0;
    osEvent event;
    int8_t rx_queue_num = 0;

    ret += at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_ATI_ACK_DEVICE, NULL, ec800m_device_in_callback);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    ret += at_com_set_cmp_str(&at_str_ack[1], EC800M_AT_ACK_OK, NULL, ec800m_device_ack_ok);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[1]);
    ret += at_com_set_cmp_str(&at_str_ack[2], EC800M_AT_ACK_ERROR, NULL, ec800m_device_ack_error);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[2]);
    if (ret != 0) {
        log_e("at_com_set_cmp_str failed, ret:%d\r\n", ret);
        return false;
    }
    at_com_send_str(&g_ec800m_at_com, EC800M_AT_CMD_ATI);
    rx_queue_num = 2;
    while (rx_queue_num > 0) {
        event = osMessageGet(g_ec800m_rx_queue_handle, 2000);
        if (event.status != osEventMessage) {
            log_e("ec800m_is_ready osMessageGet failed, status:0x%x\r\n", event.status);
            return false;
        }
        rx_queue_num--;
        switch (event.value.v) {
            case EC800M_AT_ACK_QUEUE_DEV_OK:
                has_ec800m = 1;
                break;
            case EC800M_AT_ACK_QUEUE_ERROR:
                has_ec800m = 0;
                break;
            default:
                break;
        }
    }
    if (has_ec800m != 1) {
        log_e("device is not ready\r\n");
        return false;
    }

    return true;
}

DRIVER_REGISTER(&g_ec800m_driver, ec800m)
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
