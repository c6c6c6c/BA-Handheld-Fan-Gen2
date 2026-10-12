/**
 ***********************************************************************************************************************
 * @file    fan.c
 * @brief   风扇控制模块，实现风扇档位控制、PWM 输出及转速反馈检测
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-12
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "fan.h"
#include "bsp_tim1.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风扇反馈转速检测超时时间
 *
 * fan_1ms_task() 每 1ms 累加一次 timeout，
 * 当超过该值仍未检测到新的反馈脉冲时，
 * 认为风扇转速反馈超时，并将当前转速置 0。
 */
#define FAN_FEEDBACK_DETECT_TIME        (1000)

/**
 * @brief 风扇反馈捕获计数最小有效值
 *
 * 当计算得到的反馈脉冲周期小于该值时，
 * 认为当前捕获结果异常或无效。
 */
#define FAN_FEEDBACK_CAPTURE_VALUE_MIN  (200)

/**
 * @brief 将反馈脉冲周期计数值转换为风扇转速
 *
 * @param n 相邻两次反馈脉冲之间的计数值
 *
 * @note 当前计算默认每转对应一个有效反馈脉冲。
 *       如果实际风扇每转输出多个脉冲，需要进一步除以每转脉冲数。
 */
#define COUNT_TO_SPEED(n)               ((TIM1_SYS_TIMER_FRE * 60UL) / (n))


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风扇运行状态
 *
 * 保存当前风扇档位、输入捕获数据、
 * 转速计算结果及超时状态等信息。
 */
fan_t fan;


/* External variables ------------------------------------------------------------------------------------------------*/


/* Constant ----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 各风扇档位对应的 PWM 占空比参数表
 *
 * 数组下标与 fan_mode_enum_t 一一对应：
 *
 * FAN_MODE_0     -> 0
 * FAN_MODE_1     -> 120
 * FAN_MODE_2     -> 220
 * FAN_MODE_3     -> 380
 * FAN_MODE_4     -> 460
 * FAN_MODE_5     -> 600
 * FAN_MODE_BOOST -> 850
 *
 * @note 表中数值为定时器 Compare 值，
 *       实际占空比由 TIM1 ARR 配置共同决定。
 */
const uint16_t TABLE_FAN_PWM_DUTY_NORMAL[FAN_MODE_TYPE_NUM] =
{
    0, 120, 220, 380, 460, 600, 850,
};


/*******************************************************************************
//  Function   : bsp_tim1_ch4_inputCapture_Callback
//  Description: 风扇转速反馈输入捕获回调函数
//  Param      : None
//  Return     : None
//  Others     : 根据 TIM1 当前计数状态及 CH4 捕获值计算相邻反馈脉冲周期，
//               并将脉冲周期换算为风扇转速
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim1_ch4_inputCapture_Callback(void)
{
    /*
     * 获取 TIM1 当前累计计数状态及 CH4 当前捕获值。
     */
    fan.pulseVal  = bsp_tim1_Count();
    fan.nowCapVal = bsp_tim1_ch4_inputCapture_Count();

    /*
     * 根据定时器是否发生过计数周期跨越，
     * 计算本次捕获与上一次捕获之间的实际脉冲周期。
     */
    if (fan.pulseVal != 0)
    {
        fan.pulseVal -= fan.lastCapVal;
        fan.pulseVal += fan.nowCapVal;
    }
    else
    {
        /*
         * 未发生计数器回绕。
         */
        if (fan.nowCapVal > fan.lastCapVal)
        {
            fan.pulseVal = fan.nowCapVal - fan.lastCapVal;
        }
        else
        {
            /*
             * 捕获值发生回绕时，补偿一个完整计数周期。
             */
            fan.pulseVal = fan.nowCapVal + (TIM1_SYS_TIMER_FRE - fan.lastCapVal);
        }
    }

    /*
     * 保存当前捕获值，供下一次捕获计算使用。
     */
    fan.lastCapVal = fan.nowCapVal;

    /*
     * 成功捕获到反馈脉冲后清零超时计数。
     */
    fan.timeout = 0;

    /*
     * 捕获周期过小时认为数据异常。
     */
    if (fan.pulseVal < FAN_FEEDBACK_CAPTURE_VALUE_MIN)
    {
        fan.speed = UINT32_MAX;
    }
    else
    {
        /*
         * 根据反馈脉冲周期计算风扇转速。
         */
        fan.speed = COUNT_TO_SPEED(fan.pulseVal);
    }
}


