/**
 ******************************************************************************
 * @file    IAP/IAP_Main/Inc/ymodem.h
 * @author  MCD Application Team
 * @brief   This file provides all the software function headers of the ymodem.c
 *          file.
 ******************************************************************************
 *
 * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
 * You may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *        http://www.st.com/software_license_agreement_liberty_v2
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ******************************************************************************
 */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __YMODEM_H_
#define __YMODEM_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <string.h>

/* Exported types ------------------------------------------------------------*/

/**
 * @brief  Comm status structures definition
 */
typedef enum {
    COM_OK = 0x00,
    COM_ERROR = 0x01,
    COM_ABORT = 0x02,
    COM_TIMEOUT = 0x03,
    COM_DATA = 0x04,
    COM_LIMIT = 0x05
} COM_StatusTypeDef;

typedef int32_t (*YMODEM_PORT_RX_FUNC)(uint8_t *p_data, uint32_t p_length, uint32_t timeout);
typedef int32_t (*YMODEM_PORT_TX_FUNC)(uint8_t *p_data, uint32_t p_length, uint32_t timeout);
typedef int32_t (*YMODEM_PORT_DELAY_FUNC)(uint32_t ms);
typedef int32_t (*YMODEM_PORT_FLASH_WRITE_FUNC)(uint32_t addr, uint8_t *p_data, uint32_t p_length);
typedef int32_t (*YMODEM_PORT_FLASH_ERASE_FUNC)(uint32_t addr, uint32_t size);

/**
 * @}
 */

/* Exported constants --------------------------------------------------------*/
/* Packet structure defines */
#define PACKET_HEADER_SIZE   ((uint32_t)3)
#define PACKET_DATA_INDEX    ((uint32_t)4)
#define PACKET_START_INDEX   ((uint32_t)1)
#define PACKET_NUMBER_INDEX  ((uint32_t)2)
#define PACKET_CNUMBER_INDEX ((uint32_t)3)
#define PACKET_TRAILER_SIZE  ((uint32_t)2)
#define PACKET_OVERHEAD_SIZE (PACKET_HEADER_SIZE + PACKET_TRAILER_SIZE - 1)
#define PACKET_SIZE          ((uint32_t)128)
#define PACKET_1K_SIZE       ((uint32_t)1024)

/* /-------- Packet in IAP memory ------------------------------------------\
 * | 0      |  1    |  2     |  3   |  4      | ... | n+4     | n+5  | n+6  |
 * |------------------------------------------------------------------------|
 * | unused | start | number | !num | data[0] | ... | data[n] | crc0 | crc1 |
 * \------------------------------------------------------------------------/
 * the first byte is left unused for memory alignment reasons                 */

#define FILE_NAME_LENGTH     ((uint32_t)64)
#define FILE_SIZE_LENGTH     ((uint32_t)16)

#define SOH                  ((uint8_t)0x01)  /* start of 128-byte data packet */
#define STX                  ((uint8_t)0x02)  /* start of 1024-byte data packet */
#define EOT                  ((uint8_t)0x04)  /* end of transmission */
#define ACK                  ((uint8_t)0x06)  /* acknowledge */
#define NAK                  ((uint8_t)0x15)  /* negative acknowledge */
#define CA                   ((uint32_t)0x18) /* two of these in succession aborts transfer */
#define CRC16                ((uint8_t)0x43)  /* 'C' == 0x43, request 16-bit CRC */
#define NEGATIVE_BYTE        ((uint8_t)0xFF)

#define ABORT1               ((uint8_t)0x41) /* 'A' == 0x41, abort by user */
#define ABORT2               ((uint8_t)0x61) /* 'a' == 0x61, abort by user */

#define NAK_TIMEOUT          ((uint32_t)0x100000)
#define DOWNLOAD_TIMEOUT     ((uint32_t)5000) /* Five second retry delay */
#define MAX_ERRORS           ((uint32_t)5)

#define YMODEM_TRANS_OK      (0x00)
#define YMODEM_TRANS_ERROR   (0x01)
#define YMODEM_TRANS_BUSY    (0x02)
#define YMODEM_TRANS_TIMEOUT (0x03)

#define IS_CAP_LETTER(c)     (((c) >= 'A') && ((c) <= 'F'))
#define IS_LC_LETTER(c)      (((c) >= 'a') && ((c) <= 'f'))
#define IS_09(c)             (((c) >= '0') && ((c) <= '9'))
#define ISVALIDHEX(c)        (IS_CAP_LETTER(c) || IS_LC_LETTER(c) || IS_09(c))
#define ISVALIDDEC(c)        IS_09(c)
#define CONVERTDEC(c)        (c - '0')
#define CONVERTHEX_ALPHA(c)  (IS_CAP_LETTER(c) ? ((c) - 'A' + 10) : ((c) - 'a' + 10))
#define CONVERTHEX(c)        (IS_09(c) ? ((c) - '0') : CONVERTHEX_ALPHA(c))

typedef struct
{
    YMODEM_PORT_RX_FUNC rx_func;
    YMODEM_PORT_TX_FUNC tx_func;
    YMODEM_PORT_DELAY_FUNC delay_func;
    YMODEM_PORT_FLASH_WRITE_FUNC flash_write_func;
    YMODEM_PORT_FLASH_ERASE_FUNC flash_erase_func;
    uint32_t flash_addr;
    uint32_t flash_write_start_addr;
    uint32_t flash_area_addr;
    uint32_t flash_area_size;
    char file_name[FILE_NAME_LENGTH];
    uint32_t file_size;
    uint8_t init_flg;
} YMODEM_PORT_t;

/* Exported functions ------------------------------------------------------- */
// COM_StatusTypeDef Ymodem_Receive(uint32_t *p_size);
// COM_StatusTypeDef Ymodem_Transmit(uint8_t *p_buf, const uint8_t *p_file_name, uint32_t file_size);

int32_t ymodem_port_init(YMODEM_PORT_t *p_port, YMODEM_PORT_RX_FUNC rx_func, YMODEM_PORT_TX_FUNC tx_func);
int32_t ymodem_port_set_delay_func(YMODEM_PORT_t *p_port, YMODEM_PORT_DELAY_FUNC delay_func);
int32_t ymodem_port_set_flash_write_func(YMODEM_PORT_t *p_port, YMODEM_PORT_FLASH_WRITE_FUNC flash_write_func);
int32_t ymodem_port_set_flash_erase_func(YMODEM_PORT_t *p_port, YMODEM_PORT_FLASH_ERASE_FUNC flash_read_func);
int32_t ymodem_port_set_flash_area(YMODEM_PORT_t *p_port, uint32_t flash_area_addr, uint32_t flash_area_size);
int32_t ymodem_port_receive_start(YMODEM_PORT_t *p_port, char *rx_file_name, uint32_t *size, uint32_t timeout);
int32_t ymodem_port_set_flash_write_start(YMODEM_PORT_t *p_port, uint32_t addr);

#endif /* __YMODEM_H_ */

/*******************(C)COPYRIGHT STMicroelectronics ********END OF FILE********/
