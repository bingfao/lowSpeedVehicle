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
#include <string.h>
#include <task.h>

#include "at_com.h"
#include "cmsis_os.h"
#include "ec800m_drv_at_cmd.h"
#include "elog.h"
#include "error_code.h"
#include "main.h"
#include "ring_buffer.h"
#include "usart.h"
/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef enum {
    EC800M_DRV_STRAIGHT_MODE_RX_HEAD = 0,
    EC800M_DRV_STRAIGHT_MODE_RX_LEN,
    EC800M_DRV_STRAIGHT_MODE_RX_DATA,
    EC800M_DRV_STRAIGHT_MODE_RX_END,
} EC800M_DRV_STRAIGHT_MODE_RX_STATE_t;

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */
#define EC800M_TRANS_DRV_NAME                 "usart2"

#define EC800M_AT_ACK_QUEUE_OK                0x01
#define EC800M_AT_ACK_QUEUE_ERROR             0x02
#define EC800M_AT_ACK_QUEUE_DEV_OK            0x03
#define EC800M_AT_ACK_QUEUE_CS_REG_OK         0x04
#define EC800M_AT_ACK_QUEUE_CS_REG_ERROR      0x05
#define EC800M_AT_ACK_QUEUE_TCP_CONNECT_OK    0x06
#define EC800M_AT_ACK_QUEUE_TCP_CONNECT_ERROR 0x07
#define EC800M_AT_ACK_QUEUE_REQUIRE_DATA      0x08
#define EC800M_AT_ACK_QUEUE_SEND_OK           0x09
#define EC800M_AT_ACK_QUEUE_READY             0x0a  // device is ready to receive data, after reset
#define EC800M_AT_ACK_QUEUE_NO_CARRIER        0x0b  // when tcp do direct connect, 'NO CARRIER' may occur

#define PRINT_EC800M_RX_DATA                  1  // 1: print rx data, 0: not print
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

uint32_t g_ec800m_connect_mode = 0;
char g_ec800m_tcp_host[64] = {0};
char g_ec800m_tcp_port[16] = {0};

uint8_t g_ec800m_straight_mode_rx_head[] = {"+QIURC: \"recv\",0,"};
uint8_t g_ec800m_straight_mode_rx_index = 0;
uint8_t g_ec800m_straight_mode_rx_index_start_save = 17;
EC800M_DRV_STRAIGHT_MODE_RX_STATE_t g_ec800m_straight_mode_rx_state = EC800M_DRV_STRAIGHT_MODE_RX_HEAD;

uint8_t g_ec800m_no_carrier[] = {"NO CARRIER"};
uint8_t g_ec800m_no_carrier_index = 0;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t ec800m_send_direct(char *p_buf, uint32_t len);
static int32_t ec800m_dev_send_direct_mode(uint32_t pos, char *p_buf, uint32_t len);
static int32_t ec800m_dev_send_blocking_mode(char *p_buf, uint32_t len);
static int32_t ec800m_delay_ms(uint32_t ms);

