
/**
 ***********************************************************************************************************************
 * @file    app_logic.c
 * @brief   应用层设备控制，实现风扇档位、雾化 PWM 及电源状态管理
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_system.h"
#include "app_logic.h"
#include "button.h"
#include "fan.h"
#include "bsp_tim16.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/


/* Variables ---------------------------------------------------------------------------------------------------------*/


/* Function implementations ------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : btn_fan_event
//  Description: 处理风扇档位按键事件
//  Param      : event           - 当前按键事件
//               triggerInterval - 长按连续触发的计数间隔
//  Return     : None
//  Others     : 短按释放时切换一次风扇档位；
//               风扇档位在 1~5 档之间往返切换，达到边界时反转调档方向；
//               长按保持期间仅更新触发计数
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void btn_fan_event(button_event_t event, uint16_t triggerInterval)
{
    static uint16_t Cnt = 0U;

    switch (event)
    {
        case BUTTON_EVENT_LONG_PRESS_HOLD:
            /* 长按期间累计计数，达到触发间隔后清零。 */
            if (++Cnt >= triggerInterval)
            {
                Cnt = 0U;
				
				System.logic.fan_dev += System.logic.fan_dir ? -1 : 1;

				/* 达到最高档或最低档时反转调档方向。 */
				if ((System.logic.fan_dev == FAN_MODE_5)
				||  (System.logic.fan_dev == FAN_MODE_1))
				{
					System.logic.fan_dir = !System.logic.fan_dir;
				}
            }
            break;

        case BUTTON_EVENT_SHORT_PRESS_RELEASE:
            /* 短按释放时根据当前调档方向切换一次档位。 */
            Cnt = triggerInterval;

            System.logic.fan_dev += System.logic.fan_dir ? -1 : 1;

            /* 达到最高档或最低档时反转调档方向。 */
            if ((System.logic.fan_dev == FAN_MODE_5)
            ||  (System.logic.fan_dev == FAN_MODE_1))
            {
                System.logic.fan_dir = !System.logic.fan_dir;
            }
            break;

        default:
            /* 其他事件预置触发计数器。 */
            Cnt = triggerInterval;
            break;
    }
}


/*******************************************************************************
//  Function   : btn_mist_event
//  Description: 处理雾化功能按键事件
//  Param      : event           - 当前按键事件
//               triggerInterval - 长按连续触发的计数间隔
//  Return     : None
//  Others     : 短按释放时切换雾化功能状态；
//               长按保持期间仅累计触发计数，暂不执行雾化控制
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void btn_mist_event(button_event_t event, uint16_t triggerInterval)
{
    static uint16_t Cnt = 0U;

    switch (event)
    {
        case BUTTON_EVENT_LONG_PRESS_HOLD:
            /* 长按期间累计计数，达到触发间隔后清零。 */
            if (++Cnt >= triggerInterval)
            {
                Cnt = 0U;
            }
            break;

        case BUTTON_EVENT_SHORT_PRESS_RELEASE:
            /* 短按释放时切换雾化功能状态。 */
            Cnt = triggerInterval;

            System.logic.mist_dev = !System.logic.mist_dev;
            break;

        default:
            /* 其他事件预置触发计数器。 */
            Cnt = triggerInterval;
            break;
    }
}


/*******************************************************************************
//  Function   : btn_power_event
//  Description: 根据电源按键状态更新设备开关机标志
//  Param      : None
//  Return     : None
//  Others     : ADC 电源按键按下时设置 power=1；
//               按键释放时设置 power=0
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void btn_power_event(void)
{
    /* 根据电源按键电平状态更新设备电源标志。 */
    if (botton_getCurrState(BUTTON_ITEM_ADC) == BUTTON_PRESS)
    {
        System.logic.power = 1;
    }
    else
    {
        System.logic.power = 0;
    }
}


