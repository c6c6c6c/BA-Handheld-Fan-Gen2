/**
 ***********************************************************************************************************************
 * @file    bsp_tim16.h
 * @brief   TIM16 PB6 PWM 驱动接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-12
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BSP_TIM16_H_
#define _BSP_TIM16_H_

#include "main.h"
#include "py32f032_ll_tim.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/
/* PWM 默认参数 ------------------------------------------------------------------------------------------------------*/
#define TIM16_PWM_DEFAULT_FREQUENCY_HZ                    1000U
/**
 * @brief PWM 输出引脚配置
 * GPIO : PB6
 * AF   : AF2 TIM16_CH1N
 * CH1N 与 CH1 共用 CCR1，输出使能使用 CH1N，比较逻辑配置使用 CH1。
 * PB6 与 BMS_VIN_EN 共用，应用层应统一引脚用途。
 */
#define TIM16_CH1_PWM_PORT                                GPIOB
#define TIM16_CH1_PWM_PIN                                 LL_GPIO_PIN_6
#define TIM16_CH1_PWM_AF                                  LL_GPIO_AF2_TIM16
#define TIM16_CH1_PWM_GPIO_CLOCK_EN()                     LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB)
/* 实际引脚输出通道：TIM16_CH1N。 */
#define TIM16_CH1_PWM_CHANNEL                             LL_TIM_CHANNEL_CH1N

/* Function ----------------------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Function   : bsp_tim16_Init
//  Description: 初始化 TIM16 PB6 PWM
//  Param      : None
//  Return     : None
//  Others     : PB6/AF2 使用 CH1N，默认 1 kHz、0% 高电平占空比，初始化后自动启动
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_Init(void);

/*******************************************************************************
//  Function   : bsp_tim16_Deinit
//  Description: 反初始化 TIM16 PWM
//  Param      : None
//  Return     : None
//  Others     : 复位并关闭 TIM16 时钟，PB6 保持无上下拉推挽低电平
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_Deinit(void);

/*******************************************************************************
//  Function   : bsp_tim16_Start
//  Description: 启动 TIM16 PWM
//  Param      : None
//  Return     : None
//  Others     : 初始化后调用，恢复已设置的频率和占空比
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_Start(void);

/*******************************************************************************
//  Function   : bsp_tim16_Stop
//  Description: 停止 TIM16 PWM
//  Param      : None
//  Return     : None
//  Others     : PB6 保持低电平，保留频率和占空比设置供再次启动使用
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_Stop(void);

/*******************************************************************************
//  Function   : bsp_tim16_ch1_pwm_set
//  Description: 同时设置 TIM16 PWM 频率和占空比
//  Param      : freq - 目标频率，单位 Hz
//               duty - 千分比，0~1000 对应 0%~100%
//  Return     : SUCCESS - 成功；ERROR - 未初始化或频率超范围
//  Others     : 运行期间不关闭 PWM，预装载参数在下一个更新事件同步生效
//  Author     : Chen Liang 2026-10-19
*******************************************************************************/
ErrorStatus bsp_tim16_ch1_pwm_set(uint32_t freq, uint16_t duty);

/*******************************************************************************
//  Function   : bsp_tim16_ch1_pwm_setfrequency
//  Description: 设置 TIM16 PWM 频率
//  Param      : frequency_hz - 目标频率，单位 Hz
//  Return     : SUCCESS - 成功；ERROR - 未初始化或频率超范围
//  Others     : 范围 1 Hz 至 TIM16 时钟的一半；保留占空比，运行期间不关闭 PWM
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
ErrorStatus bsp_tim16_ch1_pwm_setfrequency(uint32_t frequency_hz);

/*******************************************************************************
//  Function   : bsp_tim16_ch1_pwm_setduty
//  Description: 设置 PB6 高电平占空比
//  Param      : duty - 千分比，0~1000 对应 0%~100%，超范围限制为 1000
//  Return     : None
//  Others     : 初始化后调用，新比较值在下次更新事件生效；频率越高占空比分辨率越低
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim16_ch1_pwm_setduty(uint16_t duty);

#endif /* _BSP_TIM16_H_ */
