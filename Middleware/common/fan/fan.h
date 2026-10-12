/**
 ***********************************************************************************************************************
 * @file    fan.h
 * @brief   风扇控制模块接口及运行状态定义
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

/*
 * 电池电压获取接口占位。
 * 如后续需要根据电池电压限制风扇档位，可在此处接入实际电池电压获取函数。
 */
// #define GetBatteryVoltage()       3550

/*
 * 关闭风扇：FAN_CTRL 输出低电平，断开风机主接地通路。
 * 调用前需完成板级 GPIO 初始化；停机时先将 PWM 占空比置 0。
 */
#define FAN_CTRL_CLOSE()            LL_GPIO_ResetOutputPin(FAN_CTRL_PORT, FAN_CTRL_PIN)


/*
 * 开启风扇：FAN_CTRL 输出高电平，接通风机主接地通路。
 * 调用前需完成板级 GPIO 初始化；启动时先接通通路，再设置 PWM 占空比。
 */
#define FAN_CTRL_OPEN()             LL_GPIO_SetOutputPin(FAN_CTRL_PORT, FAN_CTRL_PIN)

/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风扇运行模式定义
 *
 * FAN_MODE_0 ~ FAN_MODE_5：
 * 常规风速档位。
 *
 * FAN_MODE_BOOST：
 * 强力/增压模式。
 *
 * FAN_MODE_TYPE_NUM：
 * 风扇模式总数，用于边界判断或数组索引。
 */
typedef enum
{
    FAN_MODE_0 = 0,          /**< 0 档 / 风扇停止 */
    FAN_MODE_1,              /**< 1 档 */
    FAN_MODE_2,              /**< 2 档 */
    FAN_MODE_3,              /**< 3 档 */
    FAN_MODE_4,              /**< 4 档 */
    FAN_MODE_5,              /**< 5 档 */
    FAN_MODE_BOOST,          /**< 强力模式 */
    FAN_MODE_TYPE_NUM,       /**< 风扇模式总数 */
} fan_mode_enum_t;


/**
 * @brief 风扇运行状态结构体
 *
 * 保存风扇当前档位、转速测量相关捕获值、
 * 超时状态以及实际转速等运行信息。
 */
typedef struct
{
    fan_mode_enum_t mode;       /**< 当前风扇运行模式 */

    uint32_t nowCapVal;         /**< 当前一次输入捕获计数值 */
    uint32_t lastCapVal;        /**< 上一次输入捕获计数值 */
    uint32_t pulseVal;          /**< 相邻两次捕获之间的计数差值 */

    uint32_t timeout;           /**< 转速检测超时计数值 */
    uint32_t speed;             /**< 当前计算得到的风扇转速 */

    bool timeoutFlg;            /**< 风扇转速检测超时标志 */
} fan_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_Init
//  Description: 初始化风扇控制模块
//  Param      : None
//  Return     : None
//  Others     : 在 GPIO 和 TIM1 初始化后调用，设置默认停机档位、清零转速并关闭输出
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_Init(void);


/*******************************************************************************
//  Function   : fan_1ms_task
//  Description: 风扇 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 用于风扇状态更新、超时检测及周期性控制逻辑
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_1ms_task(void);


/*******************************************************************************
//  Function   : fan_set_speedmode
//  Description: 设置风扇运行档位
//  Param      : mode - 风扇目标运行模式
//  Return     : None
//  Others     : mode 应对应 fan_mode_enum_t 中定义的有效模式
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_set_speedmode(uint8_t mode);


/*******************************************************************************
//  Function   : fan_get_speedmode
//  Description: 获取当前风扇运行档位
//  Param      : None
//  Return     : 当前风扇运行模式
//  Others     : 返回值类型为 fan_mode_enum_t
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
fan_mode_enum_t fan_get_speedmode(void);


/*******************************************************************************
//  Function   : fan_get_speed
//  Description: 获取当前风扇转速
//  Param      : None
//  Return     : 当前风扇转速
//  Others     : 转速单位由 fan.c 中实际计算方式决定
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
uint32_t fan_get_speed(void);


/*******************************************************************************
//  Function   : fan_set_timeoutFlg
//  Description: 设置风扇转速检测超时标志
//  Param      : None
//  Return     : None
//  Others     : 用于标记风扇在规定时间内未检测到有效转速脉冲
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_set_timeoutFlg(void);


/*******************************************************************************
//  Function   : fan_clean_timeoutFlg
//  Description: 清除风扇转速检测超时标志
//  Param      : None
//  Return     : None
//  Others     : 在重新检测到有效转速脉冲后可调用
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_clean_timeoutFlg(void);


#endif /* _FAN_H_ */

