/**
 ******************************************************************************
 * @file    IAP/IAP_Main/Src/ymodem.c
 * @author  MCD Application Team
 * @brief   This file provides all the software functions related to the ymodem
 *          protocol.
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2017 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/** @addtogroup STM32F4xx_IAP_Main
 * @{
 */

/* Includes ------------------------------------------------------------------*/
#include "ymodem.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
#define CRC16_F /* activate the CRC16 integrity */
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* @note ATTENTION - please keep this variable 32bit aligned */
uint8_t aPacketData[PACKET_1K_SIZE + PACKET_DATA_INDEX + PACKET_TRAILER_SIZE];

uint8_t aFileName[FILE_NAME_LENGTH];

/* Private function prototypes -----------------------------------------------*/
static void PrepareIntialPacket(uint8_t *p_data, const uint8_t *p_file_name, uint32_t length);
static void PreparePacket(uint8_t *p_source, uint8_t *p_packet, uint8_t pkt_nr, uint32_t size_blk);
static int32_t ReceivePacket(YMODEM_PORT_t *p_port, uint8_t *p_data, uint32_t *p_length, uint32_t timeout);
static int32_t SendPacket(YMODEM_PORT_t *p_port, uint8_t *p_data, uint32_t p_length, uint32_t timeout);
static int32_t SendByte(YMODEM_PORT_t *p_port, uint8_t byte);
uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte);
uint16_t Cal_CRC16(const uint8_t *p_data, uint32_t size);
uint8_t CalcChecksum(const uint8_t *p_data, uint32_t size);
static void int_2_str(uint8_t *p_str, uint32_t intnum);
static uint32_t str_2_int(uint8_t *p_inputstr, uint32_t *p_intnum);

/* Private functions ---------------------------------------------------------*/

/**
 * @brief  Receive a packet from sender
 * @param  data
 * @param  length
 *     0: end of transmission
 *     2: abort by sender
 *    >0: packet length
 * @param  timeout
 * @retval HAL_OK: normally return
 *         HAL_BUSY: abort by user
 */
static int32_t ReceivePacket(YMODEM_PORT_t *p_port, uint8_t *p_data, uint32_t *p_length, uint32_t timeout)
{
    uint32_t crc;
    uint32_t packet_size = 0;
    int32_t ret;
    int32_t status = 0;
    uint8_t char1;

    *p_length = 0;
    if (p_port->rx_func == NULL) {
        return -1;
    }
    ret = p_port->rx_func(&char1, 1, timeout);

    if (ret >= 0) {
        status = 0;
        switch (char1) {
            case SOH:
                packet_size = PACKET_SIZE;
                break;
            case STX:
                packet_size = PACKET_1K_SIZE;
                break;
            case EOT:
                break;
            case CA:
                if ((p_port->rx_func(&char1, 1, timeout) >= 0) && (char1 == CA)) {
                    packet_size = 2;
                } else {
                    status = YMODEM_TRANS_ERROR;
                }
                break;
            case ABORT1:
            case ABORT2:
                status = YMODEM_TRANS_BUSY;
                break;
            default:
                status = YMODEM_TRANS_ERROR;
                break;
        }
        *p_data = char1;
        *p_length += 1;

        if (packet_size >= PACKET_SIZE) {
            ret = p_port->rx_func(&p_data[PACKET_NUMBER_INDEX], packet_size + PACKET_OVERHEAD_SIZE, timeout);
            /* Simple packet sanity check */
            if (ret >= 0) {
                status = 0;
                if (p_data[PACKET_NUMBER_INDEX] != ((p_data[PACKET_CNUMBER_INDEX]) ^ NEGATIVE_BYTE)) {
                    packet_size = 0;
                    status = YMODEM_TRANS_ERROR;
                } else {
                    /* Check packet CRC */
                    crc = p_data[packet_size + PACKET_DATA_INDEX] << 8;
                    crc += p_data[packet_size + PACKET_DATA_INDEX + 1];
                    if (Cal_CRC16(&p_data[PACKET_DATA_INDEX], packet_size) != crc) {
                        packet_size = 0;
                        status = YMODEM_TRANS_ERROR;
                    }
                }
            } else {
                packet_size = 0;
                status = YMODEM_TRANS_TIMEOUT;
            }
        }
    } else {
        status = YMODEM_TRANS_TIMEOUT;
    }
    *p_length = packet_size;

    return status;
}

