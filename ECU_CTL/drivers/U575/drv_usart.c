/*
 * @Author: your name
 * @Date: 2024-10-25 14:20:51
 * @LastEditTime: 2025-01-22 19:34:05
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

#include <FreeRTOS.h>
#include <semphr.h>
#include <util.h>

#include "driver_com.h"
#include "error_code.h"
#include "fault.h"
#include "ring_buffer.h"
#include "user_os.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef void (*UART_INIT_FUN)(void);
typedef void (*UART_CALL_BACK_FUN)(void *arg);

typedef struct
{
    StaticSemaphore_t xSemaphore;
    SemaphoreHandle_t xSemHandle;
} DRV_USART_SEM_OBJ_t;

typedef struct
{
    uint8_t usart_index;  // 0:USART1, 1:USART2, 2:USART3, 3:USART4, 4:USART5, 5:USART6
    uint32_t rx_buf_size;
    uint8_t *rx_buffer;
    RINGBUFF_T rx_ring_buff;
    uint8_t rx_byte;
    uint32_t tx_buf_size;
    uint8_t *tx_buffer;
    RINGBUFF_T tx_ring_buff;
    uint32_t tx_dma_buf_size;
    uint8_t *tx_dma_buffer;
    uint8_t tx_dma_flg;
    DRV_USART_SEM_OBJ_t tx_sem;
    DRV_USART_SEM_OBJ_t tx_start_sem;
    DRV_USART_SEM_OBJ_t rx_sem;
    UART_CALL_BACK_FUN rx_it_call_back;
    void *rx_it_call_back_arg;
    UART_CALL_BACK_FUN tx_it_call_back;
    void *tx_it_call_back_arg;
    UART_HandleTypeDef *ctl_uart;
    USER_THREAD_OBJ_t *tx_thread_def;
} DRV_USART_OBJ_t;

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */
static uint8_t g_usart_1_rx_buffer[DRV_USART_1_RX_BUF_SIZE + 1];  // array size can't be 0
static uint8_t g_usart_2_rx_buffer[DRV_USART_2_RX_BUF_SIZE + 1];
static uint8_t g_usart_3_rx_buffer[DRV_USART_3_RX_BUF_SIZE + 1];
static uint8_t g_usart_4_rx_buffer[DRV_USART_4_RX_BUF_SIZE + 1];
static uint8_t g_usart_5_rx_buffer[DRV_USART_5_RX_BUF_SIZE + 1];
static uint8_t g_usart_6_rx_buffer[DRV_USART_6_RX_BUF_SIZE + 1];

static uint8_t g_usart_1_tx_buffer[DRV_USART_1_TX_BUF_SIZE + 1];  // array size can't be 0
static uint8_t g_usart_2_tx_buffer[DRV_USART_2_TX_BUF_SIZE + 1];
static uint8_t g_usart_3_tx_buffer[DRV_USART_3_TX_BUF_SIZE + 1];
static uint8_t g_usart_4_tx_buffer[DRV_USART_4_TX_BUF_SIZE + 1];
static uint8_t g_usart_5_tx_buffer[DRV_USART_5_TX_BUF_SIZE + 1];
static uint8_t g_usart_6_tx_buffer[DRV_USART_6_TX_BUF_SIZE + 1];

static uint8_t g_usart_1_tx_dma_buffer[DRV_USART_1_TX_DMA_BUF_SIZE + 1];  // array size can't be 0
static uint8_t g_usart_2_tx_dma_buffer[DRV_USART_2_TX_DMA_BUF_SIZE + 1];
static uint8_t g_usart_3_tx_dma_buffer[DRV_USART_3_TX_DMA_BUF_SIZE + 1];
static uint8_t g_usart_4_tx_dma_buffer[DRV_USART_4_TX_DMA_BUF_SIZE + 1];
static uint8_t g_usart_5_tx_dma_buffer[DRV_USART_5_TX_DMA_BUF_SIZE + 1];
static uint8_t g_usart_6_tx_dma_buffer[DRV_USART_6_TX_DMA_BUF_SIZE + 1];

/*
 * ****************************************************************************
 * ******** Private macro                                              ********
 * ****************************************************************************
 */
