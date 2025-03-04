/*
 * @Author: your name
 * @Date: 2025-02-03 15:20:21
 * @LastEditTime: 2025-03-04 10:00:12
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\bk_sram_config.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "BK_CFG"
#define LOG_LVL ELOG_LVL_DEBUG
#include "bk_config.h"

#include "elog.h"
#include "error_code.h"
#include "user_crc.h"
/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef struct
{
    uint32_t start_offset;
    uint32_t size;
    uint16_t crc16;
    uint8_t legal_flg;
} BK_CFG_t;

/*
 * ****************************************************************************
 * ******** Private constants                                          ********
 * ****************************************************************************
 */

#define BK_CFG_CRC_16_SIZE       (2)
#define BK_CFG_UTC_SEC_NSEC_ADDR (0x00000000)
#define BK_CFG_UTC_SEC_NSEC_SIZE (8)
#define BK_CFG_NETWORK_FLOW_ADDR (BK_CFG_UTC_SEC_NSEC_ADDR + 16)
// #define BK_CFG_NETWORK_FLOW_ADDR (BK_CFG_UTC_SEC_NSEC_ADDR + BK_CFG_UTC_SEC_NSEC_SIZE + BK_CFG_CRC_16_SIZE)
#define BK_CFG_NETWORK_FLOW_SIZE (8)

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
static DRIVER_OBJ_t *g_driver_bk_cfg = NULL;
static uint8_t g_bk_config_init_flag = 0;

static BK_CFG_t g_bk_cfg[BK_CFG_MAX] = {0};
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
int32_t bk_config_init(void)
{
    int32_t ret;

    g_driver_bk_cfg = get_driver(BK_CFG_DRIVER_NAME);
    if (g_driver_bk_cfg == NULL) {
        log_d("driver %s not found", BK_CFG_DRIVER_NAME);
        return -1;
    }
    ret = driver_init(g_driver_bk_cfg);
    if (ret != 0) {
        log_e("driver %s init failed", BK_CFG_DRIVER_NAME);
        return ret;
    }
    ret = driver_open(g_driver_bk_cfg, 0);
    if (ret != 0) {
        log_e("driver %s open failed", BK_CFG_DRIVER_NAME);
        return ret;
    }
    g_bk_config_init_flag = 1;

    return 0;
}

bool bk_config_is_init(void)
{
    return g_bk_config_init_flag != 0;
}

int32_t bk_config_utc_sec_nsec_read(uint8_t *data)
{
    int32_t ret;
    uint16_t crc16 = 0;
    BK_CFG_t *p_bk_cfg = &g_bk_cfg[BK_CFG_UTC_SEC_NSEC];
    uint8_t read_data[20] = {0};

    if (g_driver_bk_cfg == NULL) {
        log_d("driver %s not found", BK_CFG_DRIVER_NAME);
        return -1;
    }
    if (g_bk_config_init_flag == 0) {
        log_d("bk_config not init");
        return -1;
    }
    // read utc config
    p_bk_cfg->start_offset = BK_CFG_UTC_SEC_NSEC_ADDR;
    p_bk_cfg->size = BK_CFG_UTC_SEC_NSEC_SIZE;
    ret = driver_read(g_driver_bk_cfg, BK_CFG_UTC_SEC_NSEC_ADDR, read_data,
                      BK_CFG_UTC_SEC_NSEC_SIZE + BK_CFG_CRC_16_SIZE);
    if (ret < 0) {
        log_e("driver %s read failed", BK_CFG_DRIVER_NAME);
        return -1;
    }
    p_bk_cfg->crc16 = *(uint16_t *)&read_data[BK_CFG_UTC_SEC_NSEC_SIZE];
    if (p_bk_cfg->crc16 == 0) {
        log_e("read rtc config crc16 is 0");
        p_bk_cfg->legal_flg = 0;
        return -1;
    }
    crc16 = crc16_ccitt(read_data, BK_CFG_UTC_SEC_NSEC_SIZE, 0);
    if (crc16 != p_bk_cfg->crc16) {
        log_e("rtc config crc16 error");
        p_bk_cfg->legal_flg = 0;
        return -1;
    }
    p_bk_cfg->legal_flg = 1;
    memcpy(data, read_data, BK_CFG_UTC_SEC_NSEC_SIZE);

    return 0;
}

