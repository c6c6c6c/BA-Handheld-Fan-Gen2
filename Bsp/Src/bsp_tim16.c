/**
 ***********************************************************************************************************************
 * @file    bsp_tim16.c
 * @brief   TIM16 PB6 PWM 底层驱动实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-12
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bsp_tim16.h"

/* Private variables -------------------------------------------------------------------------------------------------*/
/* 保存运行状态和千分比占空比，改变频率后按新的周期计数值重新计算 CCR1。 */
static uint8_t  s_tim16_initialized = 0U;
static uint8_t  s_tim16_running     = 0U;
static uint16_t s_tim16_duty        = 0U;
static uint32_t s_tim16_period      = 0U;

/*******************************************************************************
//  Function   : bsp_tim16_GetClockHz
//  Description: 获取 TIM16 输入时钟
//  Param      : None
//  Return     : 输入时钟频率，单位 Hz
//  Others     : APB 不分频时使用 PCLK，APB 分频时定时器时钟为 PCLK 的两倍
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
static uint32_t bsp_tim16_GetClockHz(void)
{
    uint32_t prescaler = LL_RCC_GetAPB1Prescaler();
    uint32_t clock_hz;

    /* 根据 RCC 寄存器更新 HCLK，再结合 APB 分频计算定时器输入时钟。 */
    SystemCoreClockUpdate();
    clock_hz = __LL_RCC_CALC_PCLK1_FREQ(SystemCoreClock, prescaler);
    if (prescaler != LL_RCC_APB1_DIV_1)
    {
        clock_hz *= 2U;
    }
    return clock_hz;
}

/*******************************************************************************
//  Function   : TIM16_PWMChannel_Configuration
//  Description: 配置 TIM16 CH1N PWM 输出
//  Param      : None
//  Return     : None
//  Others     : PWM1 配合互补输出低有效极性，使 PB6 高电平占空比为 CCR1 / (ARR + 1)
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
static void TIM16_PWMChannel_Configuration(void)
{
    LL_TIM_OC_InitTypeDef TIM_OC_Initstruct = {0};

    /* -------------------------------------------------------------------------------------------------------------- */
    /* PB6 GPIO 配置                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */
    /* 先保持无上下拉推挽低电平；这里只设置 AF2，启动时才切换到复用模式。 */
    TIM16_CH1_PWM_GPIO_CLOCK_EN();
    LL_GPIO_ResetOutputPin(TIM16_CH1_PWM_PORT, TIM16_CH1_PWM_PIN);
    LL_GPIO_SetPinOutputType(TIM16_CH1_PWM_PORT, TIM16_CH1_PWM_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinPull(TIM16_CH1_PWM_PORT, TIM16_CH1_PWM_PIN, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinMode(TIM16_CH1_PWM_PORT, TIM16_CH1_PWM_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinSpeed(TIM16_CH1_PWM_PORT, TIM16_CH1_PWM_PIN, LL_GPIO_SPEED_FREQ_HIGH);
    LL_GPIO_SetAFPin_0_7(TIM16_CH1_PWM_PORT, TIM16_CH1_PWM_PIN, TIM16_CH1_PWM_AF);

    /*
     * TIM16 CH1N PWM 输出参数配置：
     *
     * OCMode       : PWM1 模式，比较逻辑由 CH1 的 CCR1 控制
     * OCState      : 关闭主输出 CH1
     * OCNState     : 开启互补输出 CH1N，对应 PB6/AF2
     * OCPolarity   : 主输出高有效，主输出当前未使能
     * OCNPolarity  : 互补输出低有效，抵消互补反相，使 PB6 高电平比例随 CCR1 增大
     * OCIdleState  : 主输出空闲电平设为低
     * OCNIdleState : 互补输出空闲电平设为低，实际空闲输出还受 OSSI 等配置影响
     * CompareValue : 初始比较值为 0，正常 PWM 输出时 PB6 高电平占空比为 0%
     *
     * PB6 高电平占空比 = CCR1 / (ARR + 1)。
     */
    TIM_OC_Initstruct.OCMode       = LL_TIM_OCMODE_PWM2;
    TIM_OC_Initstruct.OCState      = LL_TIM_OCSTATE_DISABLE;
    TIM_OC_Initstruct.OCNState     = LL_TIM_OCSTATE_ENABLE;
    TIM_OC_Initstruct.OCPolarity   = LL_TIM_OCPOLARITY_HIGH;
    TIM_OC_Initstruct.OCNPolarity  = LL_TIM_OCPOLARITY_LOW;
    TIM_OC_Initstruct.OCIdleState  = LL_TIM_OCIDLESTATE_LOW;
    TIM_OC_Initstruct.OCNIdleState = LL_TIM_OCIDLESTATE_LOW;
    TIM_OC_Initstruct.CompareValue = 0U;
    /* CH1N 与 CH1 共用 CCR1，OC_Init 的配置通道使用 CH1。 */
    LL_TIM_OC_Init(TIM16, LL_TIM_CHANNEL_CH1, &TIM_OC_Initstruct);
    /* 开启比较值及周期预装载；运行中修改占空比在更新事件时生效。 */
    LL_TIM_OC_EnablePreload(TIM16, LL_TIM_CHANNEL_CH1);
    LL_TIM_EnableARRPreload(TIM16);
}

/*******************************************************************************
//  Function   : bsp_tim16_Init
//  Description: 初始化 TIM16 PB6 PWM
//  Param      : None
//  Return     : None
//  Others     : PB6/AF2 使用 CH1N，默认 1 kHz、0% 高电平占空比，初始化后自动启动
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_Init(void)
{
    LL_TIM_InitTypeDef TIM_Initstruct = {0};

    /* 使能外设时钟，关闭输出和计数器，再复位外设以清除旧配置。 */
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM16);
    LL_TIM_DisableAllOutputs(TIM16);
    LL_TIM_DisableCounter(TIM16);
    LL_APB1_GRP2_ForceReset(LL_APB1_GRP2_PERIPH_TIM16);
    LL_APB1_GRP2_ReleaseReset(LL_APB1_GRP2_PERIPH_TIM16);

    /*
     * TIM16 基础计数器配置：
     *
     * CounterMode       : 向上计数
     * ClockDivision     : CKD 不分频，不代替 PSC 计数预分频
     * Prescaler         : 初始为 0，稍后由频率接口计算
     * AutoReload        : 初始为 999，稍后由频率接口计算
     * RepetitionCounter : 初始为 0，每个计数周期产生更新事件
     *
     * PWM 频率 = TIM16 输入时钟 / ((PSC + 1) * (ARR + 1))。
     */
    TIM_Initstruct.CounterMode   = LL_TIM_COUNTERMODE_UP;
    TIM_Initstruct.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
    TIM_Initstruct.Autoreload    = 999U;

    LL_TIM_Init(TIM16, &TIM_Initstruct);
    TIM16_PWMChannel_Configuration();
    s_tim16_initialized = 1U;
    s_tim16_running     = 0U;
    s_tim16_duty        = 0U;
    /* 应用默认频率，设置失败时释放外设资源并进入项目错误处理。 */
    if (bsp_tim16_ch1_pwm_setfrequency(TIM16_PWM_DEFAULT_FREQUENCY_HZ) != SUCCESS)
    {
        bsp_tim16_Deinit();
        APP_ErrorHandler();
        return;
    }
    bsp_tim16_Start();
}