static int32_t SendPacket(YMODEM_PORT_t *p_port, uint8_t *p_data, uint32_t p_length, uint32_t timeout)
{
    int32_t ret = 0;

    if (p_port->tx_func == NULL) {
        return -1;
    }
    ret = p_port->tx_func(p_data, p_length, timeout);
    if (ret > 0) {
        ret = p_length;
    }

    return ret;
}

static int32_t SendByte(YMODEM_PORT_t *p_port, uint8_t byte)
{
    int32_t ret = 0;
    if (p_port->tx_func == NULL) {
        return -1;
    }
    ret = p_port->tx_func(&byte, 1, 100);

    return ret;
}

/**
 * @brief  Prepare the first block
 * @param  p_data:  output buffer
 * @param  p_file_name: name of the file to be sent
 * @param  length: length of the file to be sent in bytes
 * @retval None
 */
static void PrepareIntialPacket(uint8_t *p_data, const uint8_t *p_file_name, uint32_t length)
{
    uint32_t i, j = 0;
    uint8_t astring[10];

    /* first 3 bytes are constant */
    p_data[PACKET_START_INDEX] = SOH;
    p_data[PACKET_NUMBER_INDEX] = 0x00;
    p_data[PACKET_CNUMBER_INDEX] = 0xff;

    /* Filename written */
    for (i = 0; (p_file_name[i] != '\0') && (i < FILE_NAME_LENGTH); i++) {
        p_data[i + PACKET_DATA_INDEX] = p_file_name[i];
    }

    p_data[i + PACKET_DATA_INDEX] = 0x00;

    /* file size written */
    int_2_str(astring, length);
    i = i + PACKET_DATA_INDEX + 1;
    while (astring[j] != '\0') {
        p_data[i++] = astring[j++];
    }

    /* padding with zeros */
    for (j = i; j < PACKET_SIZE + PACKET_DATA_INDEX; j++) {
        p_data[j] = 0;
    }
}

/**
 * @brief  Prepare the data packet
 * @param  p_source: pointer to the data to be sent
 * @param  p_packet: pointer to the output buffer
 * @param  pkt_nr: number of the packet
 * @param  size_blk: length of the block to be sent in bytes
 * @retval None
 */
static void PreparePacket(uint8_t *p_source, uint8_t *p_packet, uint8_t pkt_nr, uint32_t size_blk)
{
    uint8_t *p_record;
    uint32_t i, size, packet_size;

    /* Make first three packet */
    packet_size = size_blk >= PACKET_1K_SIZE ? PACKET_1K_SIZE : PACKET_SIZE;
    size = size_blk < packet_size ? size_blk : packet_size;
    if (packet_size == PACKET_1K_SIZE) {
        p_packet[PACKET_START_INDEX] = STX;
    } else {
        p_packet[PACKET_START_INDEX] = SOH;
    }
    p_packet[PACKET_NUMBER_INDEX] = pkt_nr;
    p_packet[PACKET_CNUMBER_INDEX] = (~pkt_nr);
    p_record = p_source;

    /* Filename packet has valid data */
    for (i = PACKET_DATA_INDEX; i < size + PACKET_DATA_INDEX; i++) {
        p_packet[i] = *p_record++;
    }
    if (size <= packet_size) {
        for (i = size + PACKET_DATA_INDEX; i < packet_size + PACKET_DATA_INDEX; i++) {
            p_packet[i] = 0x1A; /* EOF (0x1A) or 0x00 */
        }
    }
}

