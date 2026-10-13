/**
 ***********************************************************************************************************************
 * @file    bsp_tim3.c
 * @brief   TIM3 定周期触发 ADC 实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-12
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bsp_tim3.h"


/* Private variables -------------------------------------------------------------------------------------------------*/

/** TIM3 初始化状态 */
static uint8_t s_tim3_initialized = 0U;


/*******************************************************************************
//  Function   : bsp_tim3_GetClockHz
//  Description: 获取 TIM3 实际输入时钟
//  Param      : None
//  Return     : TIM3 输入时钟，单位 Hz
//  Others     : APB1 不分频时 TIM3=PCLK1，APB1 分频时 TIM3=2*PCLK1
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
//  Others     : TIM3 Update 作为 ADC TRGO；初始化完成后保持停止状态
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim3_Init(void)
{
    LL_TIM_InitTypeDef TIM_Initstruct = {0};
    uint32_t clock_hz = bsp_tim3_GetClockHz();
    uint32_t divider;
    uint32_t period;
    uint64_t divisor;

    /* 清除旧配置，保证重复初始化后仍处于停止状态。 */
    bsp_tim3_Deinit();

    /* 每周期至少 2 个计数，PSC/ARR 均限制在 16 位范围。 */
    if ((clock_hz == 0U) || (TIM3_ADC_SAMPLE_FREQUENCY_HZ > clock_hz / 2U))
    {
        APP_ErrorHandler();
        return;
    }

    /* 计算满足 16 位 PSC 范围的最小预分频系数。 */
    divisor = (uint64_t)TIM3_ADC_SAMPLE_FREQUENCY_HZ * 65536U;
    divider = (uint32_t)(((uint64_t)clock_hz + divisor - 1U) / divisor);

    if ((divider == 0U) || (divider > 65536U))
    {
        APP_ErrorHandler();
        return;
    }

    /* 根据预分频系数计算 ARR 周期值，并进行四舍五入。 */
    divisor = (uint64_t)divider * TIM3_ADC_SAMPLE_FREQUENCY_HZ;
    period  = (uint32_t)(((uint64_t)clock_hz + divisor / 2U) / divisor);

    if ((period < 2U) || (period > 65536U))
    {
        APP_ErrorHandler();
        return;
    }

    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);
    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_TIM3);
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_TIM3);

    /*
     * LL_TIM_Init() 过程中可能产生 UG，
     * 初始化阶段先使用 CEN 作为 TRGO，避免误触发 ADC。
     */
    LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_ENABLE);

    /* F_TRGO = F_TIM3 / ((PSC + 1) * (ARR + 1)) */
    TIM_Initstruct.CounterMode    = LL_TIM_COUNTERMODE_UP;
    TIM_Initstruct.ClockDivision  = LL_TIM_CLOCKDIVISION_DIV1;
    TIM_Initstruct.Prescaler      = divider - 1U;
    TIM_Initstruct.Autoreload     = period - 1U;

    LL_TIM_Init(TIM3, &TIM_Initstruct);

    LL_TIM_EnableARRPreload(TIM3);
    LL_TIM_SetCounter(TIM3, 0U);
    LL_TIM_ClearFlag_UPDATE(TIM3);

    /* 使用 Update 事件作为 ADC 外部触发源。 */
    LL_TIM_SetTriggerOutput(TIM3, LL_TIM_TRGO_UPDATE);

    s_tim3_initialized = 1U;
}


/*******************************************************************************
//  Function   : bsp_tim3_Start
//  Description: 启动 TIM3 ADC 定周期触发
//  Param      : None
//  Return     : None
//  Others     : 应在 ADC/DMA 准备完成后调用，首个 ADC 触发在一个完整周期后产生
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim3_Start(void)
{
    if ((s_tim3_initialized == 0U) || (LL_TIM_IsEnabledCounter(TIM3) != 0U))
    {
        return;
    }

    /*
     * 生成 UG 装载 PSC/ARR 时暂时屏蔽 Update TRGO，
     * 防止启动瞬间误触发一次 ADC。
     */
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
//  Others     : 仅停止 TIM3 计数，保留定时器配置供后续重新启动
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
//  Others     : 停止 TIM3、复位外设并关闭时钟，不修改 ADC 配置
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
