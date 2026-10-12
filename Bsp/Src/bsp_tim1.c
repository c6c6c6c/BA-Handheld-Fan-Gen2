/**
 ***********************************************************************************************************************
 * @file    bsp_tim1.c
 * @brief   TIM1 输入捕获、PWM 输出及定时器中断底层驱动
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bsp_tim1.h"

__IO uint32_t tim1_count = 0; // 定时器中断计数值

/*******************************************************************************
//  Function   : bsp_tim1_ch4_inputCapture_Callback
//  Description: TIM1 CH4 输入捕获回调函数
//  Param      : None
//  Return     : None
//  Others     : 弱定义函数，可由应用层重写；回调中应及时读取捕获值
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
__weak void bsp_tim1_ch4_inputCapture_Callback(void){}


/*******************************************************************************
//  Function   : TIM1_InputCapture_Configuration
//  Description: 配置 TIM1 输入捕获通道
//  Param      : None
//  Return     : None
//  Others     : 配置 TIM1 CH4 为直接输入捕获模式，检测输入信号上升沿，
//               捕获事件发生时触发 TIM1_CC 中断
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void TIM1_InputCapture_Configuration(void)
{
    LL_GPIO_InitTypeDef TIM1ChannelInit = {0};

    TIM1_CH4_INPUT_CAPTURE_GPIO_CLOCK_EN();
    /*
     * TIM1 CH4 输入捕获配置。
     *
     * DIRECTTI：
     * CH4 直接连接至 TI4 输入信号。
     *
     * RISING：
     * 检测输入信号上升沿并产生捕获事件。
     */
    LL_TIM_IC_SetActiveInput(TIM1, TIM1_CH4_INPUT_CAPTURE_CHANNEL, LL_TIM_ACTIVEINPUT_DIRECTTI);
    LL_TIM_IC_SetPolarity(TIM1, TIM1_CH4_INPUT_CAPTURE_CHANNEL, LL_TIM_IC_POLARITY_RISING);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 配置 TIM1 Capture/Compare 中断 & 使能 TIM1 CH 捕获比较中断                                                      */
    /* -------------------------------------------------------------------------------------------------------------- */
    NVIC_EnableIRQ(TIM1_CC_IRQn);      
    NVIC_SetPriority(TIM1_CC_IRQn, 0);
    TIM1_CH4_INPUT_CAPTURE_EnableIT_CC(TIM1);      


    /*
     * 配置 PA11 为 TIM1 CH4 复用功能。
     */
    TIM1ChannelInit.Pin        = TIM1_CH4_INPUT_CAPTURE_PIN;
    TIM1ChannelInit.Pull       = LL_GPIO_PULL_DOWN;
    TIM1ChannelInit.Mode       = LL_GPIO_MODE_ALTERNATE;
    TIM1ChannelInit.Alternate  = TIM1_CH4_INPUT_CAPTURE_AF;
    TIM1ChannelInit.Speed      = LL_GPIO_SPEED_FREQ_HIGH;
    TIM1ChannelInit.OutputType = LL_GPIO_OUTPUT_PUSHPULL;

    LL_GPIO_Init(TIM1_CH4_INPUT_CAPTURE_PORT, &TIM1ChannelInit);

    LL_TIM_CC_EnableChannel(TIM1, TIM1_CH4_INPUT_CAPTURE_CHANNEL); /* 使能 TIM1 CH 通道。 */
}


