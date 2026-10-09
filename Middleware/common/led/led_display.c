/**
 ***********************************************************************************************************************
 * @file    led_display.c
 * @brief   LED显示控制模块，实现LED开关、亮度调节、呼吸效果及周期刷新控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "led_display.h"

/* Variables ---------------------------------------------------------------------------------------------------------*/
led_manage_t ledDisplay = {
    .breat_risingtime  = 500,
    .breat_risingwait  = 0,
    .breat_fallingtime = 400,
    .breat_fallingwait = 100,
};

const led_duty_t TABLE_LED_DUTY[LED_DUTY_LEVEL_INDEX_MAX] =
{
    {.Voltage = 4200, .Duty = 30}, // 4.2 V
    {.Voltage = 4100, .Duty = 31}, // 4.1 V
    {.Voltage = 4000, .Duty = 32}, // 4.0 V
    {.Voltage = 3900, .Duty = 35}, // 3.9 V
    {.Voltage = 3800, .Duty = 39}, // 3.8 V
    {.Voltage = 3700, .Duty = 44}, // 3.7 V
    {.Voltage = 3600, .Duty = 48}, // 3.6 V
    {.Voltage = 3500, .Duty = 53}, // 3.5 V
    {.Voltage = 3400, .Duty = 58}, // 3.4 V
    {.Voltage = 3300, .Duty = 64}, // 3.3 V
    {.Voltage = 3200, .Duty = 73}, // 3.2 V
    {.Voltage = 3100, .Duty = 81}, // 3.1 V
    {.Voltage = 3000, .Duty = 90}, // 3.0 V
};

/* Macro -------------------------------------------------------------------------------------------------------------*/
DEF_LED_CTRL_FUNC(LED2,LED1_PORT,LED1_PIN) //LED 1 控制函数定义
DEF_LED_CTRL_FUNC(LED1,LED2_PORT,LED2_PIN) //LED 2 控制函数定义
DEF_LED_CTRL_FUNC(LED3,LED3_PORT,LED3_PIN) //LED 3 控制函数定义
#ifndef DEBUG_MODE
DEF_LED_CTRL_FUNC(LED4,LED4_PORT,LED4_PIN) //LED 4 控制函数定义
#endif
DEF_LED_CTRL_FUNC(LED5,LED5_PORT,LED5_PIN) //LED 5 控制函数定义


