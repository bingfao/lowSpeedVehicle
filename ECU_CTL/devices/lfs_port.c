/*
 * @Author: your name
 * @Date: 2025-02-07 22:08:34
 * @LastEditTime: 2025-02-10 09:39:18
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\lfs_port.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */

#define LOG_TAG "EX_FLASH"
#define LOG_LVL ELOG_LVL_DEBUG
#include "lfs_port.h"

#include "elog.h"
#include "error_code.h"
#include "ex_flash.h"
#include "lfs.h"
#include "user_os.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef struct
{
    lfs_file_t lfs_file;
    char path[30];
    uint8_t lfs_open_flags;
} EX_LFS_FILE_t;
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
#define LFS_READ_SIZE      (256)
#define LFS_PROG_SIZE      (256)
#define LFS_BLOCK_CYCLES   (500)
#define LFS_CACHE_SIZE     (256)
#define LFS_LOOKAHEAD_SIZE (256)

/*
 * ****************************************************************************
 * ******** Private global variables                                   ********
 * ****************************************************************************
 */
uint8_t g_lfs_port_inited = 0;
USER_MUTEX_OBJ_T g_lfs_mutex = {0};

lfs_t lfs_flash = {0};
EX_LFS_FILE_t g_lfs_files = {0};
struct lfs_config g_lfs_cfg = {0};
lfs_dir_t g_lfs_dir = {0};

/*lfs use LFS_NO_MALLOC, so use static to avoid stack overflow */
static uint8_t g_lfs_read_buffer[LFS_READ_SIZE];
static uint8_t g_lfs_prog_buffer[LFS_PROG_SIZE];
static uint8_t g_lfs_lookahead_buffer[LFS_LOOKAHEAD_SIZE];

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */

static int lfs_deskio_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size);
static int lfs_deskio_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer,
                           lfs_size_t size);
static int lfs_deskio_erase(const struct lfs_config *c, lfs_block_t block);
static int lfs_deskio_sync(const struct lfs_config *c);

/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */
int32_t lfs_port_init(void)
{
    uint32_t size = 0;
    uint32_t total_size = 0;
    int32_t ret = 0;

    if (g_lfs_port_inited) {
        return 0;
    }
    // init lfs_cfg
    g_lfs_cfg.read = lfs_deskio_read;
    g_lfs_cfg.prog = lfs_deskio_prog;
    g_lfs_cfg.erase = lfs_deskio_erase;
    g_lfs_cfg.sync = lfs_deskio_sync;
    g_lfs_cfg.read_size = LFS_READ_SIZE;
    g_lfs_cfg.prog_size = LFS_PROG_SIZE;
    ret = ex_flash_get_sector_size(&size);
    if (ret != 0) {
        log_e("get sector size error, ret: %d", ret);
        return ret;
    }
    g_lfs_cfg.block_size = size;
    ret = ex_flash_get_total_size(&total_size);
    if (ret != 0) {
        log_e("get total size error, ret: %d", ret);
        return ret;
    }
    if (total_size % size != 0) {
        log_e("total size is not a multiple of sector size");
        return -EDOM;
    }
    g_lfs_cfg.block_count = total_size / size;
    g_lfs_cfg.cache_size = LFS_CACHE_SIZE;
    g_lfs_cfg.lookahead_size = LFS_LOOKAHEAD_SIZE;
    g_lfs_cfg.block_cycles = LFS_BLOCK_CYCLES;

    g_lfs_cfg.read_buffer = g_lfs_read_buffer;
    g_lfs_cfg.prog_buffer = g_lfs_prog_buffer;
    g_lfs_cfg.lookahead_buffer = g_lfs_lookahead_buffer;

    // mount lfs
    ret = lfs_mount(&lfs_flash, &g_lfs_cfg);
    if (ret != 0) {
        ret = lfs_format(&lfs_flash, &g_lfs_cfg);
        if (ret != 0) {
            log_e("format error, ret: %d", ret);
            return ret;
        }
        ret = lfs_mount(&lfs_flash, &g_lfs_cfg);
        if (ret != 0) {
            log_e("mount error, ret: %d", ret);
            return ret;
        }
    }
    log_d("lfs_mount success");
    memset(&g_lfs_files, 0, sizeof(EX_LFS_FILE_t));
    g_lfs_mutex.mutex_handle = xSemaphoreCreateMutexStatic(&g_lfs_mutex.mutex_buf);
    g_lfs_port_inited = 1;

    return 0;
}

int32_t lfs_port_uninit(void)
{
    if (!g_lfs_port_inited) {
        return 0;
    }
    lfs_unmount(&lfs_flash);
    vSemaphoreDelete(g_lfs_mutex.mutex_handle);

    g_lfs_port_inited = 0;

    return 0;
}