/* deifne the uart name */
#define DRIVER_CONFIG_UART(num, open_flg)               ((open_flg) ? "usart" #num : "\0")

#define USART_1_NAME                                    DRIVER_CONFIG_UART(1, DRV_USAER_1_OPEN)
#define USART_2_NAME                                    DRIVER_CONFIG_UART(2, DRV_USAER_2_OPEN)
#define USART_3_NAME                                    DRIVER_CONFIG_UART(3, DRV_USAER_3_OPEN)
#define USART_4_NAME                                    DRIVER_CONFIG_UART(4, DRV_USAER_4_OPEN)
#define USART_5_NAME                                    DRIVER_CONFIG_UART(5, DRV_USAER_5_OPEN)
#define USART_6_NAME                                    DRIVER_CONFIG_UART(6, DRV_USAER_6_OPEN)

/* define the uart rx buffer */
#define DRIVER_CONFIG_UART_RX_BUFFER(num, open_flg)     ((open_flg) ? g_usart_##num##_rx_buffer : NULL)

#define USART_1_RX_BUFFER                               DRIVER_CONFIG_UART_RX_BUFFER(1, DRV_USAER_1_OPEN)
#define USART_2_RX_BUFFER                               DRIVER_CONFIG_UART_RX_BUFFER(2, DRV_USAER_2_OPEN)
#define USART_3_RX_BUFFER                               DRIVER_CONFIG_UART_RX_BUFFER(3, DRV_USAER_3_OPEN)
#define USART_4_RX_BUFFER                               DRIVER_CONFIG_UART_RX_BUFFER(4, DRV_USAER_4_OPEN)
#define USART_5_RX_BUFFER                               DRIVER_CONFIG_UART_RX_BUFFER(5, DRV_USAER_5_OPEN)
#define USART_6_RX_BUFFER                               DRIVER_CONFIG_UART_RX_BUFFER(6, DRV_USAER_6_OPEN)

/* define the uart tx buffer */
#define DRIVER_CONFIG_UART_TX_BUFFER(num, open_flg)     ((open_flg) ? g_usart_##num##_tx_buffer : NULL)

#define USART_1_TX_BUFFER                               DRIVER_CONFIG_UART_TX_BUFFER(1, DRV_USAER_1_OPEN)
#define USART_2_TX_BUFFER                               DRIVER_CONFIG_UART_TX_BUFFER(2, DRV_USAER_2_OPEN)
#define USART_3_TX_BUFFER                               DRIVER_CONFIG_UART_TX_BUFFER(3, DRV_USAER_3_OPEN)
#define USART_4_TX_BUFFER                               DRIVER_CONFIG_UART_TX_BUFFER(4, DRV_USAER_4_OPEN)
#define USART_5_TX_BUFFER                               DRIVER_CONFIG_UART_TX_BUFFER(5, DRV_USAER_5_OPEN)
#define USART_6_TX_BUFFER                               DRIVER_CONFIG_UART_TX_BUFFER(6, DRV_USAER_6_OPEN)

/* define the uart tx dma buffer */
#define DRIVER_CONFIG_UART_TX_DMA_BUFFER(num, open_flg) ((open_flg) ? g_usart_##num##_tx_dma_buffer : NULL)

#define USART_1_TX_DMA_BUFFER                           DRIVER_CONFIG_UART_TX_DMA_BUFFER(1, DRV_USAER_1_OPEN)
#define USART_2_TX_DMA_BUFFER                           DRIVER_CONFIG_UART_TX_DMA_BUFFER(2, DRV_USAER_2_OPEN)
#define USART_3_TX_DMA_BUFFER                           DRIVER_CONFIG_UART_TX_DMA_BUFFER(3, DRV_USAER_3_OPEN)
#define USART_4_TX_DMA_BUFFER                           DRIVER_CONFIG_UART_TX_DMA_BUFFER(4, DRV_USAER_4_OPEN)
#define USART_5_TX_DMA_BUFFER                           DRIVER_CONFIG_UART_TX_DMA_BUFFER(5, DRV_USAER_5_OPEN)
#define USART_6_TX_DMA_BUFFER                           DRIVER_CONFIG_UART_TX_DMA_BUFFER(6, DRV_USAER_6_OPEN)

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

