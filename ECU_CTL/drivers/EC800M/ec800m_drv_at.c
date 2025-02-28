/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "EC800M_DRV_AT"
#define LOG_LVL ELOG_LVL_DEBUG
#include "ec800m_drv_at.h"

#include <FreeRTOS.h>
#include <error_code.h>
#include <queue.h>
#include <semphr.h>
#include <stdlib.h>
#include <string.h>
#include <task.h>

#include "at_com.h"
#include "cmsis_os.h"
#include "ec800m_drv_at_cmd.h"
#include "elog.h"
#include "error_code.h"
#include "fault.h"
#include "main.h"
#include "ring_buffer.h"
#include "shell_port.h"
#include "time.h"
#include "usart.h"
#include "user_os.h"
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
#define EC800M_TRANS_DRV_NAME                        "usart2"
#define EC800M_RST_PIN_DRV_NAME                      "ec800m_rst_pin"

#define EC800M_AT_ACK_QUEUE_OK                       0x01
#define EC800M_AT_ACK_QUEUE_ERROR                    0x02
#define EC800M_AT_ACK_QUEUE_DEV_OK                   0x03
#define EC800M_AT_ACK_QUEUE_CS_REG_OK                0x04
#define EC800M_AT_ACK_QUEUE_CS_REG_ERROR             0x05
#define EC800M_AT_ACK_QUEUE_TCP_CONNECT_OK           0x06
#define EC800M_AT_ACK_QUEUE_TCP_CONNECT_ERROR        0x07
#define EC800M_AT_ACK_QUEUE_REQUIRE_DATA             0x08
#define EC800M_AT_ACK_QUEUE_SEND_OK                  0x09
#define EC800M_AT_ACK_QUEUE_READY                    0x0a  // device is ready to receive data, after reset
#define EC800M_AT_ACK_QUEUE_NO_CARRIER               0x0b  // when tcp do direct connect, 'NO CARRIER' may occur
#define EC800M_AT_ACK_QUEUE_TCP_DIRECT_CONNECT       0x0c  // tcp direct connect state
#define EC800M_AT_ACK_QUEUE_TCP_STRAIGHT_OUT_CONNECT 0x0d  // tcp straight out connect state
#define EC800M_AT_ACK_QUEUE_TCP_DISCONNECT           0x0e  // tcp disconnect state
#define EC800M_AT_ACK_QUEUE_UTC_SUCCESS              0x0f  // get UTC time success
#define EC800M_AT_ACK_QUEUE_GNSS_SUCCESS             0x10  // get GNSS location success

#define PRINT_EC800M_RX_DATA                         1  // 1: print rx data, 0: not print

#define QUEUE_LENGTH                                 10
#define ITEM_SIZE                                    sizeof(int32_t)

#define EC800M_INTER_TRAN_BUF_SIZE                   50
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
DRIVER_OBJ_t *g_ec800m_rst_pin_driver = NULL;
char g_ec800m_at_tx_buf[EC800M_DRV_AT_TX_BUF_SIZE];
char g_ec800m_at_rx_buf[EC800M_DRV_AT_RX_BUF_SIZE];

#ifdef STM32F407xx
#pragma location = ".fast_ccmram"
#endif
char g_ec800m_rx_ring_buf_data[EC800M_DRV_RX_BUF_SIZE];
RINGBUFF_T g_ec800m_rx_ring_buf_handle;

// osSemaphoreDef(g_ec800m_tx_sem);
// osSemaphoreId g_ec800m_tx_sem_handler = NULL;
StaticSemaphore_t g_ec800m_tx_sem_buffer;
SemaphoreHandle_t g_ec800m_tx_sem_handler = NULL;
// osSemaphoreDef(g_ec800m_rx_sem);
StaticSemaphore_t g_ec800m_rx_sem_buffer;
SemaphoreHandle_t g_ec800m_rx_sem_handler = NULL;

uint8_t g_ec800m_rx_queue_storage_area[QUEUE_LENGTH * ITEM_SIZE];
static StaticQueue_t g_ec800m_rx_static_queue;
QueueHandle_t g_ec800m_rx_queue_handle = NULL;
StaticSemaphore_t g_ec800m_tx_mutex_buffer;
SemaphoreHandle_t g_ec800m_tx_mutex;

uint32_t g_ec800m_connect_mode = 0;
char g_ec800m_tcp_host[64] = {0};
char g_ec800m_tcp_port[16] = {0};

uint8_t g_ec800m_straight_mode_rx_head[] = {"+QIURC: \"recv\",0,"};
uint8_t g_ec800m_straight_mode_rx_index = 0;
uint8_t g_ec800m_straight_mode_rx_index_start_save = 17;
EC800M_DRV_STRAIGHT_MODE_RX_STATE_t g_ec800m_straight_mode_rx_state = EC800M_DRV_STRAIGHT_MODE_RX_HEAD;

uint8_t g_ec800m_no_carrier[] = {"NO CARRIER"};
uint8_t g_ec800m_no_carrier_index = 0;

uint8_t g_ec800m_qiurc_closed[] = {"+QIURC: \"closed\","};
uint8_t g_ec800m_qiurc_closed_index = 0;

