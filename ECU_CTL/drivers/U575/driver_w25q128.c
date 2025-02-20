/*
 * @Author: your name
 * @Date: 2025-02-06 09:58:02
 * @LastEditTime: 2025-02-20 10:58:35
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\U575\driver_w25q128.c
 */

/*
 * ****************************************************************************
 * ******** Includes                                                   ********
 * ****************************************************************************
 */
#define LOG_TAG "W25Q128"
#define LOG_LVL ELOG_LVL_DEBUG
#include "driver_w25q128.h"

#include "elog.h"
#include "error_code.h"
#include "user_os.h"
/*
 * ****************************************************************************
 * ******** Private Types                                              ********
 * ****************************************************************************
 */
typedef struct
{
    uint8_t spi_index;  //  0 : SPI1, 1 : SPI2
    uint8_t spi_mode;   //  0 : QSPI, 1 : SPI
    OSPI_HandleTypeDef *h_ospi;
    uint8_t inited_flg;
    uint8_t open_flag;
    USER_MUTEX_OBJ_T user_mutex;
} W25Q128_PRIV_t;
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
#define SPI_MODE_QSPI 0
#define SPI_MODE_SPI  1

/*
 * ****************************************************************************
 * ******** Private global variables                                   ********
 * ****************************************************************************
 */
static W25Q128_PRIV_t g_w25q128_priv = {0};

static OSPI_HandleTypeDef *g_ospi_handle = QSPI_PORT_HANDLE;
/*
 * ****************************************************************************
 * ******** Private functions prototypes                               ********
 * ****************************************************************************
 */
static int32_t w25q128_drv_init(DRIVER_OBJ_t *p_driver);
static int32_t w25q128_drv_deinit(DRIVER_OBJ_t *p_driver);
static int32_t w25q128_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag);
static int32_t w25q128_drv_close(DRIVER_OBJ_t *p_driver);
static int32_t w25q128_drv_read(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);
static int32_t w25q128_drv_write(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size);
static int32_t w25q128_drv_control(DRIVER_OBJ_t *p_driver, uint32_t cmd, void *args, uint32_t size);

