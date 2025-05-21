/*
 * @Author: your name
 * @Date: 2025-01-23 10:36:17
 * @LastEditTime: 2025-05-22 10:24:17
 * @LastEditors: stone_honor
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\U575\driver_mcu.c
 */
/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "DRIVER_MCU"
#define LOG_LVL ELOG_LVL_DEBUG
#include "driver_mcu.h"

#include <stdint.h>

#include "elog.h"
#include "error_code.h"

/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef struct
{
    uint32_t str_addr;
    uint32_t end_addr;
    uint32_t size;
} FLASH_AREA_t;
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

FLASH_AREA_t g_flash_area[DRV_MCU_BANK_MAX] = {
    {DRV_MCU_BANK_1_BASS_ADDR, DRV_MCU_BANK_1_BASS_END, DRV_MCU_BANK_1_SIZE},
    {DRV_MCU_BANK_2_BASS_ADDR, DRV_MCU_BANK_2_BASS_END, DRV_MCU_BANK_2_SIZE},
};

uint32_t g_run_bank = 1;

/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t mcu_drv_init(DRIVER_OBJ_t *p_driver);
static int32_t mcu_drv_deinit(DRIVER_OBJ_t *p_driver);
static int32_t mcu_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag);
static int32_t mcu_drv_close(DRIVER_OBJ_t *p_driver);
static int32_t mcu_drv_read(DRIVER_OBJ_t *p_driver, uint32_t addr, void *buffer, uint32_t size);
static int32_t mcu_drv_write(DRIVER_OBJ_t *p_driver, uint32_t addr, void *buffer, uint32_t size);

static int32_t get_mcu_id(uint8_t *id);
static int32_t drv_mcu_control(DRIVER_OBJ_t *drv, uint32_t cmd, void *args, uint32_t size);
static int32_t drv_mcu_get_run_bank(uint32_t *bank);
static int32_t drv_mcu_set_run_bank(uint32_t *bank);
static bool erase_address_is_valid(uint32_t start_addr, uint32_t end_addr);
static bool flash_is_erased(uint32_t start_addr, uint32_t end_addr);
static int32_t flash_operation_start(void);
static int32_t flash_operation_end(void);
static uint32_t GetPage(uint32_t Addr);
static uint32_t GetBank(uint32_t Addr);
static int32_t flash_erase(uint32_t start_addr, uint32_t end_addr);
static int32_t drv_mcu_erase_inter_flash(uint32_t start_addr, uint32_t size);
static int32_t drv_mcu_write_inter_flash(uint32_t start_addr, uint32_t *data, uint32_t size);
static int32_t drv_mcu_read_inter_flash(uint32_t start_addr, uint32_t *data, uint32_t size);
static int32_t drv_mcu_get_upgrade_flash_area(uint32_t *addr);
static int32_t drv_mcu_get_app_flash_area(uint32_t *area);

DRIVER_CTL_t g_mcu_driver = {
    .init = mcu_drv_init,
    .deinit = mcu_drv_deinit,
    .open = mcu_drv_open,
    .close = mcu_drv_close,
    .read = mcu_drv_read,
    .write = mcu_drv_write,
    .control = drv_mcu_control,
};
/*
 * ****************************************************************************
 * ******** Extern function Definition                                 ********
 * ****************************************************************************
 */

/*
 * ****************************************************************************
 * ******** Private function Definition                                ********
 * ****************************************************************************
 */
static int32_t mcu_drv_init(DRIVER_OBJ_t *p_driver)
{
    drv_mcu_get_run_bank(&g_run_bank);
    log_d("mcu run bank: %d \r\n", g_run_bank);

    return 0;
}

static int32_t mcu_drv_deinit(DRIVER_OBJ_t *p_driver)
{
    return 0;
}

static int32_t mcu_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag)
{
    return 0;
}

static int32_t mcu_drv_close(DRIVER_OBJ_t *p_driver)
{
    return 0;
}

static int32_t mcu_drv_write(DRIVER_OBJ_t *p_driver, uint32_t addr, void *buffer, uint32_t size)
{
    int32_t ret = 0;
    ret = drv_mcu_write_inter_flash(addr, buffer, size);

    return ret;
}

static int32_t mcu_drv_read(DRIVER_OBJ_t *p_driver, uint32_t addr, void *buffer, uint32_t size)
{
    int32_t ret = 0;
    ret = drv_mcu_read_inter_flash(addr, buffer, size);

    return ret;
}

static int32_t get_mcu_id(uint8_t *id)
{
    int8_t i, j;
    uint32_t id_read[3] = {0};

    if (id == NULL) {
        return -EINVAL;
    }

    id_read[0] = *(__IO uint32_t *)(0x0BFA0700);
    id_read[1] = *(__IO uint32_t *)(0x0BFA0704);
    id_read[2] = *(__IO uint32_t *)(0x0BFA0708);

    for (i = 2; i >= 0; i--) {
        for (j = 0; j < 4; j++) {
            *id++ = id_read[i];
            id_read[i] >>= 8;
        }
    }

    return 12;
}

