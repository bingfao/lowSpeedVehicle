/*
 * @Author: your name
 * @Date: 2025-01-09 15:11:21
 * @LastEditTime: 2025-01-18 14:59:38
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

typedef struct
{
    OTA_FILE_DATA_PACKAGE_HEAD_t *package_head;
    uint8_t *data;
    uint16_t crc16;
} OTA_FILE_DATA_t;

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

#define FILE_DATA_MAX_SIZE (1024 * 20)
int32_t ota_file_data_in(uint8_t *data, uint32_t size)
{
    uint16_t crc_cal = 0;
    OTA_FILE_DATA_t file_data;

    file_data.package_head = (OTA_FILE_DATA_PACKAGE_HEAD_t *)data;
    file_data.data = data + sizeof(OTA_FILE_DATA_PACKAGE_HEAD_t);
    file_data.crc16 = *(uint16_t *)&data[size - 2];
    if (file_data.package_head->data_len > FILE_DATA_MAX_SIZE) {
        return -1;
    }
    crc_cal = crc16_ccitt((uint8_t *)(file_data.data - 6), file_data.package_head->data_len + 6, 0);
    if (crc_cal == file_data.crc16) {
        log_d("ota file data crc check pass, 0x%04x == 0x%04x", crc_cal, file_data.crc16);
    } else {
        log_e("ota file data crc check fail, 0x%04x != 0x%04x", crc_cal, file_data.crc16);
        return -1;
    }
    log_d("file_name:%s, size:%d offset:[0x%08x],type:%d\r\n", file_data.package_head->name, file_data.package_head->data_len,
          file_data.package_head->start_offset, file_data.package_head->type);
    bms_port_send(file_data.data, file_data.package_head->data_len);

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