/*******************************************************************************
//  Function   : led_Ctrl_IRQ
//  Description: LED软件PWM周期控制处理
//  Param      : None
//  Return     : None
//  Others     : 建议在固定周期定时中断中调用，根据阈值控制各LED输出状态
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void led_Ctrl_IRQ(void)
{
    uint8_t i;
    ledDisplay.count %= LED_COUNT_MAX;
    ledDisplay.count ++;

    for(i = 0 ; i < LED_ITEM_NUM ; i++)
    {
        if(ledDisplay.Ctrl[i].Func)
        {
            ledDisplay.Ctrl[i].Func(ledDisplay.count < ledDisplay.Ctrl[i].Threshold);
        }
    }

}

/*******************************************************************************
//  Function   : led_register
//  Description: 注册指定LED的硬件控制函数并初始化控制状态
//  Param      : Item - LED项目索引
//               Func - LED硬件控制函数指针
//  Return     : None
//  Others     : 注册成功后将对应LED初始化为关闭状态
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static void led_register(led_item_enum_t Item, LedCtrl_Func Func)
{
    if(Item < LED_ITEM_NUM)
    {
        ledDisplay.Ctrl[Item].Func      = Func;
        ledDisplay.Ctrl[Item].Threshold = 0;
        ledDisplay.Ctrl[Item].Mode      = LED_OFF;
        if(Func)
        {
            Func(0); // 初始化时关闭LED
        }
    }
}

/*******************************************************************************
//  Function   : led_IsBreat
//  Description: 判断当前是否存在LED处于呼吸模式
//  Param      : None
//  Return     : true  - 至少一个LED处于呼吸模式
//               false - 无LED处于呼吸模式
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static bool led_IsBreat()
{
    uint8_t i;
    for(i = 0 ; i < LED_ITEM_NUM ; i++)
    {
        if(ledDisplay.Ctrl[i].Mode == LED_BREATH)
        {
            return true;
        }
    }
    return false;
}

/*******************************************************************************
//  Function   : led_SetMode
//  Description: 设置指定LED的显示模式
//  Param      : Item - LED项目索引
//               Mode - LED显示模式
//  Return     : None
//  Others     : 参数有效且模式发生变化时更新LED控制状态
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void led_SetMode(led_item_enum_t Item, led_mode_enum_t Mode)
{
    if(Item < LED_ITEM_NUM && Mode < LED_MODE_MAX)
    {
        if(ledDisplay.Ctrl[Item].Mode == Mode) return; // LED模式未发生变化，无需重复设置

        ledDisplay.Ctrl[Item].Mode = Mode;
    }
}

/*******************************************************************************
//  Function   : led_BreathCountUpdate
//  Description: 更新LED呼吸亮度计数及呼吸方向状态
//  Param      : None
//  Return     : None
//  Others     : 根据上升、等待、下降、等待四个阶段周期更新呼吸亮度
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static void led_BreathCountUpdate(void)
{
    static uint32_t TimeOut = 0;
    static uint16_t step_time;

    if(led_IsBreat())
    {
        if(TimeOut != 0){TimeOut--;return;}

        if(ledDisplay.maxbrightness == 0)
        {
            ledDisplay.breatCount = 0;
            return;
        }

        if(ledDisplay.breatCount > ledDisplay.maxbrightness)
        {
            ledDisplay.breatCount = ledDisplay.maxbrightness;
        }

        if(ledDisplay.breatCount == ledDisplay.maxbrightness 
        || ledDisplay.breatCount == 0)
        {
            if(ledDisplay.breatCount && ledDisplay.breatDir == LED_BREATH_DIR_UP)
            {
                 ledDisplay.breatDir = LED_BREATH_DIR_UP_WAIT;
                 TimeOut             = ledDisplay.breat_risingwait;
                 return;
            }
            else if(ledDisplay.breatCount && ledDisplay.breatDir == LED_BREATH_DIR_UP_WAIT)
            {
                ledDisplay.breatDir = LED_BREATH_DIR_DOWN;
                step_time           = ledDisplay.breat_fallingtime / ledDisplay.maxbrightness;
                
            }
            else if(!ledDisplay.breatCount && ledDisplay.breatDir == LED_BREATH_DIR_DOWN)
            {
                ledDisplay.breatDir = LED_BREATH_DIR_DOWN_WAIT;
                TimeOut             = ledDisplay.breat_fallingwait;
                return;
            }
            else
            {
                ledDisplay.breatDir = LED_BREATH_DIR_UP;
                step_time           = ledDisplay.breat_risingtime / ledDisplay.maxbrightness;
            }
        }
        TimeOut = step_time == 0 ? 1 : step_time;
        if(ledDisplay.breatDir == LED_BREATH_DIR_DOWN)
        {
            if(ledDisplay.breatCount > 0)
            {
                ledDisplay.breatCount--;
            }
        }
        else if(ledDisplay.breatCount < ledDisplay.maxbrightness)
        {
            ledDisplay.breatCount++;
        }
    }
    else
    {
        ledDisplay.breatCount = ledDisplay.maxbrightness;
        ledDisplay.breatDir   = LED_BREATH_DIR_UP;
        TimeOut               = 0;
    }
} 

/*******************************************************************************
//  Function   : led_SetMaxBrightness
//  Description: 设置LED软件PWM最大亮度阈值
//  Param      : MaxBrightness - 最大亮度值
//  Return     : None
//  Others     : 输入值大于LED_COUNT_MAX时不更新亮度
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void led_SetMaxBrightness(uint8_t MaxBrightness)
{
    if(MaxBrightness > LED_COUNT_MAX) return;
    ledDisplay.maxbrightness = MaxBrightness;
}


/*******************************************************************************
//  Function   : led_1ms_task
//  Description: LED 1ms周期任务，更新呼吸状态及各LED PWM阈值
//  Param      : None
//  Return     : None
//  Others     : LED_OFF阈值为0，LED_ON使用最大亮度，LED_BREATH使用当前呼吸亮度
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void led_1ms_task(void)
{
    uint8_t i;
    //static timer_base_t _Timer;

    led_BreathCountUpdate();

    //if(!timer_base_Timing(&_Timer, 1)) return;

    for(i = 0 ; i < LED_ITEM_NUM ; i++)
    {
        switch (ledDisplay.Ctrl[i].Mode)
        {
            case LED_OFF:
                ledDisplay.Ctrl[i].Threshold = 0;
                break;
            case LED_ON:
                ledDisplay.Ctrl[i].Threshold = ledDisplay.maxbrightness;
                break;
            case LED_BREATH:
                ledDisplay.Ctrl[i].Threshold = ledDisplay.breatCount;   
                break;
            default:
                break;
        }
    }
}

/*******************************************************************************
//  Function   : led_SetDutyLevel
//  Description: 根据输入电压设置LED最大亮度等级
//  Param      : value - 输入电压值，单位与TABLE_LED_DUTY中的Voltage一致
//  Return     : None
//  Others     : 电压位于相邻档位之间时采用线性计算得到亮度值
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void led_SetDutyLevel(uint16_t value)
{
    uint8_t i;
    uint16_t temp = 0;

    for (i = 0; i < LED_DUTY_LEVEL_INDEX_MAX; i++)
    {
        if (value >= TABLE_LED_DUTY[i].Voltage)
        {
            break;
        }
    }

    if (i == LED_DUTY_4_2V)
    {
        led_SetMaxBrightness(TABLE_LED_DUTY[LED_DUTY_4_2V].Duty);
    }
    else if (i >= LED_DUTY_3_0V)
    {
        led_SetMaxBrightness(TABLE_LED_DUTY[LED_DUTY_3_0V].Duty);
    }
    else
    {
        temp = (TABLE_LED_DUTY[i - 1].Voltage - value) * (TABLE_LED_DUTY[i].Duty - TABLE_LED_DUTY[i - 1].Duty) / 100;
        led_SetMaxBrightness(TABLE_LED_DUTY[i].Duty +  temp);
    }
}


/*******************************************************************************
//  Function   : led_Display_Init
//  Description: 初始化LED显示控制模块
//  Param      : None
//  Return     : None
//  Others     : 注册各LED硬件控制函数，并设置默认最大亮度
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void led_Display_Init(void)
{
    led_register(LED_ITEM_1, GET_LED_CTRL_FUNC(LED1));
    led_register(LED_ITEM_2, GET_LED_CTRL_FUNC(LED2));
    led_register(LED_ITEM_3, GET_LED_CTRL_FUNC(LED3));
    #ifndef DEBUG_MODE
    led_register(LED_ITEM_4, GET_LED_CTRL_FUNC(LED4));
    #endif
    led_register(LED_ITEM_5, GET_LED_CTRL_FUNC(LED5));

    ledDisplay.maxbrightness = TABLE_LED_DUTY[LED_DUTY_4_2V].Duty;
}

/*******************************************************************************
//  Function   : led_All_Off
//  Description: 关闭全部LED并清除各LED输出阈值
//  Param      : None
//  Return     : None
//  Others     : 同时将所有LED模式设置为LED_OFF，并立即执行硬件关闭操作
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void led_All_Off(void)
{
    uint8_t i;

    for(i = 0 ; i < LED_ITEM_NUM ; i++)
    {
        led_SetMode((led_item_enum_t)i,LED_OFF);
        ledDisplay.Ctrl[i].Threshold = 0;
        if(ledDisplay.Ctrl[i].Func)
        {
            ledDisplay.Ctrl[i].Func(0);
        }
    }
}