static int32_t drv_mcu_get_run_bank(uint32_t *bank)
{
    FLASH_OBProgramInitTypeDef OBInit;

    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBGetConfig(&OBInit);
    if ((OBInit.USERConfig & OB_SWAP_BANK_ENABLE) == OB_SWAP_BANK_DISABLE) {
        *bank = 1;
    } else {
        *bank = 2;
    }
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    return 0;
}

static int32_t drv_mcu_set_run_bank(uint32_t *bank)
{
    FLASH_OBProgramInitTypeDef OBInit;
    if (bank == NULL || *bank < 1 || *bank > 2) {
        return -EINVAL;
    }

    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBGetConfig(&OBInit);
    if ((OBInit.USERConfig & OB_SWAP_BANK_ENABLE) == OB_SWAP_BANK_DISABLE) {
        if (*bank == 2) {
            OBInit.OptionType = OPTIONBYTE_USER;
            OBInit.USERType = OB_USER_SWAP_BANK;
            OBInit.USERConfig = OB_SWAP_BANK_ENABLE;
            HAL_FLASHEx_OBProgram(&OBInit);
            /* Launch Option bytes loading */
            HAL_FLASH_OB_Launch();
        }
    } else {
        if (*bank == 1) {
            OBInit.OptionType = OPTIONBYTE_USER;
            OBInit.USERType = OB_USER_SWAP_BANK;
            OBInit.USERConfig = OB_SWAP_BANK_DISABLE;
            HAL_FLASHEx_OBProgram(&OBInit);

            /* Launch Option bytes loading */
            HAL_FLASH_OB_Launch();
        }
    }
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    return 0;
}

static bool erase_address_is_valid(uint32_t start_addr, uint32_t end_addr)
{
    if (start_addr > end_addr) {
        return false;
    }
    // only support erase in bank 2 (0x81000000, 0x81FFFFFF)
    if (start_addr > g_flash_area[1].end_addr || end_addr < g_flash_area[1].str_addr) {
        return false;
    }

    return true;
}

static bool flash_is_erased(uint32_t start_addr, uint32_t end_addr)
{
    uint32_t *p_end_addr = (uint32_t *)end_addr;
    uint32_t *p_erase_addr = (uint32_t *)start_addr;

    while (p_erase_addr < p_end_addr) {
        if (*p_erase_addr != 0xFFFFFFFF) {
            return false;
        }
        p_erase_addr += 4;
    }

    return true;
}

static int32_t flash_operation_start(void)
{
    /* Disable instruction cache prior to internal cacheable memory update */
    if (HAL_ICACHE_Disable() != HAL_OK) {
        return -EIO;
    }

    /* Unlock the Flash to enable the flash control register access *************/
    HAL_FLASH_Unlock();
    return 0;
}

static int32_t flash_operation_end(void)
{
    /* Lock the Flash to disable the flash control register access *************/
    HAL_FLASH_Lock();
    /* Re-enable instruction cache */
    if (HAL_ICACHE_Enable() != HAL_OK) {
        return -EIO;
    }

    return 0;
}

/**
 * @brief  Gets the page of a given address
 * @param  Addr: Address of the FLASH Memory
 * @retval The page of a given address
 */
static uint32_t GetPage(uint32_t Addr)
{
    uint32_t page = 0;

    if (Addr < (FLASH_BASE + FLASH_BANK_SIZE)) {
        /* Bank 1 */
        page = (Addr - FLASH_BASE) / FLASH_PAGE_SIZE;
    } else {
        /* Bank 2 */
        page = (Addr - (FLASH_BASE + FLASH_BANK_SIZE)) / FLASH_PAGE_SIZE;
    }

    return page;
}

/**
 * @brief  Gets the bank of a given address
 * @param  Addr: Address of the FLASH Memory
 * @retval The bank of a given address
 */
static uint32_t GetBank(uint32_t Addr)
{
    if (Addr < (FLASH_BASE + FLASH_BANK_SIZE)) {
        if (g_run_bank == 1) {
            return FLASH_BANK_1;
        } else {
            return FLASH_BANK_2;
        }
    } else {
        if (g_run_bank == 1) {
            return FLASH_BANK_2;
        } else {
            return FLASH_BANK_1;
        }
    }
}

static int32_t flash_erase(uint32_t start_addr, uint32_t end_addr)
{
    int32_t ret = 0;
    uint32_t FirstPage = 0;
    uint32_t NbOfPages = 0;
    uint32_t BankNumber = 0;
    uint32_t PageError = 0;
    FLASH_EraseInitTypeDef EraseInitStruct = {0};

    ret = flash_operation_start();
    if (ret != 0) {
        return ret;
    }
    /* Get the 1st page to erase */
    FirstPage = GetPage(start_addr);

    /* Get the number of pages to erase from 1st page */
    NbOfPages = GetPage(end_addr) - FirstPage + 1;

    /* Get the bank */
    BankNumber = GetBank(start_addr);

    /* Fill EraseInit structure*/
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
    EraseInitStruct.Banks = BankNumber;
    EraseInitStruct.Page = FirstPage;
    EraseInitStruct.NbPages = NbOfPages;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK) {
        flash_operation_end();
        return -EIO;
    }

    flash_operation_end();

    return 0;
}

