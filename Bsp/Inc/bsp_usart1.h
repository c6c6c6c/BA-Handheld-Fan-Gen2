/**
 ***********************************************************************************************************************
 * @file    bsp_modbus_usart.h
 * @brief   Modbus串口底层驱动接口定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef __BSP_MODBUS_USART_H
#define __BSP_MODBUS_USART_H

#include "main.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

#define MODBUS_USART_RX_PORT                   GPIOA                                                // Modbus串口RX端口
#define MODBUS_USART_RX_PIN                    LL_GPIO_PIN_7                                        // Modbus串口RX引脚
#define MODBUS_USART_RX_AF                     LL_GPIO_AF3_UART1                                    // Modbus串口RX复用功能
#define MODBUS_USART_RX_GPIO_CLOCK_EN()        LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA)    // 使能RX GPIO时钟
																																																    
#define MODBUS_USART_TX_PORT                   GPIOA                                                // Modbus串口TX端口
#define MODBUS_USART_TX_PIN                    LL_GPIO_PIN_6                                        // Modbus串口TX引脚
#define MODBUS_USART_TX_AF                     LL_GPIO_AF1_UART1                                   // Modbus串口TX复用功能
#define MODBUS_USART_TX_GPIO_CLOCK_EN()        LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA)    // 使能TX GPIO时钟

#define MODBUS_USART                           UART1                                                // Modbus串口外设
#define MODBUS_USART_CLOCK_EN()                LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_UART1)  // 使能串口时钟
#define MODBUS_USART_CLOCK_DIS()               LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_UART1) // 关闭串口时钟
#define MODBUS_USART_IRQHandler                UART1_IRQHandler                                     // Modbus串口中断服务函数
#define MODBUS_USART_IRQn                      UART1_IRQn                                           // Modbus串口中断号

/* Function ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_modbus_usart_Init
//  Description: 初始化Modbus串口
//  Param      : 无
//  Return     : 无
//  Others     : 完成串口GPIO、外设时钟、通信参数及中断相关配置
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_Init(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_Deinit
//  Description: 反初始化Modbus串口
//  Param      : 无
//  Return     : 无
//  Others     : 停止串口收发，并关闭串口中断、外设时钟及GPIO配置
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_Deinit(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_HalfDuplexTx
//  Description: 切换Modbus串口至半双工发送模式
//  Param      : 无
//  Return     : 无
//  Others     : 用于发送Modbus数据前切换串口通信方向
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_HalfDuplexTx(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_HalfDuplexRx
//  Description: 切换Modbus串口至半双工接收模式
//  Param      : 无
//  Return     : 无
//  Others     : 用于发送完成后恢复Modbus数据接收
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_HalfDuplexRx(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_ReceiveStart
//  Description: 启动Modbus串口接收
//  Param      : 无
//  Return     : 无
//  Others     : 使能串口接收及相关接收中断
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_ReceiveStart(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_ReceiveStop
//  Description: 停止Modbus串口接收
//  Param      : 无
//  Return     : 无
//  Others     : 关闭串口接收及相关接收中断
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_ReceiveStop(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_TransmitStart
//  Description: 启动Modbus串口发送
//  Param      : 无
//  Return     : 无
//  Others     : 使能串口发送及相关发送中断
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_TransmitStart(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_TransmitStop
//  Description: 停止Modbus串口发送
//  Param      : 无
//  Return     : 无
//  Others     : 关闭串口发送及相关发送中断
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_TransmitStop(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_TransmitByte
//  Description: 发送一个Modbus串口数据字节
//  Param      : Val - 待发送数据
//  Return     : 无
//  Others     : 将指定数据写入串口发送数据寄存器
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_TransmitByte(uint8_t Val);

/*******************************************************************************
//  Function   : bsp_modbus_usart_ReceiveByte
//  Description: 获取一个Modbus串口接收数据字节
//  Param      : 无
//  Return     : 当前接收到的数据
//  Others     : 读取串口接收数据寄存器
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
uint8_t bsp_modbus_usart_ReceiveByte(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_RxEventCallback
//  Description: Modbus串口接收事件回调函数
//  Param      : 无
//  Return     : 无
//  Others     : 弱定义函数，可由上层重写用于处理串口接收事件
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_RxEventCallback(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_TxEventCallback
//  Description: Modbus串口发送事件回调函数
//  Param      : 无
//  Return     : 无
//  Others     : 弱定义函数，可由上层重写用于处理串口发送事件
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_TxEventCallback(void);

/*******************************************************************************
//  Function   : bsp_modbus_usart_ErrorEventCallback
//  Description: Modbus串口错误事件回调函数
//  Param      : 无
//  Return     : 无
//  Others     : 弱定义函数，可由上层重写用于处理串口通信异常
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_modbus_usart_ErrorEventCallback(void);

#endif /* __BSP_MODBUS_USART_H */