/**
 * @brief  Update CRC16 for input byte
 * @param  crc_in input value
 * @param  input byte
 * @retval None
 */
uint16_t UpdateCRC16(uint16_t crc_in, uint8_t byte)
{
    uint32_t crc = crc_in;
    uint32_t in = byte | 0x100;

    do {
        crc <<= 1;
        in <<= 1;
        if (in & 0x100)
            ++crc;
        if (crc & 0x10000)
            crc ^= 0x1021;
    }

    while (!(in & 0x10000));

    return crc & 0xffffu;
}

/**
 * @brief  Cal CRC16 for YModem Packet
 * @param  data
 * @param  length
 * @retval None
 */
uint16_t Cal_CRC16(const uint8_t *p_data, uint32_t size)
{
    uint32_t crc = 0;
    const uint8_t *dataEnd = p_data + size;

    while (p_data < dataEnd) crc = UpdateCRC16(crc, *p_data++);

    crc = UpdateCRC16(crc, 0);
    crc = UpdateCRC16(crc, 0);

    return crc & 0xffffu;
}

/**
 * @brief  Calculate Check sum for YModem Packet
 * @param  p_data Pointer to input data
 * @param  size length of input data
 * @retval uint8_t checksum value
 */
uint8_t CalcChecksum(const uint8_t *p_data, uint32_t size)
{
    uint32_t sum = 0;
    const uint8_t *p_data_end = p_data + size;

    while (p_data < p_data_end) {
        sum += *p_data++;
    }

    return (sum & 0xffu);
}

/**
 * @brief  Convert an Integer to a string
 * @param  p_str: The string output pointer
 * @param  intnum: The integer to be converted
 * @retval None
 */
static void int_2_str(uint8_t *p_str, uint32_t intnum)
{
    uint32_t i, divider = 1000000000, pos = 0, status = 0;

    for (i = 0; i < 10; i++) {
        p_str[pos++] = (intnum / divider) + 48;

        intnum = intnum % divider;
        divider /= 10;
        if ((p_str[pos - 1] == '0') & (status == 0)) {
            pos = 0;
        } else {
            status++;
        }
    }
}

/**
 * @brief  Convert a string to an integer
 * @param  p_inputstr: The string to be converted
 * @param  p_intnum: The integer value
 * @retval 1: Correct
 *         0: Error
 */
static uint32_t str_2_int(uint8_t *p_inputstr, uint32_t *p_intnum)
{
    uint32_t i = 0, res = 0;
    uint32_t val = 0;

    if ((p_inputstr[0] == '0') && ((p_inputstr[1] == 'x') || (p_inputstr[1] == 'X'))) {
        i = 2;
        while ((i < 11) && (p_inputstr[i] != '\0')) {
            if (ISVALIDHEX(p_inputstr[i])) {
                val = (val << 4) + CONVERTHEX(p_inputstr[i]);
            } else {
                /* Return 0, Invalid input */
                res = 0;
                break;
            }
            i++;
        }

        /* valid result */
        if (p_inputstr[i] == '\0') {
            *p_intnum = val;
            res = 1;
        }
    } else /* max 10-digit decimal input */
    {
        while ((i < 11) && (res != 1)) {
            if (p_inputstr[i] == '\0') {
                *p_intnum = val;
                /* return 1 */
                res = 1;
            } else if (((p_inputstr[i] == 'k') || (p_inputstr[i] == 'K')) && (i > 0)) {
                val = val << 10;
                *p_intnum = val;
                res = 1;
            } else if (((p_inputstr[i] == 'm') || (p_inputstr[i] == 'M')) && (i > 0)) {
                val = val << 20;
                *p_intnum = val;
                res = 1;
            } else if (ISVALIDDEC(p_inputstr[i])) {
                val = val * 10 + CONVERTDEC(p_inputstr[i]);
            } else {
                /* return 0, Invalid input */
                res = 0;
                break;
            }
            i++;
        }
    }

    return res;
}

