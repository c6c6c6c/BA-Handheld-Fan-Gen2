/**
 ***********************************************************************************************************************
 * @file    bsp_exti.h
 * @brief   EXTI 外部中断及低功耗唤醒接口定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef __BSP_EXTI_H
#define __BSP_EXTI_H

#include "main.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风扇按键睡眠唤醒 EXTI 配置
 *
 * FAN_SW 使用 PA8，对应 EXTI Line 8。
 * 系统休眠期间检测按键有效边沿，用于唤醒 MCU。
 */
#define FAN_SW_EXTI_LINE               LL_EXTI_LINE_8
#define FAN_SW_EXTI_TRIGGER            LL_EXTI_TRIGGER_RISING
#define FAN_SW_EXTI_CONFIG_PORT        LL_EXTI_CONFIG_PORTA
#define FAN_SW_EXTI_CONFIG_LINE        LL_EXTI_CONFIG_LINE8


/**
 * @brief 喷雾按键睡眠唤醒 EXTI 配置
 *
 * MIST_SW 使用 PB2，对应 EXTI Line 2。
 * 系统休眠期间检测按键有效边沿，用于唤醒 MCU。
 */
#define MIST_SW_EXTI_LINE              LL_EXTI_LINE_2
#define MIST_SW_EXTI_TRIGGER           LL_EXTI_TRIGGER_RISING
#define MIST_SW_EXTI_CONFIG_PORT       LL_EXTI_CONFIG_PORTB
#define MIST_SW_EXTI_CONFIG_LINE       LL_EXTI_CONFIG_LINE2


/**
 * @brief USB 插入唤醒 EXTI 配置
 *
 * USB_IN 用于检测 USB 输入状态。
 * USB 插入后信号产生上升沿，通过 EXTI 唤醒 MCU。
 */
#define USB_IN_EXTI_LINE               LL_EXTI_LINE_10
#define USB_IN_EXTI_TRIGGER            LL_EXTI_TRIGGER_RISING
#define USB_IN_EXTI_CONFIG_PORT        LL_EXTI_CONFIG_PORTA
#define USB_IN_EXTI_CONFIG_LINE        LL_EXTI_CONFIG_LINE10


/* Function ----------------------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Function   : bsp_exti_Init
//  Description: 初始化板级 EXTI
//  Param      : None
//  Return     : None
//  Others     : 配置按键低功耗唤醒及 USB 插入中断/唤醒功能
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_exti_Init(void);

#endif /* __BSP_EXTI_H */
