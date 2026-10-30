
/**
 ***********************************************************************************************************************
 * @file    fan.c
 * @brief   风扇控制模块，实现档位控制、PWM 平滑加减速及转速反馈检测
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

/** 风扇转速反馈超时时间，单位 ms */
#define FAN_FEEDBACK_DETECT_TIME        (1000)

/** 输入捕获脉冲周期的最小有效值，对应 200 us */
#define FAN_FEEDBACK_CAPTURE_VALUE_MIN  (TIM1_SYS_TIMER_FRE / 5000UL)

/**
 * @brief 将反馈脉冲周期转换为风扇转速
 * @note  默认每转产生一个有效反馈脉冲，结果单位为 RPM。
 *        如果每转产生多个反馈脉冲，需要按实际脉冲数进行修正。
 */
#define COUNT_TO_SPEED(n)               ((TIM1_SYS_TIMER_FRE * 60UL) / (n))


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风扇运行状态
 * @note  保存当前档位、PWM 渐变状态、输入捕获计数及反馈转速等信息。
 */
fan_t fan;


/* Constants ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风扇各运行档位对应的目标 PWM 参数表
 * @note  数组索引与 fan_mode_enum_t 对应：
 *        0 档：0
 *        1 档：300
 *        2 档：420
 *        3 档：540
 *        4 档：660
 *        5 档：780
 *
 *        表中数值用于设置 PWM 输出，实际占空比由底层接口决定。
 */
uint16_t TABLE_FAN_PWM_DUTY_NORMAL[FAN_MODE_TYPE_NUM] =
{
    0, 350, 500, 650, 800, 950, //850,
};


/* Callback functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_tim1_ch4_inputCapture_Callback
//  Description: 风扇转速反馈输入捕获回调函数
//  Param      : None
//  Return     : None
//  Others     : 根据 TIM1 计数状态及 CH4 捕获值计算反馈脉冲周期，
//               并将有效脉冲周期转换为风扇转速
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_tim1_ch4_inputCapture_Callback(void)
{
    /* 获取定时器累计计数值及当前捕获值。 */
    fan.pulseVal  = bsp_tim1_Count();
    fan.nowCapVal = bsp_tim1_ch4_inputCapture_Count();

    /* 根据定时器计数状态计算相邻反馈脉冲的周期。 */
    if (fan.pulseVal != 0U)
    {
        fan.pulseVal -= fan.lastCapVal;
        fan.pulseVal += fan.nowCapVal;
    }
    else
    {
        /* 定时器未发生回绕时直接计算捕获差值。 */
        if (fan.nowCapVal > fan.lastCapVal)
        {
            fan.pulseVal = fan.nowCapVal - fan.lastCapVal;
        }
        else
        {
            /* 捕获计数发生回绕时补偿一个计数周期。 */
            fan.pulseVal = fan.nowCapVal + (TIM1_SYS_TIMER_FRE - fan.lastCapVal);
        }
    }

    /* 保存当前捕获值，供下一次转速计算使用。 */
    fan.lastCapVal = fan.nowCapVal;

    /* 检测到反馈脉冲后清零转速超时计数。 */
    fan.timeout = 0U;

    /* 排除脉冲周期过小的异常捕获数据。 */
    if (fan.pulseVal < FAN_FEEDBACK_CAPTURE_VALUE_MIN)
    {
        fan.speed = UINT32_MAX;
    }
    else
    {
        /* 将有效反馈脉冲周期转换为风扇转速。 */
        fan.speed = COUNT_TO_SPEED(fan.pulseVal);
    }
}


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_pwm_ramp_rst
//  Description: 复位风扇 PWM 平滑加减速状态
//  Param      : None
//  Return     : None
//  Others     : 清零当前实际 PWM 输出值及渐变计时值；
//               通常在风扇停机时调用
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
void fan_pwm_ramp_rst(void)
{
    fan.ramp.realPwm = 0U;
    fan.ramp.times   = 0U;
	fan.ramp.Start_times = 180;
}