static int32_t ec800m_drv_init(DRIVER_OBJ_t *p_driver);
static int32_t ec800m_drv_deinit(DRIVER_OBJ_t *p_driver);
static int32_t ec800m_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag);
static int32_t ec800m_drv_close(DRIVER_OBJ_t *p_driver);
static int32_t ec800m_drv_send(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);
static int32_t ec800m_drv_read(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);
static int32_t ec800m_dev_ctl(DRIVER_OBJ_t *p_driver, uint32_t cmd, void *arg);
static void ec800m_it_tx_callback(void *arg);
static void ec800m_it_rx_callback(void *arg);
static void ec800m_rx_prepare(void);
static int32_t ec800m_rx_ring_save_data(char *p_buf, uint32_t len);
static int32_t ec800m_drv_straight_out_mode_save_byte(char byte);
static int32_t ec800m_drv_straight_out_mode_save_data(char *p_buf, uint32_t len);
static int32_t ec800m_dev_check_tcp_direct_to_disconnect(char *p_buf, uint32_t len);
static int32_t ec800m_drv_save_data(char *p_buf, uint32_t len);
static void ec800m_rx_task(void const *argument);
static int32_t ec800m_device_in_callback(void *args, char *str);
static int32_t ec800m_device_ack_ok(void *args, char *str);
static int32_t ec800m_device_ack_send_ok(void *args, char *str);
static int32_t ec800m_device_ack_require_data(void *args, char *str);
static int32_t ec800m_device_ack_error(void *args, char *str);
static bool ec800m_is_ready(void);
static int32_t ec800m_device_ack_message_send(uint32_t message);
static int32_t ec800m_device_set_tcp_host(char *host);
static int32_t ec800m_device_set_tcp_port(char *port);
static int32_t ec800m_device_cs_stat(void *args, char *str);
static bool ec800m_is_cs_registered(void);
static int32_t ec800m_device_tcp_transparent_connect_ok(void *args, char *str);
static int32_t ec800m_device_tcp_transparent_connect_error(void *args, char *str);
static int32_t ec800m_device_tcp_transparent_no_carrier(void *args, char *str);
static int32_t ec800m_device_tcp_straight_ack(void *args, char *str);
static int32_t ec800m_device_tcp_connect(int32_t mode);
static int32_t ec800m_device_close_socket(void);
static int32_t ec800m_device_quit_tcp_transparent_step1(void);
static void ec800m_device_quit_tcp_transparent_step2(void);
static int32_t ec800m_device_quit_tcp_transparent(void);
static int32_t ec800m_device_tcp_disconnect(int32_t mode);
static void ec800m_device_reset_pin(void);
static int32_t ec800m_device_reset(void);

DRIVER_CTL_t g_ec800m_driver = {
    .init = ec800m_drv_init,
    .deinit = ec800m_drv_deinit,
    .open = ec800m_drv_open,
    .close = ec800m_drv_close,
    .write = ec800m_drv_send,
    .read = ec800m_drv_read,
    .control = ec800m_dev_ctl,
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
    at_com_set_send_func(&g_ec800m_at_com, ec800m_dev_send_blocking_mode);
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
    RingBuffer_Init(&g_ec800m_rx_ring_buf_handle, g_ec800m_rx_ring_buf_data, sizeof(uint8_t), EC800M_DRV_RX_BUF_SIZE);
    osThreadDef(ec800m_rx, ec800m_rx_task, osPriorityNormal, 0, 256);
    g_ec800m_rx_thread = osThreadCreate(osThread(ec800m_rx), NULL);

    osMessageQDef(ec800m_rx_queue, 10, uint8_t);
    g_ec800m_rx_queue_handle = osMessageCreate(osMessageQ(ec800m_rx_queue), NULL);
    ec800m_delay_ms(500);
    ec800m_device_reset();
    if (ec800m_is_ready() == false) {
        ec800m_drv_close(p_driver);
        ec800m_drv_deinit(p_driver);
        log_d("g_ec800m_drv inited failed\r\n");
        ret = -EBUSY;
    } else {
        ec800m_drv_close(p_driver);
    }
    log_d("g_ec800m_drv inited\r\n");

    return 0;
}

static int32_t ec800m_drv_deinit(DRIVER_OBJ_t *p_driver)
{
    osMessageDelete(g_ec800m_rx_queue_handle);
    osThreadTerminate(g_ec800m_rx_thread);
    osSemaphoreDelete(g_ec800m_tx_sem_handler);
    osSemaphoreDelete(g_ec800m_rx_sem_handler);
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
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK_ARG, NULL);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK, (void *)ec800m_it_tx_callback);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK_ARG, NULL);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK, (void *)ec800m_it_rx_callback);
    driver_set_opened(p_driver);

    return 0;
}

static void ec800m_rx_prepare(void)
{
    log_d("ec800m_rx_task running...\r\n");
}