bool lfs_port_file_is_open(const char *path)
{
    if (!g_lfs_port_inited) {
        return false;
    }
    if (path == NULL) {
        return false;
    }
    if (g_lfs_files.lfs_open_flags == 0) {
        return false;
    }
    if (strcmp(path, g_lfs_files.path) == 0) {
        return true;
    }

    return false;
}

int32_t lfs_port_open(const char *path)
{
    int32_t ret = 0;

    if (!g_lfs_port_inited) {
        return -EACCES;
    }
    if (path == NULL) {
        return -EINVAL;
    }
    if (g_lfs_files.lfs_open_flags) {
        return -EMFILE;
    }
    ret = lfs_file_open(&lfs_flash, &g_lfs_files.lfs_file, path, LFS_O_RDWR | LFS_O_CREAT);
    if (ret != 0) {
        log_e("open file error, path: %s, ret: %d", path, ret);
        return ret;
    }
    g_lfs_files.lfs_open_flags = 1;
    memset(g_lfs_files.path, 0, sizeof(g_lfs_files.path));
    strncpy(g_lfs_files.path, path, sizeof(g_lfs_files.path) - 1);

    return 0;
}

int32_t lfs_port_close(void)
{
    int32_t ret = 0;

    if (!g_lfs_port_inited) {
        return -EACCES;
    }
    if (!g_lfs_files.lfs_open_flags) {
        return -EINVAL;
    }
    ret = lfs_file_close(&lfs_flash, &g_lfs_files.lfs_file);
    if (ret != 0) {
        log_e("close file error, path: %s, ret: %d", g_lfs_files.path, ret);
        return ret;
    }
    g_lfs_files.lfs_open_flags = 0;
    memset(g_lfs_files.path, 0, sizeof(g_lfs_files.path));

    return 0;
}

int32_t lfs_port_read(const char *path, uint32_t pos, uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;
    int32_t len_max = 0;
    int32_t len_read = 0;

    if (!g_lfs_port_inited) {
        return -EACCES;
    }
    if (path == NULL || buf == NULL || len == 0) {
        return -EINVAL;
    }
    if (g_lfs_files.lfs_open_flags) {
        return -EBUSY;
    }
    if (xSemaphoreTake(g_lfs_mutex.mutex_handle, 2000) != pdTRUE) {
        log_e("take mutex timeout");
        return -EBUSY;
    }
    ret = lfs_port_open(path);
    if (ret != 0) {
        xSemaphoreGive(g_lfs_mutex.mutex_handle);
        return ret;
    }
    len_max = lfs_file_size(&lfs_flash, &g_lfs_files.lfs_file);
    len_read = len_max - pos < len ? len_max - pos : len;
    if (len_read <= 0) {
        xSemaphoreGive(g_lfs_mutex.mutex_handle);
        log_e("read file error, path: %s, len_max: %d, pos: %d, len: %d", path, len_max, pos, len);
        ret = -EIO;
    }
    ret = lfs_file_seek(&lfs_flash, &g_lfs_files.lfs_file, pos, LFS_SEEK_SET);
    if (ret < 0) {
        xSemaphoreGive(g_lfs_mutex.mutex_handle);
        log_e("seek file error, path: %s, pos: %d, ret: %d", path, pos, ret);
        ret = -EIO;
    }
    ret = lfs_file_read(&lfs_flash, &g_lfs_files.lfs_file, buf, len_read);
    if (ret < 0) {
        log_e("read file error, path: %s, ret: %d", path, ret);
    } else {
        log_d("read file success, path: %s, len: %d", path, len_read);
    }
    lfs_port_close();
    xSemaphoreGive(g_lfs_mutex.mutex_handle);

    return len_read;
}

