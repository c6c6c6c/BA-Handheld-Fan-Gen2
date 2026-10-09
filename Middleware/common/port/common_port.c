/**
 ***********************************************************************************************************************
 * @file    common_port.c
 * @brief   通用异步通信物理端口接口，提供不定长接收及环形缓冲发送实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-09-02
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "common_port.h"

/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : common_port_Init
//  Description: 初始化通用端口对象、收发缓冲区及底层物理端口
//  Param      : port    - 通用端口对象
//               api     - 底层物理端口操作接口
//               rx_buf  - 接收缓冲区
//               rx_size - 接收缓冲区大小，单位 Byte
//               tx_buf  - 发送环形缓冲区
//               tx_size - 发送环形缓冲区大小，单位 Byte
//  Return     : None
//  Others     : 参数有效时会调用底层 Init 接口，并将收发状态置为空闲
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
bool common_port_Init(common_port_t *port,
                      uint8_t       *rx_buf,uint16_t rx_size,
                      uint8_t       *tx_buf,uint16_t tx_size)
{
    if ((port   == 0) 
    || (rx_buf  == 0) 
    || (rx_size == 0U) 
    || (tx_buf  == 0) 
    || (tx_size == 0U))
    {
        port->init = 0U;
        return false;
    }

    port->tx.Buffer     = tx_buf;
    port->tx.BufferSize = tx_size;
    port->tx.Count      = 0U;
    port->tx.Length     = 0U;
    port->tx.State      = COMMON_PORT_TX_IDLE;

    port->rx.Buffer     = rx_buf;
    port->rx.BufferSize = rx_size;
    port->rx.Length     = 0U;
    port->rx.TimerMs    = 0U;
    port->rx.Receiving  = 0U;
    port->rx.State      = COMMON_PORT_RX_IDLE;
    port->init = 1U;

    return true;
}

/*******************************************************************************
//  Function   : common_port_Deinit
//  Description: 反初始化底层物理端口并清空通用端口运行状态
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 不释放用户传入的收发缓冲区
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void common_port_Deinit(common_port_t *port)
{
    if (port == 0)
    {
        return;
    }

    port->init         = 0U;
    port->tx.Length    = 0U;
    port->tx.Count     = 0U;
    port->tx.State     = COMMON_PORT_TX_IDLE;
    port->rx.Length    = 0U;
    port->rx.TimerMs   = 0U;
    port->rx.Receiving = 0U;
    port->rx.State     = COMMON_PORT_RX_IDLE;
}

/*******************************************************************************
//  Function   : common_port_ReceiveState
//  Description: 获取通用端口当前接收状态
//  Param      : port - 通用端口对象
//  Return     : 当前接收状态
//  Others     : COMMON_PORT_RX_DONE 表示一帧不定长数据接收完成
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
common_port_rx_state_t common_port_ReceiveState(common_port_t *port)
{
    if (port == 0)
    {
        return COMMON_PORT_RX_ERROR;
    }

    return port->rx.State;
}

/*******************************************************************************
//  Function   : common_port_ReceiveData
//  Description: 获取通用端口接收数据缓冲区地址
//  Param      : port - 通用端口对象
//  Return     : 接收缓冲区首地址
//  Others     : 建议仅在接收状态为 COMMON_PORT_RX_DONE 时读取完整帧
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void *common_port_ReceiveData(common_port_t *port)
{
    if (port == 0)
    {
        return 0;
    }

    return port->rx.Buffer;
}

/*******************************************************************************
//  Function   : common_port_ReceiveLength
//  Description: 获取通用端口当前接收数据长度
//  Param      : port - 通用端口对象
//  Return     : 接收数据长度，单位 Byte
//  Others     : 接收未完成时返回当前累计长度
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
uint16_t common_port_ReceiveLength(common_port_t *port)
{
    if (port == 0)
    {
        return 0U;
    }

    return port->rx.Length;
}

/*******************************************************************************
//  Function   : common_port_ReceiveReset
//  Description: 清空通用端口接收状态，准备接收下一帧数据
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 上层处理完 COMMON_PORT_RX_DONE 或 COMMON_PORT_RX_ERROR 后调用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void common_port_ReceiveReset(common_port_t *port)
{
    if (port == 0)
    {
        return;
    }

    port->rx.Length    = 0U;
    port->rx.TimerMs   = 0U;
    port->rx.Receiving = 0U;
    port->rx.State     = COMMON_PORT_RX_IDLE;
}

/*******************************************************************************
//  Function   : common_port_TransferState
//  Description: 获取通用端口当前发送状态
//  Param      : port - 通用端口对象
//  Return     : 当前发送状态
//  Others     : 环形缓冲区发送完成后返回 COMMON_PORT_TX_IDLE
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
common_port_tx_state_t common_port_TransferState(common_port_t *port)
{
    if (port == 0)
    {
        return COMMON_PORT_TX_ERROR;
    }

    return port->tx.State;
}

/*******************************************************************************
//  Function   : common_port_TransferState
//  Description: 获取通用端口当前发送状态
//  Param      : port - 通用端口对象
//  Return     : 当前发送状态
//  Others     : 环形缓冲区发送完成后返回 COMMON_PORT_TX_IDLE
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
uint8_t *common_port_TransferData(common_port_t *port)
{
    if (port == 0)
    {
        return 0;
    }

    return port->tx.Buffer;
}

/*******************************************************************************
//  Function   : common_port_Transfer
//  Description: 将待发送数据写入通用端口发送环形缓冲区
//  Param      : port   - 通用端口对象
//               pData  - 待发送数据指针
//               length - 待发送数据长度，单位 Byte
//  Return     : 1 - 写入成功
//               0 - 参数错误、未初始化或缓冲区空间不足
//  Others     : 发送空闲时会立即取出首字节并调用底层 Transfer 接口启动异步发送
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
bool common_port_Transfer(common_port_t *port,uint16_t length)
{
    if ((port      == 0) 
    || (port->init == 0U) 
    || (length     == 0U)
    || (length > port->tx.BufferSize))
    {
        return false;
    }
    port->tx.State  = COMMON_PORT_TX_BUSY;
    port->tx.Count  = 1U;
    port->tx.Length = length;
    return true;
}

/*******************************************************************************
//  Function   : common_port_RxEventCallback
//  Description: 处理底层物理端口接收字节事件
//  Param      : port - 通用端口对象
//               data - 接收到的字节
//  Return     : None
//  Others     : 接收缓冲区满时进入 COMMON_PORT_RX_ERROR 状态
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void common_port_RxEventCallback(common_port_t *port, uint8_t data)
{
    if ((port == 0) || (port->init == 0U))
    {
        return;
    }

    if (port->rx.State != COMMON_PORT_RX_IDLE)
    {
        return;
    }

    if (port->rx.Length >= port->rx.BufferSize)
    {
        port->rx.State     = COMMON_PORT_RX_ERROR;
        port->rx.Receiving = 0U;
        return;
    }

    port->rx.Buffer[port->rx.Length] = data;
    port->rx.Length++;
    port->rx.TimerMs   = 0U;
    port->rx.Receiving = 1U;
}

/*******************************************************************************
//  Function   : common_port_TxEventCallback
//  Description: 处理底层物理端口发送完成或发送寄存器空事件
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 环形缓冲区有数据时继续发送下一字节，否则切换为空闲状态
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
bool common_port_TxEventCallback(common_port_t *port , uint8_t *data)
{
    if ((port == 0) || (port->init == 0U))
    {
        return false;
    }

    if (port->tx.Count < port->tx.Length)
    {
        *data = port->tx.Buffer[port->tx.Count];
        port->tx.Count++;
        return true;
    }
    port->tx.State = COMMON_PORT_TX_IDLE;
    return false;
}

/*******************************************************************************
//  Function   : common_port_ErrorEventCallback
//  Description: 处理底层物理端口通信错误事件
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 错误发生后由上层调用复位或重新初始化接口恢复状态
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
// void common_port_ErrorEventCallback(common_port_t *port)
// {
//     if (port == 0)
//     {
//         return;
//     }

//     port->rx.State     = COMMON_PORT_RX_ERROR;
//     port->rx.Receiving = 0U;
//     port->tx.State     = COMMON_PORT_TX_ERROR;
// }

/*******************************************************************************
//  Function   : common_port_1msTask
//  Description: 执行通用端口不定长接收帧间隔计时
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 应在 1ms 周期任务中调用；超时后将当前接收数据标记为完整帧
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void common_port_1msTask(common_port_t *port)
{
    if ((port == 0) || (port->init == 0U))
    {
        return;
    }

    if ((port->rx.Receiving != 0U) 
	&&  (port->rx.State == COMMON_PORT_RX_IDLE))
    {
        if (port->rx.TimerMs < 10)
        {
            port->rx.TimerMs++;
        }
        else
        {
            port->rx.Receiving = 0U;
            port->rx.State     = COMMON_PORT_RX_DONE;
        }
    }
}

/*******************************************************************************
//  Function   : common_port_api_check
//  Description: 检查通用端口底层物理接口是否完整
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 应在 1ms 周期任务中调用；超时后将当前接收数据标记为完整帧
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
bool common_port_api_check(const common_port_api_t *api)
{
    if (api == 0)
    {
        return false;
    }

    if ((api->Init           == 0) 
    ||  (api->Deinit         == 0) 
    ||  (api->receive_state  == 0) 
    ||  (api->receive_data   == 0) 
	||  (api->receive_rst    == 0)
    ||  (api->receive_length == 0) 
    ||  (api->transfer_data  == 0) 
    ||  (api->start_transfer == 0) 
    ||  (api->transfer_state == 0))
    {
        return false;
    }

    return true;
}