static uint8_t g_ec800m_inter_tran_buf[EC800M_INTER_TRAN_BUF_SIZE] = {0};
static uint8_t g_ec800m_inter_tran_buf_index = 0;
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
static int32_t ec800m_dev_ctl(DRIVER_OBJ_t *p_driver, uint32_t cmd, void *arg, uint32_t size);
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
static int32_t ec800m_device_tcp_state_check(void *args, char *str);
static bool ec800m_socket_is_connected(void);
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
static int32_t ec800m_device_utc_str_analysis(void *args, char *str);
static int32_t ec800m_device_get_utc(struct tm *local_time);
static int32_t ec800m_device_gnss_qgpsloc_str_analysis(void *args, char *str);
static int32_t ec800m_device_get_gnss(NET_PORT_GNSS_t *gnss_location);
static int32_t ec800m_device_set_traffic_statistics_recoder_period(uint32_t seconds);
static int32_t ec800m_device_get_traffic_statistics_ack(void *args, char *str);
static int32_t ec800m_device_get_traffic_statistics(uint32_t *tx_rx_bytes);
static int32_t ec800m_device_clr_traffic_statistics(void);

DRIVER_CTL_t g_ec800m_driver = {
    .init = ec800m_drv_init,
    .deinit = ec800m_drv_deinit,
    .open = ec800m_drv_open,
    .close = ec800m_drv_close,
    .write = ec800m_drv_send,
    .read = ec800m_drv_read,
    .control = ec800m_dev_ctl,
};

SET_THREAD_STACK(ec800m_rx, 512);
USER_THREAD_OBJ_t g_ec800m_rx_thread =
    USER_THREAD_OBJ_INIT(ec800m_rx_task, "ec800m_rx", 512, NULL, RTOS_PRIORITY_NORMAL, GET_THREAD_STACK(ec800m_rx));
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
    USER_THREAD_OBJ_t *thread = &g_ec800m_rx_thread;

    at_com_init(&g_ec800m_at_com, g_ec800m_at_tx_buf, EC800M_DRV_AT_TX_BUF_SIZE, g_ec800m_at_rx_buf,
                EC800M_DRV_AT_RX_BUF_SIZE);
    at_com_set_send_func(&g_ec800m_at_com, ec800m_dev_send_blocking_mode);
    at_com_set_delay_func(&g_ec800m_at_com, ec800m_delay_ms);
    g_ec800m_trans_driver = get_driver(EC800M_TRANS_DRV_NAME);
    if (g_ec800m_trans_driver != NULL) {
        ret = driver_init(g_ec800m_trans_driver);
        if (ret != 0) {
            log_e("g_ec800m_trans_driver init failed, ret:%d\r\n", ret);
            return ret;
        }
        driver_set_inted(p_driver);
        ret = ec800m_drv_open(p_driver, 0);
        if (ret != 0) {
            log_e("g_ec800m_trans_driver open failed, ret:%d\r\n", ret);
            return ret;
        }
    }
    g_ec800m_rst_pin_driver = get_driver(EC800M_RST_PIN_DRV_NAME);
    if (g_ec800m_rst_pin_driver != NULL) {
        ret = driver_init(g_ec800m_rst_pin_driver);
        if (ret != 0) {
            log_e("g_ec800m_rst_pin_driver init failed, ret:%d\r\n", ret);
            return ret;
        }
        ret = driver_open(g_ec800m_rst_pin_driver, 0);
        if (ret != 0) {
            log_e("g_ec800m_rst_pin_driver open failed, ret:%d\r\n", ret);
        }
    }
    g_ec800m_tx_sem_handler = xSemaphoreCreateBinaryStatic(&g_ec800m_tx_sem_buffer);
    g_ec800m_tx_mutex = xSemaphoreCreateMutexStatic(&g_ec800m_tx_mutex_buffer);
    g_ec800m_rx_sem_handler = xSemaphoreCreateBinaryStatic(&g_ec800m_rx_sem_buffer);
    RingBuffer_Init(&g_ec800m_rx_ring_buf_handle, g_ec800m_rx_ring_buf_data, sizeof(uint8_t), EC800M_DRV_RX_BUF_SIZE);
    thread->thread_handle = xTaskCreateStatic((TaskFunction_t)thread->thread, thread->name, thread->stack_size,
                                              thread->parameter, thread->priority, thread->thread_stack, &thread->tcb);
    fault_assert(thread->thread_handle != NULL, FAULT_CODE_NORMAL);

    // osMessageQDef(ec800m_rx_queue, 10, uint8_t);
    g_ec800m_rx_queue_handle =
        xQueueCreateStatic(QUEUE_LENGTH, ITEM_SIZE, g_ec800m_rx_queue_storage_area, &g_ec800m_rx_static_queue);
    ec800m_delay_ms(500);
    ec800m_device_reset();
    if (ec800m_is_ready() == false) {
        driver_close(g_ec800m_rst_pin_driver);
        driver_deinit(g_ec800m_rst_pin_driver);
        ec800m_drv_close(p_driver);
        ec800m_drv_deinit(p_driver);
        log_d("g_ec800m_drv inited failed\r\n");
        ret = -EBUSY;
        return ret;
    } else {
        ec800m_drv_close(p_driver);
    }
    log_d("g_ec800m_drv inited\r\n");

    return 0;
}

static int32_t ec800m_drv_deinit(DRIVER_OBJ_t *p_driver)
{
    vQueueDelete(g_ec800m_rx_queue_handle);
    vTaskDelete(g_ec800m_rx_thread.thread_handle);
    vSemaphoreDelete(g_ec800m_tx_sem_handler);
    vSemaphoreDelete(g_ec800m_tx_mutex);
    vSemaphoreDelete(g_ec800m_rx_sem_handler);
    driver_deinit(g_ec800m_trans_driver);
    driver_close(g_ec800m_rst_pin_driver);
    driver_deinit(g_ec800m_rst_pin_driver);

    return 0;
}

