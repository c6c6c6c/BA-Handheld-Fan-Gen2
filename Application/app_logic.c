
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
#include "param.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/


/* Variables ---------------------------------------------------------------------------------------------------------*/
logic_save_t logic_save = {FAN_MODE_0, MIST_MODE_CLOSE, false};

/* Function implementations ------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_bms_Restore
//  Description: 从 Flash 恢复电池 SOC/SOH 及累计电量数据
//  Param      : None
//  Return     : None
//  Others     : 从 SOC 参数分区读取 battery_soc_t 历史数据，
//               并恢复至 bms_battery.soc_info；
//               数据有效性由 Flash 参数管理模块判断
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_logic_Restore(void)
{
    param_system_read((uint8_t *)&logic_save, sizeof(logic_save_t));
	System.logic.fan_mode  = logic_save.fan_mode;
	System.logic.mist_mode = logic_save.mist_mode;
	System.logic.lastpower = logic_save.power;
	System.logic.power     = logic_save.power;
}


/*******************************************************************************
//  Function   : app_bms_Save
//  Description: 将当前电池 SOC/SOH 及累计电量数据保存至 Flash
//  Param      : None
//  Return     : None
//  Others     : 将 bms_battery.soc_info 写入 SOC 参数分区；
//               Flash 擦除及编程操作由参数管理模块完成
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_logic_Save(void)
{	
	if(logic_save.fan_mode  != System.logic.fan_mode 
	|| logic_save.mist_mode != System.logic.mist_mode
	|| logic_save.power     != System.logic.power)
	{
		logic_save.fan_mode  = System.logic.fan_mode;
		logic_save.mist_mode = System.logic.mist_mode;
		logic_save.power     = System.logic.power;
		param_system_save((uint8_t *)&logic_save, sizeof(logic_save_t));
	}
}


/*******************************************************************************
//  Function   : btn_fan_event
//  Description: 处理风扇按键事件及档位循环切换
//  Param      : event - 当前按键事件；triggerInterval - 长按计数重置间隔
//  Return     : None
//  Others     : 短按释放时档位加一并对 FAN_MODE_TYPE_NUM 取模；设置显示更新标志；长按仅累计并复位内部计数器
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
//  Description: 处理雾化按键事件及模式循环切换
//  Param      : event - 当前按键事件；triggerInterval - 长按计数重置间隔
//  Return     : None
//  Others     : 短按释放时雾化模式加一并对 MIST_MODE_NUM 取模；设置显示更新标志；长按仅更新计数器
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
//  Description: 根据启动等待、SOC 初始化和电源按键状态更新电源标志
//  Param      : None
//  Return     : None
//  Others     : 前 2000 次调用或 SOC 尚未初始化时强制关机；其后按 BUTTON_ITEM_POWER 当前按键状态更新 power；first 在 SOC 未初始化期间仍持续累加
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void btn_power_event(void)
{
	static uint16_t first = 0;
	
	if(first < 2000 || bms_battery.soc_Init == false)
	{
		System.logic.power = 0;
		System.logic.save_flg = false;
		first++;
		return;
	}
	else
	{
		if(System.logic.save_flg == false)
		{
			app_logic_Restore();
			System.logic.save_flg = true;
		}
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
//  Description: 更新 USB 输入插入状态
//  Param      : None
//  Return     : None
//  Others     : 调用 BMS_IO_USB_IS_PRESENT() 并同步至 System.logic.usb_present
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void usb_in_event(void)
{
    System.logic.usb_present = BMS_IO_USB_IS_PRESENT();
}

/*******************************************************************************
//  Function   : app_dev_limit
//  Description: 根据电池 SOC 限制风扇档位和雾化模式
//  Param      : None
//  Return     : None
//  Others     : SOC 低于 10% 时关闭风扇和雾化；SOC 在 10%～30% 时关闭雾化，
                 并尝试将风扇 4/5 档限制为 3 档；当前档位限制语句使用比较运算符而非赋值
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void app_dev_limit(void)
{
    /* SOC 低于 10% 时限制所有风扇档位并关闭雾化。 */
    if(bms_battery.soc_info.soc < 10.5)
    {
        System.logic.mist_mode = MIST_MODE_CLOSE;
        System.logic.fan_mode  = FAN_MODE_0;
    }
    /* SOC 在 10%～30% 区间时关闭雾化并处理高风扇档位。 */
    else if(bms_battery.soc_info.soc < 30.5) 
    {
        System.logic.mist_mode = MIST_MODE_CLOSE;

        if(System.logic.fan_mode == FAN_MODE_5 || System.logic.fan_mode == FAN_MODE_4)
        {
            System.logic.fan_mode = FAN_MODE_3;
        }
    }
}