/*******************************************************************************
//  Function   : app_logic_mist
//  Description: 根据雾化功能状态更新 TIM16 PWM 输出参数
//  Param      : None
//  Return     : None
//  Others     : 当前逻辑中 mist_dev 为 true 时将占空比设置为 0；
//               mist_dev 为 false 时设置频率为 150kHz、占空比参数为 200；
//               仅在目标参数变化且底层配置成功后更新参数缓存
//  Author     : 陈亮 2026-10-26
*******************************************************************************/
void app_logic_mist(void)
{
    /* 保存上一次成功配置的雾化片 PWM 参数。 */
    static uint32_t old_freq = 0U;
    static uint16_t old_duty = 0U;

    /* 根据雾化功能状态确定 PWM 输出参数。 */
    if (System.logic.mist_dev)
    {
		System.logic.mist_freq = 150000U;
        System.logic.mist_duty = 200U;
		System.logic.mistTimes ++;
    }
    else
    {
        System.logic.mist_duty = 0U;
		System.logic.mistTimes = 0;
    }

    /*
     * 目标 PWM 参数发生变化时重新配置定时器。
     * 仅在配置成功后更新缓存，失败时保留旧参数以便下次重试。
     */
    if ((System.logic.mist_freq != old_freq)
    ||  (System.logic.mist_duty != old_duty))
    {
        if (bsp_tim16_ch1_pwm_set(System.logic.mist_freq,System.logic.mist_duty) == SUCCESS)
        {
            old_freq = System.logic.mist_freq;
            old_duty = System.logic.mist_duty;
        }
    }
}


/*******************************************************************************
//  Function   : app_dev_logic
//  Description: 根据设备开关机状态更新应用层运行参数
//  Param      : None
//  Return     : None
//  Others     : 关机时将风扇档位设置为 0 并累计关机时间；
//               开机时累计运行时间，并在开机瞬间初始化风扇及雾化状态；
//               最后同步风扇档位、雾化 PWM 及历史电源状态
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void app_dev_logic(void)
{
    if (System.logic.power == 0)
    {
        /* 设备关机时关闭风扇并累计关机时间。 */
        System.logic.fan_dev = FAN_MODE_0;
        System.logic.offTimes++;

        /* 检测开机转关机事件，清零运行计时器。 */
        if (System.logic.lastpower != System.logic.power)
        {
            System.logic.offTimes = 0U;
            System.logic.onTimes  = 0U;
        }
    }
    else
    {
        /* 设备开机时累计连续运行时间。 */
        System.logic.onTimes++;

        /* 检测关机转开机事件，初始化设备运行状态。 */
        if (System.logic.lastpower != System.logic.power)
        {
            System.logic.fan_dev  = FAN_MODE_1;
            System.logic.fan_dir  = 0U;
            System.logic.mist_dev = false;

            System.logic.offTimes = 0U;
            System.logic.onTimes  = 0U;
        }
    }

    /* 同步目标风扇运行档位。 */
    fan_set_speedmode(System.logic.fan_dev);

    /* 根据雾化功能状态更新 TIM16 PWM。 */
    app_logic_mist();

    /* 保存当前电源状态，供下一次检测状态变化。 */
    System.logic.lastpower = System.logic.power;
}


/*******************************************************************************
//  Function   : add_logic_1ms_task
//  Description: 执行应用层设备 1ms 周期控制任务
//  Param      : None
//  Return     : None
//  Others     : 更新风扇及雾化按键事件，处理设备电源状态；
//               同步风扇运行档位及雾化片 PWM 输出参数
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void add_logic_1ms_task(void)
{
    /* 更新风扇按键事件，长按判定时间为 500ms。 */
    button_event_Update(BUTTON_ITEM_FAN, 500);

    /* 更新雾化按键事件，长按判定时间为 500ms。 */
    button_event_Update(BUTTON_ITEM_MIST, 500);

    /* 处理风扇按键事件，长按触发计数间隔为 200。 */
    btn_fan_event(botton_getEvent(BUTTON_ITEM_FAN), 200);

    /* 处理雾化按键事件，长按触发计数间隔为 200。 */
    btn_mist_event(botton_getEvent(BUTTON_ITEM_MIST), 200);

    /* 根据电源按键状态更新设备开关机标志。 */
    btn_power_event();

    /* 执行设备状态管理及风扇、雾化输出控制。 */
    app_dev_logic();
}