/*******************************************************************************
//  Function   : TIM1_PWMChannel_Configuration
//  Description: 配置 TIM1 PWM 输出通道
//  Param      : None
//  Return     : None
//  Others     : 配置 TIM1 CH1 为 PWM2 输出模式，初始比较值为 0，
//               经 Q11 反相后 M_PWM 初始高电平占空比为 0%
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void TIM1_PWMChannel_Configuration(void)
{
    LL_GPIO_InitTypeDef TIM1CH1MapInit = {0};
    LL_TIM_OC_InitTypeDef TIM_OC_Initstruct = {0};

    TIM1_CH1_PWM_GPIO_CLOCK_EN();
    /* 先将 MCU PWM 引脚保持为高电平，Q11 导通使 M_PWM 保持低电平。 */
    LL_GPIO_SetOutputPin(TIM1_CH1_PWM_PORT, TIM1_CH1_PWM_PIN);
    LL_GPIO_SetPinOutputType(TIM1_CH1_PWM_PORT, TIM1_CH1_PWM_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinMode(TIM1_CH1_PWM_PORT, TIM1_CH1_PWM_PIN, LL_GPIO_MODE_OUTPUT);
    /*
     * 配置 PA3 为 TIM1 CH1 PWM 输出复用功能。
     */
    TIM1CH1MapInit.Pin        = TIM1_CH1_PWM_PIN;
    TIM1CH1MapInit.Mode       = LL_GPIO_MODE_ALTERNATE;
    TIM1CH1MapInit.Alternate  = TIM1_CH1_PWM_AF;
    TIM1CH1MapInit.Speed      = LL_GPIO_SPEED_FREQ_HIGH;
    TIM1CH1MapInit.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
    TIM1CH1MapInit.Pull       = LL_GPIO_PULL_NO;

    /*
     * TIM1 CH1 PWM 输出参数配置：
     *
     * OCMode       : PWM2 模式
     * OCState      : 主通道输出使能
     * OCNState     : 互补通道关闭
     * OCPolarity   : 主通道高电平有效
     * OCNPolarity  : 互补通道高电平有效
     * OCIdleState  : 空闲状态输出低电平
     * CompareValue : 初始比较值为 0
     */
    TIM_OC_Initstruct.OCMode        = LL_TIM_OCMODE_PWM2;
    TIM_OC_Initstruct.OCState       = LL_TIM_OCSTATE_ENABLE;
    TIM_OC_Initstruct.OCNState      = LL_TIM_OCSTATE_DISABLE;
    TIM_OC_Initstruct.OCPolarity    = LL_TIM_OCPOLARITY_HIGH;
    TIM_OC_Initstruct.OCNPolarity   = LL_TIM_OCPOLARITY_HIGH;
    TIM_OC_Initstruct.OCIdleState   = LL_TIM_OCIDLESTATE_LOW;
    TIM_OC_Initstruct.OCNIdleState  = LL_TIM_OCIDLESTATE_LOW;
    TIM_OC_Initstruct.CompareValue  = 0;

    LL_TIM_OC_Init(TIM1, TIM1_CH1_PWM_CHANNEL, &TIM_OC_Initstruct);
    /* PWM2 经 Q11 反相后，M_PWM 高电平占空比为 CCR1 / (ARR + 1)。 */
    LL_TIM_OC_EnablePreload(TIM1, TIM1_CH1_PWM_CHANNEL);
    //LL_TIM_GenerateEvent_UPDATE(TIM1);
    //LL_TIM_EnableAllOutputs(TIM1);

    /* 输出逻辑准备完成后再将引脚交给 TIM1。 */
    LL_GPIO_Init(TIM1_CH1_PWM_PORT, &TIM1CH1MapInit);
}


