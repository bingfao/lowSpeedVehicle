/*
 * @Author: your name
 * @Date: 2025-01-09 15:11:21
 * @LastEditTime: 2025-02-10 23:39:05
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
#include "ebike_manage.h"
#include "elog.h"
#include "error_code.h"
#include "fault.h"
#include "lfs_port.h"
#include "user_crc.h"
#include "user_os.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef enum {
    OTA_FILE_DOWNLOAD_IDLE = 0,
    OTA_FILE_DOWNLOAD_DATA_REQUIRE,
    OTA_FILE_DOWNLOAD_END,
} OTA_FILE_DOWNLOAD_STATE_t;

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

typedef struct
{
    OTA_FILE_DOWNLOAD_INFORM_t file_inform;
    OTA_FILE_DOWNLOAD_STATE_t state;
    SemaphoreHandle_t sem_start;
    SemaphoreHandle_t sem_data_download;
    uint32_t read_size;
    uint32_t data_offset;
} OTA_FILE_DOWNLOAD_t;

typedef struct
{
    uint32_t data_pos;
    uint16_t data_len;
    uint8_t *data;
    uint16_t crc16;
} OTA_FILE_DOWNLOAD_DATA_t;

typedef struct
{
    OTA_FILE_INFO_t file_info;
    char name[32];
    uint32_t done_size;
    uint8_t start_flg;
    uint8_t start_src;
    USER_TIMER_OBJ_t timer;
    uint8_t md5_sum[16];
} FILE_WRITE_t;

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */
#define OTA_FILE_DOWNLOAD_PACKAGE_MAX_SIZE (1024 * 1)

#define OTA_FILE_WRITE_TIMEOUT_MS          (10 * 1000)
#define OTA_FILE_WRITE_TIMER_NAME          "f_write_timer"

#define F_WRITE_START_SRC_CMD_SERVER_SEND  (0)  // from server send data to client,cmd 2020/2021
#define F_WRITE_START_SRC_CMD_CLIENT_REQ   (1)  // from client request data, cmd 1022
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
OTA_FILE_DOWNLOAD_t g_file_download;
FILE_WRITE_t g_file_write = {0};

USER_THREAD_OBJ_t g_ota_file_thread;
/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static void ota_file_download_task(void const *parameter);
static int32_t file_write_start(FILE_WRITE_t *file_write);
static int32_t file_write_data(FILE_WRITE_t *file_write, uint8_t *data, uint32_t size);
static int32_t file_write_end(FILE_WRITE_t *file_write);
static void file_write_timeout_callback(TimerHandle_t xTimer);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */

int32_t ota_file_head_in(uint8_t *data, uint32_t size)
{
    uint16_t crc_cal = 0;
    OTA_FILE_HEAD_t *head = (OTA_FILE_HEAD_t *)data;
    int32_t ret = 0;

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

    if (g_file_write.start_flg != 0) {
        log_e("file write start error, start_flg:%d", g_file_write.start_flg);
        return -EBUSY;
    }
    printf("md5:");
    for (int i = 0; i < 16; i++) {
        printf("%02x", g_file.file_head_data.file_head.file_info.md5[i]);
    }
    printf("\r\n");
    memset(&g_file_write, 0, sizeof(FILE_WRITE_t));
    memcpy(&g_file_write.file_info, &g_file.file_head_data.file_head.file_info, sizeof(OTA_FILE_INFO_t));
    g_file_write.start_src = F_WRITE_START_SRC_CMD_SERVER_SEND;
    ret = file_write_start(&g_file_write);
    if (ret != 0) {
        log_e("file write start error, ret:%d", ret);
        return ret;
    }
    ret = file_write_data(&g_file_write, g_file.file_head_data.data, g_file.file_head_data.data_size);
    // bms_port_send((uint8_t *)g_file.file_head_data.data, g_file.file_head_data.data_size);

    return 0;
}

#define FILE_DATA_MAX_SIZE (1024 * 20)
int32_t ota_file_data_in(uint8_t *data, uint32_t size)
{
    uint16_t crc_cal = 0;
    OTA_FILE_DATA_t file_data;
    int32_t ret = 0;

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
    log_d("file_name:%s, size:%d offset:[0x%08x],type:%d\r\n", file_data.package_head->name,
          file_data.package_head->data_len, file_data.package_head->start_offset, file_data.package_head->type);
    if (strcmp((const char *)file_data.package_head->name, (const char *)g_file_write.file_info.name) != 0) {
        log_e("file name not match, in file name:%s, start file name:%s", file_data.package_head->name,
              g_file_write.file_info.name);
        return -1;
    }
    ret = file_write_data(&g_file_write, file_data.data, file_data.package_head->data_len);
    if (ret < 0) {
        log_e("file write data error, ret:%d", ret);
        return ret;
    }
    // bms_port_send(file_data.data, file_data.package_head->data_len);

    return 0;
}

