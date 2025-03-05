/*
 * @Author: your name
 * @Date: 2024-10-30 15:05:50
 * @LastEditTime: 2025-02-25 22:02:31
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\devices\mcu_ctl.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __MCU_CTL_H
#define __MCU_CTL_H
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

#include "driver_com.h"
/*
 * ****************************************************************************
 * ******** Exported Types                                             ********
 * ****************************************************************************
 */

typedef enum {
    MCU_CTL_CMD_NONE = DRV_CMD_MCU_OPERATION_BASE,
    MCU_CTL_CMD_RESET,
    MCU_CTL_GET_RUN_BANK,
    MCU_CTL_SET_RUN_BANK,
    MCU_CTL_FLASH_ERASE,
    MCU_CTL_GET_UPGRADE_FLASH_AREA,
    MCU_CTL_GET_MD5_ADDR_OFFSET,    // get the offset of the MD5 address in the flash
    MCU_CTL_GET_FSIZE_ADDR_OFFSET,  // get the offset of the bin file size address in the flash
} MCU_CTL_CMD_t;

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
int32_t mcu_ctl_init(void);
int32_t mcu_ctl_get_id(uint8_t *id, uint8_t len);
int32_t mcu_ctl_reset(void);
int32_t mcu_ctl_get_run_bank(uint32_t *bank);
int32_t mcu_ctl_set_run_bank(uint32_t bank);
int32_t mcu_ctl_flash_get_upgrade_area(uint32_t *addr, uint32_t *size);
int32_t mcu_ctl_flash_get_md5_addr_offset(uint32_t *offset);
int32_t mcu_ctl_flash_get_flile_size_addr_offset(uint32_t *offset);
int32_t mcu_ctl_flash_erase(uint32_t addr, uint32_t len);
int32_t mcu_ctl_flash_write(uint32_t addr, uint8_t *data, uint32_t len);
int32_t mcu_ctl_flash_read(uint32_t addr, uint8_t *data, uint32_t len);
/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__MCU_CTL_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