int32_t bk_config_utc_sec_nsec_write(uint8_t *data)
{
    int32_t ret;
    uint16_t crc16 = 0;
    BK_CFG_t *p_bk_cfg = &g_bk_cfg[BK_CFG_UTC_SEC_NSEC];
    uint8_t write_data[20] = {0};
    uint8_t read_data[20] = {0};

    if (g_driver_bk_cfg == NULL) {
        log_d("driver %s not found", BK_CFG_DRIVER_NAME);
        return -1;
    }
    if (g_bk_config_init_flag == 0) {
        log_d("bk_config not init");
        return -1;
    }
    // write utc config
    p_bk_cfg->start_offset = BK_CFG_UTC_SEC_NSEC_ADDR;
    memcpy(write_data, data, BK_CFG_UTC_SEC_NSEC_SIZE);
    crc16 = crc16_ccitt(write_data, BK_CFG_UTC_SEC_NSEC_SIZE, 0);
    *(uint16_t *)&write_data[BK_CFG_UTC_SEC_NSEC_SIZE] = crc16;
    ret = driver_write(g_driver_bk_cfg, BK_CFG_UTC_SEC_NSEC_ADDR, write_data,
                       BK_CFG_UTC_SEC_NSEC_SIZE + BK_CFG_CRC_16_SIZE);
    if (ret < 0) {
        log_e("driver %s write failed", BK_CFG_DRIVER_NAME);
        return -1;
    }
    p_bk_cfg->crc16 = crc16;
    p_bk_cfg->legal_flg = 1;
    // read utc config
    ret = driver_read(g_driver_bk_cfg, BK_CFG_UTC_SEC_NSEC_ADDR, read_data,
                      BK_CFG_UTC_SEC_NSEC_SIZE + BK_CFG_CRC_16_SIZE);
    if (ret < 0) {
        log_e("driver %s read failed", BK_CFG_DRIVER_NAME);
        return -1;
    }
    if (memcmp(write_data, read_data, BK_CFG_UTC_SEC_NSEC_SIZE + BK_CFG_CRC_16_SIZE) != 0) {
        log_e("rtc config write and read data not equal");
        return -1;
    }

    return 0;
}

int32_t bk_config_network_flow_read(uint32_t *tx_bytes, uint32_t *rx_bytes)
{
    int32_t ret;
    uint16_t crc16 = 0;
    BK_CFG_t *p_bk_cfg = &g_bk_cfg[BK_CFG_NETWORK_FLOW];
    uint8_t read_data[20] = {0};

    if (g_driver_bk_cfg == NULL) {
        log_d("driver %s not found", BK_CFG_DRIVER_NAME);
        return -1;
    }
    if (g_bk_config_init_flag == 0) {
        log_d("bk_config not init");
        return -1;
    }
    // read utc config
    p_bk_cfg->start_offset = BK_CFG_NETWORK_FLOW_ADDR;
    p_bk_cfg->size = BK_CFG_NETWORK_FLOW_SIZE;
    ret = driver_read(g_driver_bk_cfg, BK_CFG_NETWORK_FLOW_ADDR, read_data,
                      BK_CFG_NETWORK_FLOW_SIZE + BK_CFG_CRC_16_SIZE);
    if (ret < 0) {
        log_e("driver %s read failed", BK_CFG_DRIVER_NAME);
        return -1;
    }
    p_bk_cfg->crc16 = *(uint16_t *)&read_data[BK_CFG_NETWORK_FLOW_SIZE];
    if (p_bk_cfg->crc16 == 0) {
        log_e("read rtc config crc16 is 0");
        p_bk_cfg->legal_flg = 0;
        return -1;
    }
    crc16 = crc16_ccitt(read_data, BK_CFG_NETWORK_FLOW_SIZE, 0);
    if (crc16 != p_bk_cfg->crc16) {
        log_e("rtc config crc16 error");
        p_bk_cfg->legal_flg = 0;
        return -1;
    }
    p_bk_cfg->legal_flg = 1;
    *tx_bytes = *(uint32_t *)&read_data[0];
    *rx_bytes = *(uint32_t *)&read_data[4];

    return 0;
}

int32_t bk_config_network_flow_write(uint32_t tx_bytes, uint32_t rx_bytes)
{
    int32_t ret;
    uint16_t crc16 = 0;
    BK_CFG_t *p_bk_cfg = &g_bk_cfg[BK_CFG_NETWORK_FLOW];
    uint8_t write_data[20] = {0};
    uint8_t read_data[20] = {0};

    if (g_driver_bk_cfg == NULL) {
        log_d("driver %s not found", BK_CFG_DRIVER_NAME);
        return -1;
    }
    if (g_bk_config_init_flag == 0) {
        log_d("bk_config not init");
        return -1;
    }
    // write utc config
    p_bk_cfg->start_offset = BK_CFG_NETWORK_FLOW_ADDR;
    *(uint32_t *)&write_data[0] = tx_bytes;
    *(uint32_t *)&write_data[4] = rx_bytes;

    crc16 = crc16_ccitt(write_data, BK_CFG_NETWORK_FLOW_SIZE, 0);
    *(uint16_t *)&write_data[BK_CFG_NETWORK_FLOW_SIZE] = crc16;
    ret = driver_write(g_driver_bk_cfg, BK_CFG_NETWORK_FLOW_ADDR, write_data,
                       BK_CFG_NETWORK_FLOW_SIZE + BK_CFG_CRC_16_SIZE);
    if (ret < 0) {
        log_e("driver %s write failed", BK_CFG_DRIVER_NAME);
        return -1;
    }
    p_bk_cfg->crc16 = crc16;
    p_bk_cfg->legal_flg = 1;
    // read utc config
    ret = driver_read(g_driver_bk_cfg, BK_CFG_NETWORK_FLOW_ADDR, read_data,
                      BK_CFG_NETWORK_FLOW_SIZE + BK_CFG_CRC_16_SIZE);
    if (ret < 0) {
        log_e("driver %s read failed", BK_CFG_DRIVER_NAME);
        return -1;
    }
    if (memcmp(write_data, read_data, BK_CFG_NETWORK_FLOW_SIZE + BK_CFG_CRC_16_SIZE) != 0) {
        log_e("rtc config write and read data not equal");
        return -1;
    }

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
