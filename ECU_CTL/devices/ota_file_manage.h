/*
 * @Author: your name
 * @Date: 2025-01-09 15:11:34
 * @LastEditTime: 2025-02-10 17:11:01
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\file_manage.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __OTA_FILE_MANAGE_H
#define __OTA_FILE_MANAGE_H
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
#include <stdbool.h>

/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */

#pragma pack(push, 1)
typedef struct {
    uint8_t svr_time[8];
    uint32_t session_id;
} OTA_FILE_TARGET_INFO_t;
#pragma pack(push, pop)
#pragma pack(push, 1)
typedef struct {
    uint8_t type;
    uint8_t name[32];
    uint32_t size;
    uint8_t md5[16];
} OTA_FILE_INFO_t;
#pragma pack(push, pop)
#pragma pack(push, 1)
typedef struct {
    OTA_FILE_TARGET_INFO_t target_info;
    OTA_FILE_INFO_t file_info;
} OTA_FILE_HEAD_t;
#pragma pack(push, pop)

#pragma pack(push, 1)
typedef struct {
    OTA_FILE_TARGET_INFO_t target_info;
    OTA_FILE_INFO_t file_info;
    uint8_t file_url_key[16];
} OTA_FILE_DOWNLOAD_INFORM_t;
#pragma pack(push, pop)
#pragma pack(push, 1)
typedef struct
{
    uint32_t session_id;
    uint8_t type;
    uint8_t name[32];
    uint32_t start_offset;
    uint16_t data_len;
}OTA_FILE_DATA_PACKAGE_HEAD_t;
#pragma pack(push, pop)
/*
 * ****************************************************************************
 * ******** Exported constants                                         ********
 * ****************************************************************************
 */
#define OTA_FILE_TYPE_FW    0x01
#define OTA_FILE_TYPE_MEDIA 0x02
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
int32_t ota_file_thread_init(void);
int32_t ota_file_head_in(uint8_t *data, uint32_t size);
int32_t ota_file_data_in(uint8_t *data, uint32_t size);
int32_t ota_file_data_download_inform(uint8_t *data, uint32_t size);
int32_t ota_file_data_download_in(uint8_t *data, uint32_t size);
bool ota_file_download_is_start(void);
uint8_t get_file_download_file_type(void);
int32_t get_file_download_file_name(uint8_t *name, uint32_t name_max_size);
int32_t get_file_download_file_url_key(uint8_t *key, uint32_t key_max_size);

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__OTA_FILE_MANAGE_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
