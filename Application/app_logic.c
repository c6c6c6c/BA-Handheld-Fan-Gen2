
/**
 ***********************************************************************************************************************
 * @file    app_logic.c
 * @brief   应用层设备控制，实现按键处理、运行状态管理及风机异常监测
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "bsp_tim16.h"

#include "app_sampling.h"
#include "app_system.h"
#include "app_logic.h"
#include "app_bms.h"

#include "bms_config.h"
#include "timer_base.h"
#include "button.h"
#include "fan.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/


/* Variables ---------------------------------------------------------------------------------------------------------*/


/* Function implementations ------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : btn_fan_event
//  Description: 处理风扇按键事件及档位切换
//  Param      : event - 当前按键事件；triggerInterval - 长按计数重置间隔
//  Return     : None
//  Others     : 短按释放时档位加 1 并对 FAN_MODE_TYPE_NUM 取模；长按保持仅更新计数器；短按时置位显示刷新标志
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
            }
            break;

        case BUTTON_EVENT_SHORT_PRESS_RELEASE:
            /* 短按释放时档位加 1，并通过取模循环切换档位。 */
            Cnt = triggerInterval;

            System.logic.fan_mode ++;
            System.logic.fan_mode    %= FAN_MODE_TYPE_NUM;
            System.logic.display_flg  = true;
		
            break;

        default:
            /* 其他事件预置触发计数器。 */
            Cnt = triggerInterval;
            break;
    }
}


/*******************************************************************************
//  Function   : btn_mist_event
//  Description: 处理雾化按键事件及模式切换
//  Param      : event - 当前按键事件；triggerInterval - 长按计数重置间隔
//  Return     : None
//  Others     : 短按释放时模式加 1 并对 MIST_MODE_NUM 取模；长按保持仅更新计数器；短按时置位显示刷新标志
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

            System.logic.mist_mode ++;
            System.logic.mist_mode   %= MIST_MODE_NUM;
            System.logic.display_flg  = true;
            break;

        default:
            /* 其他事件预置触发计数器。 */
            Cnt = triggerInterval;
            break;
    }
}


/*******************************************************************************
//  Function   : btn_power_event
//  Description: 根据电源按键与 SOC 初始化状态更新开关机标志
//  Param      : None
//  Return     : None
//  Others     : 启动阶段先累计 first 计数；first 小于 2000 或 SOC 未初始化时强制关机；此后根据电源按键实时电平设置 power
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void btn_power_event(void)
{
	static uint16_t first = 0;
	
	if(first < 2000 || bms_battery.soc_Init == false)
	{
		System.logic.power = 0;
		first++;
		return;
	}
	
    /* 启动计数及 SOC 初始化条件满足后，根据电源按键实时状态更新电源标志。 */
    if (botton_getCurrState(BUTTON_ITEM_POWER) == BUTTON_PRESS)
    {
        System.logic.power = 1;
    }
    else
    {
        System.logic.power = 0;
    }
}

/*******************************************************************************
//  Function   : usb_in_event
//  Description: 读取 USB 插入检测信号并更新系统状态
//  Param      : None
//  Return     : None
//  Others     : 通过 BMS_IO_USB_IS_PRESENT() 更新 System.logic.usb_present
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void usb_in_event(void)
{
    System.logic.usb_present = BMS_IO_USB_IS_PRESENT();
}


/*******************************************************************************
//  Function   : app_dev_logic
//  Description: 依据电源状态更新运行模式、时间统计与风机目标档位
//  Param      : None
//  Return     : None
//  Others     : 关机时关闭风机与雾化并清除风机故障；开机时统计风机与雾化工作次数；电源状态切换时清零相关计数器
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void app_dev_logic(void)
{
    if (System.logic.power == 0)
    {
        /* 关机状态下清除风机与雾化目标模式，并清除风机故障。 */
        System.logic.fan_mode  = FAN_MODE_0;
        System.logic.mist_mode = MIST_MODE_CLOSE;
		
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
            System.logic.fan_mode  = FAN_MODE_0;
            System.logic.fan_dir   = 0U;
            System.logic.mist_mode = MIST_MODE_CLOSE;

            System.logic.offTimes  = 0U;
            System.logic.onTimes   = 0U;
            System.logic.fanTimes  = 0U;
            System.logic.mistTimes = 0U;
            System.logic.display_flg  = true;
        }

        if(System.logic.fan_mode != FAN_MODE_0)
        {
            System.logic.fanTimes ++;
        }

        if (System.logic.mist_duty != 0)
        {
            System.logic.mistTimes++;
        }
    }

    /* 同步目标风扇运行档位。 */
    fan_set_speedmode(System.logic.fan_mode);

    /* 保存当前电源状态，供下一次检测状态变化。 */
    System.logic.lastpower = System.logic.power;
}


