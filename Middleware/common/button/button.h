/**
 ***********************************************************************************************************************
 * @file    button.h
 * @brief   按键管理模块接口定义，实现按键状态读取、消抖处理及按键事件识别
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

/** 按键消抖时间，单位 ms */
#define BUTTON_DEBOUNCE_TIME            (40)


/**
 * @brief 定义按键 GPIO 状态读取函数
 *
 * LOGIC 为高时返回 BUTTON_RELEASE，为低时返回 BUTTON_PRESS。
 */
#define DEF_BTN_CTRL_FUNC(NAME, LOGIC)  \
static button_state_t NAME##_getIO(void)\
{                                       \
    if (LOGIC)                          \
    {                                   \
        return BUTTON_RELEASE;          \
    }                                   \
    else                                \
    {                                   \
        return BUTTON_PRESS;            \
    }                                   \
}


/** 获取指定按键对应的 GPIO 状态读取函数 */
#define GET_BTN_CTRL_FUNC(NAME)         NAME##_getIO


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 按键物理状态
 */
typedef enum
{
    BUTTON_RELEASE = 0,     /**< 按键释放 */
    BUTTON_PRESS            /**< 按键按下 */
} button_state_t;


/**
 * @brief 按键编号
 */
typedef enum
{
    BUTTON_ITEM_SW1 = 0,    /**< SW1 按键 */
    BUTTON_ITEM_SW2,        /**< SW2 按键 */
    BUTTON_ITEM_ADC,        /**< ADC 按键 */
    BUTTON_ITEM_NUM         /**< 按键总数量 */
} button_item_t;


/**
 * @brief 按键事件类型
 */
typedef enum
{
    BUTTON_EVENT_EMPTY = 0,             /**< 无按键事件 */
    BUTTON_EVENT_SHORT_PRESS,           /**< 短按事件 */
    BUTTON_EVENT_SHORT_PRESS_RELEASE,   /**< 短按释放事件 */
    BUTTON_EVENT_LONG_PRESS_HOLD,       /**< 长按保持事件 */
    BUTTON_EVENT_LONG_PRESS_RELEASE     /**< 长按释放事件 */
} button_event_t;


/** 按键 GPIO 状态读取函数类型 */
typedef button_state_t (*BTN_FUNC)(void);


/**
 * @brief 按键运行状态及事件管理数据
 */
typedef struct
{
    uint16_t currState : 1;     /**< 当前按键状态 */
    uint16_t pressCnt  : 15;    /**< 按键消抖计数值 */
    uint16_t eventCnt  : 16;    /**< 按键事件持续计数值 */
    button_event_t event;       /**< 当前按键事件 */
    BTN_FUNC read;              /**< 按键 GPIO 状态读取函数 */
} button_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : button_Init
//  Description: 初始化按键管理模块
//  Param      : None
//  Return     : None
//  Others     : 初始化各按键状态、计数值及 GPIO 读取接口
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
void button_Init(void);


/*******************************************************************************
//  Function   : button_1ms_task
//  Description: 按键 1ms 周期处理任务
//  Param      : None
//  Return     : None
//  Others     : 完成按键状态读取、消抖及事件持续时间统计
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
void button_1ms_task(void);


/*******************************************************************************
//  Function   : botton_getValue
//  Description: 获取按键相关状态值
//  Param      : None
//  Return     : 当前按键状态值
//  Others     : 具体返回含义由 button.c 实现决定
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
uint8_t botton_getValue(void);


/*******************************************************************************
//  Function   : botton_ClrEvent
//  Description: 清除指定按键事件
//  Param      : Item - 按键编号
//  Return     : None
//  Others     : 清除对应按键当前事件状态
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
void botton_ClrEvent(button_item_t Item);


/*******************************************************************************
//  Function   : botton_getEventCnt
//  Description: 获取指定按键事件持续计数值
//  Param      : Item - 按键编号
//  Return     : 当前事件持续计数值
//  Others     : 计数单位由 button_1ms_task() 调用周期决定
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
uint16_t botton_getEventCnt(button_item_t Item);


/*******************************************************************************
//  Function   : botton_getEvent
//  Description: 获取指定按键当前事件
//  Param      : Item - 按键编号
//  Return     : 当前 button_event_t 按键事件
//  Others     : None
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
button_event_t botton_getEvent(button_item_t Item);


/*******************************************************************************
//  Function   : botton_getCurrState
//  Description: 获取指定按键当前物理状态
//  Param      : Item - 按键编号
//  Return     : 当前按键状态 BUTTON_PRESS / BUTTON_RELEASE
//  Others     : 返回消抖后的按键状态
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
button_state_t botton_getCurrState(button_item_t Item);


/*******************************************************************************
//  Function   : button_event_Update
//  Description: 更新并获取指定按键事件
//  Param      : Item         - 按键编号
//               longPassTime - 长按判定时间
//  Return     : 当前按键事件
//  Others     : 根据当前按键状态及事件持续时间判断短按、长按及释放事件
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
button_event_t button_event_Update(button_item_t Item, uint16_t longPassTime);


#endif /* _BUTTON_H_ */
