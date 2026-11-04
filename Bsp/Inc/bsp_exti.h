
/**
 ***********************************************************************************************************************
 * @file    bsp_exti.h
 * @brief   EXTI 外部中断及低功耗唤醒接口，定义电源按键与 USB_IN 唤醒配置
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @note
 *          1. POWER_SW 使用 PA7，对应 EXTI Line7。
 *          2. USB_IN 使用 PA10，对应 EXTI Line10。
 *          3. 两路 EXTI 均配置为上升沿触发。
 *          4. 可通过 EXTI Event 配合 WFE 实现低功耗唤醒。
 *          5. EXTI 模式及 NVIC 中断使能由 bsp_exti.c 配置。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef __BSP_EXTI_H
#define __BSP_EXTI_H

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "main.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief 电源按键低功耗唤醒 EXTI 配置
 * @note  POWER_SW 使用 PA7，对应 EXTI Line7。
 *        检测到电源按键有效上升沿时产生 EXTI 触发事件。
 *        可配合 WFE 指令实现低功耗唤醒。
 *
 *        POWER_SW_EXTI_LINE        ：EXTI 中断/事件线。
 *        POWER_SW_EXTI_TRIGGER     ：EXTI 下升沿触发方式。
 *        POWER_SW_EXTI_CONFIG_PORT ：EXTI 对应 GPIOA 端口。
 *        POWER_SW_EXTI_CONFIG_LINE ：GPIO 引脚与 EXTI Line7 映射。
 */
#define POWER_SW_EXTI_LINE              LL_EXTI_LINE_7
#define POWER_SW_EXTI_TRIGGER           LL_EXTI_TRIGGER_FALLING
#define POWER_SW_EXTI_CONFIG_PORT       LL_EXTI_CONFIG_PORTA
#define POWER_SW_EXTI_CONFIG_LINE       LL_EXTI_CONFIG_LINE7


/**
 * @brief USB_IN 插入检测及低功耗唤醒 EXTI 配置
 * @note  USB_IN 使用 PA10，对应 EXTI Line10。
 *        USB 输入信号产生上升沿时触发 EXTI。
 *        可用于 USB 插入检测及 MCU 低功耗唤醒。
 *
 *        USB_IN_EXTI_LINE        ：EXTI 中断/事件线。
 *        USB_IN_EXTI_TRIGGER     ：EXTI 上升沿触发方式。
 *        USB_IN_EXTI_CONFIG_PORT ：EXTI 对应 GPIOA 端口。
 *        USB_IN_EXTI_CONFIG_LINE ：GPIO 引脚与 EXTI Line10 映射。
 *
 *        当前配置仅检测上升沿。
 *        如需检测 USB 拔出事件，应另外配置下降沿触发
 *        或通过软件周期读取 USB_IN 电平。
 */
#define USB_IN_EXTI_LINE               LL_EXTI_LINE_10
#define USB_IN_EXTI_TRIGGER            LL_EXTI_TRIGGER_RISING
#define USB_IN_EXTI_CONFIG_PORT        LL_EXTI_CONFIG_PORTA
#define USB_IN_EXTI_CONFIG_LINE        LL_EXTI_CONFIG_LINE10


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_exti_Init
//  Description: 初始化板级 EXTI 外部事件及低功耗唤醒功能
//  Param      : None
//  Return     : None
//  Others     : 使能 SYSCFG 外设时钟；
//               配置 POWER_SW 和 USB_IN 对应的 EXTI Line；
//               设置 GPIO 端口映射及上升沿触发方式；
//               当前实现使用 EXTI Event 模式，不产生 CPU 中断请求；
//               GPIO 输入模式应提前通过 bsp_gpio_Init() 配置
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_exti_Init(void);


#endif /* __BSP_EXTI_H */