/* Public functions ---------------------------------------------------------*/
/**
 * @brief  Receive a file using the ymodem protocol with CRC16.
 * @param  p_size The size of the file.
 * @retval COM_StatusTypeDef result of reception/programming
 */
COM_StatusTypeDef Ymodem_Receive(YMODEM_PORT_t *p_port, uint32_t *p_size, uint32_t timeout)
{
    uint32_t i, packet_length, session_done = 0, file_done, errors = 0, session_begin = 0;
    // uint32_t flashdestination;
    uint32_t ramsource, filesize, packets_received;
    uint8_t *file_ptr;
    uint8_t file_size[FILE_SIZE_LENGTH], tmp;
    COM_StatusTypeDef result = COM_OK;
    int32_t ret = 0;
    int32_t rec_ret = 0;
    uint32_t times = 0;

    while ((session_done == 0) && (result == COM_OK)) {
        packets_received = 0;
        file_done = 0;
        while ((file_done == 0) && (result == COM_OK)) {
            rec_ret = ReceivePacket(p_port, aPacketData, &packet_length, DOWNLOAD_TIMEOUT);
            if (rec_ret == YMODEM_TRANS_TIMEOUT) {
                times += DOWNLOAD_TIMEOUT;
                if (times >= timeout) {
                    rec_ret = YMODEM_TRANS_BUSY;
                }
            }
            switch (rec_ret) {
                case 0:
                    errors = 0;
                    switch (packet_length) {
                        case 2:
                            /* Abort by sender */
                            SendByte(p_port, ACK);
                            result = COM_ABORT;
                            break;
                        case 0:
                            /* End of transmission */
                            SendByte(p_port, ACK);
                            file_done = 1;
                            break;
                        default:
                            /* Normal packet */
                            if (aPacketData[PACKET_NUMBER_INDEX] != (uint8_t)packets_received) {
                                SendByte(p_port, NAK);
                            } else {
                                if (packets_received == 0) {
                                    /* File name packet */
                                    if (aPacketData[PACKET_DATA_INDEX] != 0) {
                                        /* File name extraction */
                                        i = 0;
                                        file_ptr = aPacketData + PACKET_DATA_INDEX;
                                        while ((*file_ptr != 0) && (i < FILE_NAME_LENGTH)) {
                                            aFileName[i++] = *file_ptr++;
                                        }

                                        /* File size extraction */
                                        aFileName[i++] = '\0';
                                        strcpy(p_port->file_name, (char *)aFileName);
                                        i = 0;
                                        file_ptr++;
                                        while ((*file_ptr != ' ') && (i < FILE_SIZE_LENGTH)) {
                                            file_size[i++] = *file_ptr++;
                                        }
                                        file_size[i++] = '\0';
                                        str_2_int(file_size, &filesize);
                                        p_port->file_size = filesize;
                                        /* Test the size of the image to be sent */
                                        /* Image size is greater than Flash size */
                                        if (*p_size > (p_port->flash_area_size + 1)) {
                                            /* End session */
                                            tmp = CA;
                                            SendPacket(p_port, &tmp, 1, NAK_TIMEOUT);
                                            SendPacket(p_port, &tmp, 1, NAK_TIMEOUT);
                                            result = COM_LIMIT;
                                        }
                                        /* erase user application area */
                                        p_port->flash_erase_func(p_port->flash_addr, p_port->flash_area_size);
                                        *p_size = filesize;

                                        SendByte(p_port, ACK);
                                        SendByte(p_port, CRC16);
                                    }
                                    /* File header packet is empty, end session */
                                    else {
                                        SendByte(p_port, ACK);
                                        file_done = 1;
                                        session_done = 1;
                                        break;
                                    }
                                } else /* Data packet */
                                {
                                    ramsource = (uint32_t)&aPacketData[PACKET_DATA_INDEX];
                                    /* Write received data in Flash */
                                    ret = p_port->flash_write_func(p_port->flash_addr, (uint8_t *)ramsource,
                                                                   packet_length);
                                    if (ret == packet_length || ret == 0) {
                                        p_port->flash_addr += packet_length;
                                        SendByte(p_port, ACK);
                                    } else /* An error occurred while writing to Flash memory */
                                    {
                                        /* End session */
                                        SendByte(p_port, CA);
                                        SendByte(p_port, CA);
                                        result = COM_DATA;
                                    }
                                }
                                packets_received++;
                                session_begin = 1;
                            }
                            break;
                    }
                    break;
                case YMODEM_TRANS_BUSY: /* Abort actually */
                    SendByte(p_port, CA);
                    SendByte(p_port, CA);
                    result = COM_ABORT;
                    break;
                default:
                    if (session_begin > 0) {
                        errors++;
                    }
                    if (errors > MAX_ERRORS) {
                        /* Abort communication */
                        SendByte(p_port, CA);
                        SendByte(p_port, CA);
                        return COM_ERROR;
                    } else {
                        SendByte(p_port, CRC16); /* Ask for a packet */
                    }
                    break;
            }
        }
    }
    return result;
}

