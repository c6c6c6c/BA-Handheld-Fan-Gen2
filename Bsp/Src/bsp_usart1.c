/**
 ***********************************************************************************************************************
 * @file    bsp_modbus_usart.c
 * @brief   Modbus UART BSP.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */
#include "bsp_modbus_usart.h"

/*******************************************************************************
//  Function   : bsp_modbus_usart_RxEventCallback
//  Description: Modbus UART 接收事件回调
//  Param      : None
//  Return     : None
//  Others     : 弱定义，用户可重写
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
__weak void bsp_usart1_RxEventCallback(void){}

/*******************************************************************************
//  Function   : bsp_modbus_usart_TxEventCallback
//  Description: Modbus UART 发送事件回调
//  Param      : None
//  Return     : None
//  Others     : 弱定义，用户可重写
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
__weak void bsp_usart1_TxEventCallback(void){}

/*******************************************************************************
//  Function   : bsp_modbus_usart_ErrorEventCallback
//  Description: Modbus UART 错误事件回调
//  Param      : None
//  Return     : None
//  Others     : 弱定义，用户可重写
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
__weak void bsp_usart1_ErrorEventCallback(void){}

/*******************************************************************************
//  Function   : bsp_modbus_io_Init
//  Description: 初始化 Modbus UART GPIO 和外设时钟
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
static void bsp_modbus_io_Init(void)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    MODBUS_USART_RX_GPIO_CLOCK_EN();
    MODBUS_USART_TX_GPIO_CLOCK_EN();
    MODBUS_USART_CLOCK_EN();

    GPIO_InitStruct.Pin        = MODBUS_USART_RX_PIN;
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_UP;
    GPIO_InitStruct.Alternate  = MODBUS_USART_RX_AF;
    LL_GPIO_Init(MODBUS_USART_RX_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin        = MODBUS_USART_TX_PIN;
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_UP;
    GPIO_InitStruct.Alternate  = MODBUS_USART_TX_AF;
    LL_GPIO_Init(MODBUS_USART_TX_PORT, &GPIO_InitStruct);
    LL_GPIO_SetOutputPin(MODBUS_USART_TX_PORT, MODBUS_USART_TX_PIN);

    NVIC_SetPriority(MODBUS_USART_IRQn, 0);
    NVIC_EnableIRQ(MODBUS_USART_IRQn);
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_Init
//  Description: 初始化 Modbus UART
//  Param      : None
//  Return     : None
//  Others     : 默认 115200 8N1
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_Init(void)
{
    LL_UART_InitTypeDef UART_InitStruct = {0};

    bsp_modbus_io_Init();

    UART_InitStruct.BaudRate    = 115200;
    UART_InitStruct.DataWidth   = LL_UART_DATAWIDTH_8B;
    UART_InitStruct.StopBits    = LL_UART_STOPBITS_1;
    UART_InitStruct.Parity      = LL_UART_PARITY_NONE;
    UART_InitStruct.BitOrder    = LL_UART_BITORDER_LSBFIRST;
    UART_InitStruct.TxMode      = LL_UART_TXMODE_0;
    UART_InitStruct.RxMode      = LL_UART_RX_ADDR_MATCH_SOFT;
    UART_InitStruct.RxMatchAddr = 0;
    LL_UART_Init(MODBUS_USART, &UART_InitStruct);

    bsp_modbus_usart_HalfDuplexRx();
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_Deinit
//  Description: 反初始化 Modbus UART
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_Deinit(void)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    NVIC_DisableIRQ(MODBUS_USART_IRQn);
    MODBUS_USART_CLOCK_DIS();

    GPIO_InitStruct.Pin        = MODBUS_USART_TX_PIN;
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_UP;
    GPIO_InitStruct.Alternate  = LL_GPIO_AF0_SWJ;
    LL_GPIO_Init(MODBUS_USART_TX_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin        = MODBUS_USART_RX_PIN;
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_UP;
    GPIO_InitStruct.Alternate  = LL_GPIO_AF0_SWJ;
    LL_GPIO_Init(MODBUS_USART_RX_PORT, &GPIO_InitStruct);
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_HalfDuplexTx
//  Description: 切换 Modbus UART 到发送模式
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_HalfDuplexTx(void)
{
    LL_GPIO_SetPinMode(MODBUS_USART_TX_PORT, MODBUS_USART_TX_PIN, LL_GPIO_MODE_ALTERNATE);
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_HalfDuplexRx
//  Description: 切换 Modbus UART 到接收模式
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_HalfDuplexRx(void)
{
    LL_GPIO_SetPinMode(MODBUS_USART_TX_PORT, MODBUS_USART_TX_PIN, LL_GPIO_MODE_INPUT);
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_ReceiveStart
//  Description: 开启 Modbus UART 接收中断
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_ReceiveStart(void)
{
    LL_UART_EnableIT_LS(MODBUS_USART);
    LL_UART_EnableIT_RXNE(MODBUS_USART);
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_ReceiveStop
//  Description: 关闭 Modbus UART 接收中断
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_ReceiveStop(void)
{
    LL_UART_DisableIT_LS(MODBUS_USART);
    LL_UART_DisableIT_RXNE(MODBUS_USART);
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_TransmitStart
//  Description: 开启 Modbus UART 发送中断
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_TransmitStart(void)
{
    LL_UART_EnableIT_TXE(MODBUS_USART);
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_TransmitStop
//  Description: 关闭 Modbus UART 发送中断
//  Param      : None
//  Return     : None
//  Others     : 发送完成后切回接收模式
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_TransmitStop(void)
{
    LL_UART_DisableIT_TXE(MODBUS_USART);
    while (LL_UART_IsActiveFlag_TXE(MODBUS_USART) == RESET)
    {
    }
    bsp_modbus_usart_HalfDuplexRx();
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_TransmitByte
//  Description: Modbus UART 发送 1 字节数据
//  Param      : Val: 待发送字节
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_TransmitByte(uint8_t Val)
{
    LL_UART_TransmitData(MODBUS_USART, Val);
}

/*******************************************************************************
//  Function   : bsp_modbus_usart_ReceiveByte
//  Description: Modbus UART 接收 1 字节数据
//  Param      : None
//  Return     : 接收到的字节
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
uint8_t bsp_modbus_usart_ReceiveByte(void)
{
    return LL_UART_ReceiveData(MODBUS_USART);
}

/*******************************************************************************
//  Function   : MODBUS_USART_IRQHandler
//  Description: Modbus UART 中断处理函数
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-08-18
*******************************************************************************/
void MODBUS_USART_IRQHandler(void)
{
    uint32_t ErrorFlags = (LL_UART_IsActiveFlag_PE(MODBUS_USART)  |
                           LL_UART_IsActiveFlag_FE(MODBUS_USART)  |
                           LL_UART_IsActiveFlag_ORE(MODBUS_USART) |
                           LL_UART_IsActiveFlag_BRI(MODBUS_USART));

    if (ErrorFlags == RESET)
    {
        if ((LL_UART_IsActiveFlag_RXNE(MODBUS_USART) != RESET) &&
            (LL_UART_IsEnabledIT_RXNE(MODBUS_USART) != RESET))
        {
            bsp_modbus_usart_RxEventCallback();
            return;
        }
    }
		
    if (ErrorFlags != RESET)
    {
        LL_UART_ClearFlag_PE(MODBUS_USART);
        LL_UART_ClearFlag_FE(MODBUS_USART);
        LL_UART_ClearFlag_ORE(MODBUS_USART);
        LL_UART_ClearFlag_BRI(MODBUS_USART);
        bsp_modbus_usart_ErrorEventCallback();
    }

    if ((LL_UART_IsActiveFlag_TXE(MODBUS_USART) != RESET) &&
        (LL_UART_IsEnabledIT_TXE(MODBUS_USART) != RESET))
    {
        bsp_modbus_usart_TxEventCallback();
        return;
    }
}
