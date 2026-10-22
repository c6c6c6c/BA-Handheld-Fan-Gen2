/**
 ***********************************************************************************************************************
 * @file    uart1_port.c
 * @brief   UART1 物理端口适配，实现 common_port 收发管理及半双工通信控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "uart1_port.h"
#include "bsp_uart1.h"


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** UART1 通用通信端口实例，管理收发缓冲区及通信状态 */
common_port_t s_uart1Port;


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : uart1_port_Init
//  Description: 初始化 UART1 通用通信端口及底层硬件
//  Param      : rx_buf  - 接收缓冲区首地址
//               rx_size - 接收缓冲区大小，单位 Byte
//               tx_buf  - 发送缓冲区首地址
//               tx_size - 发送缓冲区大小，单位 Byte
//  Return     : true：初始化成功；false：通用端口初始化失败
//  Others     : 先初始化 common_port 收发管理，再初始化 UART1 底层硬件
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
static bool uart1_port_Init(uint8_t *rx_buf, uint16_t rx_size,
                           uint8_t *tx_buf, uint16_t tx_size)
{
    if (!common_port_Init(&s_uart1Port, rx_buf, rx_size, tx_buf, tx_size))
    {
        return false;
    }

    bsp_uart1_Init();
    return true;
}


/*******************************************************************************
//  Function   : uart1_port_Deinit
//  Description: 反初始化 UART1 通信端口
//  Param      : None
//  Return     : None
//  Others     : 先关闭 UART1 硬件，再释放 common_port 通信状态
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
static void uart1_port_Deinit(void)
{
    if (s_uart1Port.init == 0U)
    {
        return;
    }

    bsp_uart1_Deinit();
    common_port_Deinit(&s_uart1Port);
}


/*******************************************************************************
//  Function   : uart1_port_ReceiveState
//  Description: 获取 UART1 当前接收状态
//  Param      : None
//  Return     : common_port_rx_state_t 接收状态
//  Others     : 直接返回 common_port 管理的接收状态
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
static common_port_rx_state_t uart1_port_ReceiveState(void)
{
    return common_port_ReceiveState(&s_uart1Port);
}


/*******************************************************************************
//  Function   : uart1_port_ReceiveData
//  Description: 获取 UART1 接收缓冲区地址
//  Param      : None
//  Return     : 接收数据缓冲区指针
//  Others     : 不复制接收数据，直接返回 common_port 内部缓冲区地址
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
static void *uart1_port_ReceiveData(void)
{
    return common_port_ReceiveData(&s_uart1Port);
}


/*******************************************************************************
//  Function   : uart1_port_ReceiveLength
//  Description: 获取 UART1 当前接收数据长度
//  Param      : None
//  Return     : 当前接收数据长度，单位 Byte
//  Others     : None
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
static uint16_t uart1_port_ReceiveLength(void)
{
    return common_port_ReceiveLength(&s_uart1Port);
}


/*******************************************************************************
//  Function   : uart1_port_ReceiveReset
//  Description: 复位 UART1 接收状态
//  Param      : None
//  Return     : None
//  Others     : 由 common_port 完成接收状态及相关管理信息复位
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
static void uart1_port_ReceiveReset(void)
{
    common_port_ReceiveReset(&s_uart1Port);
}


/*******************************************************************************
//  Function   : uart1_port_TransferData
//  Description: 获取 UART1 发送缓冲区地址
//  Param      : None
//  Return     : 发送数据缓冲区指针
//  Others     : 上层可通过该指针填写待发送数据
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
static void *uart1_port_TransferData(void)
{
    return common_port_TransferData(&s_uart1Port);
}


/*******************************************************************************
//  Function   : uart1_port_StartTransfer
//  Description: 启动 UART1 半双工数据发送
//  Param      : length - 待发送数据长度，单位 Byte
//  Return     : true：发送启动成功；false：端口忙或发送请求失败
//  Others     : 先申请 common_port 发送，再停止接收并切换为发送模式，
//               写入首字节后启动 UART1 发送
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
static bool uart1_port_StartTransfer(uint16_t length)
{
    uint8_t *data;

    /* 发送器未空闲或发送请求失败时拒绝启动。 */
    if (common_port_TransferState(&s_uart1Port) != COMMON_PORT_TX_IDLE ||
        !common_port_Transfer(&s_uart1Port, length))
    {
        return false;
    }

    data = common_port_TransferData(&s_uart1Port);

    /* 半双工通信：停止接收并切换至发送模式。 */
    bsp_uart1_ReceiveStop();
    bsp_uart1_HalfDuplexTx();

    /* 先写入首字节，再启动发送中断流程。 */
    bsp_uart1_TransmitByte(data[0]);
    bsp_uart1_TransmitStart();

    return true;
}


