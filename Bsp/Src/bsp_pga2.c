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
#include "bsp_tim1.h"

/*******************************************************************************
//  Function   : bsp_tim1_GetClockHz
//  Description: Get TIM1 clock frequency
//  Param      : None
//  Return     : TIM1 clock frequency, unit: Hz
//  Others     : None
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static uint32_t bsp_tim1_GetClockHz(void)
{
    SystemCoreClockUpdate();
    return SystemCoreClock;
}

/*******************************************************************************
//  Function   : bsp_tim1_Init
//  Description: Initialize TIM1
//  Param      : None
//  Return     : None
//  Others     : TIM1 update event outputs TRGO at 2kHz for ADC trigger
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_tim1_Init(void)
{
    LL_TIM_InitTypeDef TIM_InitStruct = {0};
    uint32_t TimerClockHz;
    uint32_t Prescaler;
    uint32_t AutoReload;

    BSP_TIM1_CLOCK_EN();

    TimerClockHz = bsp_tim1_GetClockHz();
    Prescaler = (TimerClockHz / 1000000U);
    if (Prescaler > 0U)
    {
        Prescaler--;
    }

    AutoReload = (1000000U / BSP_TIM1_SAMPLE_FREQUENCY_HZ);
    if (AutoReload > 0U)
    {
        AutoReload--;
    }

    TIM_InitStruct.Prescaler         = Prescaler;
    TIM_InitStruct.CounterMode       = LL_TIM_COUNTERMODE_UP;
    TIM_InitStruct.Autoreload        = AutoReload;
    TIM_InitStruct.ClockDivision     = LL_TIM_CLOCKDIVISION_DIV1;
    TIM_InitStruct.RepetitionCounter = 0U;
    LL_TIM_Init(BSP_TIM1, &TIM_InitStruct);

    LL_TIM_SetTriggerOutput(BSP_TIM1, LL_TIM_TRGO_UPDATE);
    LL_TIM_ClearFlag_UPDATE(BSP_TIM1);
}

/*******************************************************************************
//  Function   : bsp_tim1_Deinit
//  Description: Deinitialize TIM1
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_tim1_Deinit(void)
{
    bsp_tim1_Stop();
    BSP_TIM1_CLOCK_DIS();
}

/*******************************************************************************
//  Function   : bsp_tim1_Start
//  Description: Start TIM1 counter
//  Param      : None
//  Return     : None
//  Others     : Start 2kHz TRGO output
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_tim1_Start(void)
{
    LL_TIM_SetCounter(BSP_TIM1, 0U);
    LL_TIM_ClearFlag_UPDATE(BSP_TIM1);
    LL_TIM_EnableCounter(BSP_TIM1);
}

/*******************************************************************************
//  Function   : bsp_tim1_Stop
//  Description: Stop TIM1 counter
//  Param      : None
//  Return     : None
//  Others     : Stop TRGO output
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_tim1_Stop(void)
{
    LL_TIM_DisableCounter(BSP_TIM1);
    LL_TIM_ClearFlag_UPDATE(BSP_TIM1);
}