static int32_t drv_mcu_erase_inter_flash(uint32_t start_addr, uint32_t size)
{
    if (erase_address_is_valid(start_addr, start_addr + size - 1) == false) {
        log_e("invalid erase address or size: 0x%x, 0x%x", start_addr, size);

        return -EINVAL;
    }
    // check if the flash is already erased, the read addr is the virtual address when run bank is 2
    if (flash_is_erased(start_addr, start_addr + size -1) == true) {
        return 0;
    }
    return flash_erase(start_addr, start_addr + size - 1);
}

static int32_t drv_mcu_write_inter_flash(uint32_t start_addr, uint32_t *data, uint32_t size)
{
    int32_t ret = 0;
    uint32_t i = 0;
    uint32_t *p_data = NULL;
    uint32_t w_addr = start_addr;
    uint8_t end_16_byte[16] = {0};

    if (data == NULL || size == 0) {
        return -EINVAL;
    }
    p_data = (uint32_t *)data;
    if (start_addr % 0x10 != 0) {
        log_e("start_addr is not 16-byte aligned: 0x%x", start_addr);
        return -EINVAL;
    }

    ret = flash_operation_start();
    if (ret != 0) {
        return ret;
    }

    for (i = 0; (i + 16) < size; i += 16) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, w_addr + i, (uint32_t)p_data) != HAL_OK) {
            flash_operation_end();
            return -EIO;
        }
        p_data += 4;
    }
    // do the last 16 bytes data write
    if (i < size) {
        memset(end_16_byte, 0xFF, 16);
        memcpy(end_16_byte, (uint8_t *)p_data, size - i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_QUADWORD, w_addr + i, (uint32_t)end_16_byte) != HAL_OK) {
            ret = -EIO;
        }
    }
    flash_operation_end();

    return (int32_t)size;
}

static int32_t drv_mcu_read_inter_flash(uint32_t start_addr, uint32_t *data, uint32_t size)
{
    uint32_t i = 0;
    uint32_t *p_data = (uint32_t *)data;
    uint32_t r_addr = start_addr;

    if (data == NULL || size == 0) {
        return -EINVAL;
    }
    if (start_addr < FLASH_BASE || start_addr + size > FLASH_BASE + 2 * FLASH_BANK_SIZE) {
        return -EINVAL;
    }

    for (i = 0; i < size; i += 4) {
        *p_data = *(uint32_t *)(r_addr + i);
        p_data++;
    }

    return (int32_t)size;
}

static int32_t drv_mcu_get_upgrade_flash_area(uint32_t *area)
{
    area[0] = FLASH_BASE + FLASH_BANK_SIZE;
    area[1] = FLASH_BANK_SIZE;
    return 0;
}

static int32_t drv_mcu_get_app_flash_area(uint32_t *area)
{
    area[0] = FLASH_BASE;
    area[1] = FLASH_BANK_SIZE;
    return 0;
}

static int32_t drv_mcu_control(DRIVER_OBJ_t *drv, uint32_t cmd, void *args, uint32_t size)
{
    int32_t ret = 0;

    if (!driver_is_opened(drv)) {
        return -EACCES;
    }
    switch (cmd) {
        case DRV_CMD_GET_ID:
            ret = get_mcu_id((uint8_t *)args);
            break;
        case DRV_MCU_CTL_CMD_RESET:
            HAL_NVIC_SystemReset();
            break;
        case DRV_MCU_CTL_GET_RUN_BANK:
            ret = drv_mcu_get_run_bank((uint32_t *)args);
            break;
        case DRV_MCU_CTL_SET_RUN_BANK:
            ret = drv_mcu_set_run_bank((uint32_t *)args);
            break;
        case DRV_MCU_CTL_FLASH_ERASE:
            ret = drv_mcu_erase_inter_flash(*(uint32_t *)args, *((uint32_t *)args + 1));  // start_addr, end_addr
            break;
        case DRV_MCU_CTL_GET_UPGRADE_FLASH_AREA:
            ret = drv_mcu_get_upgrade_flash_area((uint32_t *)args);
            break;
        case DRV_MCU_CTL_GET_APP_FLASH_AREA:
            ret = drv_mcu_get_app_flash_area((uint32_t *)args);
            break;
        case DRV_MCU_CTL_GET_MD5_ADDR_OFFSET:
            *(uint32_t *)args = DRV_MCU_MD5_ADDR_OFFSET;
            ret = 0;
            break;
        case DRV_MCU_CTL_GET_FSIZE_ADDR_OFFSET:
            *(uint32_t *)args = DRV_MCU_FILE_SIZE_ADDR_OFFSET;
            ret = 0;
            break;
        case DRV_MCU_CTL_GET_APP_ACTIVE_ADDR:
            *(uint32_t *)args = DRV_MCU_APP_ACTIVE_ADDR;
            ret = 0;
            break;
        default:
            return -EINVAL;
    }

    return ret;
}

DRIVER_REGISTER(&g_mcu_driver, mcu_u575)
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