static int32_t ec800m_rx_ring_save_data(char *p_buf, uint32_t len)
{
    if (RingBuffer_IsFull(&g_ec800m_rx_ring_buf_handle)) {
        RingBuffer_Flush(&g_ec800m_rx_ring_buf_handle);
    }
    RingBuffer_InsertMult(&g_ec800m_rx_ring_buf_handle, p_buf, len);
    return len;
}

static int32_t ec800m_drv_straight_out_mode_save_byte(char byte)
{
    uint8_t *cmp_data = g_ec800m_straight_mode_rx_head;
    uint8_t *index = &g_ec800m_straight_mode_rx_index;
    static int32_t rx_data_size = 0;
    static int32_t rx_data_index = 0;
    int32_t temp_data = 0;

    switch (g_ec800m_straight_mode_rx_state) {
        case EC800M_DRV_STRAIGHT_MODE_RX_HEAD:
            if (byte == cmp_data[*index]) {
                (*index)++;
            } else {
                *index = 0;
            }
            if (*index >= g_ec800m_straight_mode_rx_index_start_save) {
                rx_data_size = 0;
                g_ec800m_straight_mode_rx_state = EC800M_DRV_STRAIGHT_MODE_RX_LEN;
            }
            break;
        case EC800M_DRV_STRAIGHT_MODE_RX_LEN:
            if (byte != 0x0d) {
                temp_data = HEX_TO_DEC(byte);
                if (temp_data < 0 || temp_data > 10) {
                    *index = 0;
                    g_ec800m_straight_mode_rx_state = EC800M_DRV_STRAIGHT_MODE_RX_HEAD;
                }
                rx_data_size = (rx_data_size * 10) + temp_data;
            }
            if (byte == 0x0a) {
                if (rx_data_size > 0) {
                    g_ec800m_straight_mode_rx_state = EC800M_DRV_STRAIGHT_MODE_RX_DATA;
                } else {
                    *index = 0;
                    g_ec800m_straight_mode_rx_state = EC800M_DRV_STRAIGHT_MODE_RX_HEAD;
                }
            }
            break;
        case EC800M_DRV_STRAIGHT_MODE_RX_DATA:
            ec800m_rx_ring_save_data(&byte, 1);
            rx_data_index++;
            if (rx_data_index >= rx_data_size) {
                g_ec800m_straight_mode_rx_state = EC800M_DRV_STRAIGHT_MODE_RX_HEAD;
                *index = 0;
            }
            return 1;
            break;
        default:
            *index = 0;
            g_ec800m_straight_mode_rx_state = EC800M_DRV_STRAIGHT_MODE_RX_HEAD;
            break;
    }

    return 0;
}

static int32_t ec800m_drv_straight_out_mode_save_data(char *p_buf, uint32_t len)
{
    int32_t ret = 0;
    int32_t i = 0;

    for (i = 0; i < len; i++) {
        ret += ec800m_drv_straight_out_mode_save_byte(p_buf[i]);
    }

    return ret;
}

/**
 * @brief ec800m check the device quit the tcp transparent mode
 * @param p_buf
 * @param len
 * @return int32_t
 */
static int32_t ec800m_dev_check_tcp_direct_to_disconnect(char *p_buf, uint32_t len)
{
    int i = 0;

    for (i = 0; i < len; i++) {
        if (g_ec800m_no_carrier[g_ec800m_no_carrier_index] == p_buf[i]) {
            g_ec800m_no_carrier_index++;
            if (g_ec800m_no_carrier_index >= sizeof(g_ec800m_no_carrier) - 1) {
                g_ec800m_no_carrier_index = 0;
                return 1;
            }
        } else {
            g_ec800m_no_carrier_index = 0;
        }
    }

    return 0;
}

static int32_t ec800m_drv_save_data(char *p_buf, uint32_t len)
{
    if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_STRAIGHT_OUT) {
        return ec800m_drv_straight_out_mode_save_data(p_buf, len);
    } else {
        if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_DIRECT) {
            if (ec800m_dev_check_tcp_direct_to_disconnect(p_buf, len)) {
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
            }
        }
        return ec800m_rx_ring_save_data(p_buf, len);
    }
}