static void ec800m_it_tx_callback(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xSemaphoreGiveFromISR(g_ec800m_tx_sem_handler, &xHigherPriorityTaskWoken) == pdTRUE) {
        // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
}

static void ec800m_it_rx_callback(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xSemaphoreGiveFromISR(g_ec800m_rx_sem_handler, &xHigherPriorityTaskWoken) == pdTRUE) {
        // portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
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
        log_e("g_ec800m_trans_driver open failed, ret:%d\r\n", ret);
        return ret;
    }
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK_ARG, NULL, 0);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK, (void *)ec800m_it_tx_callback,
                   sizeof(void *));
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK_ARG, NULL, 0);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK, (void *)ec800m_it_rx_callback,
                   sizeof(void *));
    driver_set_opened(p_driver);

    return 0;
}

static void ec800m_rx_prepare(void)
{
    log_d("ec800m_rx_task running...\r\n");
}

static int32_t ec800m_rx_ring_save_data(char *p_buf, uint32_t len)
{
    // if (RingBuffer_IsFull(&g_ec800m_rx_ring_buf_handle)) {
    //     RingBuffer_Flush(&g_ec800m_rx_ring_buf_handle);
    // }
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
            if (byte != 0x0d && byte != 0x0a) {
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
                rx_data_index = 0;
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

static int32_t ec800m_dev_check_tcp_straight_out_to_disconnect(char *p_buf, uint32_t len)
{
    int i = 0;

    for (i = 0; i < len; i++) {
        if (g_ec800m_qiurc_closed[g_ec800m_qiurc_closed_index] == p_buf[i]) {
            g_ec800m_qiurc_closed_index++;
            if (g_ec800m_qiurc_closed_index >= sizeof(g_ec800m_qiurc_closed) - 1) {
                g_ec800m_qiurc_closed_index = 0;
                return 1;
            }
        } else {
            g_ec800m_qiurc_closed_index = 0;
        }
    }

    return 0;
}

static int32_t ec800m_drv_save_data(char *p_buf, uint32_t len)
{
    if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_STRAIGHT_OUT) {
        if (ec800m_dev_check_tcp_straight_out_to_disconnect(p_buf, len)) {
            log_d("ec800m_MODE_STRAIGHT_OUT: tcp disconnect\r\n");
            g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
        }
        return ec800m_drv_straight_out_mode_save_data(p_buf, len);
    } else if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_DIRECT) {
        if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_DIRECT) {
            if (ec800m_dev_check_tcp_direct_to_disconnect(p_buf, len)) {
                log_d("ec800m_MODE_DIRECT: tcp disconnect\r\n");
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
            }
        }
        return ec800m_rx_ring_save_data(p_buf, len);
    }

    return -1;
}

static void ec800m_rx_task(void const *argument)
{
    int32_t size = 0;
    int32_t ret_size = 0;
    int32_t timeout = 100;
    char data[64] = {0};
    int32_t ret = 0;
    BaseType_t res = pdFALSE;
    int32_t in_data_size = 0;

    ec800m_rx_prepare();
    while (1) {
        timeout = 200;
        res = xSemaphoreTake(g_ec800m_rx_sem_handler, osWaitForever);
        if (res != pdPASS) {
            log_e("osSemaphoreWait failed, xReturn:%d\r\n", ret);
        }
#if PRINT_EC800M_RX_DATA
        log_i("ec800m_rx_task data:\r\n");
#endif
        if (driver_is_opened(g_ec800m_driver.drv_obj) != true) {
            log_e("ec800m_drv is not opened\r\n");
            continue;
        }
        while (timeout > 0) {
            size = driver_control(g_ec800m_trans_driver, DRV_CMD_GET_RX_SIZE, NULL, 0);
            size = size > 63 ? 63 : size;
            if (size > 0) {
                timeout = 100;
                ret_size = driver_read(g_ec800m_trans_driver, 0, data, size);
#if PRINT_EC800M_RX_DATA
                if (log_switch_is_on(LOG_SW_AT_READ)) {
                    if (ret > 0) {
                        if (in_data_size < 1000) {
                            for (int i = 0; i < ret_size; i++) {
                                if ((in_data_size + i) % 16 == 0) {
                                    log_raw("\r\n");
                                }
                                log_raw("%02x ", data[i]);
                            }
                            in_data_size += ret_size;
                            if (in_data_size >= 1000) {
                                log_raw("\r\n");
                            }
                        }
                    } else {
                        for (int i = 0; i < ret_size; i++) {
                            // printf("0x%02x[%c] ", data[i], data[i]);
                            log_raw("%c", data[i]);
                        }
                    }
                }
#endif
                ret = ec800m_drv_save_data(data, ret_size);
                at_com_data_process(&g_ec800m_at_com, data, ret_size, 0);
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
            log_d("AT rx: %d[%d] \r\n", g_ec800m_at_com.rx_buffer_index, g_ec800m_at_com.status);
            log_d("data rx: %d \r\n", in_data_size);
            in_data_size = 0;  // 512 bytes per line
            at_com_data_process(&g_ec800m_at_com, data, 1, 1);
        }
    }
}

static int32_t ec800m_drv_close(DRIVER_OBJ_t *p_driver)
{
    int32_t ret = 0;

    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK_ARG, NULL, 0);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_WRITE_DONE_CALLBACK, NULL, 0);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK_ARG, NULL, 0);
    driver_control(g_ec800m_trans_driver, DRV_CMD_SET_READ_DONE_CALLBACK, NULL, 0);

    ret = driver_close(g_ec800m_trans_driver);
    if (ret != 0) {
        log_d("g_ec800m_trans_driver close failed, ret:%d\r\n", ret);
        return ret;
    }
    driver_clear_opened(p_driver);

    return 0;
}

