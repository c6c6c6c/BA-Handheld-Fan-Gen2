/**
 ***********************************************************************************************************************
 * @file    bms_bootloader.h
 * @brief
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_H_
#define _BMS_H_

#include "main.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/

#define DATA_BLOCK_BUFFER_BYTE_SIZE     (256)
#define DATA_BLOCK_BUFFER_WORD_SIZE     (DATA_BLOCK_BUFFER_BYTE_SIZE / 4)

#define STATE_CONTROL_VAILD_DATA_SIZE   (8)
#define STATE_REPORT_VAILD_DATA_SIZE    (24)


/* Typedef -----------------------------------------------------------------------------------------------------------*/

typedef union
{
    uint8_t  ByteBuffer[DATA_BLOCK_BUFFER_BYTE_SIZE];
    uint32_t WordBuffer[DATA_BLOCK_BUFFER_WORD_SIZE];
} data_block_union_t;

typedef enum
{
    CMD_EMPTY,
    CMD_BMS_OTC_ENTER = 0x80,
    CMD_BMS_OTC_PACK_SEND,
    CMD_BMS_OTC_PACK_ACK,
    CMD_BMS_OTC_SEND_FINISH,
    CMD_BMS_OTC_EXIT,
    CMD_UI_OTC_ENTER,
    CMD_UI_OTC_PACK_SEND,
    CMD_UI_OTC_PACK_ACK,
    CMD_UI_OTC_SEND_FINISH,
    CMD_UI_OTC_EXIT,
    CMD_MACHINE_DATA_QUERY,
    CMD_MACHINE_DATA_REPORT,
    CMD_STATE_CONTROL,
    CMD_STATE_REPORT,
    CMD_FACTORY_MSG_WRITE,
    CMD_FACTORY_MSG_WEITE_SUCCESS,
    CMD_CONNECT_STATUS_QUERY,
    CMD_CONNECT_STATUS_REPORT,
    CMD_RESET_TO_FACTORY_MODE,
    CMD_RESET_TO_FACTORY_MODE_SUCCESS,
    CMD_FACTORY_TEST_ENTER,
    CMD_FACTORY_TEST_EXIT,
    CMD_MSG_WRITE,
    CMD_DATA_WRITE,
    CMD_DATA_SYNC_REQUEST,
    CMD_DATA_SYNC_REPLY,
    CMD_INVALID_REPLY,
    CMD_QUICK_SLEEP,
    CMD_RESET_STARTUP,
    CMD_RESET_STARTUP_SUCCESS,
} command_enum_t;

typedef struct
{
    uint8_t *Buffer;
    uint16_t BufferSize;
    uint16_t DataLength;
    uint16_t Count;
    uint16_t Index;
    command_enum_t Command;
} serial_data_t;

typedef union
{
    uint16_t Value;
    struct
    {
        uint8_t _0;
        uint8_t _1;
    } Byte;
} halfword_union_t;

typedef struct
{
    uint32_t UID[4];
    uint16_t Seed;
    union
    {
        uint16_t Halfword;
        struct
        {
            uint8_t Byte0;
            uint8_t Byte1;
        } Buffer;
    } Value;
} random_number_t;

typedef union
{
    uint8_t Buffer[STATE_CONTROL_VAILD_DATA_SIZE];
    struct
    {
        uint8_t  Command;
        uint8_t  SleepState;
        uint16_t DisplayState;
        uint8_t  FanSpeedMode;
        uint8_t  ResetCountData;
        uint16_t BatteryVoltage;
    } Item;
} state_control_frame_union_t;

typedef union
{
    uint8_t Buffer[STATE_REPORT_VAILD_DATA_SIZE];
    struct
    {
        uint8_t  Command;
        uint8_t  ButtonValue;
        uint16_t Reserved;
        uint32_t FeedbackSpeed;
        uint16_t HardwareVersion;
        uint16_t SoftwareVersion;
        uint32_t ModeAddPressCount;
        uint32_t ModeSubPressCount;
        uint32_t PowerSwitchChangeCount;
    } Item;
} state_report_frame_union_t;


typedef struct
{
    uint32_t ErrorCount;
} authentication_data_t;

/* Function declarations ---------------------------------------------------------------------------------------------*/

void SerialCommuncaitionInit(void);
void SerialTxDataHandler(void);
void SerialRxDataHandler(void);
void SerialAbnormalHandler(void);
bool IsCommunicationDisconnected(void);
bool IsCommunicationTimeout(void);
uint16_t GetBatteryVoltage(void);

#endif