/*******************************************************************************
//  Function   : bsp_tim16_ch1_pwm_set
//  Description: 同时设置 TIM16 PWM 频率和占空比
//  Param      : freq - 目标频率，单位 Hz
//               duty - 千分比，0~1000 对应 0%~100%
//  Return     : SUCCESS - 成功；ERROR - 未初始化或频率超范围
//  Others     : 运行期间不关闭计数器和输出，PSC、ARR、CCR1 在下一个更新事件同步生效
//  Author     : Chen Liang 2026-10-19
*******************************************************************************/
ErrorStatus bsp_tim16_ch1_pwm_set(uint32_t freq, uint16_t duty)
{
    uint32_t clock_hz;
    uint32_t divider;
    uint32_t period;
    uint64_t divisor;

    /* 在修改寄存器前校验参数，错误参数不改变原来的 PWM 设置。 */
    if ((s_tim16_initialized == 0U) || (freq == 0U))
    {
        return ERROR;
    }
    clock_hz = bsp_tim16_GetClockHz();
    if ((clock_hz == 0U) || (freq > clock_hz / 2U))
    {
        return ERROR;
    }
    if (duty > 1000U)
    {
        duty = 1000U;
    }
    /*
     * divider = PSC + 1，period = ARR + 1。
     * 周期最多 65535 个计数，使 100% 对应的 CCR1 也能放入 16 位寄存器。
     * 向上取整选择预分频因子，使用 64 位中间值避免高频参数乘法溢出。
     */
    divisor = (uint64_t)freq * 65535U;
    divider = (uint32_t)(((uint64_t)clock_hz + divisor - 1U) / divisor);
    if ((divider == 0U) || (divider > 65536U))
    {
        return ERROR;
    }
    divisor = (uint64_t)divider * freq;
    /* 将周期计数值四舍五入；实际频率可能与目标频率存在整数分频误差。 */
    period = (uint32_t)(((uint64_t)clock_hz + divisor / 2U) / divisor);
    if (period > 65535U)
    {
        period = 65535U;
    }
    if (period < 2U)
    {
        return ERROR;
    }

    /* 暂时禁止更新事件，避免写入过程中恰逢周期结束而混用新旧参数；PWM 保持输出。 */
    LL_TIM_DisableUpdateEvent(TIM16);
    LL_TIM_SetPrescaler(TIM16, divider - 1U);
    LL_TIM_SetAutoReload(TIM16, period - 1U);
    LL_TIM_OC_SetCompareCH1(TIM16, (period * duty) / 1000U);
    s_tim16_period = period;
    s_tim16_duty   = duty;
    LL_TIM_EnableUpdateEvent(TIM16);

    if (s_tim16_running == 0U)
    {
        LL_TIM_GenerateEvent_UPDATE(TIM16);
        LL_TIM_ClearFlag_UPDATE(TIM16);
    }
    return SUCCESS;
}