int32_t lfs_port_write(const char *path, uint32_t pos, const uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;
    int32_t len_write = 0;

    if (!g_lfs_port_inited) {
        return -EACCES;
    }
    if (path == NULL || buf == NULL || len == 0) {
        return -EINVAL;
    }
    if (g_lfs_files.lfs_open_flags) {
        return -EBUSY;
    }
    if (xSemaphoreTake(g_lfs_mutex.mutex_handle, 2000) != pdTRUE) {
        log_e("take mutex timeout");
        return -EBUSY;
    }
    ret = lfs_port_open(path);
    if (ret != 0) {
        xSemaphoreGive(g_lfs_mutex.mutex_handle);
        return ret;
    }
    if (pos == 0) {
        lfs_file_rewind(&lfs_flash, &g_lfs_files.lfs_file);
    } else if (pos == 0xFFFFFFFF) {
        lfs_file_seek(&lfs_flash, &g_lfs_files.lfs_file, 0, LFS_SEEK_END);
    } else {
        ret = lfs_file_seek(&lfs_flash, &g_lfs_files.lfs_file, pos, LFS_SEEK_SET);
        if (ret < 0) {
            xSemaphoreGive(g_lfs_mutex.mutex_handle);
            log_e("seek file error, path: %s, pos: %d, ret: %d", path, pos, ret);
            ret = -EIO;
            return ret;
        }
    }
    len_write = lfs_file_write(&lfs_flash, &g_lfs_files.lfs_file, buf, len);
    if (len_write != len) {
        log_e("write file error, path: %s, len_write: %d, len: %d", path, len_write, len);
        ret = -EIO;
    } else {
        log_d("write file success, path: %s, len: %d", path, len);
    }
    lfs_port_close();
    xSemaphoreGive(g_lfs_mutex.mutex_handle);

    return len;
}

/**
 * @brief write continue data to file
 * @note   write continue data to file from 0 position. If file not exist, create it. When finish, don't forget close
 * file.
 * @param path
 * @param buf
 * @param len
 * @return int32_t
 */
int32_t lfs_port_write_continue(const char *path, const uint8_t *buf, uint32_t len)
{
    int32_t ret = 0;
    int32_t len_write = 0;

    if (!g_lfs_port_inited) {
        return -EACCES;
    }
    if (path == NULL || buf == NULL || len == 0) {
        return -EINVAL;
    }
    if (xSemaphoreTake(g_lfs_mutex.mutex_handle, 2000) != pdTRUE) {
        log_e("take mutex timeout");
        return -EBUSY;
    }
    if (lfs_port_file_is_open(path) == false) {
        ret = lfs_port_open(path);
        if (ret != 0) {
            xSemaphoreGive(g_lfs_mutex.mutex_handle);
            return ret;
        }
        lfs_file_rewind(&lfs_flash, &g_lfs_files.lfs_file);
    }
    len_write = lfs_file_write(&lfs_flash, &g_lfs_files.lfs_file, buf, len);
    if (len_write != len) {
        log_e("write file error, path: %s, len_write: %d, len: %d", path, len_write, len);
        ret = -EIO;
    } else {
        log_d("write file success, path: %s, len: %d", path, len);
    }
    xSemaphoreGive(g_lfs_mutex.mutex_handle);

    return len;
}

int32_t lfs_port_erase(const char *path)
{
    int32_t ret = 0;

    if (!g_lfs_port_inited) {
        return -EACCES;
    }
    if (path == NULL) {
        return -EINVAL;
    }
    if (lfs_port_file_is_open(path) == true) {
        return -EBUSY;
    }
    if (xSemaphoreTake(g_lfs_mutex.mutex_handle, 2000) != pdTRUE) {
        log_e("take mutex timeout");
        return -EBUSY;
    }
    ret = lfs_remove(&lfs_flash, path);
    if (ret != 0) {
        log_e("remove file error, path: %s, ret: %d", path, ret);
    } else {
        log_d("remove file success, path: %s", path);
    }
    xSemaphoreGive(g_lfs_mutex.mutex_handle);

    return 0;
}

int32_t lfs_port_scan_file(const char *path, uint8_t *buf, uint32_t size)
{
    struct lfs_info info;
    uint32_t index = 0;

    int32_t ret = 0;
    if (g_lfs_port_inited == false) {
        return -EACCES;
    }
    if (path == NULL || size < 100) {
        return -EINVAL;
    }
    if (xSemaphoreTake(g_lfs_mutex.mutex_handle, 2000) != pdTRUE) {
        log_e("take mutex timeout");
        return -EBUSY;
    }
    ret = lfs_stat(&lfs_flash, path, &info);
    if (ret != 0) {
        log_e("stat file error, path: %s, ret: %d", path, ret);
        xSemaphoreGive(g_lfs_mutex.mutex_handle);
        ret = -ENOENT;
        return ret;
    }
    index = sprintf((char *)&buf[index], "File: %s\r\n", path);
    index += sprintf((char *)&buf[index], "Type: %s\r\n", info.type == LFS_TYPE_REG ? "File" : "Directory");
    index += sprintf((char *)&buf[index], "Size: %d\r\n", info.size);

    xSemaphoreGive(g_lfs_mutex.mutex_handle);

    return 0;
}

