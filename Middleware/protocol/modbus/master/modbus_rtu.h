/**
 ***********************************************************************************************************************
 * @file    modbus_rtu.h
 * @brief   Modbus RTU主机中间层，实现主机状态机、消息队列、超时重发及响应处理
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-19
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef __MODBUS_RTU_H
#define __MODBUS_RTU_H

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "modbus_queue.h"
#include "modbus_port.h"
#include "modbus_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief Modbus RTU主机通信状态
 */
typedef enum
{
    MODBUS_COMM_IDLE = 0U,    // 空闲状态，Modbus主机未初始化或已经停止运行
    MODBUS_COMM_FREE,         // 等待从轮询队列中获取新的通信消息
    MODBUS_COMM_SEND,         // 请求帧正在发送，等待底层端口发送完成
    MODBUS_COMM_WAIT_REPLY,   // 请求帧发送完成，等待从机响应数据
} modbus_rtu_master_state_t;

/**
 * @brief Modbus RTU主机控制句柄
 *
 * 用于保存物理端口接口、收发缓冲区、消息队列以及主机状态机
 * 运行过程中需要使用的相关参数。
 */
typedef struct
{
    /**
     * @brief 通用物理端口接口
     *
     * Modbus协议层通过该接口访问底层串口，避免直接依赖具体的
     * 串口、DMA、中断及RS485方向控制实现。
     */
    const common_port_api_t *api;

#if (MODBUS_RTU_SINGLE_BUFFER == 1U)

    /**
     * @brief Modbus收发共用缓冲区
     *
     * 单缓冲模式下，请求帧发送和响应帧接收共用该缓冲区。
     * 该模式能够节省RAM，但必须保证发送和接收不会同时操作缓冲区。
     */
    uint8_t Buffer[MODBUS_RTU_BUFFER_SIZE];

#else

    /**
     * @brief Modbus接收缓冲区
     *
     * 用于保存从机返回的完整响应帧。
     */
    uint8_t RxBuffer[MODBUS_RTU_RX_BUFFER_SIZE];

    /**
     * @brief Modbus发送缓冲区
     *
     * 用于保存主机生成的Modbus请求帧。
     */
    uint8_t TxBuffer[MODBUS_RTU_TX_BUFFER_SIZE];

#endif

    /**
     * @brief 当前正在处理的通信消息
     *
     * 指向从轮询队列中取出的消息项。通信完成后执行该消息
     * 对应的完成回调。
     */
    modbus_item_t *message;

    /**
     * @brief Modbus主机轮询消息队列
     *
     * 用于保存需要周期执行的读写消息。
     */
    modbus_queue_t Queue;

    /**
     * @brief Modbus主机当前通信状态
     */
    modbus_rtu_master_state_t state;

    /**
     * @brief 当前消息的超时重发次数
     *
     * 开始处理新消息时清零，等待响应超时时递增。
     */
    uint8_t RetryCount;

    /**
     * @brief 当前通信阶段的超时计数
     *
     * 在1ms周期任务中递增，单位为毫秒。
     */
    uint32_t timeMs;

    /**
     * @brief 当前Modbus请求帧长度
     *
     * 请求帧构建完成后保存其有效数据长度，超时重发时继续使用。
     */
    uint16_t length;

    /**
     * @brief 当前port打开状态
     *
     * 请求帧构建完成后保存其有效数据长度，超时重发时继续使用。
     */
    uint16_t IsOpen;
	
	/**
     * @brief 连续错误次数
     *
     * 请求帧构建完成后保存其有效数据长度，超时重发时继续使用。
     */
	uint16_t ErrCnt;     

} modbus_rtu_master_t;


/* Exported functions prototypes -------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : modbus_rtu_master_Init
//  Description: 初始化Modbus RTU主机实例
//  Param      : pHandle - Modbus RTU主机句柄
//               api     - 通用物理端口接口
//  Return     : 无
//  Others     : 初始化物理端口、通信状态和轮询消息队列
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_rtu_master_Init(modbus_rtu_master_t *pHandle,
                            const common_port_api_t *api);

/*******************************************************************************
//  Function   : modbus_rtu_master_Deinit
//  Description: 反初始化Modbus RTU主机实例
//  Param      : pHandle - Modbus RTU主机句柄
//  Return     : 无
//  Others     : 关闭物理端口并将主机恢复为空闲状态
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_rtu_master_Deinit(modbus_rtu_master_t *pHandle);

/*******************************************************************************
//  Function   : modbus_rtu_master_add_message
//  Description: 向Modbus RTU主机轮询队列中添加通信消息
//  Param      : pHandle - Modbus RTU主机句柄
//               item    - 待添加的Modbus通信消息
//  Return     : 无
//  Others     : 消息加入队列后由主机状态机依次执行
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_rtu_master_add_message(modbus_rtu_master_t *pHandle,
                                   modbus_item_t item);
/*******************************************************************************
//  Function   : modbus_rtu_master_1ms_task
//  Description: 执行Modbus RTU主机1ms通信状态机任务
//  Param      : pHandle - Modbus RTU主机句柄
//  Return     : 无
//  Others     : 负责消息获取、请求发送、响应等待、超时重发和数据解析
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_rtu_master_1ms_task(modbus_rtu_master_t *pHandle);

/*******************************************************************************
//  Function   : modbus_rtu_err_callback
//  Description: Modbus RTU主机通信错误回调
//  Param      : err - Modbus通信错误码
//  Return     : 无
//  Others     : 应用层可以重新实现该弱定义函数
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_rtu_err_callback(modbus_err_t err);                                  
#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_RTU_H */