int32_t ota_file_data_download_inform(uint8_t *data, uint32_t size)
{
    OTA_FILE_DOWNLOAD_INFORM_t *inform = (OTA_FILE_DOWNLOAD_INFORM_t *)data;
    int32_t ret = 0;

    if (inform->target_info.session_id != ebike_get_session_id()) {
        log_e("session id not match, target:%d, local:%d", inform->target_info.session_id, ebike_get_session_id());
        return -1;
    }
    memcpy(&g_file_download.file_inform, inform, sizeof(OTA_FILE_DOWNLOAD_INFORM_t));
    g_file_download.state = OTA_FILE_DOWNLOAD_DATA_REQUIRE;
    g_file_download.data_offset = 0;

    log_d("file_name:%s, size:%d,type:%d\r\n", g_file_download.file_inform.file_info.name,
          g_file_download.file_inform.file_info.size, g_file_download.file_inform.file_info.type);
    printf("md5:");
    for (int i = 0; i < 16; i++) {
        printf("%02x", g_file_download.file_inform.file_info.md5[i]);
    }
    printf("\r\n");
    if (g_file_write.start_flg != 0) {
        log_e("file write start error, start_flg:%d", g_file_write.start_flg);
        return -EBUSY;
    }
    memset(&g_file_write, 0, sizeof(FILE_WRITE_t));
    memcpy(&g_file_write.file_info, &g_file_download.file_inform.file_info, sizeof(OTA_FILE_INFO_t));
    ret = file_write_start(&g_file_write);
    if (ret != 0) {
        log_e("file write start error, ret:%d", ret);
        return ret;
    }
    xSemaphoreGive(g_file_download.sem_start);

    return 0;
}

int32_t ota_file_data_download_in(uint8_t *data, uint32_t size)
{
    uint16_t crc_cal = 0;
    int32_t ret = 0;

    if (g_file_download.state != OTA_FILE_DOWNLOAD_DATA_REQUIRE) {
        log_e("file download state error, state:%d", g_file_download.state);
        return -1;
    }
    OTA_FILE_DOWNLOAD_DATA_t file_data;
    file_data.data_pos = *(uint32_t *)&data[0];
    file_data.data_len = *(uint16_t *)&data[4];
    file_data.data = data + 6;
    file_data.crc16 = *(uint16_t *)&data[size - 2];
    if ((file_data.data_len > FILE_DATA_MAX_SIZE) || (file_data.data_len != g_file_download.read_size)) {
        log_e("file data len:%d, need read_size:%d", file_data.data_len, g_file_download.read_size);
        return -1;
    }
    crc_cal = crc16_ccitt(data, size - 2, 0);
    if (crc_cal == file_data.crc16) {
        log_d("ota file download data crc check pass, 0x%04x == 0x%04x", crc_cal, file_data.crc16);
    } else {
        log_e("ota file download data crc check fail, 0x%04x != 0x%04x", crc_cal, file_data.crc16);
        return -1;
    }
    log_d("size:%d offset:[0x%08x]\r\n", file_data.data_len, file_data.data_pos);
    // vTaskDelay(100);
    // ret = bms_port_send(file_data.data, file_data.data_len);
    ret = file_write_data(&g_file_write, file_data.data, file_data.data_len);
    if (ret < 0) {
        log_e("ota_file_data_download_in failed, ret:%d\r\n", ret);
        return -1;
    }
    g_file_download.data_offset += file_data.data_len;
    if (g_file_download.data_offset >= g_file_download.file_inform.file_info.size) {
        g_file_download.state = OTA_FILE_DOWNLOAD_END;
    }
    xSemaphoreGive(g_file_download.sem_data_download);

    return 0;
}