static void ec800m_rx_task(void const *argument)
{
    int32_t size = 0;
    int32_t ret_size = 0;
    int32_t timeout = 100;
    char data[64] = {0};
    int32_t ret = 0;

    ec800m_rx_prepare();
    while (1) {
        timeout = 100;
        ret = osSemaphoreWait(g_ec800m_rx_sem_handler, osWaitForever);
        if (ret != 0) {
            log_e("osSemaphoreWait failed, xReturn:%d\r\n", ret);
        }
#if PRINT_EC800M_RX_DATA
        log_raw("ec800m_rx_task data:\r\n");
#endif
        if (driver_is_opened(g_ec800m_driver.drv_obj) != true) {
            log_e("ec800m_drv is not opened\r\n");
            continue;
        }
        while (timeout > 0) {
            size = driver_control(g_ec800m_trans_driver, DRV_CMD_GET_SIZE, NULL);
            size = size > 63 ? 63 : size;
            if (size > 0) {
                timeout = 100;
                ret_size = driver_read(g_ec800m_trans_driver, 0, data, size);
                ec800m_drv_save_data(data, ret_size);
                at_com_data_process(&g_ec800m_at_com, data, ret_size, 0);
#if PRINT_EC800M_RX_DATA
                data[ret_size] = '\0';
                log_raw("%s", data);
#endif
            } else {
                timeout -= 10;
                ec800m_delay_ms(10);
            }
        }
        if (timeout <= 0) {
#if PRINT_EC800M_RX_DATA
            log_raw("\r\n");
#endif
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

    driver_clear_opened(p_driver);

    return 0;
}

static int32_t ec800m_dev_send_direct_mode(uint32_t pos, char *p_buf, uint32_t len)
{
    int32_t ret = 0;

    ret = osSemaphoreWait(g_ec800m_tx_sem_handler, 100);
    while (ret == 0) {
        ret = osSemaphoreWait(g_ec800m_tx_sem_handler, 100);
    }

    ec800m_send_direct(p_buf, len);
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

    return len;
}

static int32_t ec800m_dev_send_blocking_mode(char *p_buf, uint32_t len)
{
    return ec800m_dev_send_direct_mode(EC800M_DRV_POS_BLOCKING_1000, p_buf, len);
}

static int32_t ec800m_dev_send_data_len_straight_out_mode(uint32_t len)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    int32_t ret = 0;
    osEvent event;
    char cmd[128] = {0};

    at_com_clr_cmp_str(&g_ec800m_at_com);
    ret += at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_ACK_REQUIRE_DATA, NULL, ec800m_device_ack_require_data);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);

    if (ret != 0) {
        log_e("at_com_set_cmp_str failed, ret:%d\r\n", ret);
        return -EINVAL;
    }
    sprintf(cmd, "%s=0,%d\r\n", EC800M_AT_CMD_QISEND, len);
    at_com_send_str(&g_ec800m_at_com, cmd, strlen(cmd), 2000);
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        event = osMessageGet(g_ec800m_rx_queue_handle, 2000);
        if (event.status != osEventMessage) {
            log_e("osMessageGet failed, status:0x%x\r\n", event.status);
            return false;
        }
        rx_queue_num--;
        switch (event.value.v) {
            case EC800M_AT_ACK_QUEUE_REQUIRE_DATA:
                return 0;
            default:
                break;
        }
    }

    return -ETIMEDOUT;
}

static int32_t ec800m_dev_send_data_straight_out_mode(char *p_buf, uint32_t len)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    int32_t ret = 0;
    osEvent event;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    ret += at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_ACK_SEND_OK, NULL, ec800m_device_ack_send_ok);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    if (ret != 0) {
        log_e("at_com_set_cmp_str failed, ret:%d\r\n", ret);
        return -EINVAL;
    }
    at_com_send_str(&g_ec800m_at_com, p_buf, len, 2000);
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        event = osMessageGet(g_ec800m_rx_queue_handle, 2000);
        if (event.status != osEventMessage) {
            log_e("osMessageGet failed, status:0x%x\r\n", event.status);
            return -ETIMEDOUT;
        }
        rx_queue_num--;
        switch (event.value.v) {
            case EC800M_AT_ACK_QUEUE_SEND_OK:
                return len;
            default:
                break;
        }
    }

    return -ETIMEDOUT;
}