/*******************************************************************************
//  Function   : uart1_port_TransferState
//  Description: 获取 UART1 当前发送状态
//  Param      : None
//  Return     : common_port_tx_state_t 发送状态
//  Others     : 直接返回 common_port 管理的发送状态
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
static common_port_tx_state_t uart1_port_TransferState(void)
{
    return common_port_TransferState(&s_uart1Port);
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : uart1_port_1msTask
//  Description: UART1 通信端口 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 定期推进 common_port 通信状态及超时管理
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
void uart1_port_1msTask(void)
{
    common_port_1msTask(&s_uart1Port);
}


/*******************************************************************************
//  Function   : bsp_uart1_RxEventCallback
//  Description: UART1 接收事件回调函数
//  Param      : None
//  Return     : None
//  Others     : 由 UART1 底层接收中断调用，将接收到的字节交给 common_port
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
void bsp_uart1_RxEventCallback(void)
{
    common_port_RxEventCallback(&s_uart1Port, bsp_uart1_ReceiveByte());
}


/*******************************************************************************
//  Function   : bsp_uart1_TxEventCallback
//  Description: UART1 发送事件回调函数
//  Param      : None
//  Return     : None
//  Others     : 发送期间逐字节获取待发送数据；无后续数据时结束发送并恢复接收
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
void bsp_uart1_TxEventCallback(void)
{
    uint8_t data;

    /* 获取下一待发送字节。 */
    if (common_port_TxEventCallback(&s_uart1Port, &data))
    {
        bsp_uart1_TransmitByte(data);
    }
    else
    {
        /* 发送结束，停止发送并重新启动接收。 */
        bsp_uart1_TransmitStop();
        bsp_uart1_ReceiveStart();
    }
}


/*******************************************************************************
//  Function   : bsp_uart1_ErrorEventCallback
//  Description: UART1 通信错误事件回调函数
//  Param      : None
//  Return     : None
//  Others     : 接收出错时标记接收异常；若发送正在进行，则终止发送并恢复接收
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
void bsp_uart1_ErrorEventCallback(void)
{
    // /* 标记接收错误，并清除正在接收标志。 */
    // s_uart1Port.rx.State = COMMON_PORT_RX_ERROR;
    // s_uart1Port.rx.Receiving = 0U;

    // /* 发送期间发生错误时，终止发送并恢复接收。 */
    // if (s_uart1Port.tx.State == COMMON_PORT_TX_BUSY)
    // {
    //     s_uart1Port.tx.State = COMMON_PORT_TX_ERROR;
    //     bsp_uart1_TransmitStop();
    //     bsp_uart1_ReceiveStart();
    // }
}


/* Port API ----------------------------------------------------------------------------------------------------------*/

/** UART1 通用通信端口接口表 */
const common_port_api_t s_uart1PortApi =
{
    .Init           = uart1_port_Init,
    .Deinit         = uart1_port_Deinit,
    .receive_state  = uart1_port_ReceiveState,
    .receive_data   = uart1_port_ReceiveData,
    .receive_length = uart1_port_ReceiveLength,
    .receive_rst    = uart1_port_ReceiveReset,
    .transfer_data  = uart1_port_TransferData,
    .start_transfer = uart1_port_StartTransfer,
    .transfer_state = uart1_port_TransferState
};
