/**
 ***********************************************************************************************************************
 * @file    bms_bootloader.c
 * @brief   BMS串口通信模块，实现数据收发、协议解析、加解密、CRC校验及通信状态管理
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "main.h"
#include "bsp_flash.h"
#include "bsp_bms_usart.h"

#include "queue.h"
#include "param.h"
#include "xxtea.h"
#include "crc.h"

#include "bms_bootloader.h"

#include "app_bootloader.h"

#include "stdio.h"
#include "stdlib.h"
/* Macro -------------------------------------------------------------------------------------------------------------*/

#define HARDWARE_MAJOR_VERSION_NUMBER      (0)
#define HARDWARE_REVISION_VERSION_NUMBER   (3)
#define HARDWARE_VERSION                   ((HARDWARE_MAJOR_VERSION_NUMBER << 8) | HARDWARE_REVISION_VERSION_NUMBER)

#define SOFTWARE_MAJOR_VERSION_NUMBER      (3)
#define SOFTWARE_REVISION_VERSION_NUMBER   (7)
#define SOFTWARE_VERSION                   ((SOFTWARE_MAJOR_VERSION_NUMBER << 8) | SOFTWARE_REVISION_VERSION_NUMBER)

#define TX_QUEUE_BUFFER_SIZE               (128)
#define RX_QUEUE_BUFFER_SIZE               (256)
#define TX_DATA_BUFFER_SIZE                (128)
#define RX_DATA_BUFFER_SIZE                (256)

#define HEAD_CODE_BYTE_0                   (0x55)
#define HEAD_CODE_BYTE_1                   (0xAA)

#define FRAME_SIZE_MIN                     (11)

#define COMMUNICATION_DISCONNECTED_TIME    2000
#define COMMUNICATION_TIMEOUT_TIME         3000

/* Variables ---------------------------------------------------------------------------------------------------------*/
serial_data_t TxData;
queue_t       TxQueue;
uint8_t       TxDataBuffer[TX_DATA_BUFFER_SIZE];
uint8_t       TxQueueBuffer[TX_QUEUE_BUFFER_SIZE];

serial_data_t RxData;
queue_t       RxQueue;
uint8_t       RxDataBuffer[RX_DATA_BUFFER_SIZE];
uint8_t       RxQueueBuffer[RX_QUEUE_BUFFER_SIZE];

data_block_union_t DataBlock;
random_number_t    RandomNumber;

state_control_frame_union_t StateControlFrame;
state_report_frame_union_t  StateReportFrame;

/* External variables ------------------------------------------------------------------------------------------------*/
/* Constant ----------------------------------------------------------------------------------------------------------*/

const char __attribute__((used)) hardware_version[]          = "hardware version: V03";
const char __attribute__((used)) firmware_version[]          = "firmware version: V3.6";
const char __attribute__((used)) oldest_compatible_version[] = "oldest compatible version: V3.0";

const uint32_t Key[4] = {0x48455452, 0x44534A57, 0x50593332, 0x4D30504C};