static int32_t ec800m_dev_send_straight_out_mode(uint32_t pos, char *p_buf, uint32_t len)
{
    int32_t ret = 0;

    ret = ec800m_dev_send_data_len_straight_out_mode(len);
    if (ret != 0) {
        log_e("ec800m_dev_send_data_len_straight_out_mode failed, ret:%d\r\n", ret);
        return ret;
    }
    ret = ec800m_dev_send_data_straight_out_mode(p_buf, len);
    if (ret != len) {
        log_e("ec800m_dev_send_data_straight_out_mode failed, ret:%d\r\n", ret);
    }

    return ret;
}

static int32_t ec800m_drv_send(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    int32_t ret = 0;

    if (driver_is_opened(p_driver) != true) {
        log_e("ec800m_drv is not opened\r\n");
        return -EINVAL;
    }
    if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_DIRECT) {
        ret = ec800m_dev_send_direct_mode(pos, buffer, size);
    }
    if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_STRAIGHT_OUT) {
        ret = ec800m_dev_send_straight_out_mode(pos, buffer, size);
    }

    return ret;
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

static int32_t ec800m_dev_ctl(DRIVER_OBJ_t *p_driver, uint32_t cmd, void *arg)
{
    int32_t ret = 0;
    int32_t mode = *(int32_t *)arg;

    switch (cmd) {
        case DRV_EC800M_CMD_TCP_SET_HOST:
            ret = ec800m_device_set_tcp_host((char *)arg);
            break;
        case DRV_EC800M_CMD_TCP_SET_PORT:
            ret = ec800m_device_set_tcp_port((char *)arg);
            break;
        case DRV_EC800M_CMD_GET_CS_REGISTERED:
            if (ec800m_is_cs_registered()) {
                *(bool *)arg = true;
                ret = 0;
            } else {
                *(bool *)arg = false;
                ret = -ENETUNREACH;
            }
            break;
        case DRV_EC800M_CMD_TCP_CONNECT:
            if (mode != EC800M_CONNECT_MODE_STRAIGHT_OUT && mode != EC800M_CONNECT_MODE_DIRECT) {
                log_e("ec800m_dev_ctl cmd:%d arg:%d error\r\n", cmd, mode);
                ret = -EINVAL;
                break;
            }
            if (strlen(g_ec800m_tcp_host) == 0 || strlen(g_ec800m_tcp_port) == 0) {
                log_e("tcp host or port is null\r\n");
                ret = -EINVAL;
                break;
            }
            ret = ec800m_device_tcp_connect(mode);
            break;
        case DRV_EC800M_CMD_TCP_GET_MODE:
            *(int32_t *)arg = g_ec800m_connect_mode;
            break;
        case DRV_EC800M_CMD_TCP_DISCONNECT:
            if (mode != EC800M_CONNECT_MODE_STRAIGHT_OUT && mode != EC800M_CONNECT_MODE_DIRECT) {
                log_e("ec800m_dev_ctl cmd:%d arg:%d error\r\n", cmd, mode);
                ret = -EINVAL;
                break;
            }
            ret = ec800m_device_tcp_disconnect(mode);
            break;
        case DRV_EC800M_CMD_RESET:
            ret = ec800m_device_reset();
            if (ret == 0) {
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
            }
            break;
        default:
            break;
    }

    return ret;
}

static int32_t ec800m_device_ack_message_send(uint32_t message)
{
    osEvent xReturn;
    uint32_t send_data = message;

    xReturn.status = osMessagePut(g_ec800m_rx_queue_handle, send_data, 100);
    if (osOK != xReturn.status) {
        log_d("ec800m_device_ack osMessage send:%d fail\n", message);
        return -1;
    }

    return 0;
}