int32_t ota_file_thread_init(void)
{
    BaseType_t ret;

    memset(&g_ota_file_thread, 0, sizeof(USER_THREAD_OBJ_t));
    g_ota_file_thread.thread = ota_file_download_task;
    g_ota_file_thread.name = "ota_file";
    g_ota_file_thread.stack_size = 1024;
    g_ota_file_thread.parameter = NULL;
    g_ota_file_thread.priority = RTOS_PRIORITY_NORMAL;
    ret = xTaskCreate((TaskFunction_t)g_ota_file_thread.thread, g_ota_file_thread.name, g_ota_file_thread.stack_size,
                      g_ota_file_thread.parameter, g_ota_file_thread.priority, &g_ota_file_thread.thread_handle);
    if (ret != pdPASS) {
        log_e("ota_file_thread start failed\r\n");
        return -1;
    }
    log_d("ota_file_thread start success\r\n");

    return 0;
}

bool ota_file_download_is_start(void)
{
    return (g_file_download.state != OTA_FILE_DOWNLOAD_IDLE && g_file_download.state != OTA_FILE_DOWNLOAD_END);
}

uint8_t get_file_download_file_type(void)
{
    return g_file_download.file_inform.file_info.type;
}

int32_t get_file_download_file_name(uint8_t *name, uint32_t name_max_size)
{
    if (name_max_size < strlen((const char *)g_file_download.file_inform.file_info.name)) {
        return -1;
    }
    strncpy((char *)name, (const char *)g_file_download.file_inform.file_info.name, name_max_size);
    return 0;
}

int32_t get_file_download_file_url_key(uint8_t *key, uint32_t key_max_size)
{
    if (key_max_size < sizeof(g_file_download.file_inform.file_url_key)) {
        return -1;
    }
    memcpy(key, g_file_download.file_inform.file_url_key, sizeof(g_file_download.file_inform.file_url_key));

    return 0;
}

uint32_t get_file_download_file_data_offset(void)
{
    return g_file_download.data_offset;
}

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

static void ota_file_download_progress(void)
{
    BaseType_t ret = pdFALSE;

    if (g_file_download.data_offset >= g_file_download.file_inform.file_info.size) {
        g_file_download.state = OTA_FILE_DOWNLOAD_END;
    }
    switch (g_file_download.state) {
        case OTA_FILE_DOWNLOAD_DATA_REQUIRE:
            g_file_download.read_size = g_file_download.file_inform.file_info.size - g_file_download.data_offset;
            if ((int32_t)g_file_download.read_size < 0) {
                log_e("read data pos error, %d - %d =  %d\r\n", g_file_download.file_inform.file_info.size,
                      g_file_download.data_offset, g_file_download.read_size);
                g_file_download.state = OTA_FILE_DOWNLOAD_IDLE;
                g_file_download.data_offset = 0;
                break;
            }
            g_file_download.read_size = g_file_download.read_size > OTA_FILE_DOWNLOAD_PACKAGE_MAX_SIZE
                                            ? OTA_FILE_DOWNLOAD_PACKAGE_MAX_SIZE
                                            : g_file_download.read_size;
            ebike_device_file_download_require(g_file_download.data_offset, g_file_download.read_size);
            ret = xSemaphoreTake(g_file_download.sem_data_download, 10000);
            if (ret == pdPASS) {
                g_file_download.state = OTA_FILE_DOWNLOAD_DATA_REQUIRE;
                xSemaphoreGive(g_file_download.sem_start);
            } else {
                log_e("osSemaphoreWait failed, xReturn:%d\r\n", ret);
                g_file_download.state = OTA_FILE_DOWNLOAD_IDLE;
                g_file_download.data_offset = 0;
            }
            break;
        case OTA_FILE_DOWNLOAD_END:
            g_file_download.state = OTA_FILE_DOWNLOAD_IDLE;
        default:
            break;
    }
}

static void ota_file_download_prepare(void)
{
    memset(&g_file, 0, sizeof(OTA_FILE_t));
    memset(&g_file_download, 0, sizeof(OTA_FILE_DOWNLOAD_t));
    g_file_download.sem_start = xSemaphoreCreateBinary();
    fault_assert(g_file_download.sem_start != NULL, FAULT_CODE_CONSOLE);
    g_file_download.sem_data_download = xSemaphoreCreateBinary();
    fault_assert(g_file_download.sem_data_download != NULL, FAULT_CODE_CONSOLE);
}

static void ota_file_download_task(void const *parameter)
{
    BaseType_t ret = pdFALSE;

    ota_file_download_prepare();
    while (1) {
        ret = xSemaphoreTake(g_file_download.sem_start, portMAX_DELAY);
        if (ret != pdPASS) {
            log_e("osSemaphoreWait failed, xReturn:%d\r\n", ret);
        }
        ota_file_download_progress();
    }
}

