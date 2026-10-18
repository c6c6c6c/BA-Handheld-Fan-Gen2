/**
 ***********************************************************************************************************************
 * @file    bsp_usart1.c
 * @brief   UART1 底层驱动实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "bsp_uart1.h"


/* Weak callback -----------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_usart1_RxEventCallback
//  Description: UART1 接收事件回调函数
//  Param      : None
//  Return     : None
//  Others     : 弱定义函数，可由应用层重写；回调中应及时读取接收数据
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
__weak void bsp_uart1_RxEventCallback(void){}

/*******************************************************************************
//  Function   : bsp_usart1_TxEventCallback
//  Description: UART1 发送事件回调函数
//  Param      : None
//  Return     : None
//  Others     : 弱定义函数，可由应用层重写；用于装载下一个待发送字节
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
__weak void bsp_uart1_TxEventCallback(void){}

/*******************************************************************************
//  Function   : bsp_usart1_ErrorEventCallback
//  Description: UART1 错误事件回调函数
//  Param      : None
//  Return     : None
//  Others     : 弱定义函数，可由应用层重写；用于处理奇偶校验、帧、溢出及 Break 错误
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
__weak void bsp_uart1_ErrorEventCallback(void){}


/* Private function --------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_usart1_io_Init
//  Description: 初始化 UART1 收发 GPIO 及中断控制器
//  Param      : None
//  Return     : None
//  Others     : PA1 配置为 UART1_RX，PA0 配置为 UART1_TX，并开启 UART1 NVIC 中断
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void bsp_uart1_io_config(void)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* PA0 和 PA1 位于 GPIOA，初始化前先开启端口时钟。 */
    UART1_RX_GPIO_CLOCK_EN();
    UART1_TX_GPIO_CLOCK_EN();

    /* PA1 配置为 UART1_RX：复用推挽输入，上拉保持空闲高电平。 */
    GPIO_InitStruct.Pin        = UART1_RX_PIN;
    GPIO_InitStruct.Mode       = LL_GPIO_MODE_ALTERNATE;
    GPIO_InitStruct.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    GPIO_InitStruct.Pull       = LL_GPIO_PULL_UP;
    GPIO_InitStruct.Alternate  = UART1_RX_AF;
    LL_GPIO_Init(UART1_RX_PORT, &GPIO_InitStruct);

    /* PA0 配置为 UART1_TX，与头文件定义的 AF9 复用功能保持一致。 */
    GPIO_InitStruct.Pin       = UART1_TX_PIN;
    GPIO_InitStruct.Alternate = UART1_TX_AF;
    LL_GPIO_Init(UART1_TX_PORT, &GPIO_InitStruct);

    /* UART1 使用独立中断入口，接收、发送和错误事件均在其中处理。 */
    NVIC_SetPriority(UART1_IRQn, 0U);
    NVIC_EnableIRQ(UART1_IRQn);
}


/* Exported function -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_usart1_Init
//  Description: 初始化 UART1
//  Param      : None
//  Return     : None
//  Others     : 配置 UART1 为 115200、8N1，初始化完成后默认进入中断接收状态
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_Init(void)
{
    LL_UART_InitTypeDef UART_InitStruct = {0};

    /* UART1 挂载在 APB1 Group1，总线时钟必须先于寄存器配置开启。 */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART1);
    bsp_uart1_io_config();

    /* 配置为 115200 baud、8 数据位、无校验、1 停止位、低位先发。 */
    UART_InitStruct.BaudRate    = 115200U;
    UART_InitStruct.DataWidth   = LL_UART_DATAWIDTH_8B;
    UART_InitStruct.StopBits    = LL_UART_STOPBITS_1;
    UART_InitStruct.Parity      = LL_UART_PARITY_NONE;
    UART_InitStruct.BitOrder    = LL_UART_BITORDER_LSBFIRST;
    UART_InitStruct.TxMode      = LL_UART_TXMODE_0;
    UART_InitStruct.RxMode      = LL_UART_RX_ADDR_MATCH_SOFT;
    UART_InitStruct.RxMatchAddr = 0U;

    if (LL_UART_Init(UART1, &UART_InitStruct) != SUCCESS)
    {
        bsp_uart1_ErrorEventCallback();
        return;
    }

    bsp_uart1_ReceiveStart();
}

/*******************************************************************************
//  Function   : bsp_usart1_Deinit
//  Description: 反初始化 UART1
//  Param      : None
//  Return     : None
//  Others     : 关闭收发中断和外设时钟，并将收发引脚恢复为模拟模式
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_Deinit(void)
{
    LL_GPIO_InitTypeDef GPIO_InitStruct = {0};

    bsp_uart1_ReceiveStop();
    LL_UART_DisableIT_TXE(UART1);

    NVIC_DisableIRQ(UART1_IRQn);
    NVIC_ClearPendingIRQ(UART1_IRQn);

    if (LL_UART_DeInit(UART1) != SUCCESS)
    {
        bsp_uart1_ErrorEventCallback();
    }

    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_UART1);

    /* 模拟模式关闭数字输入缓冲，可降低反初始化后的静态功耗。 */
    GPIO_InitStruct.Mode = LL_GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = LL_GPIO_PULL_NO;

    GPIO_InitStruct.Pin = UART1_RX_PIN;
    LL_GPIO_Init(UART1_RX_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = UART1_TX_PIN;
    LL_GPIO_Init(UART1_TX_PORT, &GPIO_InitStruct);
}