static int32_t ec800m_device_in_callback(void *args, char *str)
{
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_DEV_OK);
}

static int32_t ec800m_device_ack_ok(void *args, char *str)
{
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_OK);
}

static int32_t ec800m_device_ack_require_data(void *args, char *str)
{
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_REQUIRE_DATA);
}

static int32_t ec800m_device_ack_send_ok(void *args, char *str)
{
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_SEND_OK);
}

static int32_t ec800m_device_ack_error(void *args, char *str)
{
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_ERROR);
}

static int32_t ec800m_device_ack_ready(void *args, char *str)
{
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_READY);
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

    at_com_clr_cmp_str(&g_ec800m_at_com);
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
    at_com_send_str(&g_ec800m_at_com, EC800M_AT_CMD_ATI, strlen(EC800M_AT_CMD_ATI), 2000);
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

static int32_t ec800m_device_set_tcp_host(char *host)
{
    if (strlen(host) > sizeof(g_ec800m_tcp_host)) {
        log_e("host is too long\r\n");
        return -EINVAL;
    }
    strcpy(g_ec800m_tcp_host, host);

    return 0;
}

static int32_t ec800m_device_set_tcp_port(char *port)
{
    if (strlen(port) > sizeof(g_ec800m_tcp_port)) {
        log_e("port is too long\r\n");
        return -EINVAL;
    }

    strcpy(g_ec800m_tcp_port, port);

    return 0;
}

static int32_t ec800m_device_cs_stat(void *args, char *str)
{
    uint32_t send_data = EC800M_AT_ACK_QUEUE_CS_REG_ERROR;

    if (strstr(str, "0,1")) {
        send_data = EC800M_AT_ACK_QUEUE_CS_REG_OK;
    }
    ec800m_device_ack_message_send(send_data);

    return 0;
}

static bool ec800m_is_cs_registered(void)
{
    AT_CMP_STR_NODE_t at_str_ack[3] = {0};
    int32_t has_registered = 0;
    int8_t rx_queue_num = 0;
    int32_t ret = 0;
    osEvent event;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    ret += at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_CREG_ASK_STAT, NULL, ec800m_device_cs_stat);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    ret += at_com_set_cmp_str(&at_str_ack[1], EC800M_AT_ACK_OK, NULL, ec800m_device_ack_ok);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[1]);
    ret += at_com_set_cmp_str(&at_str_ack[2], EC800M_AT_ACK_ERROR, NULL, ec800m_device_ack_error);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[2]);
    if (ret != 0) {
        log_e("at_com_set_cmp_str failed, ret:%d\r\n", ret);
        return false;
    }
    at_com_send_str(&g_ec800m_at_com, EC800M_AT_CMD_CREG_ASK, strlen(EC800M_AT_CMD_CREG_ASK), 2000);
    rx_queue_num = 2;
    while (rx_queue_num > 0) {
        event = osMessageGet(g_ec800m_rx_queue_handle, 2000);
        if (event.status != osEventMessage) {
            log_e("ec800m_is_cs_registered osMessageGet failed, status:0x%x\r\n", event.status);
            return false;
        }
        rx_queue_num--;
        switch (event.value.v) {
            case EC800M_AT_ACK_QUEUE_CS_REG_OK:
                has_registered = 1;
                break;
            case EC800M_AT_ACK_QUEUE_ERROR:
                has_registered = 0;
                break;
            default:
                break;
        }
    }
    if (has_registered != 1) {
        log_e("cs is registered [FAIL]\r\n");
        return false;
    }
    log_d("cs is registered [OK]\r\n");

    return true;
}

static int32_t ec800m_device_tcp_transparent_connect_ok(void *args, char *str)
{
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_TCP_CONNECT_OK);
}

static int32_t ec800m_device_tcp_transparent_connect_error(void *args, char *str)
{
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_TCP_CONNECT_ERROR);
}

