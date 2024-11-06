/*
 * @Author: your name
 * @Date: 2024-10-25 14:20:51
 * @LastEditTime: 2024-11-05 11:39:13
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\drv_usart.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "drv_usart.h"

#include "driver_com.h"
#include "error_code.h"
#include "ring_buffer.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef void (*UART_INIT_FUN)(void);
typedef void (*UART_CALL_BACK_FUN)(void *arg);

typedef struct
{
    uint8_t usart_index;  // 0:USART1, 1:USART2, 2:USART3, 3:USART4, 4:USART5, 5:USART6, 6:USART7, 7:USART8
    uint32_t rx_buf_size;
    uint8_t *rx_buffer;
    RINGBUFF_T rx_ring_buff;
    uint8_t rx_byte;
    UART_CALL_BACK_FUN rx_it_call_back;
    void *rx_it_call_back_arg;
    UART_CALL_BACK_FUN tx_it_call_back;
    void *tx_it_call_back_arg;
    UART_HandleTypeDef *ctl_uart;
} DRV_USART_OBJ_t;

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */
uint8_t g_usart_1_rx_buffer[DRV_USART_1_RX_BUF_SIZE + 1];  // array size can't be 0
uint8_t g_usart_2_rx_buffer[DRV_USART_2_RX_BUF_SIZE + 1];
uint8_t g_usart_3_rx_buffer[DRV_USART_3_RX_BUF_SIZE + 1];
uint8_t g_usart_4_rx_buffer[DRV_USART_4_RX_BUF_SIZE + 1];
uint8_t g_usart_5_rx_buffer[DRV_USART_5_RX_BUF_SIZE + 1];
uint8_t g_usart_6_rx_buffer[DRV_USART_6_RX_BUF_SIZE + 1];

/*
 * ****************************************************************************
 * ******** Private macro                                              ********
 * ****************************************************************************
 */
/* deifne the uart name */
#define DRIVER_CONFIG_UART(num, open_flg)           ((open_flg) ? "usart" #num : "\0")

#define USART_1_NAME                                DRIVER_CONFIG_UART(1, DRV_USAER_1_OPEN)
#define USART_2_NAME                                DRIVER_CONFIG_UART(2, DRV_USAER_2_OPEN)
#define USART_3_NAME                                DRIVER_CONFIG_UART(3, DRV_USAER_3_OPEN)
#define USART_4_NAME                                DRIVER_CONFIG_UART(4, DRV_USAER_4_OPEN)
#define USART_5_NAME                                DRIVER_CONFIG_UART(5, DRV_USAER_5_OPEN)
#define USART_6_NAME                                DRIVER_CONFIG_UART(6, DRV_USAER_6_OPEN)

/* define the uart rx buffer */
#define DRIVER_CONFIG_UART_RX_BUFFER(num, open_flg) ((open_flg) ? g_usart_##num##_rx_buffer : NULL)

#define USART_1_RX_BUFFER                           DRIVER_CONFIG_UART_RX_BUFFER(1, DRV_USAER_1_OPEN)
#define USART_2_RX_BUFFER                           DRIVER_CONFIG_UART_RX_BUFFER(2, DRV_USAER_2_OPEN)
#define USART_3_RX_BUFFER                           DRIVER_CONFIG_UART_RX_BUFFER(3, DRV_USAER_3_OPEN)
#define USART_4_RX_BUFFER                           DRIVER_CONFIG_UART_RX_BUFFER(4, DRV_USAER_4_OPEN)
#define USART_5_RX_BUFFER                           DRIVER_CONFIG_UART_RX_BUFFER(5, DRV_USAER_5_OPEN)
#define USART_6_RX_BUFFER                           DRIVER_CONFIG_UART_RX_BUFFER(6, DRV_USAER_6_OPEN)

/*
 * ****************************************************************************
 * ******** Private global variables                                   ********
 * ****************************************************************************
 */
DRV_USART_OBJ_t g_usart_obj[DRV_USART_NUM_MAX] = {0};

char *g_usart_name[DRV_USART_NUM_MAX] = {USART_1_NAME, USART_2_NAME, USART_3_NAME,
                                         USART_4_NAME, USART_5_NAME, USART_6_NAME};

uint8_t *g_usart_rx_buffer[DRV_USART_NUM_MAX] = {USART_1_RX_BUFFER, USART_2_RX_BUFFER, USART_3_RX_BUFFER,
                                                 USART_4_RX_BUFFER, USART_5_RX_BUFFER, USART_6_RX_BUFFER};

uint32_t g_usart_rx_buf_size[DRV_USART_NUM_MAX] = {DRV_USART_1_RX_BUF_SIZE, DRV_USART_2_RX_BUF_SIZE,
                                                   DRV_USART_3_RX_BUF_SIZE, DRV_USART_4_RX_BUF_SIZE,
                                                   DRV_USART_5_RX_BUF_SIZE, DRV_USART_6_RX_BUF_SIZE};

