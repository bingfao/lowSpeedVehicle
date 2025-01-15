/*
 * @Author: your name
 * @Date: 2024-10-25 14:22:37
 * @LastEditTime: 2025-01-14 16:49:15
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\drv_usart.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __DRV_USART_H
#define __DRV_USART_H
/*
 * ============================================================================
 * If building with a C++ compiler, make all of the definitions in this header
 * have a C binding.
 * ============================================================================
 */
#ifdef __cplusplus
extern "C" {
#endif
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "usart.h"

/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */
#define DRV_USAER_1_OPEN  1
#define DRV_USAER_2_OPEN  1
#define DRV_USAER_3_OPEN  1
#define DRV_USAER_4_OPEN  0
#define DRV_USAER_5_OPEN  0
#define DRV_USAER_6_OPEN  0

#define DRV_USART_NUM_MAX 6

#if DRV_USAER_1_OPEN
#define DRV_USART_1_RX_BUF_SIZE 512
#define DRV_USART_1_TX_BUF_SIZE 512
#define DRV_USART_1_TX_DMA_BUF_SIZE 512
#define USART_1_HANDLE          &huart1
#define USART_1_INIT            MX_USART1_UART_Init
#else
#define DRV_USART_1_RX_BUF_SIZE 0
#define DRV_USART_1_TX_BUF_SIZE 0
#define DRV_USART_1_TX_DMA_BUF_SIZE 0
#define USART_1_HANDLE          NULL
#define USART_1_INIT            NULL
#endif
#if DRV_USAER_2_OPEN
#define DRV_USART_2_RX_BUF_SIZE 2048
#define DRV_USART_2_TX_BUF_SIZE 512
#define DRV_USART_2_TX_DMA_BUF_SIZE 512
#define USART_2_HANDLE          &huart2
#define USART_2_INIT            MX_USART2_UART_Init
#else
#define DRV_USART_2_RX_BUF_SIZE 0
#define DRV_USART_2_TX_BUF_SIZE 0
#define DRV_USART_2_TX_DMA_BUF_SIZE 0
#define USART_2_HANDLE          NULL
#define USART_2_INIT            NULL
#endif
#if DRV_USAER_3_OPEN
#define DRV_USART_3_RX_BUF_SIZE 512
#define DRV_USART_3_TX_BUF_SIZE 512
#define DRV_USART_3_TX_DMA_BUF_SIZE 512
#define USART_3_HANDLE          &huart3
#define USART_3_INIT            MX_USART3_UART_Init
#else
#define DRV_USART_3_RX_BUF_SIZE 0
#define DRV_USART_3_TX_BUF_SIZE 0
#define DRV_USART_3_TX_DMA_BUF_SIZE 0
#define USART_3_HANDLE          NULL
#define USART_3_INIT            NULL
#endif
#if DRV_USAER_4_OPEN
#define DRV_USART_4_RX_BUF_SIZE 512
#define DRV_USART_4_TX_BUF_SIZE 512
#define DRV_USART_4_TX_DMA_BUF_SIZE 512
#define USART_4_HANDLE          &huart4
#define USART_4_INIT            MX_UART4_Init
#else
#define DRV_USART_4_RX_BUF_SIZE 0
#define DRV_USART_4_TX_BUF_SIZE 0
#define DRV_USART_4_TX_DMA_BUF_SIZE 0
#define USART_4_HANDLE          NULL
#define USART_4_INIT            NULL
#endif
#if DRV_USAER_5_OPEN
#define DRV_USART_5_RX_BUF_SIZE 512
#define DRV_USART_5_TX_BUF_SIZE 512
#define DRV_USART_5_TX_DMA_BUF_SIZE 512
#define USART_5_HANDLE          &huart5
#define USART_5_INIT            MX_UART5_Init
#else
#define DRV_USART_5_RX_BUF_SIZE 0
#define DRV_USART_5_TX_BUF_SIZE 0
#define DRV_USART_5_TX_DMA_BUF_SIZE 0
#define USART_5_HANDLE          NULL
#define USART_5_INIT            NULL
#endif
#if DRV_USAER_6_OPEN
#define DRV_USART_6_RX_BUF_SIZE 512
#define DRV_USART_6_TX_BUF_SIZE 512
#define DRV_USART_6_TX_DMA_BUF_SIZE 512
#define USART_6_HANDLE          &huart6
#define USART_6_INIT            MX_USART6_UART_Init
#else
#define DRV_USART_6_RX_BUF_SIZE 0
#define DRV_USART_6_TX_BUF_SIZE 0
#define DRV_USART_6_TX_DMA_BUF_SIZE 0
#define USART_6_HANDLE          NULL
#define USART_6_INIT            NULL
#endif


#define DRV_USAER_POS_BLOCKING          DEV_RXTX_POS_BLOCKING       // driver R/W position blocking
#define DRV_USAER_POS_BLOCKING_1000     DEV_RXTX_POS_BLOCKING_1000  // driver R/W position blocking 1000ms
#define DRV_USAER_POS_NONBLOCKING       DEV_RXTX_POS_NONBLOCKING
/**
 * @brief use driver cmd to control the driver, such as set tx/rx callback functions. driver_obj->control function will
 * be called by the upper layer.
 * @param cmd: driver cmd
 */

#define DRV_USART_CMD_SET_TX_IT_DONE_CALLBACK     DRV_CMD_SET_WRITE_DONE_CALLBACK  // set tx interrupt done callback
#define DRV_USART_CMD_SET_TX_IT_DONE_CALLBACK_ARG DRV_CMD_SET_WRITE_DONE_CALLBACK_ARG
#define DRV_USART_CMD_SET_RX_IT_DONE_CALLBACK     DRV_CMD_SET_READ_DONE_CALLBACK  // set rx interrupt done callback
#define DRV_USART_CMD_SET_RX_IT_DONE_CALLBACK_ARG DRV_CMD_SET_READ_DONE_CALLBACK_ARG
#define DRV_USART_CMD_GET_RX_DATA_SIZE            DRV_CMD_GET_SIZE

/*
 * ****************************************************************************
 * ******** Exported macro                                             ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported variables                                         ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Exported Function                                          ********
 * ****************************************************************************
 */

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__DRV_USART_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
