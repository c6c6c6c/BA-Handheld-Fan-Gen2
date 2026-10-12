/**
 ***********************************************************************************************************************
 * @file    bsp_tim1.h
 * @brief   TIM1定时器底层驱动接口定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef __BSP_TIM1_H
#define __BSP_TIM1_H

#include "main.h"
#include "py32f032_ll_tim.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/
#define TIM1_SYS_TIMER_AUTORELOAD                             1000       // 定时器自动重装载值
#define TIM1_SYS_TIMER_PSC                                    72         // 定时器预分频值 - 定时器时钟为72MHz，计数频率为1MHz，计数周期为1us
#define TIM1_SYS_TIMER_FRE                                    1000000UL  // 定时器计数频率为1MHz，计数周期为1us
/**
  * @brief  PWM GPIO Define
  */
#define TIM1_CH1_PWM_PORT                                    GPIOA
#define TIM1_CH1_PWM_PIN                                     LL_GPIO_PIN_3
#define TIM1_CH1_PWM_AF                                      LL_GPIO_AF13_TIM1
#define TIM1_CH1_PWM_GPIO_CLOCK_EN()                         LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA)
																							   
#define TIM1_CH1_PWM_CHANNEL                                 LL_TIM_CHANNEL_CH1
#define TIM1_CH1_PWM_OutputSet(CompareValue)                 LL_TIM_OC_SetCompareCH1(TIM1,CompareValue)

/**
  * @brief  Input Capture GPIO Define
  */
#define TIM1_CH4_INPUT_CAPTURE_PORT                          GPIOA
#define TIM1_CH4_INPUT_CAPTURE_PIN                           LL_GPIO_PIN_11
#define TIM1_CH4_INPUT_CAPTURE_AF                            LL_GPIO_AF2_TIM1
#define TIM1_CH4_INPUT_CAPTURE_GPIO_CLOCK_EN()               LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA)

#define TIM1_CH4_INPUT_CAPTURE_CHANNEL                       LL_TIM_CHANNEL_CH4
#define TIM1_CH4_INPUT_CAPTURE_EnableIT_CC                   LL_TIM_EnableIT_CC4
#define TIM1_CH4_INPUT_CAPTURE_DisableIT_CC                  LL_TIM_DisableIT_CC4
#define TIM1_CH4_INPUT_CAPTURE_GetCaptureCH                  LL_TIM_IC_GetCaptureCH4
   
#define TIM1_CH4_INPUT_CAPTURE_IsActiveFlag_CC               LL_TIM_IsActiveFlag_CC4
#define TIM1_CH4_INPUT_CAPTURE_IsEnabledIT_CC                LL_TIM_IsEnabledIT_CC4
#define TIM1_CH4_INPUT_CAPTURE_ClearFlag_CC                  LL_TIM_ClearFlag_CC4
/* Function ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_tim1_Init
//  Description: 初始化TIM1定时器
//  Param      : 无
//  Return     : 无
//  Others     : 配置 CH1 为 1 kHz PWM，经 Q11 反相后 M_PWM 初始为低电平，
//               同时配置 CH4 上升沿输入捕获及更新中断
//  Author     : Leo Chen 2026-08-18
*******************************************************************************/
void bsp_tim1_Init(void);

/*******************************************************************************
//  Function   : bsp_tim1_Deinit
//  Description: 反初始化TIM1定时器
//  Param      : 无
//  Return     : 无
//  Others     : 停止并复位 TIM1，关闭外设时钟；MCU PWM 输出高电平，
//               经 Q11 反相后 M_PWM 保持低电平
//  Author     : Leo Chen 2026-08-18
*******************************************************************************/
void bsp_tim1_Deinit(void);

/*******************************************************************************
//  Function   : bsp_tim1_ch1_pwm_setduty
//  Description: 设置 M_PWM 高电平占空比
//  Param      : duty - 千分比，0~1000 对应 0%~100%，超范围限制为 1000
//  Return     : None
//  Others     : TIM1 初始化后调用；经 Q11 反相后的占空比在下次更新事件生效
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim1_ch1_pwm_setduty(uint16_t duty);

/*******************************************************************************
//  Function   : bsp_tim1_ch4_inputCapture_Callback
//  Description: TIM1 CH4 输入捕获回调函数
//  Param      : None
//  Return     : None
//  Others     : 弱定义函数，可由应用层重写，在中断中及时读取捕获值
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim1_ch4_inputCapture_Callback(void);

/*******************************************************************************
//  Function   : bsp_tim1_ch4_inputCapture_Count
//  Description: 读取 TIM1 CH4 捕获值
//  Param      : None
//  Return     : CCR4 捕获的计数值
//  Others     : 不包含定时器溢出累计值
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
uint32_t bsp_tim1_ch4_inputCapture_Count(void);

/*******************************************************************************
//  Function   : bsp_tim1_Count
//  Description: 读取并清零定时器溢出累计计数
//  Param      : None
//  Return     : 溢出次数乘以每周期计数值
//  Others     : 不包含当前 CNT 或 CCR4 的计数值
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
uint32_t bsp_tim1_Count(void);


#endif /* __BSP_TIM1_H */