static int32_t ec800m_device_tcp_transparent_no_carrier(void *args, char *str)
{
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_TCP_CONNECT_ERROR);
    // return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_NO_CARRIER);
}

static int32_t ec800m_device_tcp_straight_ack(void *args, char *str)
{
    int32_t send_data = EC800M_AT_ACK_QUEUE_TCP_CONNECT_ERROR;

    if (strstr(str, "0,0")) {
        send_data = EC800M_AT_ACK_QUEUE_TCP_CONNECT_OK;
    }

    return ec800m_device_ack_message_send(send_data);
}

static int32_t ec800m_device_tcp_connect(int32_t mode)
{
    AT_CMP_STR_NODE_t at_str_ack[3] = {0};
    int8_t rx_queue_num = 0;
    int32_t ret = 0;
    osEvent event;
    char cmd[128] = {0};
    int32_t mode_num = 0;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    if (mode == EC800M_CONNECT_MODE_DIRECT) {
        mode_num = 2;
        ret += at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_QIOPEN_TRANSPARENT_ACK, NULL,
                                  ec800m_device_tcp_transparent_connect_ok);
        ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
        ret +=
            at_com_set_cmp_str(&at_str_ack[1], EC800M_AT_ACK_ERROR, NULL, ec800m_device_tcp_transparent_connect_error);
        ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[1]);
        ret += at_com_set_cmp_str(&at_str_ack[2], EC800M_AT_CMD_QIOPEN_NO_CARRIER_ACK, NULL,
                                  ec800m_device_tcp_transparent_no_carrier);
        ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[2]);
        rx_queue_num = 2;
    } else if (mode == EC800M_CONNECT_MODE_STRAIGHT_OUT) {
        mode_num = 1;
        ret +=
            at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_QIOPEN_STRAIGHT_ACK, NULL, ec800m_device_tcp_straight_ack);
        ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
        rx_queue_num = 1;
    } else {
        log_e("ec800m_device_tcp_connect mode error\r\n");
        return -EINVAL;
    }
    if (ret != 0) {
        log_e("at_com_set_cmp_str failed, ret:%d\r\n", ret);
        return -EINVAL;
    }
    sprintf(cmd, "%s=1,0,\"TCP\",\"%s\",%s,0,%d\r\n", EC800M_AT_CMD_QIOPEN, g_ec800m_tcp_host, g_ec800m_tcp_port,
            mode_num);
    at_com_send_str(&g_ec800m_at_com, cmd, strlen(cmd), 150000);
    while (rx_queue_num > 0) {
        event = osMessageGet(g_ec800m_rx_queue_handle, 1000);
        if (event.status != osEventMessage) {
            log_e("osMessageGet failed, status:0x%x\r\n", event.status);
            // return -EINVAL;
        }
        rx_queue_num--;
        switch (event.value.v) {
            case EC800M_AT_ACK_QUEUE_TCP_CONNECT_OK:
                g_ec800m_connect_mode = mode;
                ret = 0;
                break;
            case EC800M_AT_ACK_QUEUE_TCP_CONNECT_ERROR:
                ec800m_device_close_socket();
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
                ret = -EOPNOTSUPP;
                break;
            default:
                break;
        }
    }
    if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_DISCONNECT) {
        log_e("tcp connect failed\r\n");
        return -EOPNOTSUPP;
    }

    return 0;
}

static int32_t ec800m_device_close_socket(void)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    int32_t ret = 0;
    osEvent event;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    ret += at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_ACK_OK, NULL, ec800m_device_ack_ok);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    ret += at_com_set_cmp_str(&at_str_ack[1], EC800M_AT_ACK_ERROR, NULL, ec800m_device_ack_error);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[1]);
    if (ret != 0) {
        log_e("at_com_set_cmp_str failed, ret:%d\r\n", ret);
        return false;
    }
    at_com_send_str(&g_ec800m_at_com, EC800M_AT_CMD_QICLOSE, strlen(EC800M_AT_CMD_QICLOSE), 2000);
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        event = osMessageGet(g_ec800m_rx_queue_handle, 2000);
        if (event.status != osEventMessage) {
            log_e("ec800m_is_cs_registered osMessageGet failed, status:0x%x\r\n", event.status);
            return false;
        }
        rx_queue_num--;
        switch (event.value.v) {
            case EC800M_AT_ACK_QUEUE_OK:
                return 0;
            case EC800M_AT_ACK_QUEUE_ERROR:
                return -EOPNOTSUPP;
            default:
                break;
        }
    }

    return -EOPNOTSUPP;
}

