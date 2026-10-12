/** 
 *********************************************************************************************************************** 
 * @file    button.h 
 * @brief   按键管理模块接口定义，包括按键状态读取、消抖处理及按键事件识别
 * @author  Leo Chen 
 * @version 1.0.0 
 * @date    2026-08-02 
 *********************************************************************************************************************** 
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved. 
 *********************************************************************************************************************** 
 */ 
#ifndef _BUTTON_H_ 
#define _BUTTON_H_ 
#include "main.h" 
 
/* Macro -------------------------------------------------------------------------------------------------------------*/ 

/* 按键消抖计数时间 */ 
#define BUTTON_DEBOUNCE_TIME            (40)  // 按键消抖计数阈值 
 
/********************************************************* 
 * Description: 定义按键GPIO状态读取函数
 *********************************************************/ 
#define DEF_BTN_CTRL_FUNC(NAME,LOGIC)   \
static button_state_t NAME##_getIO()    \
{                                       \
    if(LOGIC)                           \
    {                                   \
        return BUTTON_RELEASE;          \
    }                                   \
    else                                \
    {                                   \
        return BUTTON_PRESS;            \
    }                                   \
}                                       \
 
/********************************************************* 
 * Description: 获取指定按键的GPIO状态读取函数
 *********************************************************/ 
#define GET_BTN_CTRL_FUNC(NAME)  NAME##_getIO 
 
/********************************************************* 
 * Description: 按键物理状态
 *********************************************************/ 
typedef enum 
{ 
	BUTTON_RELEASE = 0, 
	BUTTON_PRESS 
} button_state_t; 
 
 
/********************************************************* 
 * Description: 按键项目编号
 *********************************************************/ 
typedef enum 
{ 
	BUTTON_ITEM_SW1 = 0, 
	BUTTON_ITEM_SW2, 
	BUTTON_ITEM_ADC, 
	BUTTON_ITEM_NUM 
} button_item_t; 
 
/********************************************************* 
 * Description: 按键事件类型
 *********************************************************/ 
typedef enum 
{ 
	BUTTON_EVENT_EMPTY, 
	BUTTON_EVENT_SHORT_PRESS, 
	BUTTON_EVENT_SHORT_PRESS_RELEASE, 
	BUTTON_EVENT_LONG_PRESS_HOLD, 
	BUTTON_EVENT_LONG_PRESS_RELEASE, 
} button_event_t; 
 
/********************************************************* 
 * Description: 按键GPIO状态读取函数类型
 *********************************************************/ 
typedef button_state_t (*BTN_FUNC)(); 

/********************************************************* 
 * Description: 按键状态及事件管理数据
 *********************************************************/ 
typedef struct 
{ 
		uint16_t        currState : 1;    // 当前按键状态 
		uint16_t        pressCnt  : 15;   // 按键消抖计数值 
		uint16_t        eventCnt  : 16;   // 按键事件持续计数值 
		button_event_t  event;            // 当前按键事件 
		BTN_FUNC        read;             // 按键GPIO状态读取函数 
} button_t; 
 
/* Typedef -----------------------------------------------------------------------------------------------------------*/ 
 
 
/* Function declarations ---------------------------------------------------------------------------------------------*/ 
void button_Init(void); 
void button_1ms_task(void); 
uint8_t botton_getValue(void);
void botton_ClrEvent(button_item_t Item); 
uint16_t botton_getEventCnt(button_item_t Item); 
button_event_t botton_getEvent(button_item_t Item); 
button_state_t botton_getCurrState(button_item_t Item); 
button_event_t button_event_Update(button_item_t Item , uint16_t longPassTime); 
#endif 
