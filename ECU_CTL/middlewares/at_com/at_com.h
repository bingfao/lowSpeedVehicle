/*
 * @Author: your name
 * @Date: 2024-10-11 15:07:50
 * @LastEditTime: 2024-10-14 18:04:47
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \e_bike_ctrl_v1\Middlewares\at_com.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __AT_COM_H
#define __AT_COM_H
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
#include <stdint.h>

#include "user_comm.h"
#include "user_list.h"

/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */
// #define AT_TX_BUFFER_SIZE 50
// #define AT_RX_BUFFER_SIZE 2048
#define AT_CMP_STR_MAX_LEN 50
typedef int32_t (*AT_SEND_CALLBACK)(char *p_buffer, uint32_t length);
typedef int32_t (*FUN_CALLBACK)(void *p_param);
typedef int32_t (*DELAY_CALLBACK)(uint32_t delay_ms);

typedef enum {
    AT_COM_IDLE = 0,
    AT_COM_TX_DATA,
    AT_COM_ACK_DATA,
    AT_COM_MAX,
} AT_COM_STATUS_t;

typedef struct
{
    uint8_t invalid;
    char cmp_str[AT_CMP_STR_MAX_LEN];
    void *cmp_str_param;
    FUN_CALLBACK cmp_str_func;
    int32_t cmp_str_func_ret;
    SLIST_NODE_t node;
} AT_CMP_STR_NODE_t;

typedef struct
{
    char *tx_buffer;
    uint32_t tx_buffer_size;
    char *rx_buffer;
    uint32_t rx_buffer_size;
    uint32_t rx_buffer_index;
    AT_SEND_CALLBACK at_send_func;
    AT_SEND_CALLBACK at_log_func;
    DELAY_CALLBACK at_delay_func;
    SLIST_NODE_t str_head;
    AT_COM_STATUS_t status;
} AT_COM_t;

/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */

#define AT_COM_RX_STICK_MAX 1000
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

int32_t at_com_init(AT_COM_t *p_at_com, char *tx_buffer, uint32_t tx_buffer_size, char *rx_buffer,
                    uint32_t rx_buffer_size);
int32_t at_com_deinit(AT_COM_t *p_at_com);
int32_t at_com_set_send_func(AT_COM_t *p_at_com, AT_SEND_CALLBACK at_send_func);
int32_t at_com_set_log_func(AT_COM_t *p_at_com, AT_SEND_CALLBACK at_log_func);
int32_t at_com_set_delay_func(AT_COM_t *p_at_com, DELAY_CALLBACK delay_func);
int32_t at_com_set_cmp_str(AT_CMP_STR_NODE_t *p_cmp_str_node, const char *p_cmp_str, void *p_param,
                           FUN_CALLBACK cmp_str_func);
int32_t at_com_add_cmp_str(AT_COM_t *p_at_com, AT_CMP_STR_NODE_t *p_cmp_str_node);
int32_t at_com_data_process(AT_COM_t *p_at_com, char *p_data, uint32_t length, uint8_t end_flg);
/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__AT_COM_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
