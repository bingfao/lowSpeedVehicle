/*
 * ****************************************************************************
 * **********                      description                       **********
 * ****************************************************************************
 * flash map
 * bootloader: 0x08000000 - 0x08007FFF  32k  do app check, choise which app, jump to app1 or app2
 * app1:       0x08008000 - 0x0807FFFF  480k  app1 code and data,
 * app2:       0x08088000 - 0x080FFFFF  512k  app2 code and data
 *
 * ****************************************************************************
 * **********                      end                              **********
 * ****************************************************************************
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

typedef void (*RUNAPP)(void);
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

    id_read[0] = *(__IO uint32_t *)(0x1FFF7A10);
    id_read[1] = *(__IO uint32_t *)(0x1FFF7A14);
    id_read[2] = *(__IO uint32_t *)(0x1FFF7A18);

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
    uint32_t p_run_addr = (uint32_t)mcu_drv_init;

    if ((p_run_addr < g_flash_area[0].end_addr) && (p_run_addr > g_flash_area[0].str_addr)) {
        *bank = 1;
    } else {
        *bank = 2;
    }

    return 0;
}

static void mcu_jump_to_app(uint32_t app_addr)
{
    RUNAPP JumpToApplication;
    uint32_t JumpAddress;

    // disable interrupts
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;  // close all interrupt
        NVIC->ICPR[i] = 0xFFFFFFFF;  // clean interrupt pending register
    }
    // HAL_RCC_DeInit();   // to turn off the PLL and set the clock to it's default state
    HAL_DeInit();       // to disable all the peripherals
    SysTick->CTRL = 0;  // to turn off the systick
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    SCB->VTOR = app_addr;  // change this interrupt vector

    /* execute the new program */
    JumpAddress = *(__IO uint32_t *)(app_addr + 4);
    /* Jump to user application */
    JumpToApplication = (RUNAPP)JumpAddress;
    /* Initialize user application's Stack Pointer */
    __set_MSP(*(__IO uint32_t *)app_addr);
    JumpToApplication();
}

static int32_t drv_mcu_set_run_bank(uint32_t *bank)
{
    int32_t ret = 0;
    ;
    uint32_t app_addr = 0;
    if (*bank == 1) {
        app_addr = g_flash_area[0].str_addr;
    } else if (*bank == 2) {
        app_addr = g_flash_area[1].str_addr;
    } else {
        log_e("invalid bank: %d \r\n", *bank);
        return -EINVAL;
    }
    log_d("mcu set run bank: %d, app addr: 0x%x \r\n", *bank, app_addr);
    mcu_jump_to_app(app_addr);

    return ret;
}