UART_HandleTypeDef *g_usart_handle[DRV_USART_NUM_MAX] = {USART_1_HANDLE, USART_2_HANDLE, USART_3_HANDLE,
                                                         USART_4_HANDLE, USART_5_HANDLE, USART_6_HANDLE};

UART_INIT_FUN g_usart_init_fun[DRV_USART_NUM_MAX] = {USART_1_INIT, USART_2_INIT, USART_3_INIT,
                                                     USART_4_INIT, USART_5_INIT, USART_6_INIT};

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */

static int32_t drv_usart_init(DRIVER_OBJ_t *drv);
static int32_t drv_usart_deinit(DRIVER_OBJ_t *drv);
static int32_t drv_usart_open(DRIVER_OBJ_t *drv, uint32_t oflag);
static int32_t drv_usart_close(DRIVER_OBJ_t *drv);
static int32_t drv_usart_read(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size);
static int32_t drv_usart_write(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size);
static int32_t drv_usart_control(DRIVER_OBJ_t *drv, uint32_t cmd, void *args);
void uart_it_rx_finish_callback(UART_HandleTypeDef *huart);
void uart_it_tx_finish_callback(UART_HandleTypeDef *huart);

DRIVER_CTL_t g_usart_ctl[DRV_USART_NUM_MAX] = {
    {
        .init = drv_usart_init,
        .deinit = drv_usart_deinit,
    },
    {
        .init = drv_usart_init,
        .deinit = drv_usart_deinit,
    },
    {
        .init = drv_usart_init,
        .deinit = drv_usart_deinit,
    },
    {
        .init = drv_usart_init,
        .deinit = drv_usart_deinit,
    },
    {
        .init = drv_usart_init,
        .deinit = drv_usart_deinit,
    },
    {
        .init = drv_usart_init,
        .deinit = drv_usart_deinit,
    },
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
static int32_t drv_usart_init(DRIVER_OBJ_t *drv)
{
    int8_t i = 0;
    DRV_USART_OBJ_t *usart_obj = NULL;

    for (i = 0; i < DRV_USART_NUM_MAX; i++) {
        if (memcmp(drv->name, g_usart_name[i], strlen(g_usart_name[i])) != 0) {
            continue;
        }
        usart_obj = &g_usart_obj[i];
        memset(usart_obj, 0, sizeof(DRV_USART_OBJ_t));
        usart_obj->usart_index = i;
        usart_obj->rx_buf_size = g_usart_rx_buf_size[i];
        usart_obj->rx_buffer = g_usart_rx_buffer[i];
        usart_obj->ctl_uart = g_usart_handle[i];
        RingBuffer_Init(&usart_obj->rx_ring_buff, usart_obj->rx_buffer, sizeof(uint8_t), usart_obj->rx_buf_size);
        drv->driver->drv_priv = (void *)usart_obj;
        drv->driver->open = drv_usart_open;
        drv->driver->close = drv_usart_close;
        drv->driver->read = drv_usart_read;
        drv->driver->write = drv_usart_write;
        drv->driver->control = drv_usart_control;
        if (g_usart_init_fun[i] != NULL) {
            g_usart_init_fun[i]();
        }
        driver_set_inted(drv);
        return 0;
    }

    return -ENODEV;
}

static int32_t drv_usart_deinit(DRIVER_OBJ_t *drv)
{
    int8_t i = 0;
    DRV_USART_OBJ_t *usart_obj = NULL;

    for (i = 0; i < DRV_USART_NUM_MAX; i++) {
        if (memcmp(drv->name, g_usart_name[i], strlen(g_usart_name[i])) != 0) {
            continue;
        }
        usart_obj = &g_usart_obj[i];
        memset(usart_obj, 0, sizeof(DRV_USART_OBJ_t));
    }

    return 0;
}

static int32_t drv_usart_open(DRIVER_OBJ_t *drv, uint32_t oflag)
{
    DRV_USART_OBJ_t *usart_obj = (DRV_USART_OBJ_t *)drv->driver->drv_priv;

    HAL_UART_RegisterCallback(usart_obj->ctl_uart, HAL_UART_TX_COMPLETE_CB_ID, uart_it_tx_finish_callback);
    HAL_UART_RegisterCallback(usart_obj->ctl_uart, HAL_UART_RX_COMPLETE_CB_ID, uart_it_rx_finish_callback);
    HAL_UART_Receive_IT(usart_obj->ctl_uart, &usart_obj->rx_byte, 1);
    driver_set_opened(drv);

    return 0;
}

void uart_it_rx_finish_callback(UART_HandleTypeDef *huart)
{
    DRV_USART_OBJ_t *usart_obj = NULL;
    int8_t i = 0;

    for (i = 0; i < DRV_USART_NUM_MAX; i++) {
        if (g_usart_obj[i].ctl_uart == huart) {
            usart_obj = &g_usart_obj[i];
            break;
        }
    }
    RingBuffer_Insert(&usart_obj->rx_ring_buff, &usart_obj->rx_byte);
    if (usart_obj->rx_it_call_back != NULL) {
        usart_obj->rx_it_call_back(usart_obj->rx_it_call_back_arg);
    }
    HAL_UART_Receive_IT(usart_obj->ctl_uart, &usart_obj->rx_byte, 1);
}

void uart_it_tx_finish_callback(UART_HandleTypeDef *huart)
{
    DRV_USART_OBJ_t *usart_obj = NULL;
    DRIVER_OBJ_t *drv = NULL;

    int8_t i = 0;

    for (i = 0; i < DRV_USART_NUM_MAX; i++) {
        if (g_usart_obj[i].ctl_uart == huart) {
            usart_obj = &g_usart_obj[i];
            drv = g_usart_ctl[i].drv_obj;
            break;
        }
    }
    if (usart_obj != NULL && usart_obj->tx_it_call_back != NULL) {
        usart_obj->tx_it_call_back(usart_obj->tx_it_call_back_arg);
    }
    driver_clear_writing(drv);
}

static int32_t drv_usart_close(DRIVER_OBJ_t *drv)
{
    DRV_USART_OBJ_t *usart_obj = (DRV_USART_OBJ_t *)drv->driver->drv_priv;

    if (usart_obj == NULL || usart_obj->ctl_uart == NULL) {
        return -EACCES;
    }
    HAL_UART_AbortReceive_IT(usart_obj->ctl_uart);
    driver_clear_opened(drv);

    return 0;
}

/**
 * @brief driver read function, read data from ring buffer.
 * @param drv
 * @param pos
 * @param buffer
 * @param size
 * @return int32_t    0:no data, >0:data size read.
 */
static int32_t drv_usart_read(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size)
{
    if (!driver_is_opened(drv)) {
        return -EACCES;
    }
    driver_set_reading(drv);
    DRV_USART_OBJ_t *usart_obj = (DRV_USART_OBJ_t *)drv->driver->drv_priv;
    int32_t data_size = RingBuffer_GetCount(&usart_obj->rx_ring_buff);

    if (data_size <= 0) {
        driver_clear_reading(drv);
        return 0;
    }
    data_size = (data_size > size) ? size : data_size;
    RingBuffer_PopMult(&usart_obj->rx_ring_buff, buffer, data_size);
    driver_clear_reading(drv);

    return data_size;
}

/**
 * @brief driver write function, write data to USART use interrupt.
 * @param drv
 * @param pos
 * @param buffer
 * @param size
 * @return int32_t
 */
static int32_t drv_usart_write(DRIVER_OBJ_t *drv, uint32_t pos, void *buffer, uint32_t size)
{
    if (!driver_is_opened(drv)) {
        return -EACCES;
    }

    DRV_USART_OBJ_t *usart_obj = (DRV_USART_OBJ_t *)drv->driver->drv_priv;
    HAL_UART_Transmit_IT(usart_obj->ctl_uart, (uint8_t *)buffer, size);
    driver_set_writing(drv);

    return size;
}

static int32_t drv_usart_control(DRIVER_OBJ_t *drv, uint32_t cmd, void *args)
{
    int32_t ret = 0;

    if (!driver_is_opened(drv)) {
        return -EACCES;
    }

    DRV_USART_OBJ_t *usart_obj = (DRV_USART_OBJ_t *)drv->driver->drv_priv;

    switch (cmd) {
        case DRV_USART_CMD_SET_RX_IT_DONE_CALLBACK:
            usart_obj->rx_it_call_back = (UART_CALL_BACK_FUN)args;
            break;
        case DRV_USART_CMD_SET_RX_IT_DONE_CALLBACK_ARG:
            usart_obj->rx_it_call_back_arg = args;
            break;
        case DRV_USART_CMD_SET_TX_IT_DONE_CALLBACK:
            usart_obj->tx_it_call_back = (UART_CALL_BACK_FUN)args;
            break;
        case DRV_USART_CMD_SET_TX_IT_DONE_CALLBACK_ARG:
            usart_obj->tx_it_call_back_arg = args;
            break;
        case DRV_USART_CMD_GET_RX_DATA_SIZE:
            ret = RingBuffer_GetCount(&usart_obj->rx_ring_buff);
            break;
        default:
            return -EINVAL;
    }

    return ret;
}

/**
 * @brief This function handles USART1 global driver. If want to use USART1, you should define DRV_USAER_1_OPEN in your
 * config file. Then get the driver object by using get_driver("usart1") in your code.
 */
#if DRV_USAER_1_OPEN
DRIVER_REGISTER(&g_usart_ctl[0], usart1)
#endif

#if DRV_USAER_2_OPEN
DRIVER_REGISTER(&g_usart_ctl[1], usart2)
#endif

#if DRV_USAER_3_OPEN
DRIVER_REGISTER(&g_usart_ctl[2], usart3)
#endif

#if DRV_USAER_4_OPEN
DRIVER_REGISTER(&g_usart_ctl[3], usart4)
#endif

#if DRV_USAER_5_OPEN
DRIVER_REGISTER(&g_usart_ctl[4], usart5)
#endif

#if DRV_USAER_6_OPEN
DRIVER_REGISTER(&g_usart_ctl[5], usart6)
#endif

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
