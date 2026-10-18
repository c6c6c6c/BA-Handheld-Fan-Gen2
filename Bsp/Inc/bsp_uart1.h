/**
 ***********************************************************************************************************************
 * @file    bsp_uart1.h
 * @brief   USART1 底层驱动接口及 GPIO 配置定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-10
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BSP_UART1_H_
#define _BSP_UART1_H_

#include "main.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief UART1 RX 引脚配置
 *
 * GPIO : PA1
 * AF   : AF9 UART1
 */
#define UART1_RX_PORT                   GPIOA
#define UART1_RX_PIN                    LL_GPIO_PIN_1
#define UART1_RX_AF                     LL_GPIO_AF9_UART1
#define UART1_RX_GPIO_CLOCK_EN()        LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA)


/**
 * @brief UART1 TX 引脚配置
 *
 * GPIO : PA0
 * AF   : AF9 UART1
 */
#define UART1_TX_PORT                   GPIOA
#define UART1_TX_PIN                    LL_GPIO_PIN_0
#define UART1_TX_AF                     LL_GPIO_AF9_UART1
#define UART1_TX_GPIO_CLOCK_EN()        LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA)


/* Function ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_usart1_Init
//  Description: 初始化 UART1
//  Param      : None
//  Return     : None
//  Others     : 配置 UART1 为 115200、8N1，初始化完成后默认进入中断接收状态
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_uart1_Init(void);


/*******************************************************************************
//  Function   : bsp_uart1_Deinit
//  Description: 反初始化 UART1
//  Param      : None
//  Return     : None
//  Others     : 停止 UART1 收发，并关闭串口中断、外设时钟及相关 GPIO 配置
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_uart1_Deinit(void);


/*******************************************************************************
//  Function   : bsp_uart1_HalfDuplexTx
//  Description: 切换 UART1 至半双工发送模式
//  Param      : None
//  Return     : None
//  Others     : 数据发送前调用，将 UART1 通信方向切换至发送状态
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_uart1_HalfDuplexTx(void);


/*******************************************************************************
//  Function   : bsp_uart1_HalfDuplexRx
//  Description: 切换 UART1 至半双工接收模式
//  Param      : None
//  Return     : None
//  Others     : 数据发送完成后调用，将 UART1 通信方向恢复至接收状态
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_uart1_HalfDuplexRx(void);


/*******************************************************************************
//  Function   : bsp_uart1_ReceiveStart
//  Description: 启动 UART1 数据接收
//  Param      : None
//  Return     : None
//  Others     : 使能 UART1 接收功能及相关接收中断
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_uart1_ReceiveStart(void);


/*******************************************************************************
//  Function   : bsp_uart1_ReceiveStop
//  Description: 停止 USART1 数据接收
//  Param      : None
//  Return     : None
//  Others     : 关闭 USART1 接收功能及相关接收中断
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_uart1_ReceiveStop(void);


/*******************************************************************************
//  Function   : bsp_uart1_TransmitStart
//  Description: 启动 UART1 数据发送
//  Param      : None
//  Return     : None
//  Others     : 使能 UART1 发送功能及相关发送中断
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_uart1_TransmitStart(void);


/*******************************************************************************
//  Function   : bsp_uart1_TransmitStop
//  Description: 停止 UART1 数据发送
//  Param      : None
//  Return     : None
//  Others     : 关闭 UART1 发送功能及相关发送中断
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_uart1_TransmitStop(void);


/*******************************************************************************
//  Function   : bsp_uart1_TransmitByte
//  Description: 发送一个 UART1 数据字节
//  Param      : Val - 待发送的数据
//  Return     : None
//  Others     : 将指定数据写入 UART1 发送数据寄存器
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_uart1_TransmitByte(uint8_t Val);


/*******************************************************************************
//  Function   : bsp_uart1_ReceiveByte
//  Description: 读取一个 UART1 接收数据字节
//  Param      : None
//  Return     : UART1 当前接收到的数据
//  Others     : 从 UART1 接收数据寄存器读取一个字节
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
uint8_t bsp_uart1_ReceiveByte(void);


#endif /* _BSP_USART1_H_ */