/*******************************************************************************
//  Function   : bsp_tim1_Init
//  Description: 初始化 TIM1
//  Param      : None
//  Return     : None
//  Others     : 完成 TIM1 基础计数器、输入捕获、PWM输出及更新中断配置
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_tim1_Init(void)
{
    LL_TIM_InitTypeDef TIM1CountInit = {0};

    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_TIM1);
    LL_RCC_SetTIMClockSource(LL_RCC_TIM1_CLKSOURCE_PCLK1);
    NVIC_DisableIRQ(TIM1_CC_IRQn);
    NVIC_DisableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    LL_TIM_DisableCounter(TIM1);
    LL_TIM_DisableAllOutputs(TIM1);
    LL_APB1_GRP2_ForceReset(LL_APB1_GRP2_PERIPH_TIM1);
    LL_APB1_GRP2_ReleaseReset(LL_APB1_GRP2_PERIPH_TIM1);
    tim1_count = 0U;
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
    NVIC_ClearPendingIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    /*
     * TIM1 基础计数器配置。
     *
     * ClockDivision     : 不分频
     * CounterMode       : 向上计数
     * Prescaler         : 72 - 1，72 MHz 时钟下计数步长为 1 us
     * AutoReload        : 1000 - 1，PWM 和更新频率为 1 kHz
     * RepetitionCounter : 0
     */
    TIM1CountInit.ClockDivision      = LL_TIM_CLOCKDIVISION_DIV1;
    TIM1CountInit.CounterMode        = LL_TIM_COUNTERMODE_UP;
    TIM1CountInit.Prescaler          = TIM1_SYS_TIMER_PSC - 1;
    TIM1CountInit.Autoreload         = TIM1_SYS_TIMER_AUTORELOAD - 1;
    TIM1CountInit.RepetitionCounter  = 0;
    
    LL_TIM_Init(TIM1, &TIM1CountInit); /* 初始化 TIM1 基础计数器。 */

    /* -------------------------------------------------------------------------------------------------------------- */
    /* TIM1_CH4 -> 配置 TIM1 输入捕获功能。                                                                            */
    /* TIM1_CH1 -> 配置 TIM1 PWM 输出功能。                                                                            */
    /* -------------------------------------------------------------------------------------------------------------- */
    TIM1_InputCapture_Configuration();
    TIM1_PWMChannel_Configuration();

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 配置并使能 TIM1 Break / Update / Trigger / COM 共用中断 & 配置中断优先级                                          */
    /* -------------------------------------------------------------------------------------------------------------- */
    LL_TIM_ClearFlag_UPDATE(TIM1);  /* 清除 TIM1 Update 标志，防止初始化完成后立即进入更新中断。 */
    LL_TIM_EnableIT_UPDATE(TIM1);   /* 使能 TIM1 Update 中断。 */

    NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    NVIC_SetPriority(TIM1_BRK_UP_TRG_COM_IRQn, 1);  /* 与捕获中断同优先级，互不抢占。 */

    LL_TIM_EnableAllOutputs(TIM1);  /*  TIM1 开启主输出使能。*/
    LL_TIM_EnableCounter(TIM1);     /*  启动 TIM1 计数器。 */
}


/*******************************************************************************
//  Function   : bsp_tim1_Deinit
//  Description: 反初始化 TIM1
//  Param      : None
//  Return     : None
//  Others     : 关闭并复位 TIM1，释放中断资源；MCU PWM 引脚输出高电平，
//               经 Q11 反相后 M_PWM 保持低电平，捕获引脚保持上拉输入
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_tim1_Deinit(void)
{
    NVIC_DisableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);     //关闭NVIC定时器中断
    NVIC_DisableIRQ(TIM1_CC_IRQn);                 //关闭NVIC输入捕获中断

    if (LL_APB1_GRP2_IsEnabledClock(LL_APB1_GRP2_PERIPH_TIM1) != 0U)
    {
        LL_TIM_DisableAllOutputs(TIM1);             /* 关闭定时器输出。 */

        LL_TIM_DisableIT_UPDATE(TIM1);              /* 关闭更新中断。 */
        TIM1_CH4_INPUT_CAPTURE_DisableIT_CC(TIM1);  /* 关闭输入捕获中断。 */

        LL_TIM_DisableCounter(TIM1);
        LL_APB1_GRP2_ForceReset(LL_APB1_GRP2_PERIPH_TIM1);
        LL_APB1_GRP2_ReleaseReset(LL_APB1_GRP2_PERIPH_TIM1);
        LL_APB1_GRP2_DisableClock(LL_APB1_GRP2_PERIPH_TIM1);
    }
    NVIC_ClearPendingIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
    NVIC_ClearPendingIRQ(TIM1_CC_IRQn);
    tim1_count = 0U;

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 引脚 Low Power 配置                                                                                             */
    /* PWM     CH1 -> MCU 高电平，经 Q11 反相后 M_PWM 为低电平                                                           */
    /* 输入捕获 CH4 -> 上拉输入                                                                                        */
    /* -------------------------------------------------------------------------------------------------------------- */
    TIM1_CH1_PWM_GPIO_CLOCK_EN();
    TIM1_CH4_INPUT_CAPTURE_GPIO_CLOCK_EN();
    
    /* MCU PWM 输出高电平，使 M_PWM 保持 0% 高电平占空比。 */
    LL_GPIO_SetOutputPin(TIM1_CH1_PWM_PORT, TIM1_CH1_PWM_PIN);
    LL_GPIO_SetPinOutputType(TIM1_CH1_PWM_PORT, TIM1_CH1_PWM_PIN, LL_GPIO_OUTPUT_PUSHPULL);
    LL_GPIO_SetPinMode(TIM1_CH1_PWM_PORT, TIM1_CH1_PWM_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinPull(TIM1_CH1_PWM_PORT, TIM1_CH1_PWM_PIN, LL_GPIO_PULL_NO);
    
    //输入捕获引脚设置为上拉输入
    LL_GPIO_SetPinMode(TIM1_CH4_INPUT_CAPTURE_PORT, TIM1_CH4_INPUT_CAPTURE_PIN, LL_GPIO_MODE_INPUT);
    LL_GPIO_SetPinPull(TIM1_CH4_INPUT_CAPTURE_PORT, TIM1_CH4_INPUT_CAPTURE_PIN, LL_GPIO_PULL_UP);
}