/*******************************************************************************
//  Function   : app_logic_1ms_task
//  Description: 执行按键事件、开关机判断及设备状态周期管理
//  Param      : None
//  Return     : None
//  Others     : 更新风机和雾化按键事件；获取电源按键及 USB 插入状态；执行设备运行逻辑；按 1ms 调用周期设计
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void app_logic_1ms_task(void)
{
    /* 更新风扇按键事件，传入长按判定参数 2000。 */
    button_event_Update(BUTTON_ITEM_FAN, 2000);

    /* 更新雾化按键事件，传入长按判定参数 2000。 */
    button_event_Update(BUTTON_ITEM_MIST, 2000);

    /* 处理风扇按键事件，长按触发计数间隔为 200。 */
    btn_fan_event(botton_getEvent(BUTTON_ITEM_FAN), 200);

    /* 处理雾化按键事件，长按触发计数间隔为 200。 */
    btn_mist_event(botton_getEvent(BUTTON_ITEM_MIST), 200);

    /* 根据电源按键状态更新设备开关机标志。 */
    btn_power_event();

    /* 同步 USB 插入检测状态。 */
    usb_in_event();

    /* 更新设备模式与计数，并向风机模块同步目标档位。 */
    app_dev_logic();
}

/*******************************************************************************
//  Function   : fan_usb_drop_check
//  Description: 检测 USB 电压相邻采样突降并尝试复位风机 PWM 渐变
//  Param      : None
//  Return     : None
//  Others     : 检测相邻 USB 电压下降超过 2V 的单次事件；满足开机且档位不低于 3 时调用 fan_pwm_ramp_rst()；事件标志每次调用重新计算
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
static void fan_usb_drop_check(void)
{
    static double last_usb_v = 0.0;
    static bool boost_flg = false;
    static timer_base_t _timer_boost = {0};

    /* 检测相邻两次 USB 输入电压是否下降超过 2V。 */
    boost_flg = (last_usb_v - bms_battery.usb_in_v) > 2.0;

    /* 保存当前 USB 输入电压。 */
    last_usb_v = bms_battery.usb_in_v;

    if (boost_flg == false)
    {
        /* 未检测到突降时重置事件计时器。 */
        timer_base_Rst(&_timer_boost);
    }
    else
    {
        /* 检查瞬时突降事件的计时状态；标志将在下一次调用时重新计算。 */
        if (timer_base_Count(&_timer_boost) > 200U)
        {
            /* 当前计时器超过阈值时清除本次突降标志。 */
            boost_flg = false;
        }
    }

    /*
     * 风机高档运行期间，如果检测到 USB 电压突降，
     * 则重新初始化风机 PWM 渐变状态。
     */
    if (boost_flg 
    &&  System.logic.power 
	&& (System.logic.fan_mode >= FAN_MODE_3))
    {
        fan_pwm_ramp_rst();
    }
}


/** 电机采样滑动差分窗口实例；记录差分数组、累计值及采样基准。 */

motor_diff_t fan_curr_diff;

/*******************************************************************************
//  Function   : motor_diff_rst
//  Description: 复位电机采样差分窗口累计状态
//  Param      : None
//  Return     : None
//  Others     : 清零窗口计数、累计值和写入索引；使用当前电机采样序号及数值作为后续差分基准；不清空窗口数组
//  Author     : Chen Liang 2026-11-17
*******************************************************************************/
void motor_diff_rst()
{
	fan_curr_diff.count   = 0;
	fan_curr_diff.sum     = 0;
	fan_curr_diff.index   = 0;
	fan_curr_diff.lastSeq = get_motor_seq();
	fan_curr_diff.lastVal = get_motor_val();
}