/*******************************************************************************
//  Function   : app_dev_logic
//  Description: 处理开关机状态转换、运行时长统计和风扇目标档位
//  Param      : None
//  Return     : None
//  Others     : 关机时复位风扇及雾化模式、累计关机时间；开机时累计运行、
                 风扇及有效雾化时间；状态变化时清零相关计数；同步风扇目标档位和历史电源状态
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void app_dev_logic(void)
{
    if (System.logic.power == 0)
    {
        /* 关机时关闭风扇与雾化目标模式；此处不直接清除故障位。 */
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
//  Description: 执行应用层按键、电源及设备限制的周期调度
//  Param      : None
//  Return     : None
//  Others     : 更新风扇及雾化按键事件；读取电源按键和 USB 插入状态；
                 先执行 SOC 限制，再更新设备运行状态；设计按 1ms 周期调用
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void app_logic_1ms_task(void)
{
    /* 更新风扇按键事件，传入长按判定参数 2000。 */
    button_event_Update(BUTTON_ITEM_FAN, 2000);

    /* 更新雾化按键事件，传入长按判定参数 2000。 */
    button_event_Update(BUTTON_ITEM_MIST, 2000);

	if(System.logic.save_flg)
	{
		/* 处理风扇按键事件，长按触发计数间隔为 200。 */
		btn_fan_event(botton_getEvent(BUTTON_ITEM_FAN), 200);

		/* 处理雾化按键事件，长按触发计数间隔为 200。 */
		btn_mist_event(botton_getEvent(BUTTON_ITEM_MIST), 200);
	}

    /* 根据电源按键状态更新设备开关机标志。 */
    btn_power_event();

    /* 同步 USB 插入检测状态。 */
    usb_in_event();

    /* 根据 SOC 阈值限制风扇档位与雾化功能。 */
    app_dev_limit();

	if(System.logic.save_flg)
	{
		/* 更新设备模式与计数，并向风机模块同步目标档位。 */
		app_dev_logic();
		
		app_logic_Save();
	}
}

/*******************************************************************************
//  Function   : fan_usb_drop_check
//  Description: 检测相邻 USB 电压采样的瞬时突降并复位风扇 PWM 渐变
//  Param      : None
//  Return     : None
//  Others     : 电压相邻差值超过 2V 且已开机、风扇档位不低于 3 时重置 PWM 渐变；
                 每次调用重新计算事件标志，未保持 200ms 的锁存状态
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


/**
 * @brief 电机采样滑动差分窗口实例
 * @note  保存窗口内各次差分、差分累计值、写入索引及最近一次 ADC 采样基准。
 */

motor_diff_t fan_curr_diff;

/*******************************************************************************
//  Function   : motor_diff_rst
//  Description: 复位电机 ADC 采样差分滑动窗口
//  Param      : None
//  Return     : None
//  Others     : 清空样本计数、累计差值及写入索引；以当前电机采样序号和数值作为新的差分基准；
                 不清空 window 数组
//  Author     : Chen Liang 2026-11-17
*******************************************************************************/
void motor_diff_rst()
{
    /* 清除窗口有效差分数量、累计值及循环写入位置。 */
	fan_curr_diff.count   = 0;
	fan_curr_diff.sum     = 0;
	fan_curr_diff.index   = 0;
	fan_curr_diff.lastSeq = get_motor_seq();
	fan_curr_diff.lastVal = get_motor_val();
}