uint8_t *g_usart_tx_buffer[DRV_USART_NUM_MAX] = {USART_1_TX_BUFFER, USART_2_TX_BUFFER, USART_3_TX_BUFFER,
                                                 USART_4_TX_BUFFER, USART_5_TX_BUFFER, USART_6_TX_BUFFER};
uint32_t g_usart_tx_buf_size[DRV_USART_NUM_MAX] = {DRV_USART_1_TX_BUF_SIZE, DRV_USART_2_TX_BUF_SIZE,
                                                   DRV_USART_3_TX_BUF_SIZE, DRV_USART_4_TX_BUF_SIZE,
                                                   DRV_USART_5_TX_BUF_SIZE, DRV_USART_6_TX_BUF_SIZE};

uint8_t *g_usart_tx_dma_buffer[DRV_USART_NUM_MAX] = {USART_1_TX_DMA_BUFFER, USART_2_TX_DMA_BUFFER,
                                                     USART_3_TX_DMA_BUFFER, USART_4_TX_DMA_BUFFER,
                                                     USART_5_TX_DMA_BUFFER, USART_6_TX_DMA_BUFFER};
uint32_t g_usart_tx_dma_buf_size[DRV_USART_NUM_MAX] = {DRV_USART_1_TX_DMA_BUF_SIZE, DRV_USART_2_TX_DMA_BUF_SIZE,
                                                       DRV_USART_3_TX_DMA_BUF_SIZE, DRV_USART_4_TX_DMA_BUF_SIZE,
                                                       DRV_USART_5_TX_DMA_BUF_SIZE, DRV_USART_6_TX_DMA_BUF_SIZE};
uint32_t g_usart_tx_dma_flg[DRV_USART_NUM_MAX] = {DRV_USART_1_TX_DMA_FLG, DRV_USART_2_TX_DMA_FLG,
                                                  DRV_USART_3_TX_DMA_FLG, DRV_USART_4_TX_DMA_FLG,
                                                  DRV_USART_5_TX_DMA_FLG, DRV_USART_6_TX_DMA_FLG};

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
static void usart_dma_tx_task(void const *argument);

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

USER_THREAD_OBJ_t g_usart_tx_thread[DRV_USART_NUM_MAX] = {
    USER_THREAD_OBJ_INIT(usart_dma_tx_task, "usart1_dma_tx", 512, NULL, RTOS_PRIORITY_NORMAL),
    USER_THREAD_OBJ_INIT(usart_dma_tx_task, "usart2_dma_tx", 512, NULL, RTOS_PRIORITY_NORMAL),
    USER_THREAD_OBJ_INIT(usart_dma_tx_task, "usart3_dma_tx", 512, NULL, RTOS_PRIORITY_NORMAL),
    USER_THREAD_OBJ_INIT(usart_dma_tx_task, "usart4_dma_tx", 512, NULL, RTOS_PRIORITY_NORMAL),
    USER_THREAD_OBJ_INIT(usart_dma_tx_task, "usart5_dma_tx", 512, NULL, RTOS_PRIORITY_NORMAL),
    USER_THREAD_OBJ_INIT(usart_dma_tx_task, "usart6_dma_tx", 512, NULL, RTOS_PRIORITY_NORMAL),
};

/* define the uart tx dma rhread */
#define DRIVER_CONFIG_UART_TX_DMA_THREAD(thread, open_flg) ((open_flg) ? thread : NULL)
#define USART_1_TX_DMA_THREAD                              DRIVER_CONFIG_UART_TX_DMA_THREAD(&g_usart_tx_thread[0], DRV_USAER_1_OPEN)
#define USART_2_TX_DMA_THREAD                              DRIVER_CONFIG_UART_TX_DMA_THREAD(&g_usart_tx_thread[1], DRV_USAER_2_OPEN)
#define USART_3_TX_DMA_THREAD                              DRIVER_CONFIG_UART_TX_DMA_THREAD(&g_usart_tx_thread[2], DRV_USAER_3_OPEN)
#define USART_4_TX_DMA_THREAD                              DRIVER_CONFIG_UART_TX_DMA_THREAD(&g_usart_tx_thread[3], DRV_USAER_4_OPEN)
#define USART_5_TX_DMA_THREAD                              DRIVER_CONFIG_UART_TX_DMA_THREAD(&g_usart_tx_thread[4], DRV_USAER_5_OPEN)
#define USART_6_TX_DMA_THREAD                              DRIVER_CONFIG_UART_TX_DMA_THREAD(&g_usart_tx_thread[5], DRV_USAER_6_OPEN)

