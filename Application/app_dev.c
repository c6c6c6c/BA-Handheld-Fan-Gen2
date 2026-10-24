/**
 ***********************************************************************************************************************
 * @file    app_dev.c
 * @brief   系统初始化、周期调度及风机/按键/LED 业务控制
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "app_system.h"
#include "app_dev.h"
#include "button.h"
#include "fan.h"
/* Macro -------------------------------------------------------------------------------------------------------------*/

/* Variables ---------------------------------------------------------------------------------------------------------*/

/* External variables ------------------------------------------------------------------------------------------------*/

/* Function implementations ------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Function   : add_btn_event
//  Description: 处理加档按键事件
//  Param      : event: 按键事件; triggerInterval: 长按连续触发间隔
//  Return     : None
//  Others     : 短按单次加档；长按按设定间隔连续加档；达到5档后触发上限闪烁
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void btn_fan_event(button_event_t event,uint16_t triggerInterval)
{
    static uint16_t Cnt = 0;

    switch (event)
    {
        case BUTTON_EVENT_LONG_PRESS_HOLD:
            /* 长按保持期间按 triggerInterval 周期连续加档。 */
            if(++Cnt >= triggerInterval)
            {
                Cnt = 0;

            }
        break;
        case BUTTON_EVENT_SHORT_PRESS_RELEASE:
            /* 短按释放时立即加一档；已到5档则提示达到上限。 */
            Cnt = triggerInterval;

            System.dev.fan_dev +=  System.dev.fan_dir ? -1 : 1;

            if(System.dev.fan_dev == FAN_MODE_5 || System.dev.fan_dev == FAN_MODE_1)
            {
                System.dev.fan_dir = !System.dev.fan_dir;
            }


        break;
        default:
            /* 非目标事件时预置计数器，避免刚进入长按状态立即触发。 */
            Cnt = triggerInterval;
        break;
    }
}

/*******************************************************************************
//  Function   : sub_btn_event
//  Description: 处理减档按键事件
//  Param      : event: 按键事件; triggerInterval: 长按连续触发间隔
//  Return     : None
//  Others     : 短按单次减档；长按按设定间隔连续减档；低于1档时触发下限闪烁
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void btn_mist_event(button_event_t event,uint16_t triggerInterval)
{
    static uint16_t Cnt = 0;

    switch (event)
    {
        case BUTTON_EVENT_LONG_PRESS_HOLD:
            /* 长按保持期间按 triggerInterval 周期连续加档。 */
            if(++Cnt >= triggerInterval)
            {
                Cnt = 0;
            }
        break;
        case BUTTON_EVENT_SHORT_PRESS_RELEASE:
            /* 短按释放时立即加一档；已到5档则提示达到上限。 */

            Cnt = triggerInterval;
        break;
        default:
            /* 非目标事件时预置计数器，避免刚进入长按状态立即触发。 */
            Cnt = triggerInterval;
        break;
    }
}

/*******************************************************************************
//  Function   : sub_btn_event
//  Description: 处理减档按键事件
//  Param      : event: 按键事件; triggerInterval: 长按连续触发间隔
//  Return     : None
//  Others     : 短按单次减档；长按按设定间隔连续减档；低于1档时触发下限闪烁
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void btn_power_event(void)
{
    if(botton_getCurrState(BUTTON_ITEM_ADC) == BUTTON_PRESS)
    {
        System.dev.power = 1;
    }
    else
    {
        System.dev.power = 0;
    }
}


/*******************************************************************************
//  Function   : button_all_passCallback
//  Description: 按键按下确定回调
//  Param      : None
//  Return     : None
//  Others     : 依次处理电源、连续工作保护、开机动画、按键事件及输出刷新
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void add_dev_logic(void)
{
    if(System.dev.power == 0)
    {
        System.dev.fan_dev = 0;
        System.offTimes ++;
        if(System.dev.lastpower != System.dev.power)
        {
            System.offTimes = 0;
            System.onTimes  = 0;
        }
    }
    else
    {
        System.onTimes ++;
        if(System.dev.lastpower != System.dev.power)
        {
            System.dev.fan_dev  = 1;
            System.dev.fan_dir  = 0;
            System.dev.mist_dev = false;
            System.offTimes     = 0;
            System.onTimes      = 0;
        }
    }

	fan_set_speedmode(System.dev.fan_dev);
    System.dev.lastpower = System.dev.power;
}


/*******************************************************************************
//  Function   : button_all_passCallback
//  Description: 按键按下确定回调
//  Param      : None
//  Return     : None
//  Others     : 依次处理电源、连续工作保护、开机动画、按键事件及输出刷新
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void add_dev_1ms_task(void)
{
    /* 刷新三个功能按键事件，长按判定时间为500ms。 */
    button_event_Update(BUTTON_ITEM_FAN, 500);
    button_event_Update(BUTTON_ITEM_MIST,500);

    btn_fan_event(botton_getEvent(BUTTON_ITEM_FAN),200);
    btn_mist_event(botton_getEvent(BUTTON_ITEM_MIST),200);
    btn_power_event();

    add_dev_logic();

}
