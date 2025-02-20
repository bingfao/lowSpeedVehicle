/*
 * @Author: your name
 * @Date: 2025-02-09 11:08:12
 * @LastEditTime: 2025-02-20 10:57:42
 * @LastEditors: DESKTOP-SPAS98O
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\F407\driver_w25q128.c
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
    SPI_HandleTypeDef *h_ospi;
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

static SPI_HandleTypeDef *g_ospi_handle = SPI_PORT_HANDLE;
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
static void spi_cs_control(uint8_t enable);
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
            p_priv->spi_mode = SPI_MODE_SPI;
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

static void spi_cs_control(uint8_t enable)
{
    if (enable == 0) {
        HAL_GPIO_WritePin(SPI_CS_GPIO_PORT, SPI_CS_PIN, GPIO_PIN_RESET);
    } else {
        HAL_GPIO_WritePin(SPI_CS_GPIO_PORT, SPI_CS_PIN, GPIO_PIN_SET);
    }
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
    HAL_StatusTypeDef ret;
    uint8_t send_buff[2] = {0};
    uint8_t rev_buff[2] = {0};  // 存储OSPI读到的数据
    uint32_t times = 2000;      // 超时时间

    send_buff[0] = W25Qxx_CMD_ReadStatus_REG1;  // 查询状态寄存器命令
    rev_buff[1] = 0;
    do {
        spi_cs_control(0);
        ret = HAL_SPI_TransmitReceive(g_ospi_handle, send_buff, rev_buff, 2, 1000);
        spi_cs_control(1);
        vTaskDelay(1);
        if (ret != HAL_OK) {
            return W25Qxx_ERROR_AUTOPOLLING;
        }
        times--;
    } while ((rev_buff[1] & W25Qxx_Status_REG1_BUSY) == W25Qxx_Status_REG1_BUSY &&
             times > 0);  // 等待 W25Qxx_Status_REG1_BUSY 位为0

    if (times == 0) {
        return W25Qxx_ERROR_AUTOPOLLING;  // 超时
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
    HAL_StatusTypeDef ret;
    uint8_t cmd[4] = {0};
    uint8_t rev_buff[4] = {0};  // 存储OSPI读到的数据
    uint32_t w25qxx_id;         // 器件的ID

    cmd[0] = W25Qxx_CMD_JedecID;  // 发送JEDEC ID命令
    spi_cs_control(0);
    ret = HAL_SPI_TransmitReceive(g_ospi_handle, (uint8_t *)&cmd, rev_buff, 4, 1000);
    spi_cs_control(1);
    if (ret != HAL_OK) {
        return W25Qxx_ERROR_INIT;
    }
    w25qxx_id = (rev_buff[1] << 16) | (rev_buff[2] << 8) | rev_buff[3];

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
    HAL_StatusTypeDef ret;
    uint8_t send_buff[2] = {0};
    uint8_t rev_buff[2] = {0};  // 存储OSPI读到的数据
    uint32_t times = 2000;      // 超时时间

    send_buff[0] = W25Qxx_CMD_WriteEnable;  // 写使能命令

    spi_cs_control(0);
    ret = HAL_SPI_Transmit(g_ospi_handle, (uint8_t *)send_buff, 1, 1000);
    spi_cs_control(1);
    if (ret != HAL_OK) {
        return W25Qxx_ERROR_WriteEnable;
    }
    send_buff[0] = W25Qxx_CMD_ReadStatus_REG1;  // 查询状态寄存器命令
    rev_buff[1] = 0;
    do {
        spi_cs_control(0);
        ret = HAL_SPI_TransmitReceive(g_ospi_handle, send_buff, rev_buff, 2, 1000);
        spi_cs_control(1);
        vTaskDelay(1);
        if (ret != HAL_OK) {
            return W25Qxx_ERROR_AUTOPOLLING;
        }
        times--;
    } while ((rev_buff[1] & W25Qxx_Status_REG1_WEL) != W25Qxx_Status_REG1_WEL &&
             times > 0);  // 等待 W25Qxx_Status_REG1_WEL 位为1

    if (times == 0) {
        return W25Qxx_ERROR_AUTOPOLLING;  // 超时
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
    uint8_t send_buff[4] = {0};

    send_buff[0] = W25Qxx_CMD_SectorErase;  // 扇区擦除指令，每次擦除4K字节
    send_buff[1] = (SectorAddress >> 16) & 0xFF;
    send_buff[2] = (SectorAddress >> 8) & 0xFF;
    send_buff[3] = SectorAddress & 0xFF;

    // 发送写使能
    if (ospi_w25qxx_write_enable() != OSPI_W25Qxx_OK) {
        return W25Qxx_ERROR_WriteEnable;  // 写使能失败
    }
    // 发送擦除指令
    spi_cs_control(0);
    if (HAL_SPI_Transmit(g_ospi_handle, send_buff, 4, 1000) != HAL_OK) {
        spi_cs_control(1);
        return W25Qxx_ERROR_AUTOPOLLING;  // 轮询等待无响应
    }
    spi_cs_control(1);
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
    uint8_t send_buff[4] = {0};

    send_buff[0] = W25Qxx_CMD_PageProgram;  // 页编程指令
    send_buff[1] = (WriteAddr >> 16) & 0xFF;
    send_buff[2] = (WriteAddr >> 8) & 0xFF;
    send_buff[3] = WriteAddr & 0xFF;

    // 写使能
    if (ospi_w25qxx_write_enable() != OSPI_W25Qxx_OK) {
        return W25Qxx_ERROR_WriteEnable;  // 写使能失败
    }
    // 写命令
    spi_cs_control(0);
    if (HAL_SPI_Transmit(g_ospi_handle, send_buff, 4, 1000) != HAL_OK) {
        spi_cs_control(1);
        return W25Qxx_ERROR_TRANSMIT;  // 传输数据错误
    }
    // 开始传输数据
    if (HAL_SPI_Transmit(g_ospi_handle, (uint8_t *)pBuffer, NumByteToWrite, 1000) != HAL_OK) {
        spi_cs_control(1);
        return W25Qxx_ERROR_TRANSMIT;  // 传输数据错误
    }
    spi_cs_control(1);
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
    uint8_t send_buff[4] = {0};  // 存储OSPI读到的数据

    send_buff[0] = W25Qxx_CMD_ReadData;  // 读数据指令
    send_buff[1] = (ReadAddr >> 16) & 0xFF;
    send_buff[2] = (ReadAddr >> 8) & 0xFF;
    send_buff[3] = ReadAddr & 0xFF;

    // 写命令
    spi_cs_control(0);
    if (HAL_SPI_Transmit(g_ospi_handle, send_buff, 4, 1000) != HAL_OK) {
        spi_cs_control(1);
        return W25Qxx_ERROR_TRANSMIT;  // 传输数据错误
    }
    //  接收数据
    if (HAL_SPI_Receive(g_ospi_handle, pBuffer, NumByteToRead, 1000) != HAL_OK) {
        spi_cs_control(1);
        return W25Qxx_ERROR_TRANSMIT;  // 传输数据错误
    }
    spi_cs_control(1);
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
