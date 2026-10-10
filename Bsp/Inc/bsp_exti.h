/**
 ***********************************************************************************************************************
 * @file    bsp_exti.h
 * @brief   TIM1定时器底层驱动接口定义
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

/* 睡眠唤醒 - Fan按键 -------------------------------------------------------------------------------------------------*/
#define FAN_SW_EXTI_LINE               LL_EXTI_LINE_8   
#define FAN_SW_EXTI_TRIGGER            LL_EXTI_TRIGGER_RISING
#define FAN_SW_EXTI_CONFIG_PORT        LL_EXTI_CONFIG_PORTA
#define FAN_SW_EXTI_CONFIG_LINE        LL_EXTI_CONFIG_LINE8

/* 睡眠唤醒 - MIST按键 -------------------------------------------------------------------------------------------------*/
#define MIST_SW_EXTI_LINE              LL_EXTI_LINE_9   
#define MIST_SW_EXTI_TRIGGER           LL_EXTI_TRIGGER_RISING
#define MIST_SW_EXTI_CONFIG_PORT       LL_EXTI_CONFIG_PORTA
#define MIST_SW_EXTI_CONFIG_LINE       LL_EXTI_CONFIG_LINE9

/* Function ----------------------------------------------------------------------------------------------------------*/

#endif /* __BSP_TIM1_H */