/**
 * @brief  Transmit a file using the ymodem protocol
 * @param  p_buf: Address of the first byte
 * @param  p_file_name: Name of the file sent
 * @param  file_size: Size of the transmission
 * @retval COM_StatusTypeDef result of the communication
 */
COM_StatusTypeDef Ymodem_Transmit(YMODEM_PORT_t *p_port, uint8_t *p_buf, const uint8_t *p_file_name, uint32_t file_size)
{
    uint32_t errors = 0, ack_recpt = 0, size = 0, pkt_size;
    uint8_t *p_buf_int;
    COM_StatusTypeDef result = COM_OK;
    uint32_t blk_number = 1;
    uint8_t a_rx_ctrl[2];
    uint8_t i;
#ifdef CRC16_F
    uint32_t temp_crc;
#else  /* CRC16_F */
    uint8_t temp_chksum;
#endif /* CRC16_F */

    /* Prepare first block - header */
    PrepareIntialPacket(aPacketData, p_file_name, file_size);

    while ((!ack_recpt) && (result == COM_OK)) {
        /* Send Packet */
        SendPacket(p_port, &aPacketData[PACKET_START_INDEX], PACKET_SIZE + PACKET_HEADER_SIZE, NAK_TIMEOUT);

        /* Send CRC or Check Sum based on CRC16_F */
#ifdef CRC16_F
        temp_crc = Cal_CRC16(&aPacketData[PACKET_DATA_INDEX], PACKET_SIZE);
        SendByte(p_port, temp_crc >> 8);
        SendByte(p_port, temp_crc & 0xFF);
#else  /* CRC16_F */
        temp_chksum = CalcChecksum(&aPacketData[PACKET_DATA_INDEX], PACKET_SIZE);
        serial_put_byte(temp_chksum);
#endif /* CRC16_F */

        /* Wait for Ack and 'C' */
        if (p_port->rx_func(&a_rx_ctrl[0], 1, NAK_TIMEOUT) >= 0) {
            if (a_rx_ctrl[0] == ACK) {
                ack_recpt = 1;
            } else if (a_rx_ctrl[0] == CA) {
                if ((p_port->rx_func(&a_rx_ctrl[0], 1, NAK_TIMEOUT) >= 0) && (a_rx_ctrl[0] == CA)) {
                    if (p_port->delay_func != NULL) {
                        p_port->delay_func(2);
                    }
                    // __HAL_UART_FLUSH_DRREGISTER(serial_dbg);
                    result = COM_ABORT;
                }
            }
        } else {
            errors++;
        }
        if (errors >= MAX_ERRORS) {
            result = COM_ERROR;
        }
    }

    p_buf_int = p_buf;
    size = file_size;

    /* Here 1024 bytes length is used to send the packets */
    while ((size) && (result == COM_OK)) {
        /* Prepare next packet */
        PreparePacket(p_buf_int, aPacketData, blk_number, size);
        ack_recpt = 0;
        a_rx_ctrl[0] = 0;
        errors = 0;

        /* Resend packet if NAK for few times else end of communication */
        while ((!ack_recpt) && (result == COM_OK)) {
            /* Send next packet */
            if (size >= PACKET_1K_SIZE) {
                pkt_size = PACKET_1K_SIZE;
            } else {
                pkt_size = PACKET_SIZE;
            }
            SendPacket(p_port, &aPacketData[PACKET_START_INDEX], pkt_size + PACKET_HEADER_SIZE, NAK_TIMEOUT);

            /* Send CRC or Check Sum based on CRC16_F */
#ifdef CRC16_F
            temp_crc = Cal_CRC16(&aPacketData[PACKET_DATA_INDEX], pkt_size);
            SendByte(p_port, temp_crc >> 8);
            SendByte(p_port, temp_crc & 0xFF);
#else  /* CRC16_F */
            temp_chksum = CalcChecksum(&aPacketData[PACKET_DATA_INDEX], pkt_size);
            SendByte(p_port, temp_chksum);
#endif /* CRC16_F */

            /* Wait for Ack */
            if ((p_port->rx_func(&a_rx_ctrl[0], 1, NAK_TIMEOUT) >= 0) && (a_rx_ctrl[0] == ACK)) {
                ack_recpt = 1;
                if (size > pkt_size) {
                    p_buf_int += pkt_size;
                    size -= pkt_size;
                    if (blk_number == (p_port->flash_area_size / PACKET_1K_SIZE)) {
                        result = COM_LIMIT; /* boundary error */
                    } else {
                        blk_number++;
                    }
                } else {
                    p_buf_int += pkt_size;
                    size = 0;
                }
            } else {
                errors++;
            }

            /* Resend packet if NAK  for a count of 10 else end of communication */
            if (errors >= MAX_ERRORS) {
                result = COM_ERROR;
            }
        }
    }

    /* Sending End Of Transmission char */
    ack_recpt = 0;
    a_rx_ctrl[0] = 0x00;
    errors = 0;
    while ((!ack_recpt) && (result == COM_OK)) {
        SendByte(p_port, EOT);

        /* Wait for Ack */
        if (p_port->rx_func(&a_rx_ctrl[0], 1, NAK_TIMEOUT) >= 0) {
            if (a_rx_ctrl[0] == ACK) {
                ack_recpt = 1;
            } else if (a_rx_ctrl[0] == CA) {
                if ((p_port->rx_func(&a_rx_ctrl[0], 1, NAK_TIMEOUT) >= 0) && (a_rx_ctrl[0] == CA)) {
                    if (p_port->delay_func != NULL) {
                        p_port->delay_func(2);
                    }
                    // __HAL_UART_FLUSH_DRREGISTER(serial_dbg);
                    result = COM_ABORT;
                }
            }
        } else {
            errors++;
        }

        if (errors >= MAX_ERRORS) {
            result = COM_ERROR;
        }
    }

    /* Empty packet sent - some terminal emulators need this to close session */
    if (result == COM_OK) {
        /* Preparing an empty packet */
        aPacketData[PACKET_START_INDEX] = SOH;
        aPacketData[PACKET_NUMBER_INDEX] = 0;
        aPacketData[PACKET_CNUMBER_INDEX] = 0xFF;
        for (i = PACKET_DATA_INDEX; i < (PACKET_SIZE + PACKET_DATA_INDEX); i++) {
            aPacketData[i] = 0x00;
        }

        /* Send Packet */
        SendPacket(p_port, &aPacketData[PACKET_START_INDEX], PACKET_SIZE + PACKET_HEADER_SIZE, NAK_TIMEOUT);

        /* Send CRC or Check Sum based on CRC16_F */
#ifdef CRC16_F
        temp_crc = Cal_CRC16(&aPacketData[PACKET_DATA_INDEX], PACKET_SIZE);
        SendByte(p_port, temp_crc >> 8);
        SendByte(p_port, temp_crc & 0xFF);
#else  /* CRC16_F */
        temp_chksum = CalcChecksum(&aPacketData[PACKET_DATA_INDEX], PACKET_SIZE);
        SendByte(p_port, temp_chksum);
#endif /* CRC16_F */

        /* Wait for Ack and 'C' */
        if (p_port->rx_func(&a_rx_ctrl[0], 1, NAK_TIMEOUT) >= 0) {
            if (a_rx_ctrl[0] == CA) {
                if (p_port->delay_func != NULL) {
                    p_port->delay_func(2);
                }
                // __HAL_UART_FLUSH_DRREGISTER(serial_dbg);
                result = COM_ABORT;
            }
        }
    }

    return result; /* file transmitted successfully */
}

