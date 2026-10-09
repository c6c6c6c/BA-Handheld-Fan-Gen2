/**
 ***********************************************************************************************************************
 * @file    modbus_rtu.c
 * @brief   Modbus RTU主机协议中间层，实现消息队列管理、请求发送、响应等待、超时重发及报文解析
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-19
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "modbus_rtu.h"
#include "modbus_port.h"
#include "modbus_register.h"

/* Private functions -------------------------------------------------------------------------------------------------*/

/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : modbus_rtu_err_handle
//  Description: Modbus RTU主机通信错误回调函数
//  Param      : err - Modbus通信错误码
//  Return     : 无
//  Others     : 弱定义函数，应用层可重新实现该函数以处理通信异常
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
__weak void modbus_rtu_err_callback(modbus_err_t err)
{
    (void)err;
}

/*******************************************************************************
//  Function   : modbus_rtu_err_handle
//  Description: Modbus RTU主机通信错误回调函数
//  Param      : err - Modbus通信错误码
//  Return     : 无
//  Others     : 弱定义函数，应用层可重新实现该函数以处理通信异常
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
static void modbus_rtu_err_handle(modbus_rtu_master_t *pHandle,modbus_err_t err)
{
    if(pHandle->ErrCnt < 0xffff)
	{
		pHandle->ErrCnt++;
	}
    modbus_rtu_err_callback(err);
}

/*******************************************************************************
//  Function   : modbus_rtu_master_Init
//  Description: 初始化Modbus RTU主机实例
//  Param      : pHandle - Modbus RTU主机句柄
//               api     - 通用物理端口接口
//  Return     : 无
//  Others     : 检查端口接口有效性，初始化收发缓冲区、物理端口及消息队列
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_rtu_master_Init(modbus_rtu_master_t *pHandle,
                            const common_port_api_t *api)
{
    uint8_t res;

    if (pHandle == 0)
    {
        return;
    }

    if (!common_port_api_check(api))
    {
        pHandle->state = MODBUS_COMM_IDLE;
        return;
    }

    pHandle->api = api;

#if (MODBUS_RTU_SINGLE_BUFFER == 1U)
    uint8_t Buffer[MODBUS_RTU_BUFFER_SIZE];

    res = pHandle->api->Init(pHandle->Buffer,
                             MODBUS_RTU_BUFFER_SIZE,
                             pHandle->Buffer,
                             MODBUS_RTU_BUFFER_SIZE);
#else
    res = pHandle->api->Init(pHandle->RxBuffer,
                             MODBUS_RTU_RX_BUFFER_SIZE,
                             pHandle->TxBuffer,
                             MODBUS_RTU_TX_BUFFER_SIZE);
#endif

    if (res)
    {
        pHandle->state   = MODBUS_COMM_FREE;
        pHandle->message = 0;
        pHandle->IsOpen  = 1;

        modbus_queue_Init(&pHandle->Queue);
    }
    else
    {
        pHandle->state   = MODBUS_COMM_IDLE;
        pHandle->message = 0;
    }
}


/*******************************************************************************
//  Function   : modbus_rtu_master_Deinit
//  Description: 反初始化Modbus RTU主机实例
//  Param      : pHandle - Modbus RTU主机句柄
//  Return     : 无
//  Others     : 释放物理端口，将主机状态恢复为空闲状态并解除端口绑定
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_rtu_master_Deinit(modbus_rtu_master_t *pHandle)
{
    if ((pHandle == 0) ||
        (pHandle->state == MODBUS_COMM_IDLE) ||
        (!common_port_api_check(pHandle->api)))
    {
        return;
    }

    pHandle->api->Deinit();
    pHandle->IsOpen  = 0;
    pHandle->api     = 0;
    pHandle->state   = MODBUS_COMM_IDLE;
    pHandle->message = 0;
}



/*******************************************************************************
//  Function   : modbus_rtu_master_Deinit
//  Description: 反初始化Modbus RTU主机实例
//  Param      : pHandle - Modbus RTU主机句柄
//  Return     : 无
//  Others     : 释放物理端口，将主机状态恢复为空闲状态并解除端口绑定
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
static void modbus_open_port(modbus_rtu_master_t *pHandle)
{
	uint8_t res;
    if(pHandle->IsOpen == 1)return;
    #if (MODBUS_RTU_SINGLE_BUFFER == 1U)
        uint8_t Buffer[MODBUS_RTU_BUFFER_SIZE];

        res = pHandle->api->Init(pHandle->Buffer,
                                MODBUS_RTU_BUFFER_SIZE,
                                pHandle->Buffer,
                                MODBUS_RTU_BUFFER_SIZE);
    #else
        res = pHandle->api->Init(pHandle->RxBuffer,
                                MODBUS_RTU_RX_BUFFER_SIZE,
                                pHandle->TxBuffer,
                                MODBUS_RTU_TX_BUFFER_SIZE);
    #endif
    if(res)
    {
        pHandle->IsOpen = 1;
    }
}

/*******************************************************************************
//  Function   : modbus_rtu_master_Deinit
//  Description: 反初始化Modbus RTU主机实例
//  Param      : pHandle - Modbus RTU主机句柄
//  Return     : 无
//  Others     : 释放物理端口，将主机状态恢复为空闲状态并解除端口绑定
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
static void modbus_close_port(modbus_rtu_master_t *pHandle)
{
    if(pHandle->IsOpen == 0)return;
    pHandle->api->Deinit();
    pHandle->IsOpen  = 0;
}

/*******************************************************************************
//  Function   : modbus_rtu_master_add_message
//  Description: 向Modbus RTU主机轮询队列中添加一条通信消息
//  Param      : pHandle - Modbus RTU主机句柄
//               item    - 待添加的Modbus通信消息
//  Return     : 无
//  Others     : 消息加入队列后，由1ms状态机任务依次取出并执行
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_rtu_master_add_message(modbus_rtu_master_t *pHandle,
                                   modbus_item_t item)
{
    if ((pHandle == 0) ||
        (pHandle->state == MODBUS_COMM_IDLE))
    {
        return;
    }

    if (!modbus_queue_add(&pHandle->Queue, item))
    {
        modbus_rtu_err_handle(pHandle,MODBUS_ERR_OVERFLOW);
    }
}

/*******************************************************************************
//  Function   : start_transfer
//  Description: 根据当前消息构建Modbus请求帧并启动发送
//  Param      : pHandle - Modbus RTU主机句柄
//  Return     : 无
//  Others     : 组帧成功后进入发送状态；组帧失败时返回等待消息状态并上报错误
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
static void start_transfer(modbus_rtu_master_t *pHandle)
{
    modbus_err_t err;

    err = ModbusRegister_BuildRequest(pHandle->message,
                                      pHandle->api->transfer_data(),
                                      &pHandle->length);

    if (err == MODBUS_ERR_NONE)
    {
        pHandle->api->start_transfer(pHandle->length);

        pHandle->state  = MODBUS_COMM_SEND;
        pHandle->timeMs = 0U;
    }
    else
    {
        pHandle->state = MODBUS_COMM_FREE;

        modbus_rtu_err_handle(pHandle,err);
    }
}

/*******************************************************************************
//  Function   : modbus_rtu_master_1ms_task
//  Description: 执行Modbus RTU主机通信状态机
//  Param      : pHandle - Modbus RTU主机句柄
//  Return     : 无
//  Others     : 应每1ms调用一次，依次完成消息获取、请求发送、响应等待、
//               超时重发、响应解析及消息完成回调
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_rtu_master_1ms_task(modbus_rtu_master_t *pHandle)
{
    if ((pHandle == 0) ||
        (pHandle->state == MODBUS_COMM_IDLE))
    {
        return;
    }

    /* 执行物理端口1ms周期任务，用于接收帧间隔计时等处理 */
    ModbusPort_1msTask();

    switch ((uint8_t)pHandle->state)
    {
        /***********************************************************************
         * 等待消息状态
         *
         * 从轮询队列中获取下一条待处理消息。获取成功后构建请求帧并启动
         * 发送，同时清除本次通信的重试计数。
         ***********************************************************************/
        case MODBUS_COMM_FREE:
        {
            pHandle->message = modbus_queue_get(&pHandle->Queue);

            if (pHandle->message != 0)
            {
                modbus_open_port(pHandle);
                start_transfer(pHandle);

                pHandle->RetryCount = 0U;
            }
            else
            {
                modbus_close_port(pHandle);
            }
        }
        break;

        /***********************************************************************
         * 请求帧发送状态
         *
         * 等待底层端口将请求帧全部发送完成，并监测发送错误及发送超时。
         * 发送完成后进入等待从机响应状态。
         ***********************************************************************/
        case MODBUS_COMM_SEND:
        {
            pHandle->timeMs++;

            if ((pHandle->api->transfer_state() == COMMON_PORT_TX_ERROR) ||
                (pHandle->timeMs > 200U))
            {
                pHandle->state = MODBUS_COMM_FREE;

                modbus_rtu_err_handle(pHandle,MODBUS_ERR_SEND);
            }
            else if (pHandle->api->transfer_state() ==
                     COMMON_PORT_TX_IDLE)
            {
                /* 请求帧发送完成，开始等待从机响应 */
                pHandle->timeMs = 0U;
                pHandle->state  = MODBUS_COMM_WAIT_REPLY;
            }
        }
        break;

        /***********************************************************************
         * 等待从机响应状态
         *
         * 等待底层端口接收到一帧完整响应数据。等待超时时按照设定次数重新
         * 发送原请求帧；达到最大重试次数后上报接收错误。
         ***********************************************************************/
        case MODBUS_COMM_WAIT_REPLY:
        {
            pHandle->timeMs++;

            if (pHandle->timeMs > MODBUS_RTU_MESSAGE_TIMEOUT_MS)
            {
                pHandle->RetryCount++;

                if (pHandle->RetryCount < MODBUS_RTU_MESSAGE_RETRY_MAX)
                {
                    /* 响应超时，重新发送原请求帧 */
                    pHandle->api->start_transfer(pHandle->length);

                    pHandle->state  = MODBUS_COMM_SEND;
                    pHandle->timeMs = 0U;
                }
                else
                {
                    /* 达到最大重试次数，结束当前通信 */
                    pHandle->state = MODBUS_COMM_FREE;

                    modbus_rtu_err_handle(pHandle,MODBUS_ERR_RECEIVE);
                }
            }
            else if (pHandle->api->receive_state() == COMMON_PORT_RX_DONE)
            {
                modbus_err_t err;

                /*
                 * 已接收到一帧完整响应数据，先将状态恢复为等待消息，
                 * 再解析从机响应帧。
                 */
                pHandle->state = MODBUS_COMM_FREE;

                err = ModbusRegister_ParseResponse(
                    pHandle->message,
                    pHandle->api->receive_data(),
                    pHandle->api->receive_length());

                if (err == MODBUS_ERR_NONE)
                {
                    /*
                     * 响应帧解析成功，执行当前消息对应的完成回调。
                     */
                    if (pHandle->message->finish != 0)
                    {
                        pHandle->message->finish(pHandle->message);
                    }
					pHandle->ErrCnt = 0;
                }
                else
                {
                    /* 响应帧校验或解析失败 */
                    modbus_rtu_err_handle(pHandle,err);
                }
				pHandle->api->receive_rst();
            }
        }
        break;

        default:
        {
            /*
             * 状态异常时恢复到等待消息状态，避免状态机停滞。
             */
            pHandle->state = MODBUS_COMM_FREE;
        }
        break;
    }
}