static int32_t ec800m_dev_send_direct_mode(uint32_t pos, char *p_buf, uint32_t len)
{
    BaseType_t ret = pdPASS;

    ret = xSemaphoreTake(g_ec800m_tx_sem_handler, 100);
    while (ret == pdPASS) {
        ret = xSemaphoreTake(g_ec800m_tx_sem_handler, 100);
    }

    ec800m_send_direct(p_buf, len);
    if (pos == EC800M_DRV_POS_BLOCKING) {
        xSemaphoreTake(g_ec800m_tx_sem_handler, osWaitForever);
    } else if (pos == EC800M_DRV_POS_BLOCKING_1000) {
        ret = xSemaphoreTake(g_ec800m_tx_sem_handler, 1000);
    } else {
        ret = pdPASS;
    }
    if (ret != pdPASS) {
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
    char cmd[128] = {0};
    BaseType_t res = pdFAIL;
    uint32_t msg;

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
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("osMessageGet failed\r\n");
            return false;
        }
        rx_queue_num--;
        switch (msg) {
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
    BaseType_t res = pdFAIL;
    uint32_t msg;

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
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("osMessageGet failed\r\n");
            return -ETIMEDOUT;
        }
        rx_queue_num--;
        switch (msg) {
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
    if (xSemaphoreTake(g_ec800m_tx_mutex, 1000) != pdPASS) {
        log_e("ec800m_drv_send failed, osSemaphoreTake failed\r\n");
        return -ETIMEDOUT;
    }
    if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_DIRECT) {
        ret = ec800m_dev_send_direct_mode(pos, buffer, size);
    }
    if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_STRAIGHT_OUT) {
        ret = ec800m_dev_send_straight_out_mode(pos, buffer, size);
    }
    xSemaphoreGive(g_ec800m_tx_mutex);

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
    int32_t time_out = 0;

    if (driver_is_opened(p_driver) != true) {
        log_e("ec800m_drv is not opened\r\n");
        return -EINVAL;
    }
    if (pos == EC800M_DRV_POS_BLOCKING_1000) {
        time_out = 1000;
    } else if (pos == EC800M_DRV_POS_BLOCKING) {
        time_out = HAL_MAX_DELAY;
    } else if (pos == EC800M_DRV_POS_NONBLOCKING) {
        time_out = 0;
    }
    while (size_read < size && time_out > 0) {
        if (RingBuffer_GetCount(&g_ec800m_rx_ring_buf_handle) == 0) {
            ec800m_delay_ms(10);
            time_out -= 10;
            continue;
        }
        ret = RingBuffer_PopMult(&g_ec800m_rx_ring_buf_handle, &((uint8_t *)buffer)[size_read], size - size_read);
        size_read += ret;
    }

    return size_read;
}

static int32_t ec800m_dev_ctl(DRIVER_OBJ_t *p_driver, uint32_t cmd, void *arg, uint32_t size)
{
    int32_t ret = 0;
    bool ret_bool = false;
    int32_t mode = 0;
    uint32_t argsize = 0;

    if (p_driver == NULL) {
        return -EINVAL;
    }
    if (arg != NULL) {
        mode = *(int32_t *)arg;
    }

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
            if (ret == 0) {
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
            }
            break;
        case DRV_EC800M_CMD_RESET:
            ret = ec800m_device_reset();
            if (ret == 0) {
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
            }
            break;
        case DRV_EC800M_CMD_TCP_REF_STATE:
            ret_bool = ec800m_socket_is_connected();
            if (ret_bool == false) {
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
            }
            *(int32_t *)arg = g_ec800m_connect_mode;
            break;
        case DRV_EC800M_CMD_SET_DIS_STATE:
            g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
            break;
        case DRV_EC800M_CMD_GET_UTC_TIME:
            ret = ec800m_device_get_utc((struct tm *)arg);
            break;
        case DRV_EC800M_CMD_GET_GNSS:
            if (arg == NULL) {
                ret = -EINVAL;
                break;
            }
            argsize = *(uint32_t *)arg;
            if (argsize < sizeof(NET_PORT_GNSS_t)) {
                log_e("size:%d is not equal to NET_PORT_GNSS_t size:%d\r\n", argsize, sizeof(NET_PORT_GNSS_t));
                return -EINVAL;
            }
            ret = ec800m_device_get_gnss((NET_PORT_GNSS_t *)((uint32_t *)arg + 1));
        case DRV_EC800M_CMD_SET_QAUGDCNT:
            if (arg == NULL) {
                ret = -EINVAL;
                break;
            }
            ret = ec800m_device_set_traffic_statistics_recoder_period(*(uint32_t *)arg);
            break;
        case DRV_EC800M_CMD_GET_QGDCNT:
            ret = ec800m_device_get_traffic_statistics((uint32_t *)arg);
            break;
        case DRV_EC800M_CMD_CLR_FLOW:
            ret = ec800m_device_clr_traffic_statistics();
            break;
        default:
            break;
    }

    return ret;
}

