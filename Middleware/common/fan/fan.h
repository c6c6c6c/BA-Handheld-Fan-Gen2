
/**
 ***********************************************************************************************************************
 * @file    fan.h
 * @brief   风扇控制模块接口，定义风速档位、PWM 缓启动及转速检测状态
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-12
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _FAN_H_
#define _FAN_H_

#include "main.h"
#include "bsp_gpio.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** PWM 占空比每次调整的步进值 */
#define FAN_RAMP_STEP               2

/** PWM 占空比渐变的时间间隔，单位 ms */
#define FAN_RAMP_STEP_TIME          15

/*
 * 电池电压获取接口预留。
 * 后续可根据电池电压动态限制风扇输出功率。
 */
// #define GetBatteryVoltage()       3550


/* GPIO Control ------------------------------------------------------------------------------------------------------*/

/**
 * @brief 关闭风扇主供电通路
 * @note  FAN_CTRL 输出低电平，断开风机主接地通路；
 *        建议关闭前先将 PWM 占空比设置为 0。
 */
#define FAN_CTRL_CLOSE()            LL_GPIO_ResetOutputPin(FAN_CTRL_PORT, FAN_CTRL_PIN)

/**
 * @brief 开启风扇主供电通路
 * @note  FAN_CTRL 输出高电平，接通风机主接地通路；
 *        建议先接通供电通路，再设置 PWM 占空比。
 */
#define FAN_CTRL_OPEN()             LL_GPIO_SetOutputPin(FAN_CTRL_PORT, FAN_CTRL_PIN)


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风扇运行档位枚举
 * @note  支持关闭档位及 1~5 档风速控制。
 *        FAN_MODE_TYPE_NUM 用于档位数量统计及有效性检查。
 */
typedef enum
{
    FAN_MODE_0 = 0,             /**< 0 档，关闭风扇 */
    FAN_MODE_1,                 /**< 1 档 */
    FAN_MODE_2,                 /**< 2 档 */
    FAN_MODE_3,                 /**< 3 档 */
    FAN_MODE_4,                 /**< 4 档 */
    FAN_MODE_5,                 /**< 5 档 */
//  FAN_MODE_BOOST,             /**< 强力模式，暂未启用 */
    FAN_MODE_TYPE_NUM,          /**< 风扇档位枚举数量 */
} fan_mode_enum_t;


/**
 * @brief 风扇 PWM 渐变控制状态
 * @note  用于记录当前实际 PWM 输出及渐变过程的时间信息，
 *        配合 FAN_RAMP_STEP 和 FAN_RAMP_STEP_TIME 实现平滑加减速。
 */
typedef struct
{
	uint32_t Start_times;        /**< 当前实际 PWM 输出值 */
    uint16_t realPwm;            /**< 当前实际 PWM 输出值 */
    uint32_t times;              /**< PWM 渐变计时值 */
} fan_ramp_t;


/**
 * @brief 风扇运行状态结构体
 * @note  保存当前档位、输入捕获数据、转速检测状态及 PWM 渐变信息。
 */
typedef struct
{
    fan_mode_enum_t mode;        /**< 当前风扇运行档位 */

    uint32_t nowCapVal;          /**< 当前输入捕获计数值 */
    uint32_t lastCapVal;         /**< 上一次输入捕获计数值 */
    uint32_t pulseVal;           /**< 相邻两次捕获的计数差值 */

    uint32_t timeout;            /**< 转速反馈检测超时计数值 */
    uint32_t speed;              /**< 当前计算得到的风扇转速 */

    bool timeoutFlg;             /**< 转速检测超时标志 */

    fan_ramp_t ramp;             /**< 风扇 PWM 平滑加减速状态 */
} fan_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_Init
//  Description: 初始化风扇控制模块
//  Param      : None
//  Return     : None
//  Others     : 初始化档位、转速检测及 PWM 渐变状态；
//               应在 GPIO 和 TIM1 初始化完成后调用
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_Init(void);


/*******************************************************************************
//  Function   : fan_1ms_task
//  Description: 执行风扇 1ms 周期控制任务
//  Param      : None
//  Return     : None
//  Others     : 用于处理风扇 PWM 渐变、转速反馈超时检测
//               及周期性运行状态更新
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_1ms_task(void);


/*******************************************************************************
//  Function   : fan_set_speedmode
//  Description: 设置风扇目标运行档位
//  Param      : mode - 目标风速档位，范围 FAN_MODE_0~FAN_MODE_5
//  Return     : None
//  Others     : 设置目标档位后，由风扇周期任务完成相应的输出控制
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_set_speedmode(uint8_t mode);


/*******************************************************************************
//  Function   : fan_get_speedmode
//  Description: 获取当前风扇运行档位
//  Param      : None
//  Return     : 当前风扇运行档位，类型为 fan_mode_enum_t
//  Others     : 可用于显示模块及其他应用逻辑查询风扇状态
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
fan_mode_enum_t fan_get_speedmode(void);


/*******************************************************************************
//  Function   : fan_get_speed
//  Description: 获取当前风扇实际转速
//  Param      : None
//  Return     : 当前计算得到的风扇转速
//  Others     : 转速根据输入捕获反馈计算，具体单位由 fan.c 实现确定
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
uint32_t fan_get_speed(void);


/*******************************************************************************
//  Function   : fan_set_timeoutFlg
//  Description: 设置风扇转速反馈超时标志
//  Param      : None
//  Return     : None
//  Others     : 用于标记风扇在规定时间内未检测到有效反馈脉冲
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_set_timeoutFlg(void);


/*******************************************************************************
//  Function   : fan_clean_timeoutFlg
//  Description: 清除风扇转速反馈超时标志
//  Param      : None
//  Return     : None
//  Others     : 用于重新检测到有效反馈脉冲或重新启动检测时清除超时状态
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_clean_timeoutFlg(void);

/*******************************************************************************
//  Function   : fan_pwm_ramp_rst
//  Description: 复位风扇 PWM 平滑加减速状态
//  Param      : None
//  Return     : None
//  Others     : 清零当前实际 PWM 输出值及渐变计时值；
//               通常在风扇停机时调用
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
void fan_pwm_ramp_rst(void);

#endif /* _FAN_H_ */