/*******************************************************************************
//  Function   : motor_diff_update
//  Description: 按新增电机采样更新滑动窗口差分累加值
//  Param      : None
//  Return     : 窗口差分累计值；窗口满时当前实现返回 0
//  Others     : 采样序号变化时计算本次采样与上次采样的有符号差分；窗口未满时追加，满时移除最旧差分；当前实现仅在 count != MOTOR_DIFF_WINDOW_MAX 时返回累加值，窗口满时返回 0
//  Author     : Chen Liang 2026-11-17
*******************************************************************************/
int motor_diff_update()
{
	int diff;
	if(fan_curr_diff.lastSeq != get_motor_seq())
	{
		uint16_t val = get_motor_val();
		
		fan_curr_diff.lastSeq = get_motor_seq();
		
		diff = val - fan_curr_diff.lastVal;
		
		if(fan_curr_diff.count < MOTOR_DIFF_WINDOW_MAX)
		{
			fan_curr_diff.count ++;
			fan_curr_diff.sum += diff;
		}
		else
		{
			fan_curr_diff.sum -= fan_curr_diff.window[fan_curr_diff.index];
			fan_curr_diff.sum += diff;
		}
		
		fan_curr_diff.window[fan_curr_diff.index] = diff;
		
		fan_curr_diff.index ++;
		
		fan_curr_diff.index %= MOTOR_DIFF_WINDOW_MAX;
		
		fan_curr_diff.lastVal = val;
	}
	
	if(fan_curr_diff.count < MOTOR_DIFF_WINDOW_MAX)
	{
		return 0;
	}
	return fan_curr_diff.sum;
}

/*******************************************************************************
//  Function   : fan_curr_check
//  Description: 依据电机差分累计值检测突降与风机故障
//  Param      : None
//  Return     : None
//  Others     : PWM 尚未达到目标或风机关闭时复位窗口、计数和计时器；否则低于 -20 时复位渐变；累计触发后尝试在 10000ms 内报告风机故障；当前 lastflg 未更新
//  Author     : Chen Liang 2026-11-17
*******************************************************************************/
static void fan_step_fault_handle()
{
	static timer_base_t _fault_timer = {0};
	static timer_base_t _recovery_timer = {0};
	
	System.fan_curr_diff = motor_diff_update();
	
	if(fan.mode == FAN_MODE_0)
	{
		System.err.bit.fan_step_err = 0;
        timer_base_Rst(&_fault_timer);
		timer_base_Rst(&_recovery_timer);
	}
	else
	{
		if(fan.ramp.realPwm == fan.ramp.tagrtPwm)
		{
			bool recovery_flg = 0;
			bool fault_flg    = 0;
			
			if(System.logic.mist_duty)
			{
				recovery_flg = System.fan_curr_diff < 18 && System.fan_curr_diff > -18;
				fault_flg    = System.fan_curr_diff < -28;
			}
			else
			{
				recovery_flg = System.fan_curr_diff < 10 && System.fan_curr_diff > -10;
				fault_flg    = System.fan_curr_diff < -20;
			}
			
			if(fault_flg)
			{
				System.err.bit.fan_step_err = 1;
				fan_pwm_ramp_rst();
			}
			
			
			if(recovery_flg)
			{
				if(timer_base_Count(&_recovery_timer) > 5000U)
				{
					System.err.bit.fan_step_err = 0;
				}
				
			}
			else
			{
				timer_base_Rst(&_recovery_timer);
			}
				
		}
	}
}

/*******************************************************************************
//  Function   : fan_curr_check
//  Description: 依据电机差分累计值检测突降与风机故障
//  Param      : None
//  Return     : None
//  Others     : PWM 尚未达到目标或风机关闭时复位窗口、计数和计时器；否则低于 -20 时复位渐变；累计触发后尝试在 10000ms 内报告风机故障；当前 lastflg 未更新
//  Author     : Chen Liang 2026-11-17
*******************************************************************************/
static void fan_run_fault_check()
{
	static timer_base_t _fault_timer = {0};
	static timer_base_t _recovery_timer = {0};
	
	if(fan.mode == FAN_MODE_0)
	{
		System.err.bit.fan_err = 0;
		timer_base_Rst(&_fault_timer);
		timer_base_Rst(&_recovery_timer);
	}
	else
	{
		if(get_motor_val() < 2)
        {
            if(timer_base_Count(&_fault_timer) > 5000U)
            {
                System.err.bit.fan_err = 1;
            }
			timer_base_Rst(&_recovery_timer);
        }
        else
        {
			if(timer_base_Count(&_recovery_timer) > 5000U)
            {
                System.err.bit.fan_err = 0;
            }
            timer_base_Rst(&_fault_timer);
        }
	}
}

/*******************************************************************************
//  Function   : app_logic_fan
//  Description: 执行应用层风机相关检测入口
//  Param      : None
//  Return     : None
//  Others     : 当前仅调用 fan_usb_drop_check()；未调用 fan_curr_check()
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void app_logic_fan(void)
{
	fan_run_fault_check();
	fan_step_fault_handle();
	fan_usb_drop_check();
}