static int32_t ec800m_device_ack_message_send(uint32_t message)
{
    uint32_t send_data = message;
    BaseType_t res = pdFAIL;

    res = xQueueSend(g_ec800m_rx_queue_handle, &send_data, 100);
    if (res != pdPASS) {
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
    int8_t rx_queue_num = 0;
    BaseType_t res = pdFAIL;
    uint32_t msg;

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
    at_com_send_str(&g_ec800m_at_com, EC800M_AT_CMD_ATI, strlen(EC800M_AT_CMD_ATI), 5000);
    rx_queue_num = 2;
    while (rx_queue_num > 0) {
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 5000);
        if (res != pdPASS) {
            log_e("ec800m_is_ready osMessageGet failed\r\n");
            return -ETIMEDOUT;
        }
        rx_queue_num--;
        switch (msg) {
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
    BaseType_t res = pdFAIL;
    uint32_t msg;

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
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("ec800m_is_cs_registered osMessageGet failed\r\n");
            return -ETIMEDOUT;
        }
        rx_queue_num--;
        switch (msg) {
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

static int32_t ec800m_device_tcp_state_check(void *args, char *str)
{
    char *token;
    char delim[] = ",";  // separator  ","
    int8_t index = 0;
    int8_t connect_flg = 0;
    uint32_t connect_mode = 0;

    token = strtok(str, delim);
    if (token != NULL) {
        index++;
        // log_raw("[%d]%s\r\n", index, token);
    }

    // get next token
    while ((token = strtok(NULL, delim)) != NULL) {
        index++;
        // log_raw("[%d]%s\r\n", index, token);
        if (index == 6) {
            if (strcmp(token, "2") == 0) {
                connect_flg = 1;
            } else {
                connect_flg = 0;
            }
        } else if (index == 9) {
            if (strcmp(token, "1") == 0) {
                connect_mode = EC800M_AT_ACK_QUEUE_TCP_STRAIGHT_OUT_CONNECT;
            } else if (strcmp(token, "2") == 0) {
                connect_mode = EC800M_AT_ACK_QUEUE_TCP_DIRECT_CONNECT;
            } else {
                connect_mode = EC800M_AT_ACK_QUEUE_TCP_DISCONNECT;
            }
        }
    }
    if (connect_flg == 0) {
        connect_mode = EC800M_AT_ACK_QUEUE_TCP_DISCONNECT;
    }
    log_d("tcp state check connect_flg:%d connect_mode:%d\r\n", connect_flg, connect_mode);
    ec800m_device_ack_message_send(connect_mode);

    return 0;
}

static bool ec800m_socket_is_connected(void)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int32_t has_connected = 0;
    int8_t rx_queue_num = 0;
    int32_t ret = 0;
    BaseType_t res = pdFAIL;
    uint32_t msg;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    ret += at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_QISTATE_ACK, NULL, ec800m_device_tcp_state_check);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    ret += at_com_set_cmp_str(&at_str_ack[1], EC800M_AT_ACK_OK, NULL, ec800m_device_ack_ok);
    ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[1]);
    if (ret != 0) {
        log_e("at_com_set_cmp_str failed, ret:%d\r\n", ret);
        return false;
    }
    at_com_send_str(&g_ec800m_at_com, EC800M_AT_CMD_QISTATE, strlen(EC800M_AT_CMD_QISTATE), 2000);
    rx_queue_num = 2;
    while (rx_queue_num > 0) {
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("socket state osMessageGet failed,[%d] \r\n", rx_queue_num);
            return -ETIMEDOUT;
        }
        rx_queue_num--;
        switch (msg) {
            case EC800M_AT_ACK_QUEUE_TCP_DIRECT_CONNECT:
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DIRECT;
                has_connected = 1;
                break;
            case EC800M_AT_ACK_QUEUE_TCP_STRAIGHT_OUT_CONNECT:
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_STRAIGHT_OUT;
                has_connected = 1;
                break;
            case EC800M_AT_ACK_QUEUE_TCP_DISCONNECT:
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
                has_connected = 0;
                break;
            default:
                break;
        }
    }
    if (has_connected != 1) {
        log_e("TCP is not connected \r\n");
        g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
        return false;
    }
    log_d("TCP is connected \r\n");

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

static int32_t ec800m_device_tcp_transparent_qiurc_close(void *args, char *str)
{
    log_d("ec800m_device_tcp_transparent_qiurc_close\r\n");
    return ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_TCP_CONNECT_ERROR);
}

static int32_t ec800m_device_tcp_straight_ack(void *args, char *str)
{
    int32_t send_data = EC800M_AT_ACK_QUEUE_TCP_CONNECT_ERROR;

    if (strstr(str, "0,0")) {
        log_d("ec800m_device_tcp_straight_ack OK\r\n");
        send_data = EC800M_AT_ACK_QUEUE_TCP_CONNECT_OK;
    } else {
        log_d("ec800m_device_tcp_straight_ack err\r\n");
    }
    return ec800m_device_ack_message_send(send_data);
}

