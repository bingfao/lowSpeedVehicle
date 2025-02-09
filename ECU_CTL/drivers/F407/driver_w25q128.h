/*
 * @Author: your name
 * @Date: 2025-02-09 11:08:12
 * @LastEditTime: 2025-02-09 17:21:26
 * @LastEditors: stone_honor
 * @Description: In User Settings Edit
 * @FilePath: \ebike_ECU\ECU_CTL\drivers\F407\driver_w25q128.h
 */

/*
 * ****************************************************************************
 * ******** Define to prevent recursive inclusion                      ********
 * ****************************************************************************
 */

#ifndef __DRIVER_W25Q128_H
#define __DRIVER_W25Q128_H
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
#include "driver_com.h"
#include "spi.h"
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
#define SPI_PORT_HANDLE                       &hspi1
#define SPI_CS_PIN                            SPI1_CS_Pin
#define SPI_CS_GPIO_PORT                      SPI1_CS_GPIO_Port

#define DRV_W25Q128_CMD_ERASE_BUFFER          DRV_CMD_ERASE_BUFFER
#define DRV_W25Q128_CMD_GET_BUFFER_TOTAL_SIZE DRV_CMD_GET_BUFFER_TOTAL_SIZE
#define DRV_W25Q128_CMD_GET_BUFFER_BLOCK_SIZE DRV_CMD_GET_BUFFER_BLOCK_SIZE
#define DRV_W25Q128_CMD_SET_DATA_MODE         DRV_CMD_SET_DATA_MODE

#define OSPI_W25Qxx_OK                        0   // W25Qxx通信正常
#define W25Qxx_ERROR_INIT                     -1  // 初始化错误
#define W25Qxx_ERROR_WriteEnable              -2  // 写使能错误
#define W25Qxx_ERROR_AUTOPOLLING              -3  // 轮询等待错误，无响应
#define W25Qxx_ERROR_Erase                    -4  // 擦除错误
#define W25Qxx_ERROR_TRANSMIT                 -5  // 传输错误
#define W25Qxx_ERROR_MemoryMapped             -6  // 内存映射模式错误

#define W25Qxx_CMD_EnableReset                0x66  // 使能复位
#define W25Qxx_CMD_ResetDevice                0x99  // 复位器件
#define W25Qxx_CMD_JedecID                    0x9F  // JEDEC ID
#define W25Qxx_CMD_WriteEnable                0X06  // 写使能

#define W25Qxx_CMD_SectorErase                0x20  // 扇区擦除，4K字节， 参考擦除时间 45ms
#define W25Qxx_CMD_BlockErase_32K             0x52  // 块擦除，  32K字节，参考擦除时间 120ms
#define W25Qxx_CMD_BlockErase_64K             0xD8  // 块擦除，  64K字节，参考擦除时间 150ms
#define W25Qxx_CMD_ChipErase                  0xC7  // 整片擦除，参考擦除时间 40S

#define W25Qxx_CMD_QuadInputPageProgram       0x32  // 1-1-4模式下(1线指令1线地址4线数据)，页编程指令，参考写入时间 0.4ms
#define W25Qxx_CMD_PageProgram                0x02  // 1-1-1模式下(1线指令1线地址1线数据)，页编程指令
#define W25Qxx_CMD_FastReadQuad_IO            0xEB  // 1-4-4模式下(1线指令4线地址4线数据)，快速读取指令
#define W25Qxx_CMD_FastRead                   0x0B  // 1-1-1模式下(1线指令1线地址1线数据)，快速读取指令
#define W25Qxx_CMD_ReadData                   0x03  // 1-1-1模式下(1线指令1线地址1线数据)，读取数据指令

#define W25Qxx_CMD_ReadStatus_REG1            0X05  // 读状态寄存器1
#define W25Qxx_Status_REG1_BUSY               0x01  // 读状态寄存器1的第0位（只读），Busy标志位，当正在擦除/写入数据/写命令时会被置1
#define W25Qxx_Status_REG1_WEL                0x02  // 读状态寄存器1的第1位（只读），WEL写使能标志位，该标志位为1时，代表可以进行写操作

#define W25Qxx_PageSize                       256        // 页大小，256字节
#define W25Qxx_SectorSize                     4096       // 扇区大小，4K字节
#define W25Qxx_FlashSize                      0x1000000  // W25Q128JV大小，16M字节
#define W25Qxx_FLASH_ID                       0xEF4018   // W25Q128JV JEDEC ID
#define W25Qxx_ChipErase_TIMEOUT_MAX          200000U    // 超时等待时间，W25Q128JV整片擦除所需最大时间是200S

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
#endif /*__DRIVER_W25Q128_H */
/*
 * ****************************************************************************
 * End File
 * ****************************************************************************
 */
