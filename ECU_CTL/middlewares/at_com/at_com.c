/*
 * @Author: your name
 * @Date: 2024-10-11 15:07:36
 * @LastEditTime: 2024-10-14 14:50:29
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \e_bike_ctrl_v1\Middlewares\at_com.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#include "at_com.h"

#include <string.h>
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

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t at_com_cmp_str(AT_COM_t *p_at_com, char *src_str);
static int32_t at_wait_idle(AT_COM_t *p_at_com, uint32_t timeout);
static void at_com_log_str(AT_COM_t *p_at_com, char *log_str);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t at_com_init(AT_COM_t *p_at_com, char *tx_buffer, uint32_t tx_buffer_size, char *rx_buffer,
                    uint32_t rx_buffer_size)
{
    memset(p_at_com, 0, sizeof(AT_COM_t));
    p_at_com->tx_buffer = tx_buffer;
    p_at_com->tx_buffer_size = tx_buffer_size;
    p_at_com->rx_buffer = rx_buffer;
    p_at_com->rx_buffer_size = rx_buffer_size;
    slist_init(&p_at_com->str_head);

    return 0;
}

int32_t at_com_deinit(AT_COM_t *p_at_com)
{
    memset(p_at_com, 0, sizeof(AT_COM_t));
    return 0;
}

int32_t at_com_set_send_func(AT_COM_t *p_at_com, AT_SEND_CALLBACK at_send_func)
{
    p_at_com->at_send_func = at_send_func;
    return 0;
}

int32_t at_com_set_log_func(AT_COM_t *p_at_com, AT_SEND_CALLBACK at_log_func)
{
    p_at_com->at_log_func = at_log_func;
    return 0;
}

int32_t at_com_set_delay_func(AT_COM_t *p_at_com, DELAY_CALLBACK delay_func)
{
    p_at_com->at_delay_func = delay_func;
    return 0;
}

int32_t at_com_set_cmp_str(AT_CMP_STR_NODE_t *p_cmp_str_node, const char *p_cmp_str, void *p_param,
                           FUN_CALLBACK cmp_str_func)
{
    int32_t cmp_str_len = strlen(p_cmp_str);

    if (p_cmp_str_node == NULL || cmp_str_len == 0 || cmp_str_len > AT_CMP_STR_MAX_LEN) {
        return USER_ERROR_PARAM;
    }
    memset(p_cmp_str_node, 0, sizeof(AT_CMP_STR_NODE_t));
    memcpy(p_cmp_str_node->cmp_str, p_cmp_str, cmp_str_len);
    p_cmp_str_node->cmp_str_param = p_param;
    p_cmp_str_node->cmp_str_func = cmp_str_func;
    slist_init(&p_cmp_str_node->node);

    return 0;
}

int32_t at_com_add_cmp_str(AT_COM_t *p_at_com, AT_CMP_STR_NODE_t *p_cmp_str_node)
{
    if (p_cmp_str_node == NULL || p_cmp_str_node->cmp_str == NULL || p_at_com == NULL) {
        return USER_ERROR_PARAM;
    }
    slist_add(&p_at_com->str_head, &p_cmp_str_node->node);

    return 0;
}

int32_t at_com_send_str(AT_COM_t *p_at_com, char *tx_str)
{
    int32_t tx_len = strlen(tx_str);
    uint32_t data_size = 0;
    int32_t ret = 0;

    if (p_at_com == NULL || tx_str == NULL || tx_len == 0) {
        return USER_ERROR_PARAM;
    }
    ret = at_wait_idle(p_at_com, 1000);
    if (ret != 0) {
        return ret;
    }
    data_size = (p_at_com->tx_buffer_size) > tx_len ? tx_len : (p_at_com->tx_buffer_size);
    memcpy(p_at_com->tx_buffer, tx_str, data_size);
    if (p_at_com->at_send_func) {
        p_at_com->at_send_func(p_at_com->tx_buffer, data_size);
        if (slist_len(&p_at_com->str_head) > 0) {
            p_at_com->status = AT_COM_TX_DATA;
            ret = at_wait_idle(p_at_com, 1000);
            if (ret != 0) {
                p_at_com->status = AT_COM_IDLE;
                at_com_log_str(p_at_com, "AT send data timeout");
                return ret;
            }
        }
    }

    return 0;
}

/**
 * @brief AT cmd ack data in progress. Need to be used in the AT cmd ACK progress thread.
 * @param p_at_com
 * @param p_data
 * @param length
 * @param end_flg 0:continue data, 1:end data, if the last data, need to set end_flg=1
 * @return int32_t
 */
int32_t at_com_data_process(AT_COM_t *p_at_com, char *p_data, uint32_t length, uint8_t end_flg)
{
    uint8_t end_f = end_flg;
    uint32_t data_size = 0;

    if (p_at_com == NULL || p_data == NULL || length == 0) {
        return USER_ERROR_PARAM;
    }
    if (p_at_com->status != AT_COM_TX_DATA && p_at_com->status != AT_COM_ACK_DATA) {
        return USER_ERROR_STATE;
    }
    data_size = (p_at_com->rx_buffer_size - p_at_com->rx_buffer_index) > length
                    ? length
                    : (p_at_com->rx_buffer_size - p_at_com->rx_buffer_index);
    p_at_com->status = AT_COM_ACK_DATA;
    memcpy(p_at_com->rx_buffer + p_at_com->rx_buffer_index, p_data, data_size);
    p_at_com->rx_buffer_index += data_size;
    if (end_f || p_at_com->rx_buffer_index == p_at_com->rx_buffer_size) {
        if (end_f) {
            p_at_com->rx_buffer[p_at_com->rx_buffer_index++] = '\0';
        }
        if (p_at_com->rx_buffer_index == p_at_com->rx_buffer_size) {
            p_at_com->rx_buffer[p_at_com->rx_buffer_index - 1] = '\0';
        }
        at_com_cmp_str(p_at_com, p_at_com->rx_buffer);
        p_at_com->rx_buffer_index = 0;
        p_at_com->status = AT_COM_IDLE;
    }

    return 0;
}

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */
char *at_com_scarch_str(char *src_str, char *cmp_str)
{
    char *strx = 0;
    strx = strstr((const char *)src_str, (const char *)cmp_str);
    return strx;
}

static int32_t at_com_cmp_str(AT_COM_t *p_at_com, char *src_str)
{
    SLIST_NODE_t *pos = NULL;
    AT_CMP_STR_NODE_t *str_node = NULL;

    SLIST_FOR_EACH(pos, &p_at_com->str_head)
    {
        str_node = SLIST_ELEMENT_ENTRY(pos, AT_CMP_STR_NODE_t, node);
        if (str_node->cmp_str_func) {  // search the cmp_str_func
            if (strstr(src_str, str_node->cmp_str)) {
                str_node->cmp_str_func_ret = str_node->cmp_str_func(str_node->cmp_str_param);
            }
        }
    }

    return 0;
}

static int32_t at_wait_idle(AT_COM_t *p_at_com, uint32_t timeout)
{
    uint32_t delay_times = 0;

    while (p_at_com->status != AT_COM_IDLE) {
        if (p_at_com->at_delay_func) {
            p_at_com->at_delay_func(10);
        }
        delay_times += 10;
        if (delay_times >= timeout) {
            return USER_ERROR_TIME_OUT;
        }
    }

    return 0;
}

static void at_com_log_str(AT_COM_t *p_at_com, char *log_str)
{
    if (p_at_com->at_log_func) {
        p_at_com->at_log_func(log_str, strlen(log_str));
    }
}
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
