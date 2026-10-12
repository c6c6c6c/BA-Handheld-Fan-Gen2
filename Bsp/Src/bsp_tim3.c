/**
 ***********************************************************************************************************************
 * @file    bsp_tim3.c
 * @brief   TIM3 定周期 ADC 触发实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-12
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bsp_tim3.h"

/* Private variables -------------------------------------------------------------------------------------------------*/
static uint8_t s_tim3_initialized = 0U;

/*******************************************************************************
//  Function   : bsp_tim3_GetClockHz
//  Description: 获取 TIM3 实际输入时钟
//  Param      : None
//  Return     : 定时器输入时钟，单位 Hz
//  Others     : APB 不分频时使用 PCLK，APB 分频时定时器时钟为 PCLK 的两倍
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
static uint32_t bsp_tim3_GetClockHz(void)
{
    uint32_t prescaler = LL_RCC_GetAPB1Prescaler();
    uint32_t clock_hz;

    SystemCoreClockUpdate();
    clock_hz = __LL_RCC_CALC_PCLK1_FREQ(SystemCoreClock, prescaler);
    if (prescaler != LL_RCC_APB1_DIV_1)
    {
        clock_hz *= 2U;
    }
    return clock_hz;
}

/*******************************************************************************
//  Function   : bsp_tim3_Init
//  Description: 初始化 TIM3 ADC 定周期触发
//  Param      : None
//  Return     : None
//  Others     : 更新事件产生 TRGO，初始化后保持停止；参数无效时进入项目错误处理
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim3_Init(void)
{
    LL_TIM_InitTypeDef TIM_Initstruct = {0};
    uint32_t clock_hz = bsp_tim3_GetClockHz();
    uint32_t divider;
    uint32_t period;
    uint64_t divisor;

    /* 停止旧触发并清除旧配置，重复初始化后也保持停止状态。 */
    bsp_tim3_Deinit();

    /* 每周期至少两个计数，PSC 和 ARR 都必须处于 16 位寄存器范围。 */
    if ((clock_hz == 0U) || (TIM3_ADC_SAMPLE_FREQUENCY_HZ > clock_hz / 2U))
    {
        APP_ErrorHandler();
        return;
    }
    divisor = (uint64_t)TIM3_ADC_SAMPLE_FREQUENCY_HZ * 65536U;
    divider = (uint32_t)(((uint64_t)clock_hz + divisor - 1U) / divisor);
    if ((divider == 0U) || (divider > 65536U))
    {
        APP_ErrorHandler();
        return;
    }
    divisor = (uint64_t)divider * TIM3_ADC_SAMPLE_FREQUENCY_HZ;
    period = (uint32_t)(((uint64_t)clock_hz + divisor / 2U) / divisor);
    if ((period < 2U) || (period > 65536U))
    {
        APP_ErrorHandler();
        return;
    }

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_TIM3);
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_TIM3);

    /*
     * 初始化会产生 UG 更新事件。
     * 暂时用 CEN 作为 TRGO 源，CEN 保持为 0，避免 UG 被发送给 ADC。
     */
    LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_ENABLE);
    /* F_TRGO = F_TIM3 / ((PSC + 1) * (ARR + 1))，整数分频可能产生频率误差。 */
    TIM_Initstruct.CounterMode = LL_TIM_COUNTERMODE_UP;
    TIM_Initstruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    TIM_Initstruct.Prescaler = divider - 1U;
    TIM_Initstruct.Autoreload = period - 1U;
    LL_TIM_Init(TIM3, &TIM_Initstruct);
    LL_TIM_EnableARRPreload(TIM3);
    LL_TIM_SetCounter(TIM3, 0U);
    LL_TIM_ClearFlag_UPDATE(TIM3);
    LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_UPDATE);
    s_tim3_initialized = 1U;
}

/*******************************************************************************
//  Function   : bsp_tim3_Start
//  Description: 启动 TIM3 ADC 定周期触发
//  Param      : None
//  Return     : None
//  Others     : ADC/DMA 就绪后调用；首个触发在完整周期结束时产生
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim3_Start(void)
{
    if ((s_tim3_initialized == 0U) || (LL_TIM_IsEnabledCounter(TIM3) != 0U))
    {
        return;
    }

    /* 装载并复位预分频计数器时暂时屏蔽 UG 到 ADC 的触发通路。 */
    LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_ENABLE);
    LL_TIM_GenerateEvent_UPDATE(TIM3);
    LL_TIM_SetCounter(TIM3, 0U);
    LL_TIM_ClearFlag_UPDATE(TIM3);
    LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_UPDATE);
    LL_TIM_EnableCounter(TIM3);
}

/*******************************************************************************
//  Function   : bsp_tim3_Stop
//  Description: 停止 TIM3 ADC 定周期触发
//  Param      : None
//  Return     : None
//  Others     : 保留配置供再次启动，不停止 ADC 当前已开始的转换
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim3_Stop(void)
{
    if (LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_TIM3) == 0U)
    {
        return;
    }
    LL_TIM_DisableCounter(TIM3);
    LL_TIM_ClearFlag_UPDATE(TIM3);
}

/*******************************************************************************
//  Function   : bsp_tim3_Deinit
//  Description: 反初始化 TIM3
//  Param      : None
//  Return     : None
//  Others     : 停止 TRGO 触发，复位外设并关闭时钟；不改变 ADC 或其他定时器配置
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim3_Deinit(void)
{
    if (LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_TIM3) != 0U)
    {
        bsp_tim3_Stop();
        LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_TIM3);
        LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_TIM3);
        LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM3);
    }
    s_tim3_initialized = 0U;
}