/*******************************************************************************
//  Function   : DataEncrypt
//  Description: 使用XXTEA算法对指定数据缓冲区进行加密
//  Param      : buffer - 待加密数据缓冲区
//               length - 待加密数据长度，单位Byte
//  Return     : None
//  Others     : 数据长度按32位字进行XXTEA加密处理
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void DataEncrypt(uint8_t *buffer, uint8_t length)
{
    memcpy(DataBlock.ByteBuffer, buffer, length);
    xxtea_encrypt(DataBlock.WordBuffer, length / 4, Key);
    memcpy(buffer, DataBlock.ByteBuffer, length);
}

/*******************************************************************************
//  Function   : DataDecrypt
//  Description: 使用XXTEA算法对指定数据缓冲区进行解密
//  Param      : buffer - 待解密数据缓冲区
//               length - 待解密数据长度，单位Byte
//  Return     : None
//  Others     : 数据长度按32位字进行XXTEA解密处理
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void DataDecrypt(uint8_t *buffer, uint8_t length)
{
    memcpy(DataBlock.ByteBuffer, buffer, length);
    xxtea_decrypt(DataBlock.WordBuffer, length / 4, Key);
    memcpy(buffer, DataBlock.ByteBuffer, length);
}

/*******************************************************************************
//  Function   : DataTransmitStartProcess
//  Description: 将待发送数据写入发送队列并启动半双工串口发送
//  Param      : queue - 发送队列指针
//               src   - 待发送串口数据结构指针
//  Return     : true - 发送启动完成
//  Others     : 首字节立即送入串口，后续数据由发送回调继续发送
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
bool DataTransmitStartProcess(queue_t *queue, serial_data_t *src)
{
    uint8_t temp;

    QueueDataIn(queue, src->Buffer, src->DataLength);
    QueueDataOut(queue, &temp, 1);
    bsp_bms_usart_HalfDuplexTx();
    bsp_bms_usart_TransmitByte(temp);
    bsp_bms_usart_TransmitStart();

    return true;
}

/*******************************************************************************
//  Function   : DataReceiveStartProcess
//  Description: 从接收队列中提取数据并按通信帧格式组帧
//  Param      : queue - 接收队列指针
//               src   - 接收串口数据结构指针
//  Return     : true  - 接收到一帧完整数据
//               false - 当前数据不足、帧异常或缓冲区溢出
//  Others     : 根据帧头和长度字段判断完整帧，并进行接收缓冲区边界检查
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
bool DataReceiveStartProcess(queue_t *queue, serial_data_t *src)
{
    uint8_t temp;

    while (!IsQueueEmpty(queue))
    {
        QueueDataOut(queue, &temp, 1);
        switch (src->Index)
        {
        case 0:
            if (temp == HEAD_CODE_BYTE_0)
            {
                src->Buffer[src->Index++] = temp;
            }
            break;

        case 1:
            if (temp == HEAD_CODE_BYTE_1)
            {
                src->Buffer[src->Index++] = temp;
            }
            break;

        case 2:
            src->DataLength = temp;
            src->Buffer[src->Index++] = temp;
            break;

        default:
            if (src->Index + 1 >= src->BufferSize)
            {
                src->Index = 0;
                return false;
            }
            src->Buffer[src->Index++] = temp;
            if (src->Index == src->DataLength)
            {
                src->Index = 0;
                return true;
            }
            break;
        }
    }

    return false;
}

/*******************************************************************************
//  Function   : USART_TxEventCallback
//  Description: 串口发送事件回调，逐字节发送发送队列中的数据
//  Param      : None
//  Return     : None
//  Others     : 队列发送完成后停止串口发送
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void bsp_bms_usart_TxEventCallback(void)
{
    uint8_t temp;

    if (!IsQueueEmpty(&TxQueue))
    {
        QueueDataOut(&TxQueue, &temp, 1);
        bsp_bms_usart_TransmitByte(temp);
    }
    else
    {
        bsp_bms_usart_TransmitStop();
    }
}

/*******************************************************************************
//  Function   : USART_RxEventCallback
//  Description: 串口接收事件回调，将接收到的字节写入接收队列
//  Param      : None
//  Return     : None
//  Others     : 接收队列已满时丢弃当前接收字节
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void bsp_bms_usart_RxEventCallback(void)
{
    uint8_t temp = bsp_bms_usart_ReceiveByte();
    if (!IsQueueFull(&RxQueue))
    {
        QueueDataIn(&RxQueue, &temp, 1);
    }
}

/*******************************************************************************
//  Function   : SerialCommuncaitionInit
//  Description: 初始化串口通信数据结构、随机数种子及半双工接收状态
//  Param      : None
//  Return     : None
//  Others     : 使用芯片UID生成随机数种子，并启动BMS串口接收
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void SerialCommuncaitionInit(void)
{
    TxQueue.Buffer     = TxQueueBuffer;
    TxQueue.BufferSize = sizeof(TxQueueBuffer);
    RxQueue.Buffer     = RxQueueBuffer;
    RxQueue.BufferSize = sizeof(RxQueueBuffer);

    TxData.Buffer     = TxDataBuffer;
    TxData.BufferSize = sizeof(TxDataBuffer);
    RxData.Buffer     = RxDataBuffer;
    RxData.BufferSize = sizeof(RxDataBuffer);

    bsp_flash_ReadUID(RandomNumber.UID);
    RandomNumber.Seed = RandomNumber.UID[0] + RandomNumber.UID[1] + RandomNumber.UID[2] + RandomNumber.UID[3];
    srand(RandomNumber.Seed);

    bsp_bms_usart_HalfDuplexRx();
    bsp_bms_usart_ReceiveStart();
}

/*******************************************************************************
//  Function   : SerialAbnormalHandler
//  Description: 更新通信异常计时计数
//  Param      : None
//  Return     : None
//  Others     : 计数最大限制为COMMUNICATION_TIMEOUT_TIME
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void SerialAbnormalHandler(void)
{
    if (RxData.Count < COMMUNICATION_TIMEOUT_TIME)
    {
        RxData.Count++;
    }
}

/*******************************************************************************
//  Function   : IsCommunicationDisconnected
//  Description: 判断通信是否达到断开判定时间
//  Param      : None
//  Return     : true  - 通信已达到断开判定时间
//               false - 通信未达到断开判定时间
//  Others     : 根据RxData.Count与COMMUNICATION_DISCONNECTED_TIME比较判断
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
bool IsCommunicationDisconnected(void)
{
    return (RxData.Count >= COMMUNICATION_DISCONNECTED_TIME);
}

/*******************************************************************************
//  Function   : IsCommunicationTimeout
//  Description: 判断通信是否达到超时时间
//  Param      : None
//  Return     : true  - 通信已超时
//               false - 通信未超时
//  Others     : 根据RxData.Count与COMMUNICATION_TIMEOUT_TIME比较判断
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
bool IsCommunicationTimeout(void)
{
    return (RxData.Count >= COMMUNICATION_TIMEOUT_TIME);
}

/*******************************************************************************
//  Function   : SerialTxDataHandler
//  Description: 根据待发送命令组装通信帧并启动串口发送
//  Param      : None
//  Return     : None
//  Others     : 完成帧头、长度、有效数据、随机数、CRC及XXTEA加密处理
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void SerialTxDataHandler(void)
{
    uint8_t i = 0;
    halfword_union_t crc16;

    if (TxData.Command == CMD_EMPTY)
    {
        return;
    }

    RandomNumber.Value.Halfword = rand();

    TxData.Buffer[i++] = HEAD_CODE_BYTE_0;
    TxData.Buffer[i++] = HEAD_CODE_BYTE_1;
    i++;
    switch (TxData.Command)
    {
    case CMD_UI_OTC_PACK_ACK:
        TxData.Buffer[i++] = CMD_UI_OTC_PACK_ACK;
        break;

    case CMD_UI_OTC_EXIT:
        TxData.Buffer[i++] = CMD_UI_OTC_EXIT;
        break;

    default:
        TxData.Buffer[i++] = CMD_INVALID_REPLY;
        break;
    }
    TxData.Buffer[i++] = 0;
    TxData.Buffer[i++] = 0;
    TxData.Buffer[i++] = 0;
    TxData.Buffer[i++] = RandomNumber.Value.Buffer.Byte0;
    TxData.Buffer[i++] = RandomNumber.Value.Buffer.Byte1;
    TxData.Buffer[2] = i + 2;
    crc16.Value = CRC_Calculate(TxData.Buffer, i);
    TxData.Buffer[i++] = crc16.Byte._0;
    TxData.Buffer[i++] = crc16.Byte._1;
    DataEncrypt(&TxData.Buffer[3], 8);
    TxData.Command = CMD_EMPTY;
    TxData.DataLength = i;
    DataTransmitStartProcess(&TxQueue, &TxData);
}
/*******************************************************************************
//  Function   : SerialRxDataHandler
//  Description: 接收并解析完整通信帧，根据命令执行对应业务处理
//  Param      : None
//  Return     : None
//  Others     : 完成帧长度检查、XXTEA解密、CRC校验及命令分发；有效帧会清零通信计时计数
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void SerialRxDataHandler(void)
{
    uint8_t i = 3;
    halfword_union_t crc16;

    if (DataReceiveStartProcess(&RxQueue, &RxData) == false)
    {
        return;
    }

    if (RxData.DataLength < FRAME_SIZE_MIN)
    {
        return;
    }

    DataDecrypt(&RxData.Buffer[i], RxData.DataLength - 3);
    crc16.Value = CRC_Calculate(RxData.Buffer, RxData.DataLength - 2);
    if ((crc16.Byte._0 != RxData.Buffer[RxData.DataLength - 2]) || (crc16.Byte._1 != RxData.Buffer[RxData.DataLength - 1]))
    {
        return;
    }

    RxData.Command = (command_enum_t)RxData.Buffer[i];
    switch (RxData.Command)
    {
    case CMD_UI_OTC_ENTER:
        bootloader_ProgrammingInit();
        TxData.Command = CMD_UI_OTC_PACK_ACK;
        break;

    case CMD_UI_OTC_PACK_SEND:
        if (bootloader_FlashPageProgramming(&RxData.Buffer[i + 4], RxData.Buffer[i + 3]))
        {
            TxData.Command = CMD_UI_OTC_PACK_ACK;
        }
        break;

    case CMD_UI_OTC_SEND_FINISH:
		param_ota_save(OTC_STATUS_SUCCESS);
		if(OTC_STATUS_SUCCESS == param_ota_state())
		{
			TxData.Command = CMD_UI_OTC_EXIT;
		}

        break;

    default:
        TxData.Command = CMD_INVALID_REPLY;
        break;
    }
}



