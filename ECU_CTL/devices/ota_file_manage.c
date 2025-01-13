/*
 * @Author: your name
 * @Date: 2025-01-09 15:11:21
 * @LastEditTime: 2025-01-09 17:21:05
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

#include "user_crc.h"
#include "elog.h"
#include "bms_port.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */

typedef struct
{
    OTA_FILE_HEAD_t file_head;
    uint8_t *data;
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

    memcpy(&g_file.file_head, head, sizeof(OTA_FILE_HEAD_t));
    g_file.data = data + sizeof(OTA_FILE_HEAD_t);
    g_file.size = (uint32_t)data[size - 6];
    g_file.crc = (uint16_t)data[size - 2];

    crc_cal = crc16_ccitt((uint8_t *)head, size, 0);

    if (crc_cal == g_file.crc) {
        log_d("ota file head crc check pass, 0x%04x == 0x%04x", crc_cal, g_file.crc);
    } else {
        log_e("ota file head crc check fail, 0x%04x != 0x%04x", crc_cal, g_file.crc);
    }
    bms_port_send((uint8_t *)head, size);

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