/*******************************************************************************
//  Function   : bsp_usart1_HalfDuplexTx
//  Description: 切换 UART1 至半双工发送模式
//  Param      : None
//  Return     : None
//  Others     : 将 TX 引脚切换为 UART1 复用功能，使能总线发送方向
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_HalfDuplexTx(void)
{
    LL_GPIO_SetPinMode(UART1_TX_PORT, UART1_TX_PIN, LL_GPIO_MODE_ALTERNATE);
}

/*******************************************************************************
//  Function   : bsp_uart1_HalfDuplexRx
//  Description: 切换 UART1 至半双工接收模式
//  Param      : None
//  Return     : None
//  Others     : 将 TX 引脚切换为输入高阻，释放半双工通信总线
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_HalfDuplexRx(void)
{
    LL_GPIO_SetPinMode(UART1_TX_PORT, UART1_TX_PIN, LL_GPIO_MODE_INPUT);
}

/*******************************************************************************
//  Function   : bsp_usart1_ReceiveStart
//  Description: 启动 UART1 接收
//  Param      : None
//  Return     : None
//  Others     : 切换至接收方向，并开启线路状态和接收数据中断
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_ReceiveStart(void)
{
    bsp_uart1_HalfDuplexRx();
    LL_UART_EnableIT_LS(UART1);
    LL_UART_EnableIT_RXNE(UART1);
}

/*******************************************************************************
//  Function   : bsp_usart1_ReceiveStop
//  Description: 停止 UART1 接收
//  Param      : None
//  Return     : None
//  Others     : 关闭线路状态和接收数据中断
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_ReceiveStop(void)
{
    LL_UART_DisableIT_LS(UART1);
    LL_UART_DisableIT_RXNE(UART1);
}

/*******************************************************************************
//  Function   : bsp_usart1_TransmitStart
//  Description: 启动 UART1 发送
//  Param      : None
//  Return     : None
//  Others     : 停止接收、切换至发送方向，并开启发送数据寄存器空中断
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_TransmitStart(void)
{
    LL_UART_EnableIT_TXE(UART1);
}

/*******************************************************************************
//  Function   : bsp_usart1_TransmitStop
//  Description: 停止 UART1 发送
//  Param      : None
//  Return     : None
//  Others     : 等待最后一个字节发送完成，然后恢复中断接收状态
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_TransmitStop(void)
{
    LL_UART_DisableIT_TXE(UART1);
}

/*******************************************************************************
//  Function   : bsp_uart1_TransmitByte
//  Description: 发送一个 UART1 数据字节
//  Param      : Val - 待发送的数据
//  Return     : None
//  Others     : 将指定数据写入 UART1 发送数据寄存器
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_TransmitByte(uint8_t Val)
{
    LL_UART_TransmitData(UART1, Val);
}

/*******************************************************************************
//  Function   : bsp_usart1_ReceiveByte
//  Description: 获取一个 UART1 接收数据字节
//  Param      : None
//  Return     : 当前接收到的数据
//  Others     : 从 UART1 接收数据寄存器读取一个字节
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
uint8_t bsp_uart1_ReceiveByte(void)
{
    return LL_UART_ReceiveData(UART1);
}


/* Interrupt handler -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : UART1_IRQHandler
//  Description: UART1 中断服务函数
//  Param      : None
//  Return     : None
//  Others     : 按错误、接收、发送的优先顺序处理 UART1 中断事件
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void UART1_IRQHandler(void)
{
    uint32_t ErrorFlags;

    /* 汇总所有线路错误，优先于正常收发事件处理。 */
    ErrorFlags = (LL_UART_IsActiveFlag_PE(UART1)  | LL_UART_IsActiveFlag_FE(UART1) 
                | LL_UART_IsActiveFlag_ORE(UART1) | LL_UART_IsActiveFlag_BRI(UART1));

    if (ErrorFlags != RESET)
    {
        LL_UART_ClearFlag_PE(UART1);
        LL_UART_ClearFlag_FE(UART1);
        LL_UART_ClearFlag_ORE(UART1);
        LL_UART_ClearFlag_BRI(UART1);
        bsp_uart1_ErrorEventCallback();
        return;
    }

    /* 回调函数负责调用 bsp_usart1_ReceiveByte() 及时取走数据。 */
    if ((LL_UART_IsActiveFlag_RXNE(UART1) != RESET) && (LL_UART_IsEnabledIT_RXNE(UART1) != RESET))
    {
        bsp_uart1_RxEventCallback();
        return;
    }

    /* 回调函数负责装载下一个待发送字节或结束本次发送。 */
    if ((LL_UART_IsActiveFlag_TXE(UART1) != RESET) && (LL_UART_IsEnabledIT_TXE(UART1) != RESET))
    {
        bsp_uart1_TxEventCallback();
    }

}