int32_t ymodem_port_init(YMODEM_PORT_t *p_port, YMODEM_PORT_RX_FUNC rx_func, YMODEM_PORT_TX_FUNC tx_func)
{
    if (p_port == NULL || rx_func == NULL || tx_func == NULL) {
        return -1;
    }
    memset(p_port, 0, sizeof(YMODEM_PORT_t));
    p_port->rx_func = rx_func;
    p_port->tx_func = tx_func;
    p_port->init_flg = 1;
    return 0;
}

int32_t ymodem_port_set_delay_func(YMODEM_PORT_t *p_port, YMODEM_PORT_DELAY_FUNC delay_func)
{
    if (p_port == NULL) {
        return -1;
    }
    p_port->delay_func = delay_func;
    return 0;
}

int32_t ymodem_port_set_flash_write_func(YMODEM_PORT_t *p_port, YMODEM_PORT_FLASH_WRITE_FUNC flash_write_func)
{
    if (p_port == NULL) {
        return -1;
    }
    p_port->flash_write_func = flash_write_func;
    return 0;
}
int32_t ymodem_port_set_flash_erase_func(YMODEM_PORT_t *p_port, YMODEM_PORT_FLASH_ERASE_FUNC flash_erase_func)
{
    if (p_port == NULL) {
        return -1;
    }
    p_port->flash_erase_func = flash_erase_func;
    return 0;
}