static int32_t ec800m_device_tcp_connect(int32_t mode)
{
    AT_CMP_STR_NODE_t at_str_ack[3] = {0};
    int8_t rx_queue_num = 0;
    int32_t ret = 0;
    char cmd[128] = {0};
    int32_t mode_num = 0;
    BaseType_t res = pdFAIL;
    uint32_t msg;

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
        ret += at_com_set_cmp_str(&at_str_ack[1], EC800M_AT_CMD_QIURC_CLOSED_ACK, NULL,
                                  ec800m_device_tcp_transparent_qiurc_close);
        ret += at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[1]);
        rx_queue_num = 2;
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
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 5000);
        if (res != pdPASS) {
            log_e("osMessageGet failed\r\n");
            rx_queue_num--;
            continue;
        }
        rx_queue_num--;
        switch (msg) {
            case EC800M_AT_ACK_QUEUE_TCP_CONNECT_OK:
                g_ec800m_connect_mode = mode;
                log_d("osMessageGet OK [%d]rx_queue[%d]\r\n", g_ec800m_connect_mode, rx_queue_num);
                at_com_set_status(&g_ec800m_at_com, AT_COM_ACK_DATA);
                ret = 0;
                break;
            case EC800M_AT_ACK_QUEUE_TCP_CONNECT_ERROR:
                log_d("osMessageGet ERROR, rx_queue[%d]\r\n", rx_queue_num);
                ec800m_device_close_socket();
                g_ec800m_connect_mode = EC800M_CONNECT_MODE_DISCONNECT;
                ret = -EOPNOTSUPP;
                break;
            default:
                break;
        }
    }
    at_com_set_status(&g_ec800m_at_com, AT_COM_IDLE);
    if (g_ec800m_connect_mode == EC800M_CONNECT_MODE_DISCONNECT) {
        log_e("tcp connect failed\r\n");
        return -EOPNOTSUPP;
    }
    log_d("tcp connect success\r\n");

    return 0;
}

static int32_t ec800m_device_close_socket(void)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    int32_t ret = 0;
    BaseType_t res = pdFAIL;
    uint32_t msg;

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
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("ec800m_is_cs_registered osMessageGet failed\r\n");
            return false;
        }
        rx_queue_num--;
        switch (msg) {
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
    BaseType_t res = pdFAIL;
    uint32_t msg;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_ACK_OK, NULL, ec800m_device_ack_ok);
    at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);

    at_com_send_str(&g_ec800m_at_com, EC800M_AT_CMD_QUIT_TCP_TRANSPARENT_1,
                    strlen(EC800M_AT_CMD_QUIT_TCP_TRANSPARENT_1), 2000);
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("quit_tcp1 osMessageGet failed\r\n");
            return -ETIME;
        }
        rx_queue_num--;
        switch (msg) {
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
    uint32_t pin_val = 0;

    if (g_ec800m_rst_pin_driver == NULL) {
        log_e("g_ec800m_rst_pin_driver is NULL\r\n");
        return;
    }
    driver_write(g_ec800m_rst_pin_driver, 0, &pin_val, 1);
    ec800m_delay_ms(1000);
    pin_val = 1;
    driver_write(g_ec800m_rst_pin_driver, 0, &pin_val, 1);
}

static int32_t ec800m_device_reset(void)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    char cmd = 0;
    BaseType_t res = pdFAIL;
    uint32_t msg;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_READY, NULL, ec800m_device_ack_ready);
    at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);

    ec800m_device_reset_pin();
    at_com_send_str(&g_ec800m_at_com, &cmd, 1, 10000);  // send fake reset command, to start check the "RDY" ACK
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 10000);
        if (res != pdPASS) {
            log_e("ec800m reset osMessageGet failed\r\n");
            return false;
        }
        rx_queue_num--;
        switch (msg) {
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

static int32_t ec800m_device_utc_str_analysis(void *args, char *str)
{
    struct tm *local_time = (struct tm *)g_ec800m_inter_tran_buf;
    uint32_t year, month, day, hour, minute, second;
    int32_t ret = 0;
    if (str == NULL) {
        log_e("str is NULL\r\n");
        return -EINVAL;
    }

    ret = sscanf(str, "+CCLK: \"%2d/%2d/%2d,%2d:%2d:%2d+32\"", &year, &month, &day, &hour, &minute, &second);
    if (ret != 6) {
        log_e("utc str analysis failed, ret:%d (%s)\r\n", ret, str);
        return -EINVAL;
    }
    local_time->tm_year = 2000 + year - 1900;
    local_time->tm_mon = month - 1;
    local_time->tm_mday = day;
    local_time->tm_hour = hour;
    local_time->tm_min = minute;
    local_time->tm_sec = second;
    g_ec800m_inter_tran_buf_index = sizeof(struct tm);
    log_d("utc str analysis success, year:%d, month:%d, day:%d, hour:%d, minute:%d, second:%d\r\n", local_time->tm_year,
          local_time->tm_mon, local_time->tm_mday, local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
    ret = ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_UTC_SUCCESS);

    return ret;
}

static int32_t ec800m_device_get_utc(struct tm *local_time)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    char cmd[20] = {0};
    BaseType_t res = pdFAIL;
    uint32_t msg;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_GET_UTC_CCLK_ACK, NULL, ec800m_device_utc_str_analysis);
    at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    memset(g_ec800m_inter_tran_buf, 0, sizeof(g_ec800m_inter_tran_buf));
    g_ec800m_inter_tran_buf_index = 0;
    sprintf(cmd, "%s\r\n", EC800M_AT_CMD_GET_UTC_CCLK);
    at_com_send_str(&g_ec800m_at_com, cmd, strlen(cmd), 2000);  // send AT+CCLK?
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("ec800m get utc osMessageGet failed\r\n");
            return -ETIME;
        }
        rx_queue_num--;
        switch (msg) {
            case EC800M_AT_ACK_QUEUE_UTC_SUCCESS:
                if (g_ec800m_inter_tran_buf_index == sizeof(struct tm)) {
                    memcpy(local_time, g_ec800m_inter_tran_buf, sizeof(struct tm));
                    log_d("ec800m_device_get_utc success\r\n");
                    log_d("ec800m get_utc local_time:%d-%d-%d %d:%d:%d\r\n", local_time->tm_year + 1900,
                          local_time->tm_mon + 1, local_time->tm_mday, local_time->tm_hour, local_time->tm_min,
                          local_time->tm_sec);
                } else {
                    log_e("ec800m utc buffer size error\r\n");
                    return -EINVAL;
                }
                return 0;
            default:
                break;
        }
    }
    log_d("ec800m_device_get_utc fail\r\n");

    return -ETIME;
}