/*******************************************************************************
//  Function   : fan_Init
//  Description: 初始化风扇控制模块
//  Param      : None
//  Return     : None
//  Others     : 在 GPIO 和 TIM1 初始化后调用，默认停机并清零当前转速
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_Init(void)
{
    fan.mode  = FAN_MODE_0;
    fan.speed = 0;
    bsp_tim1_ch1_pwm_setduty(0);
    FAN_CTRL_CLOSE();
}


/*******************************************************************************
//  Function   : fan_1ms_task
//  Description: 风扇 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 完成转速反馈超时检测、风扇档位有效性检查及 PWM 占空比更新
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_1ms_task(void)
{
    /*
     * 风扇转速反馈超时检测。
     *
     * 如果规定时间内没有新的输入捕获反馈，
     * 则认为风扇当前无有效转速反馈，并将 speed 置 0。
     */
    if (fan.timeout < FAN_FEEDBACK_DETECT_TIME)
    {
        fan.timeout++;
    }
    else
    {
        fan.speed = 0;
    }

    /*
     * 风扇模式合法性检查。
     *
     * 如果模式超出有效范围，
     * 强制恢复为 FAN_MODE_0。
     */
    if (fan.mode >= FAN_MODE_TYPE_NUM)
    {
        fan.mode = FAN_MODE_0;
    }

    /*
     * 停机时先清零 PWM，再关闭风机主接地通路；
     * 运行时先接通通路，再根据当前档位更新 PWM 占空比。
     */
    if (fan.mode == FAN_MODE_0)
    {
        bsp_tim1_ch1_pwm_setduty(0);
        FAN_CTRL_CLOSE();
    }
    else
    {
        FAN_CTRL_OPEN();
        bsp_tim1_ch1_pwm_setduty(TABLE_FAN_PWM_DUTY_NORMAL[fan.mode]);
    }
}


/*******************************************************************************
//  Function   : fan_set_speedmode
//  Description: 设置风扇运行档位
//  Param      : mode - 目标风扇运行档位
//  Return     : None
//  Others     : 档位合法性由 fan_1ms_task() 进行检查
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_set_speedmode(uint8_t mode)
{
    fan.mode = (fan_mode_enum_t)mode;
}


/*******************************************************************************
//  Function   : fan_get_speedmode
//  Description: 获取当前风扇运行档位
//  Param      : None
//  Return     : 当前风扇运行模式
//  Others     : 返回值类型为 fan_mode_enum_t
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
fan_mode_enum_t fan_get_speedmode(void)
{
    return fan.mode;
}


/*******************************************************************************
//  Function   : fan_get_speed
//  Description: 获取当前风扇反馈转速
//  Param      : None
//  Return     : 当前风扇转速
//  Others     : 转速由输入捕获反馈周期计算得到；反馈超时时转速被置为 0
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
uint32_t fan_get_speed(void)
{
    return fan.speed;
}


/*******************************************************************************
//  Function   : fan_set_timeoutFlg
//  Description: 设置风扇超时状态标志
//  Param      : None
//  Return     : None
//  Others     : 仅设置 timeoutFlg 标志，具体处理逻辑由上层模块或后续控制逻辑决定
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_set_timeoutFlg(void)
{
    fan.timeoutFlg = true;
}


/*******************************************************************************
//  Function   : fan_clean_timeoutFlg
//  Description: 清除风扇超时状态标志
//  Param      : None
//  Return     : None
//  Others     : 仅清除 timeoutFlg 标志，不直接修改风扇 PWM 或当前运行档位
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_clean_timeoutFlg(void)
{
    fan.timeoutFlg = false;
}
