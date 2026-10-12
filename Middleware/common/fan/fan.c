/** 
 *********************************************************************************************************************** 
 * @file    fan.c 
 * @brief   风扇控制模块，实现风扇档位控制、PWM输出、转速反馈检测及低电量降功率控制
 * @author  Leo Chen
 * @version 1.0.0 
 * @date    2025-09-03 
 *********************************************************************************************************************** 
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved. 
 *********************************************************************************************************************** 
 */ 
 
/* Includes ----------------------------------------------------------------------------------------------------------*/ 
#include "fan.h" 
#include "bsp_tim1.h" 

/* Macro -------------------------------------------------------------------------------------------------------------*/ 

/* 风扇反馈转速检测超时时间 */ 
#define FAN_FEEDBACK_DETECT_TIME        (1000) 

/* 风扇反馈捕获计数最小有效值 */ 
#define FAN_FEEDBACK_CAPTURE_VALUE_MIN  (200) 

/* 将反馈脉冲周期计数转换为风扇转速 */ 
#define COUNT_TO_SPEED(n)               ((TIM1_SYS_TIMER_FRE * 60UL) / n) 
 
/* Variables ---------------------------------------------------------------------------------------------------------*/ 
 
fan_t fan; 
 
/* External variables ------------------------------------------------------------------------------------------------*/ 
 
/* Constant ----------------------------------------------------------------------------------------------------------*/ 

/********************************************************* 
 * Description: 正常电量模式下各风扇档位PWM占空比表
 *********************************************************/ 
const uint16_t TABLE_FAN_PWM_DUTY_NORMAL[FAN_MODE_TYPE_NUM] = 
{ 
    0, 120, 220, 380, 460, 600, 850, 
}; 
 
 
/******************************************************************************* 
//  Function   : fan_IRQCallback 
//  Description: 风扇转速反馈输入捕获中断回调函数
//  Param      : None 
//  Return     : None 
//  Others     : 根据定时器计数值及输入捕获值计算相邻反馈脉冲周期，并换算风扇转速
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
void bsp_tim1_ch4_inputCapture_Callback(void) 
{ 
    fan.pulseVal  = bsp_tim1_Count(); 
    fan.nowCapVal = bsp_tim1_ch4_inputCapture_Count(); 
		
	if(fan.pulseVal != 0)
	{
		fan.pulseVal -= fan.lastCapVal;
		fan.pulseVal += fan.nowCapVal;
	}
	else
	{
		if(fan.nowCapVal > fan.lastCapVal)
		{
			fan.pulseVal = (fan.nowCapVal - fan.lastCapVal);
		}
		else
		{
			fan.pulseVal = (fan.nowCapVal + (TIM1_SYS_TIMER_FRE - fan.lastCapVal));
		}				
	}
    fan.lastCapVal = fan.nowCapVal; 
    fan.timeout    = 0; 
    if (fan.pulseVal < FAN_FEEDBACK_CAPTURE_VALUE_MIN) 
    { 
        fan.speed = UINT32_MAX; 
    } 
    else 
    { 
        fan.speed = COUNT_TO_SPEED(fan.pulseVal); 
    } 
} 
 
/******************************************************************************* 
//  Function   : fan_Init 
//  Description: 初始化风扇控制模块
//  Param      : None 
//  Return     : None 
//  Others     : 默认风扇档位设置为FAN_MODE_0，反馈转速清零
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
void fan_Init(void) 
{ 
    fan.mode  = FAN_MODE_0; 
    fan.speed = 0; 
} 
 
/******************************************************************************* 
//  Function   : fan_10ms_task 
//  Description: 风扇10ms周期控制任务
//  Param      : None 
//  Return     : None 
//  Others     : 完成反馈超时检测、档位有效性检查、电池低电量判断及PWM占空比更新
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
void fan_1ms_task(void) 
{ 
 
    if (fan.timeout < FAN_FEEDBACK_DETECT_TIME) 
    { 
        fan.timeout++; 
    } 
    else 
    { 
        fan.speed = 0; 
    } 
 
    if (fan.mode >= FAN_MODE_TYPE_NUM) 
    { 
        fan.mode = FAN_MODE_0; 
    } 
 
    bsp_tim1_ch1_pwm_setduty(TABLE_FAN_PWM_DUTY_NORMAL[fan.mode]); 

} 
 
/******************************************************************************* 
//  Function   : fan_SetSpeedMode 
//  Description: 设置风扇运行档位
//  Param      : mode - 风扇运行档位
//  Return     : None 
//  Others     : 实际档位有效性在fan_10ms_task中进行检查
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
void fan_SetSpeedMode(uint8_t mode) 
{ 
    fan.mode = (fan_mode_enum_t)mode; 
} 
 
/******************************************************************************* 
//  Function   : fan_GetSpeedMode 
//  Description: 获取当前风扇运行档位
//  Param      : None 
//  Return     : 当前风扇运行档位
//  Others     : None 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
fan_mode_enum_t fan_GetSpeedMode(void) 
{ 
    return fan.mode; 
} 
 
/******************************************************************************* 
//  Function   : fan_getSpeed 
//  Description: 获取当前风扇反馈转速
//  Param      : None 
//  Return     : 当前风扇反馈转速
//  Others     : 转速由输入捕获反馈周期计算得到，反馈超时时转速被置为0
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
uint32_t fan_getSpeed(void) 
{ 
    return fan.speed; 
} 
 
/******************************************************************************* 
//  Function   : fan_SetTimeoutFlg 
//  Description: 设置风扇工作超时标志
//  Param      : None 
//  Return     : None 
//  Others     : 标志置位后，周期任务将风扇PWM输出设置为FAN_MODE_0对应占空比
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
void fan_SetTimeoutFlg(void) 
{ 
    fan.timeoutFlg = true; 
} 
 
/******************************************************************************* 
//  Function   : fan_CleanTimeoutFlg 
//  Description: 清除风扇工作超时标志
//  Param      : None 
//  Return     : None 
//  Others     : 清除后恢复按照当前电量状态及风扇档位控制PWM输出
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
void fan_CleanTimeoutFlg(void) 
{ 
    fan.timeoutFlg = false; 
} 