/*******************************************************************************
//  Function   : motor_diff_update
//  Description: 按新采样更新电机 ADC 差分滑动窗口累计值
//  Param      : None
//  Return     : 窗口差分累计值（未填满时返回 0）
//  Others     : 仅当采样序号变化时计算相邻采样值之差；窗口未满时追加，满时替换最旧差分；窗口未满返回 0，满窗后返回累计差值
//  Author     : Chen Liang 2026-11-17
*******************************************************************************/
int motor_diff_update()
{
	int diff;
    /* 仅在电机 ADC 序号变化时处理新的采样点。 */
	if(fan_curr_diff.lastSeq != get_motor_seq())
	{
		uint16_t val = get_motor_val();
		
		fan_curr_diff.lastSeq = get_motor_seq();
		
        /* 计算当前采样值与前一次采样值的有符号差分。 */
		diff = val - fan_curr_diff.lastVal;
		
		if(fan_curr_diff.count < MOTOR_DIFF_WINDOW_MAX)
		{
			fan_curr_diff.count ++;
			fan_curr_diff.sum += diff;
		}
        /* 满窗后先扣除最旧差分，再加入新差分。 */
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
	
    /* 窗口未积满前返回 0；积满后提供差分累加值。 */
	if(fan_curr_diff.count < MOTOR_DIFF_WINDOW_MAX)
	{
		return 0;
	}
	return fan_curr_diff.sum;
}

/*******************************************************************************
//  Function   : fan_step_fault_handle
//  Description: 根据电机采样窗口差分检测风机突降故障
//  Param      : None
//  Return     : None
//  Others     : 先更新采样差分；风机关闭时清除突降故障并复位计时器；PWM 到达目标值后判断差分阈值；雾化开启和关闭时使用不同阈值；触发时复位 PWM 渐变；差分恢复稳定超过 5s 时清故障
//  Author     : Chen Liang 2026-11-17
*******************************************************************************/
static void fan_step_fault_handle()
{
	static timer_base_t _fault_timer    = {0};
	static timer_base_t _recovery_timer = {0};
	
    /* 更新系统状态中用于风机突降判断的累计差分值。 */
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
			
            /* 雾化开启时适当放宽差分故障与恢复判定区间。 */
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
			
            /* 突降故障成立时置位标志并重启风机 PWM 渐变。 */
			if(fault_flg)
			{
				System.err.bit.fan_step_err = 1;
				fan_pwm_ramp_rst();
			}
			
            /* 差分持续处于恢复区间超过 5s 时清除突降故障。 */
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
//  Function   : fan_run_fault_check
//  Description: 检测风机运行采样值异常及故障恢复
//  Param      : None
//  Return     : None
//  Others     : 风机关闭时清除运行故障；运行期间电机采样值低于 2 且持续超过 5s 时置故障；
                 采样恢复到不低于 2 且持续超过 5s 时清故障
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
        /* 电机采样值低于 2 时开始累计持续异常时长。 */
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
//  Description: 执行风机运行异常、突降及 USB 电压检测
//  Param      : None
//  Return     : None
//  Others     : 依次执行风机运行故障检测、采样差分突降检测、USB 输入电压突降检测
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void app_logic_fan(void)
{
    /* 风机运行状态检测：持续低采样值及恢复计时。 */
	fan_run_fault_check();
    /* 风机突降检测：滑动差分窗口和 PWM 渐变处理。 */
	fan_step_fault_handle();
    /* USB 电压相邻样本突降检测。 */
	fan_usb_drop_check();
}