/* w25q128 driver interface */
static int32_t ospi_w25qxx_auto_polling_mem_ready(void);
static uint32_t ospi_w25qxx_read_id(void);
static int32_t ospi_w25qxx_write_enable(void);
static int32_t ospi_w25qxx_sector_erase(uint32_t SectorAddress);
static uint32_t data_address_to_sector_address(uint32_t data_address);
static int32_t ospi_w25qxx_erase_buffer(uint32_t address, int32_t size);
static int32_t ospi_w25qxx_write_page(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
static int32_t ospi_w25qxx_write_buffer(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t Size);
static int32_t ospi_w25qxx_read_buffer(uint8_t *pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead);

DRIVER_CTL_t g_driver_w25q128 = {
    .drv_priv = &g_w25q128_priv,
    .init = w25q128_drv_init,
    .deinit = w25q128_drv_deinit,
    .open = w25q128_drv_open,
    .close = w25q128_drv_close,
    .read = w25q128_drv_read,
    .write = w25q128_drv_write,
    .control = w25q128_drv_control,
};

DRIVER_REGISTER(&g_driver_w25q128, w25q128)
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
static int32_t w25q128_drv_init(DRIVER_OBJ_t *p_driver)
{
    uint32_t id = 0;
    W25Q128_PRIV_t *p_priv = (W25Q128_PRIV_t *)p_driver->driver->drv_priv;

    if (p_priv->inited_flg == 0) {
        memset(p_priv, 0, sizeof(W25Q128_PRIV_t));
    }
    id = ospi_w25qxx_read_id();
    if (id != W25Qxx_FLASH_ID) {
        log_e("W25Q128 ID error, id:0x%x != 0x%x\r\n", id, W25Qxx_FLASH_ID);
        return -EIO;
    }
    log_d("W25Q128 ID ok, id:0x%x\r\n", id);
    p_priv->spi_index = 0;
    p_priv->h_ospi = g_ospi_handle;
    p_priv->inited_flg = 1;

    return 0;
}

static int32_t w25q128_drv_deinit(DRIVER_OBJ_t *p_driver)
{
    W25Q128_PRIV_t *p_priv = (W25Q128_PRIV_t *)p_driver->driver->drv_priv;

    p_priv->inited_flg = 0;

    return 0;
}

static int32_t w25q128_drv_open(DRIVER_OBJ_t *p_driver, uint32_t oflag)
{
    W25Q128_PRIV_t *p_priv = (W25Q128_PRIV_t *)p_driver->driver->drv_priv;

    if (p_priv->inited_flg == 0) {
        return -1;
    }
    if (p_priv->open_flag == 1) {
        return -EMFILE;
    }
    p_priv->open_flag = 1;

    return 0;
}

static int32_t w25q128_drv_close(DRIVER_OBJ_t *p_driver)
{
    W25Q128_PRIV_t *p_priv = (W25Q128_PRIV_t *)p_driver->driver->drv_priv;

    if (p_priv->inited_flg == 0) {
        return -1;
    }
    p_priv->open_flag = 0;

    return 0;
}

static int32_t w25q128_drv_read(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    int32_t ret = 0;

    ret = ospi_w25qxx_read_buffer((uint8_t *)buffer, pos, size);

    return ret;
}

static int32_t w25q128_drv_write(DRIVER_OBJ_t *p_driver, uint32_t pos, void *buffer, uint32_t size)
{
    int32_t ret = 0;

    ret = ospi_w25qxx_write_buffer((uint8_t *)buffer, pos, size);
    if (ret != 0) {
        return -EIO;
    }

    return size;
}

static int32_t w25q128_drv_control(DRIVER_OBJ_t *p_driver, uint32_t cmd, void *args, uint32_t size)
{
    W25Q128_PRIV_t *p_priv = (W25Q128_PRIV_t *)p_driver->driver->drv_priv;
    uint32_t address = 0;
    uint32_t erasesize = 0;
    uint32_t *data = (uint32_t *)args;

    if (p_priv->open_flag == 0) {
        return -EACCES;
    }

    switch (cmd) {
        case DRV_W25Q128_CMD_ERASE_BUFFER:
            if (data == NULL) {
                return -EINVAL;
            }
            address = *(uint32_t *)args;
            erasesize = *((uint32_t *)args + 1);
            if (ospi_w25qxx_erase_buffer(address, erasesize) != 0) {
                return -EIO;
            }
            break;
        case DRV_W25Q128_CMD_GET_BUFFER_TOTAL_SIZE:
            *(uint32_t *)args = W25Qxx_FlashSize;
            break;
        case DRV_W25Q128_CMD_GET_BUFFER_BLOCK_SIZE:
            *(uint32_t *)args = W25Qxx_SectorSize;
            break;
        case DRV_W25Q128_CMD_SET_DATA_MODE:
            if (data == NULL) {
                return -EINVAL;
            }
            if (data[0] == 0) {
                p_priv->spi_mode = SPI_MODE_QSPI;
            } else {
                p_priv->spi_mode = SPI_MODE_SPI;
            }
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

/*************************************************************************************************
 *  函 数 名: ospi_w25qxx_auto_polling_mem_ready
 *  入口参数: 无
 *  返 回 值: OSPI_W25Qxx_OK - 通信正常结束，W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
 *  函数功能: 使用自动轮询标志查询，等待通信结束
 *  说    明: 每一次通信都应该调用此函数，等待通信结束，避免错误的操作
 ******************************************************************************************HQYJ*****/

static int32_t ospi_w25qxx_auto_polling_mem_ready(void)
{
    OSPI_RegularCmdTypeDef sCommand;  // OSPI传输配置
    OSPI_AutoPollingTypeDef sConfig;  // 轮询比较相关配置参数

    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;             // 通用配置
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;                          // flash ID
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;          // 1线指令模式
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;          // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;  // 禁止指令DTR模式
    sCommand.Address = 0x0;                                          // 地址0
    sCommand.AddressMode = HAL_OSPI_ADDRESS_NONE;                    // 无地址模式
    sCommand.AddressSize = HAL_OSPI_ADDRESS_24_BITS;                 // 地址长度24位
    sCommand.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;          // 禁止地址DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;     // 无交替字节
    sCommand.DataMode = HAL_OSPI_DATA_1_LINE;                        // 1线数据模式
    sCommand.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;                // 禁止数据DTR模式
    sCommand.NbData = 1;                                             // 通信数据长度
    sCommand.DummyCycles = 0;                                        // 空周期个数
    sCommand.DQSMode = HAL_OSPI_DQS_DISABLE;                         // 不使用DQS
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;                // 每次传输数据都发送指令

    sCommand.Instruction = W25Qxx_CMD_ReadStatus_REG1;  // 读状态信息寄存器

    if (HAL_OSPI_Command(g_ospi_handle, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_AUTOPOLLING;  // 轮询等待无响应
    }

    // 不停的查询 W25Qxx_CMD_ReadStatus_REG1 寄存器，将读取到的状态字节中的 W25Qxx_Status_REG1_BUSY 不停的与0作比较
    // 读状态寄存器1的第0位（只读），Busy标志位，当正在擦除/写入数据/写命令时会被置1，空闲或通信结束为0
    // HQYJ
    sConfig.Match = 0;                                       // 匹配值
    sConfig.MatchMode = HAL_OSPI_MATCH_MODE_AND;             // 与运算
    sConfig.Interval = 0x10;                                 // 轮询间隔
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;  // 自动停止模式
    sConfig.Mask = W25Qxx_Status_REG1_BUSY;                  // 对在轮询模式下接收的状态字节进行屏蔽，只比较需要用到的位

    // 发送轮询等待命令
    if (HAL_OSPI_AutoPolling(g_ospi_handle, &sConfig, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_AUTOPOLLING;  // 轮询等待无响应
    }
    return OSPI_W25Qxx_OK;  // 通信正常结束
}

/*************************************************************************************************
 *  函 数 名: ospi_w25qxx_read_id
 *  入口参数: 无
 *  返 回 值: w25qxx_id - 读取到的器件ID，W25Qxx_ERROR_INIT - 通信、初始化错误
 *  函数功能: 初始化 OSPI 配置，读取器件ID
 *  说    明: 无
 **************************************************************************************************/

static uint32_t ospi_w25qxx_read_id(void)
{
    OSPI_RegularCmdTypeDef sCommand;  // OSPI传输配置

    uint8_t OSPI_ReceiveBuff[3];  // 存储OSPI读到的数据
    uint32_t w25qxx_id;           // 器件的ID

    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;             // 通用配置
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;                          // flash ID
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;          // 1线指令模式
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;          // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;  // 禁止指令DTR模式
    sCommand.AddressMode = HAL_OSPI_ADDRESS_NONE;                    // 无地址模式
    sCommand.AddressSize = HAL_OSPI_ADDRESS_24_BITS;                 // 地址长度24位
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;     // 无交替字节
    sCommand.DataMode = HAL_OSPI_DATA_1_LINE;                        // 1线数据模式
    sCommand.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;                // 禁止数据DTR模式
    sCommand.NbData = 3;                                             // 传输数据的长度
    sCommand.DummyCycles = 0;                                        // 空周期个数
    sCommand.DQSMode = HAL_OSPI_DQS_DISABLE;                         // 不使用DQS
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;                // 每次传输数据都发送指令

    sCommand.Instruction = W25Qxx_CMD_JedecID;  // 执行读器件ID命令

    HAL_OSPI_Command(g_ospi_handle, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);  // 发送指令

    HAL_OSPI_Receive(g_ospi_handle, OSPI_ReceiveBuff, HAL_OSPI_TIMEOUT_DEFAULT_VALUE);  // 接收数据

    w25qxx_id = (OSPI_ReceiveBuff[0] << 16) | (OSPI_ReceiveBuff[1] << 8) | OSPI_ReceiveBuff[2];  // 将得到的数据组合成ID

    return w25qxx_id;  // 返回ID
}

/*************************************************************************************************
 *  函 数 名: OSPI_W25Qxx_WriteEnable
 *  入口参数: 无
 *  返 回 值: OSPI_W25Qxx_OK - 写使能成功，W25Qxx_ERROR_WriteEnable - 写使能失败
 *  函数功能: 发送写使能命令
 *  说    明: 无
 **************************************************************************************************/
static int32_t ospi_w25qxx_write_enable(void)
{
    OSPI_RegularCmdTypeDef sCommand;  // OSPI传输配置
    OSPI_AutoPollingTypeDef sConfig;  // 轮询比较相关配置参数

    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;                    // 通用配置
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;                                 // flash ID
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;                 // 1线指令模式
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;                 // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;         // 禁止指令DTR模式
    sCommand.Address = 0;                                                   // 地址0
    sCommand.AddressMode = HAL_OSPI_ADDRESS_NONE;                           // 无地址模式
    sCommand.AddressSize = HAL_OSPI_ADDRESS_24_BITS;                        // 地址长度24位
    sCommand.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;                 // 禁止地址DTR模式
    sCommand.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;  // 禁止替字节DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;            // 无交替字节
    sCommand.DataMode = HAL_OSPI_DATA_NONE;                                 // 无数据模式
    sCommand.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;                       // 禁止数据DTR模式
    sCommand.DummyCycles = 0;                                               // 空周期个数
    sCommand.DQSMode = HAL_OSPI_DQS_DISABLE;                                // 不使用DQS
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;                       // 每次传输数据都发送指令

    sCommand.Instruction = W25Qxx_CMD_WriteEnable;  // 写使能命令

    // 发送写使能命令
    if (HAL_OSPI_Command(g_ospi_handle, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_WriteEnable;
    }
    // 发送查询状态寄存器命令
    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;             // 通用配置
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;                          // flash ID
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;          // 1线指令模式
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;          // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;  // 禁止指令DTR模式
    sCommand.AddressMode = HAL_OSPI_ADDRESS_NONE;                    // 无地址模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;     // 无交替字节
    sCommand.DummyCycles = 0;                                        // 空周期个数
    sCommand.DQSMode = HAL_OSPI_DQS_DISABLE;                         // 不使用DQS
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;                // 每次传输数据都发送指令
    sCommand.DataMode = HAL_OSPI_DATA_1_LINE;                        // 1线数据模式
    sCommand.NbData = 1;                                             // 通信数据长度

    sCommand.Instruction = W25Qxx_CMD_ReadStatus_REG1;  // 查询状态寄存器命令

    if (HAL_OSPI_Command(g_ospi_handle, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_WriteEnable;
    }

    // 不停的查询 W25Qxx_CMD_ReadStatus_REG1 寄存器，将读取到的状态字节中的 W25Qxx_Status_REG1_WEL 不停的与 0x02 作比较
    // 读状态寄存器1的第1位（只读），WEL写使能标志位，该标志位为1时，代表可以进行写操作
    // HQYJ
    sConfig.Match = 0x02;                                    // 匹配值
    sConfig.MatchMode = HAL_OSPI_MATCH_MODE_AND;             // 与运算
    sConfig.Interval = 0x10;                                 // 轮询间隔
    sConfig.AutomaticStop = HAL_OSPI_AUTOMATIC_STOP_ENABLE;  // 自动停止模式
    sConfig.Mask = W25Qxx_Status_REG1_WEL;                   // 对在轮询模式下接收的状态字节进行屏蔽，只比较需要用到的位

    if (HAL_OSPI_AutoPolling(g_ospi_handle, &sConfig, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_AUTOPOLLING;  // 轮询等待无响应
    }
    return OSPI_W25Qxx_OK;  // 通信正常结束
}

/*************************************************************************************************
 *
 *  函 数 名: ospi_w25qxx_sector_erase
 *
 *  入口参数: SectorAddress - 要擦除的地址
 *
 *  返 回 值: OSPI_W25Qxx_OK - 擦除成功
 *            W25Qxx_ERROR_Erase - 擦除失败
 *            W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
 *
 *  函数功能: 进行扇区擦除操作，每次擦除4K字节
 *
 *  说    明: 1.按照 W25Q128JV 数据手册给出的擦除参考时间，典型值为 45ms，最大值为400ms
 *            2.实际的擦除速度可能大于45ms，也可能小于45ms
 *            3.flash使用的时间越长，擦除所需时间也会越长
 *
 **************************************************************************************************/

static int32_t ospi_w25qxx_sector_erase(uint32_t SectorAddress)
{
    OSPI_RegularCmdTypeDef sCommand;  // OSPI传输配置

    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;             // 通用配置
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;                          // flash ID
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;          // 1线指令模式
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;          // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;  // 禁止指令DTR模式
    sCommand.Address = SectorAddress;                                // 地址
    sCommand.AddressMode = HAL_OSPI_ADDRESS_1_LINE;                  // 1线地址模式
    sCommand.AddressSize = HAL_OSPI_ADDRESS_24_BITS;                 // 地址长度24位
    sCommand.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;          // 禁止地址DTR模式
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;     // 无交替字节
    sCommand.DataMode = HAL_OSPI_DATA_NONE;                          // 无数据模式
    sCommand.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;                // 禁止数据DTR模式
    sCommand.DummyCycles = 0;                                        // 空周期个数
    sCommand.DQSMode = HAL_OSPI_DQS_DISABLE;                         // 不使用DQS
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;                // 每次传输数据都发送指令

    sCommand.Instruction = W25Qxx_CMD_SectorErase;  // 扇区擦除指令，每次擦除4K字节

    // 发送写使能
    if (ospi_w25qxx_write_enable() != OSPI_W25Qxx_OK) {
        return W25Qxx_ERROR_WriteEnable;  // 写使能失败
    }
    // 发送擦除指令
    if (HAL_OSPI_Command(g_ospi_handle, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_AUTOPOLLING;  // 轮询等待无响应
    }
    // 使用自动轮询标志位，等待擦除的结束
    if (ospi_w25qxx_auto_polling_mem_ready() != OSPI_W25Qxx_OK) {
        return W25Qxx_ERROR_AUTOPOLLING;  // 轮询等待无响应
    }
    return OSPI_W25Qxx_OK;  // 擦除成功
}

static uint32_t data_address_to_sector_address(uint32_t data_address)
{
    return data_address & ~(W25Qxx_SectorSize - 1);
}

static int32_t ospi_w25qxx_erase_buffer(uint32_t address, int32_t size)
{
    int32_t ret = 0;
    int32_t sector_address = data_address_to_sector_address(address);
    int32_t i = 0;

    if (address + size > W25Qxx_FlashSize) {
        return -ENOMEM;
    }
    for (i = (int32_t)address - sector_address; i < size; i += W25Qxx_SectorSize) {
        sector_address = data_address_to_sector_address(address + i);
        ret = ospi_w25qxx_sector_erase(sector_address);
        if (ret != OSPI_W25Qxx_OK) {
            return ret;
        }
    }

    return OSPI_W25Qxx_OK;
}

/**********************************************************************************************************
 *
 *   函 数 名: ospi_w25qxx_write_page
 *
 *   入口参数: pBuffer        - 要写入的数据
 *            WriteAddr        - 要写入 W25Qxx 的地址
 *            NumByteToWrite - 数据长度，最大只能256字节
 *
 *   返 回 值: OSPI_W25Qxx_OK             - 写数据成功
 *             W25Qxx_ERROR_WriteEnable - 写使能失败
 *             W25Qxx_ERROR_TRANSMIT     - 传输失败
 *             W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
 *
 *   函数功能: 按页写入，最大只能256字节，在数据写入之前，请务必完成擦除操作
 *
 *   说    明: 1.Flash的写入时间和擦除时间一样，是限定的，并不是说OSPI驱动时钟133M就可以以这个速度进行写入
 *             2.按照 W25Q128JV 数据手册给出的 页(256字节) 写入参考时间，典型值为 0.4ms，最大值为3ms
 *             3.实际的写入速度可能大于0.4ms，也可能小于0.4ms
 *             4.Flash使用的时间越长，写入所需时间也会越长
 *             5.在数据写入之前，请务必完成擦除操作
 *
 ***********************************************************************************************************/
static int32_t ospi_w25qxx_write_page(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    OSPI_RegularCmdTypeDef sCommand;  // OSPI传输配置

    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;  // 通用配置
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;               // flash ID

    if (g_w25q128_priv.spi_mode == SPI_MODE_QSPI) {
        sCommand.Instruction = W25Qxx_CMD_QuadInputPageProgram;  // 1-1-4模式下(1线指令1线地址4线数据)，页编程指令
    } else {
        sCommand.Instruction = W25Qxx_CMD_PageProgram;  // 1-1-1模式下(1线指令1线地址1线数据)，页编程指令
    }
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;          // 1线指令模式
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;          // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;  // 禁止指令DTR模式

    sCommand.Address = WriteAddr;                            // 地址
    sCommand.AddressMode = HAL_OSPI_ADDRESS_1_LINE;          // 1线地址模式
    sCommand.AddressSize = HAL_OSPI_ADDRESS_24_BITS;         // 地址长度24位
    sCommand.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;  // 禁止地址DTR模式

    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;            // 无交替字节
    sCommand.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;  // 禁止替字节DTR模式

    if (g_w25q128_priv.spi_mode == SPI_MODE_QSPI) {
        sCommand.DataMode = HAL_OSPI_DATA_4_LINES;  // 4线数据模式
    } else {
        sCommand.DataMode = HAL_OSPI_DATA_1_LINE;  // 1线数据模式
    }
    sCommand.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;  // 禁止数据DTR模式
    sCommand.NbData = NumByteToWrite;                  // 数据长度

    sCommand.DummyCycles = 0;                          // 空周期个数
    sCommand.DQSMode = HAL_OSPI_DQS_DISABLE;           // 不使用DQS
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;  // 每次传输数据都发送指令

    // 写使能
    if (ospi_w25qxx_write_enable() != OSPI_W25Qxx_OK) {
        return W25Qxx_ERROR_WriteEnable;  // 写使能失败
    }
    // 写命令
    if (HAL_OSPI_Command(g_ospi_handle, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_TRANSMIT;  // 传输数据错误
    }
    // 开始传输数据
    if (HAL_OSPI_Transmit(g_ospi_handle, pBuffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_TRANSMIT;  // 传输数据错误
    }
    // 使用自动轮询标志位，等待写入的结束
    if (ospi_w25qxx_auto_polling_mem_ready() != OSPI_W25Qxx_OK) {
        return W25Qxx_ERROR_AUTOPOLLING;  // 轮询等待无响应
    }
    return OSPI_W25Qxx_OK;  // 写数据成功
}

/**********************************************************************************************************
 *
 *   函 数 名: OSPI_W25Qxx_WriteBuffer
 *
 *   入口参数: pBuffer        - 要写入的数据
 *                       WriteAddr        - 要写入 W25Qxx 的地址
 *                       NumByteToWrite - 数据长度，最大不能超过flash芯片的大小
 *
 *   返 回 值: OSPI_W25Qxx_OK             - 写数据成功
 *            W25Qxx_ERROR_WriteEnable - 写使能失败
 *            W25Qxx_ERROR_TRANSMIT     - 传输失败
 *            W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
 *
 *   函数功能: 写入数据，最大不能超过flash芯片的大小，请务必完成擦除操作
 *
 *   说    明: 1.Flash的写入时间和擦除时间一样，是有限定的，并不是说OSPI驱动时钟133M就可以以这个速度进行写入
 *             2.按照 W25Q128JV 数据手册给出的 页 写入参考时间，典型值为 0.4ms，最大值为3ms
 *             3.实际的写入速度可能大于0.4ms，也可能小于0.4ms
 *             4.Flash使用的时间越长，写入所需时间也会越长
 *             5.在数据写入之前，请务必完成擦除操作
 *             6.该函数移植于 stm32h743i_eval_qspi.c
 *
 **********************************************************************************************************/

static int32_t ospi_w25qxx_write_buffer(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t Size)
{
    uint32_t end_addr, current_size, current_addr;
    uint8_t *write_data;  // 要写入的数据

    current_size = W25Qxx_PageSize - (WriteAddr % W25Qxx_PageSize);  // 计算当前页还剩余的空间

    if (current_size > Size)  // 判断当前页剩余的空间是否足够写入所有数据
    {
        current_size = Size;  // 如果足够，则直接获取当前长度
    }

    current_addr = WriteAddr;     // 获取要写入的地址
    end_addr = WriteAddr + Size;  // 计算结束地址
    write_data = pBuffer;         // 获取要写入的数据

    do {
        // 按页写入数据
        if (ospi_w25qxx_write_page(write_data, current_addr, current_size) != OSPI_W25Qxx_OK) {
            return W25Qxx_ERROR_TRANSMIT;
        }

        else  // 按页写入数据成功，进行下一次写数据的准备工作
        {
            current_addr += current_size;  // 计算下一次要写入的地址
            write_data += current_size;    // 获取下一次要写入的数据存储区地址
            // 计算下一次写数据的长度
            current_size = ((current_addr + W25Qxx_PageSize) > end_addr) ? (end_addr - current_addr) : W25Qxx_PageSize;
        }
    } while (current_addr < end_addr);  // 判断数据是否全部写入完毕

    return OSPI_W25Qxx_OK;  // 写入数据成功
}

/**********************************************************************************************************************************
 *
 *  函 数 名: OSPI_W25Qxx_ReadBuffer
 *
 *  入口参数: pBuffer        - 要读取的数据
 *           ReadAddr         - 要读取 W25Qxx 的地址
 *           NumByteToRead  - 数据长度，最大不能超过flash芯片的大小
 *
 *  返 回 值: OSPI_W25Qxx_OK             - 读数据成功
 *            W25Qxx_ERROR_TRANSMIT     - 传输失败
 *            W25Qxx_ERROR_AUTOPOLLING - 轮询等待无响应
 *
 *  函数功能: 读取数据，最大不能超过flash芯片的大小
 *
 *  说    明: 1.Flash的读取速度取决于OSPI的通信时钟，最大不能超过133M
 *            2.这里使用的是1-4-4模式下(1线指令4线地址4线数据)，快速读取指令 Fast Read Quad I/O
 *            3.使用快速读取指令是有空周期的，具体参考W25Q128JV的手册  Fast Read Quad I/O  （0xEB）指令
 *            4.实际使用中，是否使用DMA、编译器的优化等级以及数据存储区的位置(内部 TCM SRAM 或者 AXI
 *SRAM)都会影响读取的速度 HQYJ
 *****************************************************************************************************************HQYJ************/

static int32_t ospi_w25qxx_read_buffer(uint8_t *pBuffer, uint32_t ReadAddr, uint32_t NumByteToRead)
{
    OSPI_RegularCmdTypeDef sCommand;  // OSPI传输配置

    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;  // 通用配置
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;               // flash ID

    if (g_w25q128_priv.spi_mode == SPI_MODE_QSPI) {
        sCommand.Instruction = W25Qxx_CMD_FastReadQuad_IO;  // 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令
    } else {
        sCommand.Instruction = W25Qxx_CMD_FastRead;  // 1-1-1模式下(1线指令1线地址1线数据)，快速读取指令
    }
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_1_LINE;          // 1线指令模式
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_8_BITS;          // 指令长度8位
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;  // 禁止指令DTR模式

    sCommand.Address = ReadAddr;  // 地址
    if (g_w25q128_priv.spi_mode == SPI_MODE_QSPI) {
        sCommand.AddressMode = HAL_OSPI_ADDRESS_4_LINES;  // 4线地址模式
    } else {
        sCommand.AddressMode = HAL_OSPI_ADDRESS_1_LINE;  // 1线地址模式
    }
    sCommand.AddressSize = HAL_OSPI_ADDRESS_24_BITS;         // 地址长度24位
    sCommand.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;  // 禁止地址DTR模式

    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;            // 无交替字节
    sCommand.AlternateBytesDtrMode = HAL_OSPI_ALTERNATE_BYTES_DTR_DISABLE;  // 禁止替字节DTR模式

    if (g_w25q128_priv.spi_mode == SPI_MODE_QSPI) {
        sCommand.DataMode = HAL_OSPI_DATA_4_LINES;  // 4线数据模式
    } else {
        sCommand.DataMode = HAL_OSPI_DATA_1_LINE;  // 1线数据模式
    }
    sCommand.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;  // 禁止数据DTR模式
    sCommand.NbData = NumByteToRead;                   // 数据长度

    if (g_w25q128_priv.spi_mode == SPI_MODE_QSPI) {
        sCommand.DummyCycles = 6;  // 空周期个数
    } else {
        sCommand.DummyCycles = 8;  // 空周期个数
    }
    sCommand.DQSMode = HAL_OSPI_DQS_DISABLE;           // 不使用DQS
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;  // 每次传输数据都发送指令

    // 写命令
    if (HAL_OSPI_Command(g_ospi_handle, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_TRANSMIT;  // 传输数据错误
    }
    //  接收数据
    if (HAL_OSPI_Receive(g_ospi_handle, pBuffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK) {
        return W25Qxx_ERROR_TRANSMIT;  // 传输数据错误
    }
    // 使用自动轮询标志位，等待接收的结束
    if (ospi_w25qxx_auto_polling_mem_ready() != OSPI_W25Qxx_OK) {
        return W25Qxx_ERROR_AUTOPOLLING;  // 轮询等待无响应
    }
    return OSPI_W25Qxx_OK;  // 读取数据成功
}

/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
