/**
 ***********************************************************************************************************************
 * @file    modbus_port.c
 * @brief   Modbus RTU master physical port adapter based on common_port.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-19
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "modbus_port.h"
#include "bsp_modbus_usart.h"

/* Private variables -------------------------------------------------------------------------------------------------*/
common_port_t        s_modbusPort;

/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : modbus_port_api_Init
//  Description: 初始化 Modbus USART 底层端口并启动接收
//  Param      : None
//  Return     : None
//  Others     : 作为 common_port_api_t.Init 回调使用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
static bool modbus_port_api_Init( uint8_t *rx_buf,uint16_t rx_size,
                                  uint8_t *tx_buf,uint16_t tx_size)
{
    if(!common_port_Init(&s_modbusPort,rx_buf,rx_size,tx_buf,tx_size))
    {
        return false;
    }
    bsp_modbus_usart_Init();
    bsp_modbus_usart_HalfDuplexRx();
    bsp_modbus_usart_ReceiveStart();
    return true;
}

/*******************************************************************************
//  Function   : modbus_port_api_Deinit
//  Description: 反初始化 Modbus USART 底层端口
//  Param      : None
//  Return     : None
//  Others     : 作为 common_port_api_t.Deinit 回调使用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
static void modbus_port_api_Deinit(void)
{
    if(s_modbusPort.init == 0U)
    {
        return;
    }
    common_port_Deinit(&s_modbusPort);
    bsp_modbus_usart_ReceiveStop();
    bsp_modbus_usart_TransmitStop();
    bsp_modbus_usart_Deinit();
}

/*******************************************************************************
//  Function   : modbus_port_api_ReceiveState
//  Description: 获取 common_port 当前接收状态
//  Param      : None
//  Return     : 当前接收状态
//  Others     : 作为 common_port_api_t.ReceiveState 回调使用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
static common_port_rx_state_t modbus_port_api_ReceiveState(void)
{
    return common_port_ReceiveState(&s_modbusPort);
}

/*******************************************************************************
//  Function   : modbus_port_api_ReceiveData
//  Description: 获取 common_port 当前接收缓冲区地址
//  Param      : None
//  Return     : 接收缓冲区首地址
//  Others     : 作为 common_port_api_t.ReceiveData 回调使用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
static void *modbus_port_api_ReceiveData(void)
{
    return common_port_ReceiveData(&s_modbusPort);
}

/*******************************************************************************
//  Function   : modbus_port_api_ReceiveData
//  Description: 获取 common_port 当前接收缓冲区地址
//  Param      : None
//  Return     : 接收缓冲区首地址
//  Others     : 作为 common_port_api_t.ReceiveData 回调使用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
static void modbus_port_api_ReceiveRst(void)
{
     common_port_ReceiveReset(&s_modbusPort);
}

/*******************************************************************************
//  Function   : modbus_port_api_ReceiveLength
//  Description: 获取 common_port 当前接收数据长度
//  Param      : None
//  Return     : 接收数据长度，单位 Byte
//  Others     : 作为 common_port_api_t.ReceiveLength 回调使用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
static uint16_t modbus_port_api_ReceiveLength(void)
{
    return common_port_ReceiveLength(&s_modbusPort);
}

/*******************************************************************************
//  Function   : modbus_port_api_Transfer
//  Description: 向 Modbus USART 底层端口写入一个发送字节并启动发送中断
//  Param      : pData  - 待发送数据指针
//               length - 待发送长度
//  Return     : 1 - 启动成功
//               0 - 参数错误或长度不支持
//  Others     : common_port 当前按字节驱动该接口，length 应为 1
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
static bool modbus_port_api_StartTransfer(uint16_t length)
{
    if(common_port_Transfer(&s_modbusPort, length))
    {
        uint8_t *pData = common_port_TransferData(&s_modbusPort);
        bsp_modbus_usart_HalfDuplexTx();
        bsp_modbus_usart_TransmitByte(pData[0]);
        bsp_modbus_usart_TransmitStart();
        return true;
    }
    return false;
}

/*******************************************************************************
//  Function   : modbus_port_api_TransferState
//  Description: 获取 common_port 当前发送状态
//  Param      : None
//  Return     : 当前发送状态
//  Others     : 作为 common_port_api_t.TransferState 回调使用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
static common_port_tx_state_t modbus_port_api_TransferState(void)
{
    return common_port_TransferState(&s_modbusPort);
}

/*******************************************************************************
//  Function   : modbus_port_api_TransferData
//  Description: 获取 common_port 当前发送数据
//  Param      : None
//  Return     : 当前发送数据
//  Others     : 作为 common_port_api_t.TransferData 回调使用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
static void *modbus_port_api_TransferData(void)
{
    return common_port_TransferData(&s_modbusPort);
}


/*******************************************************************************
//  Function   : ModbusPort_1msTask
//  Description: 执行 Modbus master 端口层 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 调用 common_port 的不定长接收帧间隔计时，并同步接收长度
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void ModbusPort_1msTask(void)
{
    common_port_1msTask(&s_modbusPort);
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_RxEventCallback
//  Description: 处理 Modbus USART 接收字节事件
//  Param      : None
//  Return     : None
//  Others     : 将底层接收字节写入 common_port 接收缓冲区并同步 Modbus 状态机
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void bsp_modbus_usart_RxEventCallback(void)
{
    common_port_RxEventCallback(&s_modbusPort, bsp_modbus_usart_ReceiveByte());
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_TxEventCallback
//  Description: 处理 Modbus USART 发送事件
//  Param      : None
//  Return     : None
//  Others     : 由 common_port 环形缓冲区续发下一字节，全部完成后置位 TxDone
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void bsp_modbus_usart_TxEventCallback(void)
{
    uint8_t pData;
    if (common_port_TxEventCallback(&s_modbusPort, &pData))
    {
        bsp_modbus_usart_TransmitByte(pData);
    }
    else
    {
        bsp_modbus_usart_TransmitStop();
        //bsp_modbus_usart_HalfDuplexRx();
    }
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_ErrorEventCallback
//  Description: 处理 Modbus USART 通信错误事件
//  Param      : None
//  Return     : None
//  Others     : 同步 common_port 错误状态和 Modbus master 串口错误标志
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void bsp_modbus_usart_ErrorEventCallback(void)
{
    //common_port_ErrorEventCallback(&s_modbusPort);
}

/*******************************************************************************
//  Description: 处理 Modbus USART 通信错误事件
*******************************************************************************/
const common_port_api_t s_modbusPortApi =
{
    .Init           = modbus_port_api_Init,
    .Deinit         = modbus_port_api_Deinit,

    .receive_state  = modbus_port_api_ReceiveState,
    .receive_data   = modbus_port_api_ReceiveData,
    .receive_length = modbus_port_api_ReceiveLength,
	.receive_rst    = modbus_port_api_ReceiveRst,

    .transfer_data  = modbus_port_api_TransferData,
    .start_transfer = modbus_port_api_StartTransfer,
    .transfer_state = modbus_port_api_TransferState
};