static int32_t ec800m_device_quit_tcp_transparent_step1(void)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    osEvent event;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_ACK_OK, NULL, ec800m_device_ack_ok);
    at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);

    at_com_send_str(&g_ec800m_at_com, EC800M_AT_CMD_QUIT_TCP_TRANSPARENT_1,
                    strlen(EC800M_AT_CMD_QUIT_TCP_TRANSPARENT_1), 2000);
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        event = osMessageGet(g_ec800m_rx_queue_handle, 2000);
        if (event.status != osEventMessage) {
            log_e("quit_tcp1 osMessageGet failed, status:0x%x\r\n", event.status);
            return -ETIME;
        }
        rx_queue_num--;
        switch (event.value.v) {
            case EC800M_AT_ACK_QUEUE_OK:
                return 0;
            default:
                break;
        }
    }

    return -ETIME;
}

static void ec800m_device_quit_tcp_transparent_step2(void)
{
    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_send_str(&g_ec800m_at_com, EC800M_AT_CMD_QUIT_TCP_TRANSPARENT_2,
                    strlen(EC800M_AT_CMD_QUIT_TCP_TRANSPARENT_2), 1000);
}

static int32_t ec800m_device_quit_tcp_transparent(void)
{
    int32_t ret = 0;

    ec800m_device_quit_tcp_transparent_step1();
    ec800m_delay_ms(1000);
    ec800m_device_quit_tcp_transparent_step2();
    ec800m_delay_ms(500);
    ret = ec800m_device_close_socket();
    if (ret != 0) {
        log_e("close_socket failed, ret:%d\r\n", ret);
        return ret;
    }

    return 0;
}

static int32_t ec800m_device_tcp_disconnect(int32_t mode)
{
    if (mode == EC800M_CONNECT_MODE_DIRECT) {
        return ec800m_device_quit_tcp_transparent();
    } else if (mode == EC800M_CONNECT_MODE_STRAIGHT_OUT) {
        return ec800m_device_close_socket();
    } else {
        log_e("ec800m_device_tcp_disconnect mode error\r\n");
        return -EINVAL;
    }
}

static void ec800m_device_reset_pin(void)
{
    HAL_GPIO_WritePin(EC800M_RESET_GPIO_Port, EC800M_RESET_Pin, GPIO_PIN_RESET);
    ec800m_delay_ms(1000);
    HAL_GPIO_WritePin(EC800M_RESET_GPIO_Port, EC800M_RESET_Pin, GPIO_PIN_SET);
}

static int32_t ec800m_device_reset(void)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    osEvent event;
    char cmd = 0;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_READY, NULL, ec800m_device_ack_ready);
    at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);

    ec800m_device_reset_pin();
    at_com_send_str(&g_ec800m_at_com, &cmd, 1, 10000);  // send fake reset command, to start check the "RDY" ACK
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        event = osMessageGet(g_ec800m_rx_queue_handle, 10000);
        if (event.status != osEventMessage) {
            log_e("ec800m reset osMessageGet failed, status:0x%x\r\n", event.status);
            return false;
        }
        rx_queue_num--;
        switch (event.value.v) {
            case EC800M_AT_ACK_QUEUE_READY:
                log_d("ec800m_device_reset success\r\n");
                return 0;
            default:
                break;
        }
    }
    log_d("ec800m_device_reset fail\r\n");

    return -ETIME;
}

DRIVER_REGISTER(&g_ec800m_driver, ec800m)
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
