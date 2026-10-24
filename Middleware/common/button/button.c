/** 
 *********************************************************************************************************************** 
 * @file    button.c 
 * @brief   按键管理模块，实现按键扫描、消抖、短按/长按事件识别及状态查询
 * @author  Leo Chen
 * @version 1.0.0 
 * @date    2025-09-03 
 *********************************************************************************************************************** 
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved. 
 *********************************************************************************************************************** 
 */ 
 
/* Includes ----------------------------------------------------------------------------------------------------------*/ 
#include "button.h" 
#include "bsp_gpio.h" 
#include "app_sampling.h"

/* Variables ---------------------------------------------------------------------------------------------------------*/ 
button_t buttonList[BUTTON_ITEM_NUM] = {0}; 
 
/* Macro -------------------------------------------------------------------------------------------------------------*/ 
/* 按键读取函数定义 */ 
DEF_BTN_CTRL_FUNC(FAN, (LL_GPIO_ReadInputPort(FAN_SW_PORT)  & FAN_SW_PIN)) 
DEF_BTN_CTRL_FUNC(MIST,(LL_GPIO_ReadInputPort(MIST_SW_PORT) & MIST_SW_PIN)) 
DEF_BTN_CTRL_FUNC(ADC, (get_sw_val() > 3200)) 
 
 /*******************************************************************************
//  Function   : button_all_passCallback
//  Description: 按键按下回调
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
__weak void button_all_passCallback(button_item_t Item){}
/******************************************************************************* 
//  Function   : button_register 
//  Description: 注册指定按键的GPIO状态读取函数 
//  Param      : Item - 按键项目索引 
//               Func - 按键状态读取函数指针 
//  Return     : None 
//  Others     : 仅当Item索引有效时进行注册 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
static void button_register(button_item_t Item, BTN_FUNC Func) 
{ 
    if((uint8_t)Item < BUTTON_ITEM_NUM) 
    { 
        buttonList[Item].read = Func; 
    } 
} 
 
/******************************************************************************* 
//  Function   : button_1ms_task 
//  Description: 按键1ms周期扫描及消抖处理 
//  Param      : None 
//  Return     : None 
//  Others     : 按键稳定按下达到消抖时间后更新按键状态，并累计按键按下次数 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
void button_1ms_task(void) 
{ 
    uint8_t i; 
    button_t *btn; 
    for(i = 0 ; i < BUTTON_ITEM_NUM ; i++) 
    { 
        btn = &buttonList[i]; 
        if(!btn->read) 
        { 
            btn->currState = BUTTON_RELEASE; 
            btn->pressCnt  = 0; 
            continue; 
        } 
 
        if (btn->read()) 
        { 
            if(btn->pressCnt < BUTTON_DEBOUNCE_TIME) 
            { 
                btn->pressCnt ++; 
            } 
            else if(btn->pressCnt == BUTTON_DEBOUNCE_TIME) 
            { 
                btn->pressCnt++; 
                btn->currState = BUTTON_PRESS; 
                button_all_passCallback((button_item_t)i);
            } 
        } 
        else 
        { 
            if(btn->pressCnt > 0) 
            btn->pressCnt --; 
            else 
            btn->currState = BUTTON_RELEASE; 
        } 
    } 
} 
 
/******************************************************************************* 
//  Function   : button_event_Update 
//  Description: 更新并获取指定按键的短按、长按及释放事件 
//  Param      : Item         - 按键项目索引 
//               longPassTime - 长按事件触发计数阈值 
//  Return     : 当前按键事件 
//  Others     : 按下期间累计eventCnt，释放后转换为对应的释放事件 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
button_event_t button_event_Update(button_item_t Item , uint16_t longPassTime) 
{ 
    button_t *btn; 
     
    if(Item >= BUTTON_ITEM_NUM)return BUTTON_EVENT_EMPTY; 
 
    btn = &buttonList[Item]; 
     
    if ( btn->currState == BUTTON_PRESS) 
    { 
        if (btn->eventCnt < 0xffff) 
        { 
            btn->eventCnt++; 
        } 
 
        if (btn->eventCnt == 1) 
        { 
            btn->event = BUTTON_EVENT_SHORT_PRESS; 
        } 
        else if (btn->eventCnt == longPassTime) 
        { 
            btn->event = BUTTON_EVENT_LONG_PRESS_HOLD; 
        } 
    } 
    else 
    { 
        btn->eventCnt = 0; 
        if (btn->event == BUTTON_EVENT_SHORT_PRESS) 
        { 
            btn->event = BUTTON_EVENT_SHORT_PRESS_RELEASE; 
        } 
        else if (btn->event == BUTTON_EVENT_LONG_PRESS_HOLD) 
        { 
            btn->event = BUTTON_EVENT_LONG_PRESS_RELEASE; 
        } 
        else 
        { 
            btn->event = BUTTON_EVENT_EMPTY; 
        } 
    } 
    return btn->event; 
} 
 
/******************************************************************************* 
//  Function   : button_Init 
//  Description: 初始化按键管理模块 
//  Param      : None 
//  Return     : None 
//  Others     : 注册SW1、SW2、SW3及电源按键的GPIO读取函数 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
void button_Init(void) 
{ 
    button_register(BUTTON_ITEM_FAN,   GET_BTN_CTRL_FUNC(FAN)); 
    button_register(BUTTON_ITEM_MIST,   GET_BTN_CTRL_FUNC(MIST)); 
    button_register(BUTTON_ITEM_ADC,   GET_BTN_CTRL_FUNC(ADC)); 
} 

/******************************************************************************* 
//  Function   : botton_getCurrState 
//  Description: 获取指定按键当前状态 
//  Param      : Item - 按键项目索引 
//  Return     : 当前按键状态；索引无效时返回BUTTON_RELEASE 
//  Others     : None 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
button_state_t botton_getCurrState(button_item_t Item) 
{ 
    button_t *btn; 
 
    if(Item >= BUTTON_ITEM_NUM)return BUTTON_RELEASE; 
 
    btn = &buttonList[Item]; 
 
    return (button_state_t)btn->currState; 
} 

/******************************************************************************* 
//  Function   : botton_getEventCnt 
//  Description: 获取指定按键当前事件持续计数值 
//  Param      : Item - 按键项目索引 
//  Return     : 当前事件计数值；索引无效时返回0 
//  Others     : 该计数值在按键保持按下期间持续累加 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
uint16_t botton_getEventCnt(button_item_t Item) 
{ 
    button_t *btn; 
     
    if(Item >= BUTTON_ITEM_NUM)return 0; 
 
    btn = &buttonList[Item]; 
 
    return btn->eventCnt; 
} 

/******************************************************************************* 
//  Function   : botton_getEvent 
//  Description: 获取指定按键当前事件 
//  Param      : Item - 按键项目索引 
//  Return     : 当前按键事件；索引无效时返回BUTTON_EVENT_EMPTY 
//  Others     : None 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
button_event_t botton_getEvent(button_item_t Item) 
{ 
    button_t *btn; 
     
    if(Item >= BUTTON_ITEM_NUM)return BUTTON_EVENT_EMPTY; 
 
    btn = &buttonList[Item]; 
 
    return btn->event; 
} 

/******************************************************************************* 
//  Function   : botton_ClrEvent 
//  Description: 清除指定按键的当前事件及事件计数 
//  Param      : Item - 按键项目索引 
//  Return     : None 
//  Others     : 索引无效时直接返回 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
void botton_ClrEvent(button_item_t Item) 
{ 
    button_t *btn; 
     
    if(Item >= BUTTON_ITEM_NUM)return; 
 
    btn = &buttonList[Item]; 
 
    btn->event    = BUTTON_EVENT_EMPTY; 
    btn->eventCnt = 0; 
} 
/******************************************************************************* 
//  Function   : botton_ClrEvent 
//  Description: 清除指定按键的当前事件及事件计数 
//  Param      : Item - 按键项目索引 
//  Return     : None 
//  Others     : 索引无效时直接返回 
//  Author     : 陈亮 2026-06-06 
*******************************************************************************/ 
uint8_t botton_getValue(void)
{
    uint8_t i,Val = 0;
	
	for(i = 0 ; i < BUTTON_ITEM_NUM ; i++)
	{
		Val |= ((buttonList[i].currState == BUTTON_PRESS) << i);
	}
	return Val;
}
