/*
 * @Author: your name
 * @Date: 2025-01-09 15:11:21
 * @LastEditTime: 2025-01-16 10:48:58
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\file_manage.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "ota_file"
#define LOG_LVL ELOG_LVL_DEBUG
#include "ota_file_manage.h"

#include "bms_port.h"
#include "elog.h"
#include "user_crc.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef struct
{
    OTA_FILE_HEAD_t file_head;
    uint8_t *data;
    uint32_t data_size;
} OTA_FILE_HEAD_DATA_t;

typedef struct
{
    OTA_FILE_HEAD_DATA_t file_head_data;
    uint32_t size;
    uint16_t crc;
} OTA_FILE_t;

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
OTA_FILE_t g_file;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */

int32_t ota_file_head_in(uint8_t *data, uint32_t size)
{
    uint16_t crc_cal = 0;
    OTA_FILE_HEAD_t *head = (OTA_FILE_HEAD_t *)data;

    memcpy(&g_file.file_head_data.file_head, head, sizeof(OTA_FILE_HEAD_t));
    g_file.file_head_data.data = data + sizeof(OTA_FILE_HEAD_t);
    g_file.size = *(uint32_t *)&data[size - 6];
    g_file.crc = *(uint16_t *)&data[size - 2];
    g_file.file_head_data.data_size = g_file.size - sizeof(OTA_FILE_HEAD_t);

    crc_cal = crc16_ccitt((uint8_t *)head, g_file.size, 0);

    if (crc_cal == g_file.crc) {
        log_d("ota file head crc check pass, 0x%04x == 0x%04x", crc_cal, g_file.crc);
    } else {
        log_e("ota file head crc check fail, 0x%04x != 0x%04x", crc_cal, g_file.crc);
        return -1;
    }
    log_d("file_name:%s, size:%d,type:%d\r\n", g_file.file_head_data.file_head.file_info.name,
          g_file.file_head_data.file_head.file_info.size, g_file.file_head_data.file_head.file_info.type);

    bms_port_send((uint8_t *)g_file.file_head_data.data, g_file.file_head_data.data_size);

    return 0;
}
/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