static int32_t file_write_start(FILE_WRITE_t *file_write)
{
    int32_t ret = 0;
    int8_t need_back_file = 0;
    char name[32] = {0};
    int32_t index = 0;

    if (strlen((const char *)file_write->file_info.name) <= 0) {
        return -EINVAL;
    }
    sprintf(name, "%s", file_write->file_info.name);
    do {
        ret = lfs_port_get_file_size(name);
        if (ret >= 0) {
            need_back_file = 1;
            memset(name, 0, sizeof(name));
            sprintf(name, "%s_%d", file_write->file_info.name, index);
            index++;
        } else if (ret == -ENOENT) {
            need_back_file = 0;
            break;
        } else {
            log_e("get file (%s)size failed, ret:%d\r\n", name, ret);
            return -EIO;
        }
    } while (need_back_file == 1);
    ret = lfs_port_open(name);
    if (ret < 0) {
        log_e("open file failed, ret:%d\r\n", ret);
        return -EIO;
    }
    memcpy(&file_write->name, name, sizeof(name));
    file_write->done_size = 0;
    file_write->start_flg = 1;
    file_write->timer.name = OTA_FILE_WRITE_TIMER_NAME;
    file_write->timer.period = pdMS_TO_TICKS(OTA_FILE_WRITE_TIMEOUT_MS);
    file_write->timer.auto_reload = false;
    file_write->timer.timer_handle =
        xTimerCreateStatic(file_write->timer.name, file_write->timer.period, file_write->timer.auto_reload, (void *)0,
                           file_write_timeout_callback, &file_write->timer.timer_buf);
    if (file_write->timer.timer_handle == NULL) {
        log_e("create timer failed\r\n");
        return -ENOMEM;
    }
    ret = xTimerStart(file_write->timer.timer_handle, 0);
    if (ret != pdPASS) {
        log_e("start timer failed, ret:%d\r\n", ret);
        return -EIO;
    }

    return 0;
}

static int32_t file_write_data(FILE_WRITE_t *file_write, uint8_t *data, uint32_t size)
{
    int32_t ret = 0;
    // static int32_t test_times = 8;

    if (file_write->start_flg == 0) {
        log_e("file write not start\r\n");
        return -EINVAL;
    }
    xTimerReset(file_write->timer.timer_handle, 0);
    ret = lfs_port_write_continue(file_write->name, data, size);
    if (ret < 0) {
        log_e("write file failed, ret:%d\r\n", ret);
    }
    file_write->done_size += size;
    if (file_write->done_size >= file_write->file_info.size) {
        log_d("file write end, size:%d\r\n", file_write->done_size);
        ret = file_write_end(file_write);
    }

    return ret;
}

static int32_t file_write_end(FILE_WRITE_t *file_write)
{
    int32_t ret = 0;

    if (file_write->start_flg == 0) {
        log_e("file write not start\r\n");
        return -EINVAL;
    }
    file_write->start_flg = 0;
    xTimerDelete(file_write->timer.timer_handle, 0);
    ret = lfs_port_close();
    if (ret < 0) {
        log_e("close file failed, ret:%d\r\n", ret);
        return -EIO;
    }
    lfs_port_get_md5(file_write->name, file_write->md5_sum, sizeof(file_write->md5_sum));
    printf("file write end, name:%s, size:%d, md5:", file_write->name, file_write->done_size);
    for (int i = 0; i < 16; i++) {
        printf("%02x", file_write->md5_sum[i]);
    }
    printf("\r\n");
    if (memcmp(file_write->md5_sum, file_write->file_info.md5, sizeof(file_write->md5_sum)) != 0) {
        log_e("file md5 check fail\r\n");
        return -EIO;
    }
    if (strcmp((const char *)file_write->file_info.name, (const char *)file_write->name) != 0) {
        ret = lfs_port_delete((char *)file_write->file_info.name);
        if (ret < 0) {
            log_e("delete file: %s failed, ret:%d\r\n", file_write->file_info.name, ret);
            return -EIO;
        }
        ret = lfs_port_rename((char *)file_write->name, (char *)file_write->file_info.name);
    }

    return ret;
}

static void file_write_timeout_callback(TimerHandle_t xTimer)
{
    FILE_WRITE_t *file_write = &g_file_write;

    if (file_write->start_flg == 0) {
        return;
    }
    log_e("file write timeout\r\n");
    file_write_end(file_write);

    return;
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