static bool erase_address_is_valid(uint32_t start_addr, uint32_t end_addr)
{
    uint32_t s_addr = 0;
    uint32_t e_addr = 0;

    if (start_addr > end_addr) {
        return false;
    }
    if (g_run_bank == 1) {
        s_addr = g_flash_area[0].str_addr;
        e_addr = g_flash_area[0].end_addr;
    } else if (g_run_bank == 2) {
        s_addr = g_flash_area[1].str_addr;
        e_addr = g_flash_area[1].end_addr;
    }
    if (start_addr < s_addr || end_addr > e_addr) {
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
    /* Unlock the Flash to enable the flash control register access *************/
    HAL_FLASH_Unlock();
    /* Clear pending flags (if any) */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR |
                           FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    return 0;
}

static int32_t flash_operation_end(void)
{
    /* Lock the Flash to disable the flash control register access *************/
    HAL_FLASH_Lock();

    return 0;
}

/**
 * @brief  Gets the sector of a given address
 * @param  Address: Flash address
 * @retval The sector of a given address
 */
static uint32_t GetSector(uint32_t Address)
{
    uint32_t sector = 0;

    if ((Address < ADDR_FLASH_SECTOR_1) && (Address >= ADDR_FLASH_SECTOR_0)) {
        sector = FLASH_SECTOR_0;
    } else if ((Address < ADDR_FLASH_SECTOR_2) && (Address >= ADDR_FLASH_SECTOR_1)) {
        sector = FLASH_SECTOR_1;
    } else if ((Address < ADDR_FLASH_SECTOR_3) && (Address >= ADDR_FLASH_SECTOR_2)) {
        sector = FLASH_SECTOR_2;
    } else if ((Address < ADDR_FLASH_SECTOR_4) && (Address >= ADDR_FLASH_SECTOR_3)) {
        sector = FLASH_SECTOR_3;
    } else if ((Address < ADDR_FLASH_SECTOR_5) && (Address >= ADDR_FLASH_SECTOR_4)) {
        sector = FLASH_SECTOR_4;
    } else if ((Address < ADDR_FLASH_SECTOR_6) && (Address >= ADDR_FLASH_SECTOR_5)) {
        sector = FLASH_SECTOR_5;
    } else if ((Address < ADDR_FLASH_SECTOR_7) && (Address >= ADDR_FLASH_SECTOR_6)) {
        sector = FLASH_SECTOR_6;
    } else if ((Address < ADDR_FLASH_SECTOR_8) && (Address >= ADDR_FLASH_SECTOR_7)) {
        sector = FLASH_SECTOR_7;
    } else if ((Address < ADDR_FLASH_SECTOR_9) && (Address >= ADDR_FLASH_SECTOR_8)) {
        sector = FLASH_SECTOR_8;
    } else if ((Address < ADDR_FLASH_SECTOR_10) && (Address >= ADDR_FLASH_SECTOR_9)) {
        sector = FLASH_SECTOR_9;
    } else if ((Address < ADDR_FLASH_SECTOR_11) && (Address >= ADDR_FLASH_SECTOR_10)) {
        sector = FLASH_SECTOR_10;
    } else /*(Address < FLASH_END_ADDR) && (Address >= ADDR_FLASH_SECTOR_11))*/
    {
        sector = FLASH_SECTOR_11;
    }
    return sector;
}

static int32_t flash_erase(uint32_t start_addr, uint32_t end_addr)
{
    int32_t ret = 0;
    uint32_t UserStartSector;
    uint32_t UserEndSector;
    uint32_t SectorError;
    FLASH_EraseInitTypeDef EraseInitStruct = {0};

    ret = flash_operation_start();
    if (ret != 0) {
        return ret;
    }
    UserStartSector = GetSector(start_addr);
    UserEndSector = GetSector(end_addr);

    /* Fill EraseInit structure*/
    EraseInitStruct.TypeErase = TYPEERASE_SECTORS;
    EraseInitStruct.Sector = UserStartSector;
    EraseInitStruct.NbSectors = UserEndSector - UserStartSector + 1;
    EraseInitStruct.VoltageRange = VOLTAGE_RANGE_3;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
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
    if (flash_is_erased(start_addr, start_addr + size - 1) == true) {
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
    uint8_t end_4_byte[4] = {0};

    if (data == NULL || size == 0) {
        return -EINVAL;
    }
    p_data = (uint32_t *)data;
    if (start_addr % 4 != 0) {
        log_e("start_addr is not 4-byte aligned: 0x%x", start_addr);
        return -EINVAL;
    }

    ret = flash_operation_start();
    if (ret != 0) {
        return ret;
    }

    for (i = 0; (i + 4) < size; i += 4) {
        if (HAL_FLASH_Program(TYPEPROGRAM_WORD, w_addr + i, (uint32_t)p_data) != HAL_OK) {
            flash_operation_end();
            return -EIO;
        }
        p_data += 1;
    }
    // do the last 16 bytes data write
    if (i < size) {
        memset(end_4_byte, 0xFF, 4);
        memcpy(end_4_byte, (uint8_t *)p_data, size - i);
        if (HAL_FLASH_Program(TYPEPROGRAM_WORD, w_addr + i, (uint32_t)end_4_byte) != HAL_OK) {
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
    if (start_addr < FLASH_BASE || start_addr + size > FLASH_END) {
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
    if (g_run_bank == 1) {
        area[0] = g_flash_area[1].str_addr;
        area[1] = g_flash_area[1].size;
    } else {
        area[0] = g_flash_area[0].str_addr;
        area[1] = g_flash_area[0].size;
    }

    return 0;
}

static int32_t drv_mcu_get_app_flash_area(uint32_t *area)
{
    if (g_run_bank == 1) {
        area[0] = g_flash_area[0].str_addr;
        area[1] = g_flash_area[0].size;
    } else {
        area[0] = g_flash_area[1].str_addr;
        area[1] = g_flash_area[1].size;
    }

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

DRIVER_REGISTER(&g_mcu_driver, mcu_407)
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
