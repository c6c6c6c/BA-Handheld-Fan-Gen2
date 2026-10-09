/**
 ***********************************************************************************************************************
 * @file    modbus_def.h
 * @brief   Modbus RTU common definitions.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */
#ifndef __MODBUS_DEF_H
#define __MODBUS_DEF_H

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include <stdint.h>

/* Type compatibility ------------------------------------------------------------------------------------------------*/

/* Macro -------------------------------------------------------------------------------------------------------------*/
#define MODBUS_RTU_SINGLE_BUFFER                   0U      // 是否为单缓冲
#define MODBUS_RTU_MESSAGE_RETRY_MAX               3U      // 单条轮询报文响应超时后的最大重发次数
#define MODBUS_RTU_MESSAGE_TIMEOUT_MS              1000U   // 单条轮询报文请求和响应超时时间，单位：ms

#if (MODBUS_RTU_SINGLE_BUFFER == 1U)
    #define MODBUS_RTU_BUFFER_SIZE                 256U   // Modbus 
#else
    #define MODBUS_RTU_RX_BUFFER_SIZE              256U   // Modbus RTU主机接收缓冲区大小
    #define MODBUS_RTU_TX_BUFFER_SIZE              256U   // Modbus RTU主机发送缓冲区大小
#endif


/* Typedef -----------------------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Description: Modbus 错误码定义
*******************************************************************************/
typedef enum
{
    MODBUS_ERR_NONE = 0,              // 无错误
    MODBUS_ERR_PARAM,                 // 参数错误
    MODBUS_ERR_BUSY,                  // Modbus 当前忙
    MODBUS_ERR_TIMEOUT,               // 通信响应超时
    MODBUS_ERR_CRC,                   // CRC 校验错误
    MODBUS_ERR_FRAME,                 // 数据帧格式错误
    MODBUS_ERR_ADDR,                  // 从机地址错误
    MODBUS_ERR_FUNC,                  // 功能码错误
    MODBUS_ERR_LENGTH,                // 数据长度错误
    MODBUS_ERR_OVERFLOW,              // 接收或发送缓冲区溢出
    MODBUS_ERR_SEND,                  // 数据发送失败
    MODBUS_ERR_RECEIVE,               // 数据接收失败
    MODBUS_ERR_EXCEPTION,             // 从机返回 Modbus 异常响应
    MODBUS_ERR_MAX                    // 错误码数量/边界值
} modbus_err_t;


/*******************************************************************************
//  Description: Modbus 功能码定义
*******************************************************************************/
typedef enum
{
    MODBUS_FUNC_READ_COILS              = 0x01,  // 读取线圈状态
    MODBUS_FUNC_READ_DISCRETE_INPUTS    = 0x02,  // 读取离散输入状态
    MODBUS_FUNC_READ_HOLDING_REGISTERS  = 0x03,  // 读取保持寄存器
    MODBUS_FUNC_READ_INPUT_REGISTERS    = 0x04,  // 读取输入寄存器
    MODBUS_FUNC_WRITE_SINGLE_COIL       = 0x05,  // 写单个线圈
    MODBUS_FUNC_WRITE_SINGLE_REGISTER   = 0x06,  // 写单个保持寄存器
    MODBUS_FUNC_WRITE_MULTIPLE_COILS    = 0x0F,  // 写多个线圈
    MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS= 0x10,  // 写多个保持寄存器

    MODBUS_FUNC_MAX
} modbus_func_t;


/*******************************************************************************
//  Description: Modbus 标准异常码定义
*******************************************************************************/
typedef enum
{
    MODBUS_EXCEPTION_ILLEGAL_FUNCTION      = 0x01,  // 非法功能码
    MODBUS_EXCEPTION_ILLEGAL_DATA_ADDRESS  = 0x02,  // 非法数据地址
    MODBUS_EXCEPTION_ILLEGAL_DATA_VALUE    = 0x03,  // 非法数据值
    MODBUS_EXCEPTION_SLAVE_DEVICE_FAILURE  = 0x04,  // 从机设备故障
    MODBUS_EXCEPTION_ACKNOWLEDGE           = 0x05,  // 请求已接收，正在处理
    MODBUS_EXCEPTION_SLAVE_DEVICE_BUSY     = 0x06,  // 从机设备忙
    MODBUS_EXCEPTION_MEMORY_PARITY_ERROR   = 0x08,  // 存储奇偶校验错误
    MODBUS_EXCEPTION_GATEWAY_PATH_FAILED   = 0x0A,  // 网关路径不可用
    MODBUS_EXCEPTION_GATEWAY_NO_RESPONSE   = 0x0B   // 网关目标设备无响应
} modbus_exception_t;

#endif /* __MODBUS_DEF_H */
