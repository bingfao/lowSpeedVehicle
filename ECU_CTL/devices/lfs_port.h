/*
 * @Author: your name
 * @Date: 2025-02-07 22:08:51
 * @LastEditTime: 2025-02-10 19:13:27
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\lfs_port.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __LFS_PORT_H
#define __LFS_PORT_H
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
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

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
int32_t lfs_port_init(void);
int32_t lfs_port_uninit(void);
bool lfs_port_file_is_open(const char *path);
int32_t lfs_port_open(const char *path);
int32_t lfs_port_close(void);
int32_t lfs_port_read(const char *path, uint32_t pos, uint8_t *buf, uint32_t len);
int32_t lfs_port_write(const char *path, uint32_t pos, const uint8_t *buf, uint32_t len);
int32_t lfs_port_write_continue(const char *path, const uint8_t *buf, uint32_t len);
int32_t lfs_port_delete(const char *path);
int32_t lfs_port_scan_file(const char *path, uint8_t *buf, uint32_t size);
int32_t lfs_port_get_file_size(const char *path);
int32_t lfs_port_get_files(uint8_t *buf, uint32_t size);
int32_t lfs_port_get_avail_size(int32_t *total_sz, int32_t *avail_sz);
int32_t lfs_port_get_md5(const char *path, uint8_t *md5_buf, uint32_t md5_len);
int32_t lfs_port_rename(const char *old_path, const char *new_path);

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__LFS_PORT_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