static int32_t ec800m_device_gnss_qgpsloc_str_analysis(void *args, char *str)
{
    int32_t ret = 0;
    float time = 0;
    float latitude = 0;
    float longitude = 0;
    float hdop = 0;
    float altitude = 0;
    int32_t fix_type = 0;
    float cog = 0;
    float spkm = 0;
    float spkn = 0;
    float date = 0;
    float nsat = 0;
    struct tm utc_time = {0};

    NET_PORT_GNSS_t *gnss_location = (NET_PORT_GNSS_t *)g_ec800m_inter_tran_buf;
    if (str == NULL) {
        log_e("str is NULL\r\n");
        return -EINVAL;
    }
    ret = sscanf(str, "+QGPSLOC: %f,%f,%f,%f,%f,%d,%f,%f,%f,%f,%f", &time, &latitude, &longitude, &hdop, &altitude,
                 &fix_type, &cog, &spkm, &spkn, &date, &nsat);
    if (ret != 11) {
        log_e("gnss str analysis 1 failed, ret:%d (%s)\r\n", ret, str);
        ret = sscanf(str, "+QGPSLOC: %f,%f,%f,%f,%f,%d,,%f,%f,%f,%f", &time, &latitude, &longitude, &hdop, &altitude,
                     &fix_type, &spkm, &spkn, &date, &nsat);
        if (ret != 10) {
            log_e("gnss str analysis 2 failed, ret:%d\r\n", ret);
            return -EINVAL;
        }
    }
    gnss_location->latitude = latitude;
    gnss_location->longitude = longitude;
    gnss_location->altitude = altitude;
    gnss_location->hdop = hdop;
    gnss_location->hour = (int32_t)time / 10000;
    gnss_location->minute = (int32_t)(time - gnss_location->hour * 10000) / 100;
    gnss_location->second = (int32_t)(time - gnss_location->hour * 10000 - gnss_location->minute * 100);
    gnss_location->day = (int32_t)date / 10000;
    gnss_location->month = (int32_t)(date - gnss_location->day * 10000) / 100;
    gnss_location->year = 2000 + (int32_t)(date - gnss_location->day * 10000 - gnss_location->month * 100);
    // convert utc time to time_t format, and save it to timestamp
    utc_time.tm_year = gnss_location->year - 1900;
    utc_time.tm_mon = gnss_location->month - 1;
    utc_time.tm_mday = gnss_location->day;
    utc_time.tm_hour = gnss_location->hour;
    utc_time.tm_min = gnss_location->minute;
    utc_time.tm_sec = gnss_location->second;
    gnss_location->timestamp = mktime(&utc_time);

    g_ec800m_inter_tran_buf_index = sizeof(NET_PORT_GNSS_t);
    log_d(
        "gnss str analysis success, time:%f, latitude:%f, longitude:%f, hdop:%f, altitude:%f, fix_type:%d, cog:%f, "
        "spkm:%f, spkn:%f, date:%f, nsat:%f\r\n",
        (double)time, (double)latitude, (double)longitude, (double)hdop, (double)altitude, fix_type, (double)cog,
        (double)spkm, (double)spkn, (double)date, (double)nsat);
    ret = ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_GNSS_SUCCESS);

    return ret;
}

static int32_t ec800m_device_get_gnss(NET_PORT_GNSS_t *gnss_location)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    char cmd[20] = {0};
    BaseType_t res = pdFAIL;
    uint32_t msg;

    if (gnss_location == NULL) {
        log_e("gnss_location is NULL\r\n");
        return -EINVAL;
    }
    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_GET_GNSS_QGPSLOC_ACK, NULL,
                       ec800m_device_gnss_qgpsloc_str_analysis);
    at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    memset(g_ec800m_inter_tran_buf, 0, sizeof(g_ec800m_inter_tran_buf));
    g_ec800m_inter_tran_buf_index = 0;
    sprintf(cmd, "%s\r\n", EC800M_AT_CMD_GET_GNSS_QGPSLOC);
    at_com_send_str(&g_ec800m_at_com, cmd, strlen(cmd), 2000);  // send AT+QGPSLOC=2
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("ec800m get gnss osMessageGet failed\r\n");
            return -ETIME;
        }
        rx_queue_num--;
        switch (msg) {
            case EC800M_AT_ACK_QUEUE_GNSS_SUCCESS:
                if (g_ec800m_inter_tran_buf_index == sizeof(NET_PORT_GNSS_t)) {
                    memcpy(gnss_location, g_ec800m_inter_tran_buf, sizeof(NET_PORT_GNSS_t));
                    log_d("ec800m_device_get_gnss success\r\n");
                } else {
                    log_e("ec800m gnss buffer size error\r\n");
                    return -EINVAL;
                }
                return 0;
            default:
                break;
        }
    }
    log_d("ec800m_device_get_utc fail\r\n");

    return -ETIME;
}