/*******************************************************************************
//  Function   : fan_pwm_ramp_ctrl
//  Description: 根据目标 PWM 值执行平滑加减速控制
//  Param      : tagrtPwm - 目标 PWM 输出值
//  Return     : None
//  Others     : 首次启动时直接设置为一档 PWM；
//               之后每隔 FAN_RAMP_STEP_TIME 个任务周期，
//               按 FAN_RAMP_STEP 逐步调整实际 PWM；
//               接近目标值时直接设置为目标值，避免越过目标
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
#define FAN_START_PWM  980
static void fan_pwm_ramp_ctrl(uint16_t tagrtPwm)
{
    /* 首次启动时设置最低运行档位的 PWM。 */
	
	if(fan.ramp.Start_times > 0)
	{
		fan.ramp.realPwm = FAN_START_PWM;
		fan.ramp.Start_times --;
		if(fan.ramp.Start_times == 0)
		{
			fan.ramp.realPwm = TABLE_FAN_PWM_DUTY_NORMAL[1];
			fan.ramp.times   = 0U;
		}
	}
    else
    {
        /* 累计 PWM 渐变控制周期。 */
        fan.ramp.times++;

        /* 达到调整间隔且尚未到达目标 PWM 时执行一次调整。 */
        if ((fan.ramp.times > FAN_RAMP_STEP_TIME)
        &&  (fan.ramp.realPwm != tagrtPwm))
        {
            fan.ramp.times = 0U;

            /* 目标差值小于单次步进时，直接设置目标 PWM。 */
            if ((fan.ramp.realPwm > tagrtPwm && (fan.ramp.realPwm - tagrtPwm) < FAN_RAMP_STEP)
            ||  (fan.ramp.realPwm < tagrtPwm && (tagrtPwm - fan.ramp.realPwm) < FAN_RAMP_STEP))
            {
                fan.ramp.realPwm = tagrtPwm;
                return;
            }

            /* 根据目标 PWM 与实际 PWM 的大小关系执行加速或减速。 */
            fan.ramp.realPwm += tagrtPwm > fan.ramp.realPwm ? FAN_RAMP_STEP : -FAN_RAMP_STEP;
        }
    }
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_Init
//  Description: 初始化风扇控制模块
//  Param      : None
//  Return     : None
//  Others     : 默认设置为停机档位，清零转速并关闭 PWM 和风扇控制输出；
//               应在 GPIO 和 TIM1 初始化完成后调用
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_Init(void)
{
    fan.mode  = FAN_MODE_0;
    fan.speed = 0U;

    bsp_tim1_ch1_pwm_setduty(0U);
    FAN_CTRL_CLOSE();
}


/*******************************************************************************
//  Function   : fan_1ms_task
//  Description: 执行风扇 1ms 周期控制任务
//  Param      : None
//  Return     : None
//  Others     : 完成转速反馈超时检测、档位有效性检查、PWM 平滑加减速
//               以及风扇主供电通路控制
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_1ms_task(void)
{
    /* 转速反馈超时检测，无有效脉冲超过规定时间后将转速置零。 */
    if (fan.timeout < FAN_FEEDBACK_DETECT_TIME)
    {
        fan.timeout++;
    }
    else
    {
        fan.speed = 0U;
    }

    /* 档位越界时强制恢复为停机档位。 */
    if (fan.mode >= FAN_MODE_TYPE_NUM)
    {
        fan.mode = FAN_MODE_0;
    }

    /* 根据当前档位控制风扇启停及 PWM 输出。 */
    if (fan.mode == FAN_MODE_0)
    {
        /* 停机时先清零 PWM，再复位渐变状态并关闭主接地通路。 */
        bsp_tim1_ch1_pwm_setduty(0U);

        fan_pwm_ramp_rst();

        FAN_CTRL_CLOSE();
    }
    else
    {
        /* 启动时先接通风扇主接地通路。 */
        FAN_CTRL_OPEN();

        /* 根据目标档位执行 PWM 平滑加减速。 */
        fan_pwm_ramp_ctrl(TABLE_FAN_PWM_DUTY_NORMAL[fan.mode]);

        /* 更新实际 PWM 输出值。 */
        bsp_tim1_ch1_pwm_setduty(fan.ramp.realPwm);
    }
}


/*******************************************************************************
//  Function   : fan_set_speedmode
//  Description: 设置风扇目标运行档位
//  Param      : mode - 目标风扇档位
//  Return     : None
//  Others     : 将目标档位保存至运行状态；
//               档位有效性由 fan_1ms_task() 统一检查
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
//  Return     : 当前风扇运行档位
//  Others     : 返回 fan_mode_enum_t 类型的档位值
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
//  Return     : 当前计算得到的风扇转速
//  Others     : 正常情况下单位为 RPM；反馈超时后返回 0，
//               捕获周期过小时可能返回 UINT32_MAX
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
uint32_t fan_get_speed(void)
{
    return fan.speed;
}


/*******************************************************************************
//  Function   : fan_set_timeoutFlg
//  Description: 设置风扇转速检测超时标志
//  Param      : None
//  Return     : None
//  Others     : 仅设置 timeoutFlg，不修改风扇 PWM 或运行档位
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_set_timeoutFlg(void)
{
    fan.timeoutFlg = true;
}


/*******************************************************************************
//  Function   : fan_clean_timeoutFlg
//  Description: 清除风扇转速检测超时标志
//  Param      : None
//  Return     : None
//  Others     : 仅清除 timeoutFlg，不直接修改转速及风扇输出
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void fan_clean_timeoutFlg(void)
{
    fan.timeoutFlg = false;
}