/*******************************************************************************
//  Function   : bsp_tim1_ch1_pwm_setduty
//  Description: 设置经 Q11 反相后的 M_PWM 高电平占空比
//  Param      : duty - 千分比，0~1000 对应 0%~100%，超范围限制为 1000
//  Return     : None
//  Others     : TIM1 初始化后调用；预装载使新占空比在下次更新事件生效
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim1_ch1_pwm_setduty(uint16_t duty)
{
    if (LL_APB1_GRP2_IsEnabledClock(LL_APB1_GRP2_PERIPH_TIM1) == 0U)
    {
        return;
    }

    if (duty > 1000U)
    {
        duty = 1000U;
    }

    TIM1_CH1_PWM_OutputSet(((uint32_t)duty * TIM1_SYS_TIMER_AUTORELOAD) / 1000U);
}

/*******************************************************************************
//  Function   : bsp_inputCapture_Count
//  Description: 获取输入捕获计数值
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
uint32_t bsp_tim1_ch4_inputCapture_Count(void)
{
    return TIM1_CH4_INPUT_CAPTURE_GetCaptureCH(TIM1);
}


/*******************************************************************************
//  Function   : bsp_timer_Count
//  Description: 获取定时器计数值
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
uint32_t bsp_tim1_Count(void)
{
    uint32_t count  = tim1_count * TIM1_SYS_TIMER_AUTORELOAD;  //由于定时器的中断优先级比输入捕获中断的优先级低，暂不关闭中断
    tim1_count = 0;
    return count;
}

/*******************************************************************************
//  Function   : TIM1_CC_IRQHandler
//  Description: TIM1 捕获比较中断服务函数
//  Param      : None
//  Return     : None
//  Others     : 当前处理 TIM1 CH4 捕获比较中断，调用输入捕获回调后清除 CC4 标志
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void TIM1_CC_IRQHandler(void)
{
    if (TIM1_CH4_INPUT_CAPTURE_IsActiveFlag_CC(TIM1) && TIM1_CH4_INPUT_CAPTURE_IsEnabledIT_CC(TIM1))
    {
        bsp_tim1_ch4_inputCapture_Callback();
        TIM1_CH4_INPUT_CAPTURE_ClearFlag_CC(TIM1); 
    }
}

/******************************************************************************
//  Function   : TIM1_BRK_UP_TRG_COM_IRQHandler
//  Description: TIM1 Break / Update / Trigger / COM 共用中断服务函数
//  Param      : None
//  Return     : None
//  Others     : 当前处理 TIM1 CH1 捕获比较中断，清除 CC1 标志后调用应用层回调
//  Author     : Chen Liang 2026-08-18
******************************************************************************/
void TIM1_BRK_UP_TRG_COM_IRQHandler(void)
{
    if(LL_TIM_IsActiveFlag_UPDATE(TIM1) && LL_TIM_IsEnabledIT_UPDATE(TIM1))
    {
        tim1_count++;
        LL_TIM_ClearFlag_UPDATE(TIM1);
    }
}