static int32_t ec800m_device_set_traffic_statistics_recoder_period(uint32_t seconds)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    char cmd[20] = {0};
    BaseType_t res = pdFAIL;
    uint32_t msg;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_ACK_OK, NULL, ec800m_device_ack_ok);
    at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    memset(g_ec800m_inter_tran_buf, 0, sizeof(g_ec800m_inter_tran_buf));
    g_ec800m_inter_tran_buf_index = 0;
    sprintf(cmd, "%s%d\r\n", EC800M_AT_CMD_SET_QAUGDCNT, seconds);
    at_com_send_str(&g_ec800m_at_com, cmd, strlen(cmd), 2000);  // send AT+QAUGDCNT=
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("ec800m set traffic statistics recoder period time out\r\n");
            return -ETIME;
        }
        rx_queue_num--;
        switch (msg) {
            case EC800M_AT_ACK_QUEUE_OK:
                return 0;
            default:
                break;
        }
    }

    return -ETIME;
}

static int32_t ec800m_device_get_traffic_statistics_ack(void *args, char *str)
{
    uint32_t tx_bytes = 0;
    uint32_t rx_bytes = 0;
    int32_t ret = 0;


    if (str == NULL) {
        log_e("str is NULL\r\n");
        return -EINVAL;
    }

    ret = sscanf(str, "+QGDCNT: %d,%d", &tx_bytes, &rx_bytes);
    if (ret != 2) {
        log_e("traffic statistics str analysis failed, ret:%d (%s)\r\n", ret, str);
        return -EINVAL;
    }
    *(uint32_t *)&g_ec800m_inter_tran_buf[0] = tx_bytes;
    *(uint32_t *)&g_ec800m_inter_tran_buf[4] = rx_bytes;
    g_ec800m_inter_tran_buf_index = 8;
    log_d("traffic statistics str analysis success, tx_bytes:%d, rx_bytes:%d\r\n", tx_bytes, rx_bytes);
    ret = ec800m_device_ack_message_send(EC800M_AT_ACK_QUEUE_OK);

    return ret;
}

static int32_t ec800m_device_get_traffic_statistics(uint32_t *tx_rx_bytes)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    char cmd[20] = {0};
    BaseType_t res = pdFAIL;
    uint32_t msg;
    uint32_t tx_bytes = 0;
    uint32_t rx_bytes = 0;

    if (tx_rx_bytes == NULL) {
        log_e("tx_rx_bytes is NULL\r\n");
        return -EINVAL;
    }
    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_CMD_GET_QGDCNT_ACK, NULL, ec800m_device_get_traffic_statistics_ack);
    at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    memset(g_ec800m_inter_tran_buf, 0, sizeof(g_ec800m_inter_tran_buf));
    g_ec800m_inter_tran_buf_index = 0;
    sprintf(cmd, "%s\r\n", EC800M_AT_CMD_GET_QGDCNT);
    at_com_send_str(&g_ec800m_at_com, cmd, strlen(cmd), 2000);  // send AT+QGDCNT?
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("ec800m get traffic statistics time out\r\n");
            return -ETIME;
        }
        rx_queue_num--;
        switch (msg) {
            case EC800M_AT_ACK_QUEUE_OK:
                if (g_ec800m_inter_tran_buf_index != 8) {
                    log_e("ec800m get traffic statistics buffer size error\r\n");
                    return -EINVAL;
                }
                tx_bytes = *(uint32_t *)(&g_ec800m_inter_tran_buf[0]);
                rx_bytes = *(uint32_t *)(&g_ec800m_inter_tran_buf[4]);
                *tx_rx_bytes = tx_bytes + rx_bytes;
                return 0;
            default:
                break;
        }
    }

    return -ETIME;
}

static int32_t ec800m_device_clr_traffic_statistics(void)
{
    AT_CMP_STR_NODE_t at_str_ack[2] = {0};
    int8_t rx_queue_num = 0;
    char cmd[20] = {0};
    BaseType_t res = pdFAIL;
    uint32_t msg;

    at_com_clr_cmp_str(&g_ec800m_at_com);
    at_com_set_cmp_str(&at_str_ack[0], EC800M_AT_ACK_OK, NULL, ec800m_device_ack_ok);
    at_com_add_cmp_str(&g_ec800m_at_com, &at_str_ack[0]);
    memset(g_ec800m_inter_tran_buf, 0, sizeof(g_ec800m_inter_tran_buf));
    g_ec800m_inter_tran_buf_index = 0;
    sprintf(cmd, "%s\r\n", EC800M_AT_CMD_SET_QGDCNT_0);
    at_com_send_str(&g_ec800m_at_com, cmd, strlen(cmd), 2000);  // send AT+QGDCNT?
    rx_queue_num = 1;
    while (rx_queue_num > 0) {
        res = xQueueReceive(g_ec800m_rx_queue_handle, &msg, 2000);
        if (res != pdPASS) {
            log_e("ec800m get traffic statistics time out\r\n");
            return -ETIME;
        }
        rx_queue_num--;
        switch (msg) {
            case EC800M_AT_ACK_QUEUE_OK:
                return 0;
            default:
                break;
        }
    }

    return -ETIME;
}

DRIVER_REGISTER(&g_ec800m_driver, ec800m)
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
