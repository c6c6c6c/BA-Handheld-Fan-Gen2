/**
 ***********************************************************************************************************************
 * @file    bsp_tim1.c
 * @brief   TIM1 BSP.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */
#include "bsp_exti.h"
#include "bsp_gpio.h"

/*******************************************************************************
//  Function   : bsp_tim1_GetClockHz
//  Description: Get TIM1 clock frequency
//  Param      : None
//  Return     : TIM1 clock frequency, unit: Hz
//  Others     : None
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void bsp_exti_sleep_Init(void)
{
    LL_EXTI_InitTypeDef EXTI_InitStruct = {0};

    EXTI_InitStruct.LineCommand = ENABLE;
    EXTI_InitStruct.Mode        = LL_EXTI_MODE_EVENT;

    EXTI_InitStruct.Line        = LL_EXTI_LINE_6;
    EXTI_InitStruct.Trigger     = LL_EXTI_TRIGGER_FALLING;
    LL_EXTI_Init(&EXTI_InitStruct);

    /* Set EXTI6 to connected to PA6 pin */
    LL_EXTI_SetEXTISource(LL_EXTI_CONFIG_PORTA,LL_EXTI_CONFIG_LINE6);
}

/*******************************************************************************
//  Function   : bsp_tim1_Init
//  Description: Initialize TIM1
//  Param      : None
//  Return     : None
//  Others     : TIM1 update event outputs TRGO at 2kHz for ADC trigger
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void bsp_exti_ti_Init(void)
{
    LL_EXTI_InitTypeDef EXTI_InitStruct = {0};
}


