/** 
 *********************************************************************************************************************** 
 * @file    led_display.h 
 * @brief   LED显示控制模块接口定义，包括LED开关、亮度调节、呼吸效果及PWM控制
 * @author  Leo Chen 
 * @version 1.0.0 
 * @date    2025-09-03 
 *********************************************************************************************************************** 
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved. 
 *********************************************************************************************************************** 
 */ 
#ifndef _LED_DISPLAY_H_ 
#define _LED_DISPLAY_H_ 

/* Includes ----------------------------------------------------------------------------------------------------------*/ 
 
#include "main.h" 
#include "bsp_gpio.h" 
#include "timer_base.h" 

/* Macro -------------------------------------------------------------------------------------------------------------*/ 
 
/********************************************************* 
 * Description: 定义LED硬件控制函数
 *********************************************************/ 
#define DEF_LED_CTRL_FUNC(NAME,PORT,PIN)    \
static void NAME##_setIO(uint8_t state)     \
{                                           \
    if(state)                               \
    {                                       \
        LL_GPIO_ResetOutputPin(PORT, PIN);  \
    }                                       \
    else                                    \
    {                                       \
        LL_GPIO_SetOutputPin(PORT, PIN);    \
    }                                       \
}                                           \
 
/********************************************************* 
 * Description: 获取指定LED的硬件控制函数
 *********************************************************/ 
#define GET_LED_CTRL_FUNC(NAME)  NAME##_setIO 
 
/********************************************************* 
 * Description: LED软件PWM周期计数最大值
 *********************************************************/ 
#define LED_COUNT_MAX                100  // LED软件PWM周期计数值
 
/* Typedef -----------------------------------------------------------------------------------------------------------*/ 

/*********************************************** 
 * Enum Description: LED显示模式
 ***********************************************/ 
typedef enum 
{ 
    LED_OFF = 0,  // LED关闭
    LED_ON,       // LED常亮
    LED_BREATH,   // LED呼吸
    LED_MODE_MAX 
} led_mode_enum_t; 
 
/*********************************************** 
 * Enum Description: LED呼吸亮度变化阶段
 ***********************************************/ 
typedef enum 
{ 
    LED_BREATH_DIR_DOWN = 0,  // 呼吸亮度下降阶段
    LED_BREATH_DIR_DOWN_WAIT, // 呼吸亮度下降完成等待阶段
    LED_BREATH_DIR_UP,        // 呼吸亮度上升阶段
    LED_BREATH_DIR_UP_WAIT,   // 呼吸亮度上升完成等待阶段
    LED_BREATH_DIR_MAX 
} led_breath_dir_enum_t; 
 
/*********************************************** 
 * Enum Description: 电池电压对应的LED亮度等级索引
 ***********************************************/ 
typedef enum 
{ 
    LED_DUTY_4_2V, 
    LED_DUTY_4_1V, 
    LED_DUTY_4_0V, 
    LED_DUTY_3_9V, 
    LED_DUTY_3_8V, 
    LED_DUTY_3_7V, 
    LED_DUTY_3_6V, 
    LED_DUTY_3_5V, 
    LED_DUTY_3_4V, 
    LED_DUTY_3_3V, 
    LED_DUTY_3_2V, 
    LED_DUTY_3_1V, 
    LED_DUTY_3_0V, 
    LED_DUTY_LEVEL_INDEX_MAX, 
} led_duty_level_enum_t; 
 
/*********************************************** 
 * Enum Description: LED项目编号
 ***********************************************/ 
typedef enum 
{ 
    LED_ITEM_1 = 0, 
    LED_ITEM_2, 
    LED_ITEM_3, 
    LED_ITEM_4, 
    LED_ITEM_5, 
    LED_ITEM_NUM, 
} led_item_enum_t; 
 
/*********************************************** 
 * Struct Description: 电池电压与LED亮度占空比映射数据
 ***********************************************/ 
typedef struct 
{ 
    uint16_t Voltage; 
    uint16_t Duty; 
} led_duty_t; 
 
/*********************************************** 
 * Struct Description: LED硬件控制函数类型
 ***********************************************/ 
typedef void (*LedCtrl_Func)(uint8_t State); 
 
/*********************************************** 
 * Struct Description: 单个LED控制参数
 ***********************************************/ 
typedef struct 
{ 
    LedCtrl_Func          Func;       // LED硬件控制函数
    uint8_t               Threshold;  // 软件PWM输出阈值
    led_mode_enum_t       Mode;       // LED显示模式
}led_ctrl_t; 
 
 
/*********************************************** 
 * Struct Description: LED显示管理参数
 ***********************************************/ 
typedef struct 
{ 
    led_ctrl_t            Ctrl[LED_ITEM_NUM]; 
    uint8_t               count;          // 软件PWM周期计数
    uint8_t               maxbrightness;  // LED最大亮度阈值

    // 呼吸灯控制参数
    uint8_t               breatCount;         // 当前呼吸亮度计数
    led_breath_dir_enum_t breatDir;           // 当前呼吸亮度变化阶段
    timer_base_t          breatTimer;         // 呼吸灯软件计时器
    uint16_t              breat_risingtime;   // 亮度上升时间(ms)
    uint16_t              breat_risingwait;   // 亮度上升完成等待时间(ms)
    uint16_t              breat_fallingtime;  // 亮度下降时间(ms)
    uint16_t              breat_fallingwait;  // 亮度下降完成等待时间(ms)
}led_manage_t; 
 
/* Exported functions prototypes -------------------------------------------------------------------------------------*/ 
void led_Display_Init(void); 
void led_SetMaxBrightness(uint8_t MaxBrightness); 
void led_SetMode(led_item_enum_t Item, led_mode_enum_t Mode); 
void led_Ctrl_IRQ(void); 
void led_1ms_task(void); 
void led_All_Off(void); 
void led_SetDutyLevel(uint16_t value); 

#endif