int32_t ymodem_port_set_flash_area(YMODEM_PORT_t *p_port, uint32_t flash_area_addr, uint32_t flash_area_size)
{
    if (p_port == NULL) {
        return -1;
    }
    p_port->flash_area_addr = flash_area_addr;
    p_port->flash_area_size = flash_area_size;
    return 0;
}

int32_t ymodem_port_set_flash_write_start(YMODEM_PORT_t *p_port, uint32_t addr)
{
    if (p_port == NULL) {
        return -1;
    }
    p_port->flash_write_start_addr = addr;

    return 0;
}

int32_t ymodem_port_receive_start(YMODEM_PORT_t *p_port, char *rx_file_name, uint32_t *size, uint32_t timeout)
{
    COM_StatusTypeDef status = COM_OK;
    uint32_t file_size = 0;

    if (p_port == NULL || p_port->init_flg == 0 || p_port->flash_write_start_addr == 0 || p_port->tx_func == NULL ||
        p_port->rx_func == NULL || p_port->flash_write_func == NULL || p_port->flash_erase_func == NULL) {
        return -1;
    }
    p_port->flash_addr = p_port->flash_write_start_addr;
    status = Ymodem_Receive(p_port, &file_size, timeout);
    if (status == COM_OK) {
        *size = file_size;
        strncpy(rx_file_name, p_port->file_name, FILE_NAME_LENGTH);
    } else {
        return -1;
    }

    return 0;
}

/**
 * @}
 */

/*******************(C)COPYRIGHT 2016 STMicroelectronics *****END OF FILE****/