int32_t lfs_port_get_file_size(const char *path)
{
    int32_t ret = 0;
    struct lfs_info info;

    if (g_lfs_port_inited == false) {
        return -EACCES;
    }
    if (path == NULL) {
        return -EINVAL;
    }
    if (xSemaphoreTake(g_lfs_mutex.mutex_handle, 2000) != pdTRUE) {
        log_e("take mutex timeout");
        return -EBUSY;
    }
    ret = lfs_stat(&lfs_flash, path, &info);
    if (ret != 0) {
        log_e("stat file error, path: %s, ret: %d", path, ret);
        xSemaphoreGive(g_lfs_mutex.mutex_handle);
        ret = -ENOENT;
    } else {
        ret = info.size;
    }
    xSemaphoreGive(g_lfs_mutex.mutex_handle);

    return ret;
}

int32_t lfs_port_get_files(uint8_t *buf, uint32_t size)
{
    int32_t ret = 0;
    int32_t index = 0;
    struct lfs_info info;
    lfs_dir_t dir;

    memset(&dir, 0, sizeof(dir));
    if (g_lfs_port_inited == false) {
        return -EACCES;
    }
    if (buf == NULL || size < 40) {
        return -EINVAL;
    }
    if (xSemaphoreTake(g_lfs_mutex.mutex_handle, 2000) != pdTRUE) {
        log_e("take mutex timeout");
        return -EBUSY;
    }
    ret = lfs_dir_open(&lfs_flash, &dir, "/");
    if (ret != 0) {
        log_e("open dir error, ret: %d", ret);
        xSemaphoreGive(g_lfs_mutex.mutex_handle);
        ret = -ENOENT;
    }
    while (1) {
        ret = lfs_dir_read(&lfs_flash, &dir, &info);
        if (ret <= 0) {
            break;
        }
        if (info.type == LFS_TYPE_REG) {
            index += sprintf((char *)&buf[index], "File: %s, size: %d\r\n", info.name, info.size);
        }
        if (index + 40 >= size) {
            log_e("buf size is not enough");
            break;
        }
    }
    lfs_dir_close(&lfs_flash, &dir);
    xSemaphoreGive(g_lfs_mutex.mutex_handle);

    return 0;
}

int32_t lfs_port_get_avail_size(int32_t *total_sz, int32_t *avail_sz)
{
    int32_t ret = 0;
    if (g_lfs_port_inited == false) {
        return -EACCES;
    }
    if (total_sz == NULL || avail_sz == NULL) {
        return -EINVAL;
    }
    if (xSemaphoreTake(g_lfs_mutex.mutex_handle, 2000) != pdTRUE) {
        log_e("take mutex timeout");
        return -EBUSY;
    }
    lfs_ssize_t total_size = g_lfs_cfg.block_count * g_lfs_cfg.block_size;
    lfs_ssize_t avail_size = total_size - lfs_fs_size(&lfs_flash) * g_lfs_cfg.block_size;
    if (total_size < 0 || avail_size < 0) {
        log_e("get size error, total_size: %d, avail_size: %d", total_size, avail_size);
        ret = -EIO;
    } else {
        *total_sz = total_size;
        *avail_sz = avail_size;
    }
    xSemaphoreGive(g_lfs_mutex.mutex_handle);

    return ret;
}
/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */

static int lfs_deskio_read(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, void *buffer, lfs_size_t size)
{
    int32_t ret = 0;
    uint32_t addr = (uint32_t)block * c->block_size + off;
    ret = ex_flash_read(addr, buffer, size);
    if (ret <= 0) {
        log_e("read error, addr: 0x%x, size: %d, ret: %d", addr, size, ret);
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}

static int lfs_deskio_prog(const struct lfs_config *c, lfs_block_t block, lfs_off_t off, const void *buffer,
                           lfs_size_t size)
{
    int32_t ret = 0;
    uint32_t addr = (uint32_t)block * c->block_size + off;
    ret = ex_flash_write(addr, (uint8_t *)buffer, size);
    if (ret <= 0) {
        log_e("write error, addr: 0x%x, size: %d, ret: %d", addr, size, ret);
        return LFS_ERR_IO;
    }
    return LFS_ERR_OK;
}

static int lfs_deskio_erase(const struct lfs_config *c, lfs_block_t block)
{
    int32_t ret = 0;
    uint32_t addr = (uint32_t)block * c->block_size;
    // erase a block
    ret = ex_flash_erase(addr, c->block_size);
    if (ret != 0) {
        log_e("erase error, addr: 0x%x, size: %d, ret: %d", addr, c->block_size, ret);
        return LFS_ERR_IO;
    }

    return LFS_ERR_OK;
}

static int lfs_deskio_sync(const struct lfs_config *c)
{
    return LFS_ERR_OK;
}
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
