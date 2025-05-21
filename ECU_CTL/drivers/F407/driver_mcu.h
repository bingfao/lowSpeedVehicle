
/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __DRIVER_MCU_H
#define __DRIVER_MCU_H
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

#include <main.h>

#include "driver_com.h"
#include "mcu_ctl.h"

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
#define DRV_MCU_CTL_CMD_RESET              MCU_CTL_CMD_RESET
#define DRV_MCU_CTL_GET_RUN_BANK           MCU_CTL_GET_RUN_BANK
#define DRV_MCU_CTL_SET_RUN_BANK           MCU_CTL_SET_RUN_BANK
#define DRV_MCU_CTL_FLASH_ERASE            MCU_CTL_FLASH_ERASE
#define DRV_MCU_CTL_GET_UPGRADE_FLASH_AREA MCU_CTL_GET_UPGRADE_FLASH_AREA
#define DRV_MCU_CTL_GET_MD5_ADDR_OFFSET    MCU_CTL_GET_MD5_ADDR_OFFSET
#define DRV_MCU_CTL_GET_FSIZE_ADDR_OFFSET  MCU_CTL_GET_FSIZE_ADDR_OFFSET

#define DRV_MCU_BANK_1_BASS_ADDR           0x08008000
#define DRV_MCU_BANK_1_BASS_END            0x0807FFFF
#define DRV_MCU_BANK_1_SIZE                0x00078000

#define DRV_MCU_BANK_2_BASS_ADDR           0x08080000
#define DRV_MCU_BANK_2_BASS_END            0x080F7FFF
#define DRV_MCU_BANK_2_SIZE                0x00078000

#define DRV_MCU_MD5_ADDR_OFFSET            0x00000300
#define DRV_MCU_FILE_SIZE_ADDR_OFFSET      0x00000310

#define DRV_MCU_BANK_MAX                   2

#define ADDR_FLASH_SECTOR_0                ((uint32_t)0x08000000) /* Base @ of Sector 0, 16 Kbyte */
#define ADDR_FLASH_SECTOR_1                ((uint32_t)0x08004000) /* Base @ of Sector 1, 16 Kbyte */
#define ADDR_FLASH_SECTOR_2                ((uint32_t)0x08008000) /* Base @ of Sector 2, 16 Kbyte */
#define ADDR_FLASH_SECTOR_3                ((uint32_t)0x0800C000) /* Base @ of Sector 3, 16 Kbyte */
#define ADDR_FLASH_SECTOR_4                ((uint32_t)0x08010000) /* Base @ of Sector 4, 64 Kbyte */
#define ADDR_FLASH_SECTOR_5                ((uint32_t)0x08020000) /* Base @ of Sector 5, 128 Kbyte */
#define ADDR_FLASH_SECTOR_6                ((uint32_t)0x08040000) /* Base @ of Sector 6, 128 Kbyte */
#define ADDR_FLASH_SECTOR_7                ((uint32_t)0x08060000) /* Base @ of Sector 7, 128 Kbyte */
#define ADDR_FLASH_SECTOR_8                ((uint32_t)0x08080000) /* Base @ of Sector 8, 128 Kbyte */
#define ADDR_FLASH_SECTOR_9                ((uint32_t)0x080A0000) /* Base @ of Sector 9, 128 Kbyte */
#define ADDR_FLASH_SECTOR_10               ((uint32_t)0x080C0000) /* Base @ of Sector 10, 128 Kbyte */
#define ADDR_FLASH_SECTOR_11               ((uint32_t)0x080E0000) /* Base @ of Sector 11, 128 Kbyte */
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

/* ************************************************************************* */
#ifdef __cplusplus
}
#endif
#endif /*__DRIVER_MCU_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
