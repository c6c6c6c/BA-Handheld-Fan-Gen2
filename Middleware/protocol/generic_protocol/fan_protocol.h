
/**
 ***********************************************************************************************************************
 * @file    fan_protocol.h
 * @brief   风扇通信协议接口，定义协议帧参数、命令类型及协议管理结构体
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-30
 ***********************************************************************************************************************
 * @note
 *          1. 统一管理风扇通信协议相关配置及命令定义。
 *          2. 协议帧包含帧头、命令、数据及校验信息。
 *          3. 通过 UART1 通信接口完成数据接收及发送。
 *          4. 协议字段及命令编码需与上位机保持一致。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _FAN_PROTOCOL_H_
#define _FAN_PROTOCOL_H_

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include <stdint.h>
#include "uart1_port.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/
#define PROTO_VERSION               0x01U /** 当前通信协议版本 */
#define PROTO_SOF1                  0x55U /** 协议帧头第一个字节 */
#define PROTO_SOF2                  0xAAU /** 协议帧头第二个字节 */

/* Protocol frame indexes --------------------------------------------------------------------------------------------*/
#define PROTO_SOF1_INDEX            0U    /** 帧头第一个字节的缓冲区索引 */
#define PROTO_SOF2_INDEX            1U    /** 帧头第二个字节的缓冲区索引 */
#define PROTO_CMD_INDEX             2U    /** 命令字段的缓冲区索引 */
#define PROTO_LEN_INDEX             3U    /** 命令字段的缓冲区索引 */
#define PROTO_DATA_INDEX            4U    /** 命令字段的缓冲区索引 */

/* Protocol frame lengths --------------------------------------------------------------------------------------------*/
#define PROTO_MAX_DATA_LEN          256U  /** 协议数据区最大长度，单位 Byte */
#define FAN_PROTOCOL_MIN_FRAME_LEN  6U    /** 最小协议帧长度：SOF1 + SOF2 + CMD + LEN + CRC16 */

/** 协议帧最大长度，包含数据区及固定开销 */
#define PROTO_MAX_FRAME_LEN         (PROTO_MAX_DATA_LEN + FAN_PROTOCOL_MIN_FRAME_LEN)


/* Protocol command macros -------------------------------------------------------------------------------------------*/

/**
 * @brief 生成请求命令编码
 * @note  将命令编号与 0x80 进行按位或运算。
 */
#define PROTO_REQUEST(x)            (0x80U | (x))

/**
 * @brief 生成应答命令编码
 * @note  当前采用与请求相同的编码规则，
 *        具体规则需要与上位机协议保持一致。
 */
#define PROTO_RESPONSE(x)           (0x00U | (x))


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风扇通信协议命令类型
 * @note  包含设备信息查询、系统控制、恢复出厂设置、
 *        制冷功能控制以及 OTA 固件升级命令。
 */
typedef enum
{
    PROTO_IDLE                     = 0x00U, /**< 空闲状态 */

    PROTO_INFO_REQUEST             = PROTO_REQUEST(1), /**< 设备信息请求 */
    PROTO_SYSTEM_REQUEST           = PROTO_REQUEST(2), /**< 系统状态及控制请求 */
    PROTO_BATTERY_INFO_REQUEST     = PROTO_REQUEST(3), /**< 系统状态及控制请求 */
    PROTO_FACTORY_RESET_REQUEST    = PROTO_REQUEST(4), /**< 恢复出厂设置请求 */

    PROTO_COOL_REQUEST             = PROTO_REQUEST(5), /**< 制冷功能控制请求 */

    PROTO_ENTER_OTA_REQUEST        = PROTO_REQUEST(6), /**< 进入 OTA 模式请求 */
    PROTO_ENTER_UPDATE_REQUEST     = PROTO_REQUEST(7), /**< 进入固件更新状态请求 */
    PROTO_ENTER_COMPLETION_REQUEST = PROTO_REQUEST(8), /**< 固件更新完成请求 */
	
	
	PROTO_INFO_RESPONSE             = PROTO_RESPONSE(1), /**< 设备信息请求 */
    PROTO_SYSTEM_RESPONSE           = PROTO_RESPONSE(2), /**< 系统状态及控制请求 */
    PROTO_BATTERY_INFO_RESPONSE     = PROTO_RESPONSE(3), /**< 系统状态及控制请求 */
    PROTO_FACTORY_RESET_RESPONSE    = PROTO_RESPONSE(4), /**< 恢复出厂设置请求 */

    PROTO_COOL_RESPONSE             = PROTO_RESPONSE(5), /**< 制冷功能控制请求 */

    PROTO_ENTER_OTA_RESPONSE        = PROTO_RESPONSE(6), /**< 进入 OTA 模式请求 */
    PROTO_ENTER_UPDATE_RESPONSE     = PROTO_RESPONSE(7), /**< 进入固件更新状态请求 */
    PROTO_ENTER_COMPLETION_RESPONSE = PROTO_RESPONSE(8), /**< 固件更新完成请求 */

    PROTO_ERR                       = 0xFFU  /**< 协议解析错误 */
} proto_cmd_t;


/**
 * @brief 风扇通信协议管理结构体
 * @note  保存 UART 收发缓冲区、底层通信接口及当前协议命令。
 */
typedef struct
{
    uint8_t RxDat[PROTO_MAX_FRAME_LEN]; /**< UART 接收数据缓冲区 */
    uint8_t TxDat[PROTO_MAX_FRAME_LEN]; /**< UART 发送数据缓冲区 */

    const common_port_api_t *api;       /**< UART 通信接口指针 */

    proto_cmd_t request;                /**< 当前接收到的协议命令 */
	proto_cmd_t response;               /**< 当前接收到的协议命令 */
} fan_protocol_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_protocol_Init
//  Description: 初始化风扇通信协议管理模块
//  Param      : None
//  Return     : None
//  Others     : 绑定 UART1 接口并初始化接收、发送缓冲区
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void fan_protocol_Init(void);


/*******************************************************************************
//  Function   : fan_protocol_Deinit
//  Description: 反初始化风扇通信协议管理模块
//  Param      : None
//  Return     : None
//  Others     : 释放 UART1 通信接口相关资源
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void fan_protocol_Deinit(void);


/*******************************************************************************
//  Function   : fan_protocol_1msTask
//  Description: 执行风扇通信协议 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 推进 UART 状态机，完成接收帧校验、命令解析及应答处理
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void fan_protocol_1ms_task(void);


#endif /* _FAN_PROTOCOL_H_ */