/*******************************************************************************
//  Function   : bsp_tim16_ch1_pwm_setfrequency
//  Description: 设置 TIM16 PWM 频率
//  Param      : frequency_hz - 目标频率，单位 Hz
//  Return     : SUCCESS - 成功；ERROR - 未初始化或频率超范围
//  Others     : 保留当前占空比，运行期间不关闭 PWM
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
ErrorStatus bsp_tim16_ch1_pwm_setfrequency(uint32_t frequency_hz)
{
    return bsp_tim16_ch1_pwm_set(frequency_hz, s_tim16_duty);
}

/*******************************************************************************
//  Function   : bsp_tim16_ch1_pwm_setduty
//  Description: 设置 PB6 高电平占空比
//  Param      : duty - 千分比，0~1000 对应 0%~100%，超范围限制为 1000
//  Return     : None
//  Others     : 初始化后调用，新比较值在下次更新事件生效；频率越高占空比分辨率越低
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_ch1_pwm_setduty(uint16_t duty)
{
    /* 未初始化不访问外设；大于 1000 的参数按 100% 处理。 */
    if (s_tim16_initialized == 0U)
    {
        return;
    }
    if (duty > 1000U)
    {
        duty = 1000U;
    }
    s_tim16_duty = duty;
    /* 千分比换算为 CCR1；0 对应恒低，period 对应恒高，中间值按计数分辨率量化。 */
    LL_TIM_OC_SetCompareCH1(TIM16, (s_tim16_period * duty) / 1000U);
}

/*******************************************************************************
//  Function   : bsp_tim16_Start
//  Description: 启动 TIM16 PWM
//  Param      : None
//  Return     : None
//  Others     : 初始化后调用，恢复已设置的频率和占空比
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_Start(void)
{
    if ((s_tim16_initialized == 0U) || (s_tim16_running != 0U))
    {
        return;
    }
    /* 装载停止期间设置的参数，然后开启 CH1N 和全局主输出使能 MOE。 */
    LL_TIM_GenerateEvent_UPDATE(TIM16);
    LL_TIM_ClearFlag_UPDATE(TIM16);
    LL_TIM_CC_EnableChannel(TIM16, TIM16_CH1_PWM_CHANNEL);
    LL_TIM_EnableAllOutputs(TIM16);
    /* 波形配置完成后将 PB6 交给 TIM16，再启动计数器。 */
    LL_GPIO_SetPinMode(TIM16_CH1_PWM_PORT, TIM16_CH1_PWM_PIN, LL_GPIO_MODE_ALTERNATE);
    LL_TIM_EnableCounter(TIM16);
    s_tim16_running = 1U;
}

/*******************************************************************************
//  Function   : bsp_tim16_Stop
//  Description: 停止 TIM16 PWM
//  Param      : None
//  Return     : None
//  Others     : PB6 保持低电平，保留频率和占空比设置供再次启动使用
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_Stop(void)
{
    if (s_tim16_initialized == 0U)
    {
        return;
    }
    /* 先写低输出锁存值，再切换 GPIO 模式，明确保持 PB6 为低电平。 */
    LL_GPIO_ResetOutputPin(TIM16_CH1_PWM_PORT, TIM16_CH1_PWM_PIN);
    LL_GPIO_SetPinMode(TIM16_CH1_PWM_PORT, TIM16_CH1_PWM_PIN, LL_GPIO_MODE_OUTPUT);
    /* 关闭计数和输出，软件保留频率及占空比供再次启动使用。 */
    LL_TIM_DisableCounter(TIM16);
    LL_TIM_DisableAllOutputs(TIM16);
    LL_TIM_ClearFlag_UPDATE(TIM16);
    s_tim16_running = 0U;
}

/*******************************************************************************
//  Function   : bsp_tim16_Deinit
//  Description: 反初始化 TIM16 PWM
//  Param      : None
//  Return     : None
//  Others     : 复位并关闭 TIM16 时钟，PB6 保持无上下拉推挽低电平
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_Deinit(void)
{
    if (s_tim16_initialized == 0U)
    {
        return;
    }
    bsp_tim16_Stop();
    /* 恢复 TIM16 寄存器默认值并关闭外设时钟；保留共享 GPIOB 时钟。 */
    LL_APB1_GRP2_ForceReset(LL_APB1_GRP2_PERIPH_TIM16);
    LL_APB1_GRP2_ReleaseReset(LL_APB1_GRP2_PERIPH_TIM16);
    LL_APB1_GRP2_DisableClock(LL_APB1_GRP2_PERIPH_TIM16);
    s_tim16_initialized = 0U;
    s_tim16_duty        = 0U;
    s_tim16_period      = 0U;
}