USER_THREAD_OBJ_t *g_usart_tx_thread_def[DRV_USART_NUM_MAX] = {USART_1_TX_DMA_THREAD, USART_2_TX_DMA_THREAD,
                                                               USART_3_TX_DMA_THREAD, USART_4_TX_DMA_THREAD,
                                                               USART_5_TX_DMA_THREAD, USART_6_TX_DMA_THREAD};

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
        // usart_obj init
        usart_obj = &g_usart_obj[i];
        memset(usart_obj, 0, sizeof(DRV_USART_OBJ_t));
        usart_obj->usart_index = i;
        usart_obj->rx_buf_size = g_usart_rx_buf_size[i];
        usart_obj->rx_buffer = g_usart_rx_buffer[i];
        usart_obj->ctl_uart = g_usart_handle[i];
        RingBuffer_Init(&usart_obj->rx_ring_buff, usart_obj->rx_buffer, sizeof(uint8_t), usart_obj->rx_buf_size);
        usart_obj->tx_buf_size = g_usart_tx_buf_size[i];
        usart_obj->tx_buffer = g_usart_tx_buffer[i];
        RingBuffer_Init(&usart_obj->tx_ring_buff, usart_obj->tx_buffer, sizeof(uint8_t), usart_obj->tx_buf_size);
        usart_obj->tx_dma_buf_size = g_usart_tx_dma_buf_size[i];
        usart_obj->tx_dma_buffer = g_usart_tx_dma_buffer[i];
        usart_obj->tx_dma_flg = g_usart_tx_dma_flg[i];
        usart_obj->tx_sem.xSemHandle = xSemaphoreCreateBinaryStatic(&usart_obj->tx_sem.xSemaphore);
        fault_assert(usart_obj->tx_sem.xSemHandle != NULL, FAULT_CODE_CONSOLE);
        usart_obj->tx_start_sem.xSemHandle = xSemaphoreCreateBinaryStatic(&usart_obj->tx_start_sem.xSemaphore);
        fault_assert(usart_obj->tx_start_sem.xSemHandle != NULL, FAULT_CODE_CONSOLE);
        usart_obj->rx_sem.xSemHandle = xSemaphoreCreateBinaryStatic(&usart_obj->rx_sem.xSemaphore);
        fault_assert(usart_obj->rx_sem.xSemHandle != NULL, FAULT_CODE_CONSOLE);
        // create tx thread
        usart_obj->tx_thread_def = g_usart_tx_thread_def[i];
        if (usart_obj->tx_thread_def != NULL) {
            usart_obj->tx_thread_def->parameter = (void *)usart_obj;
            USER_THREAD_OBJ_t *thread = usart_obj->tx_thread_def;
            xTaskCreate((TaskFunction_t)thread->thread, thread->name, thread->stack_size, thread->parameter,
                        thread->priority, &thread->thread_handle);
            fault_assert(thread->thread_handle != NULL, FAULT_CODE_CONSOLE);
        }

        // drv function init
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
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    for (i = 0; i < DRV_USART_NUM_MAX; i++) {
        if (g_usart_obj[i].ctl_uart == huart) {
            usart_obj = &g_usart_obj[i];
            break;
        }
    }
    if (usart_obj == NULL) {
        return;
    }
    xSemaphoreGiveFromISR(usart_obj->rx_sem.xSemHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
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
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    for (i = 0; i < DRV_USART_NUM_MAX; i++) {
        if (g_usart_obj[i].ctl_uart == huart) {
            usart_obj = &g_usart_obj[i];
            drv = g_usart_ctl[i].drv_obj;
            break;
        }
    }
    if (usart_obj == NULL || drv == NULL) {
        return;
    }
    driver_clear_writing(drv);
    xSemaphoreGiveFromISR(usart_obj->tx_sem.xSemHandle, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    if (usart_obj != NULL && usart_obj->tx_it_call_back != NULL) {
        usart_obj->tx_it_call_back(usart_obj->tx_it_call_back_arg);
    }
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
    uint32_t time_out = 0;
    BaseType_t ret = 0;
    int32_t data_size = 0;

    if (!driver_is_opened(drv)) {
        return -EACCES;
    }
    if (pos == DRV_USAER_POS_BLOCKING) {
        time_out = HAL_MAX_DELAY;
    } else if (pos == DRV_USAER_POS_BLOCKING_1000) {
        time_out = 1000;
    }
    DRV_USART_OBJ_t *usart_obj = (DRV_USART_OBJ_t *)drv->driver->drv_priv;
    driver_set_reading(drv);
    data_size = RingBuffer_GetCount(&usart_obj->rx_ring_buff);

    if (time_out != 0 && data_size <= 0) {
        ret = xSemaphoreTake(usart_obj->rx_sem.xSemHandle, time_out);
        if (ret != pdTRUE) {
            return -ETIME;
        }
        data_size = RingBuffer_GetCount(&usart_obj->rx_ring_buff);
    }
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
    uint32_t time_out = 0;
    BaseType_t ret = 0;
    int count = 0;
    int i = 0;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint8_t *p_buffer = (uint8_t *)buffer;

    if (!driver_is_opened(drv)) {
        return -EACCES;
    }
    if (pos == DRV_USAER_POS_BLOCKING) {
        time_out = HAL_MAX_DELAY;
    } else if (pos == DRV_USAER_POS_BLOCKING_1000) {
        time_out = 1000;
    }

    DRV_USART_OBJ_t *usart_obj = (DRV_USART_OBJ_t *)drv->driver->drv_priv;
    if (usart_obj->tx_dma_flg == 0) {
        HAL_UART_Transmit_IT(usart_obj->ctl_uart, (uint8_t *)buffer, size);
        driver_set_writing(drv);
        if (time_out != 0) {
            ret = xSemaphoreTake(usart_obj->tx_sem.xSemHandle, time_out);
            if (ret != pdTRUE) {
                driver_clear_writing(drv);
                return -ETIME;
            }
        }
    } else {
        for (i = 0; i < size;) {
            count = RingBuffer_GetFree(&usart_obj->tx_ring_buff);
            count = count > size - i ? size - i : count;
            if (count > 0) {
                RingBuffer_InsertMult(&usart_obj->tx_ring_buff, &p_buffer[i], count);
                i += count;
                xSemaphoreGiveFromISR(usart_obj->tx_start_sem.xSemHandle, &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            } else {
                if (time_out <= 0) {
                    return -ETIME;
                }
                vTaskDelay(10);
                time_out -= 10;
            }
        }
    }

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

static void usart_dma_tx_task(void const *argument)
{
    int count = 0;
    int send_count = 0;
    DRV_USART_OBJ_t *usart_obj = (DRV_USART_OBJ_t *)argument;

    if (usart_obj->usart_index > DRV_USART_NUM_MAX) {
        vTaskSuspend(NULL);
    }

    while (1) {
        count = RingBuffer_GetCount(&usart_obj->tx_ring_buff);
        if (count == 0) {
            xSemaphoreTake(usart_obj->tx_start_sem.xSemHandle, 1000);
            count = RingBuffer_GetCount(&usart_obj->tx_ring_buff);
        }
        if (count > 0) {
            send_count = count > usart_obj->tx_dma_buf_size ? usart_obj->tx_dma_buf_size : count;
            RingBuffer_PopMult(&usart_obj->tx_ring_buff, usart_obj->tx_dma_buffer, send_count);
            HAL_UART_Transmit_DMA(usart_obj->ctl_uart, usart_obj->tx_dma_buffer, send_count);
            xSemaphoreTake(usart_obj->tx_sem.xSemHandle, OS_MS(1000));
        }
    }
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
