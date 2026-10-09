/** 
 *********************************************************************************************************************** 
 * @file    sleep.c 
 * @brief   系统休眠管理模块，实现休眠状态控制、BMS唤醒及低功耗进入/退出处理
 * @author  Su Jiawei 
 * @version 1.0.0 
 * @date    2025-09-03 
 *********************************************************************************************************************** 
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved. 
 *********************************************************************************************************************** 
 */ 
 
/* Includes ----------------------------------------------------------------------------------------------------------*/ 
 
#include "main.h" 
#include "bsp_bms_usart.h" 
#include "bsp_gpio.h" 
#include "bsp_rcc.h" 
#include "bsp_timer.h" 
#include "button.h" 
#include "bms.h" 
#include "led_display.h" 
#include "sleep.h" 
 
/* Macro -------------------------------------------------------------------------------------------------------------*/ 
 
/* 系统正常休眠延时时间 */
#define SYSTEM_SLEEP_DELAY_TIME         (9400U) 

/* 快速休眠延时时间 */
#define SYSTEM_QUICK_SLEEP_DELAY_TIME   (9000U) 

/* BMS断开后主动唤醒间隔时间 */
#define ACTIVE_BMS_INTERVAL_TIME        (2000U) 

/* 低电压状态下按键IO下拉阈值 */
#define BATTERY_VOLTAGE_LOW_THRESHOLD   (2900) 
 
/* Variables ---------------------------------------------------------------------------------------------------------*/ 
 
sleep_t Sleep; 
 
/*******************************************************************************
//  Function   : sleep_SetState
//  Description: 设置系统休眠状态
//  Param      : state - 目标休眠状态
//  Return     : None
//  Others     : 状态发生变化时清零休眠计数
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void sleep_SetState(sleep_state_t state) 
{ 
    if (state >= SLEEP_STATE_MAX) 
    { 
        return; 
    } 
 
    if (Sleep.state != state) 
    { 
        Sleep.sleepCnt = 0; 
        Sleep.state    = state; 
    } 
} 
 
/*******************************************************************************
//  Function   : sleep_getState
//  Description: 获取当前系统休眠状态
//  Param      : None
//  Return     : 当前系统休眠状态
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
sleep_state_t sleep_getState(void) 
{ 
    return Sleep.state; 
} 
 
/*******************************************************************************
//  Function   : sleep_ClrCnt
//  Description: 清零系统休眠计数
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void sleep_ClrCnt(void) 
{ 
    Sleep.sleepCnt = 0; 
} 

/*******************************************************************************
//  Function   : sleep_ClrCnt
//  Description: 清零系统休眠计数
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
bool sleep_close_attachment(void) 
{ 
    return Sleep.sleepCnt >= SYSTEM_QUICK_SLEEP_DELAY_TIME / 2; 
} 

/*******************************************************************************
//  Function   : bms_active
//  Description: 主动唤醒BMS并恢复BMS串口接收
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static void bms_active(void)
{
    bsp_bms_usart_Deinit();
    bsp_bms_usart_rx_OutputPulse();
    bsp_bms_usart_Init();
    bsp_bms_usart_HalfDuplexRx();
    bsp_bms_usart_ReceiveStart();
}
 
/*******************************************************************************
//  Function   : sleep_QuickSleep
//  Description: 触发系统快速休眠
//  Param      : None
//  Return     : None
//  Others     : 设置快速休眠延时，并将系统状态切换为进入休眠
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void sleep_QuickSleep(void) 
{ 
    Sleep.sleepCnt      = SYSTEM_QUICK_SLEEP_DELAY_TIME; 
    Sleep.state         = ENTER_SLEEP; 
    Sleep.quickSleepflg = true; // 快速休眠标志
} 
 
/*******************************************************************************
//  Function   : sleep_ClrQuickFlg
//  Description: 清除快速休眠标志
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void sleep_ClrQuickFlg(void) 
{ 
    Sleep.quickSleepflg = false; // 清除快速休眠标志
} 

/*******************************************************************************
//  Function   : SetInputPullDown
//  Description: 将指定GPIO配置为输入下拉模式
//  Param      : port - GPIO端口
//               pin  - GPIO引脚
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static void SetInputPullDown(GPIO_TypeDef *port, uint32_t pin) 
{ 
    LL_GPIO_SetPinMode(port, pin, LL_GPIO_MODE_INPUT); 
    LL_GPIO_SetPinPull(port, pin, LL_GPIO_PULL_DOWN); 
} 
 
/*******************************************************************************
//  Function   : SetInputPullUp
//  Description: 将指定GPIO配置为输入上拉模式
//  Param      : port - GPIO端口
//               pin  - GPIO引脚
//  Return     : None
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static void SetInputPullUp(GPIO_TypeDef *port, uint32_t pin) 
{ 
    LL_GPIO_SetPinMode(port, pin, LL_GPIO_MODE_INPUT); 
    LL_GPIO_SetPinPull(port, pin, LL_GPIO_PULL_UP); 
} 
 
/*******************************************************************************
//  Function   : SwitchPinPullDown
//  Description: 系统进入低功耗前调整按键GPIO下拉状态
//  Param      : None
//  Return     : None
//  Others     : 电源按键按下时配置SW4下拉；低电压时所有按键配置为下拉
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static void SwitchPinPullDown(void) 
{ 
    if (botton_getCurrState(BUTTON_ITEM_POWER) == BUTTON_PRESS) 
    { 
        SetInputPullDown(SW4_PORT, SW4_PIN); 
    } 
 
    if (GetBatteryVoltage() <= BATTERY_VOLTAGE_LOW_THRESHOLD) 
    { 
        SetInputPullDown(SW1_PORT, SW1_PIN); 
        SetInputPullDown(SW2_PORT, SW2_PIN); 
        SetInputPullDown(SW3_PORT, SW3_PIN); 
        SetInputPullDown(SW4_PORT, SW4_PIN); 
    } 
} 
 
/*******************************************************************************
//  Function   : SwitchPinPullUp
//  Description: 系统退出低功耗后恢复按键GPIO上拉状态
//  Param      : None
//  Return     : None
//  Others     : 恢复SW1、SW2、SW3、SW4为输入上拉模式
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static void SwitchPinPullUp(void) 
{ 
    SetInputPullUp(SW1_PORT, SW1_PIN); 
    SetInputPullUp(SW2_PORT, SW2_PIN); 
    SetInputPullUp(SW3_PORT, SW3_PIN); 
    SetInputPullUp(SW4_PORT, SW4_PIN); 
} 
 
/*******************************************************************************
//  Function   : bms_connect_task
//  Description: BMS通信连接维护任务
//  Param      : None
//  Return     : None
//  Others     : BMS通信断开后按照固定时间间隔主动唤醒BMS
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void bms_connect_task(void) 
{ 
    if (!IsCommunicationDisconnected())
    {
        Sleep.bmsCnt = 0;
        return;
    }

    if (Sleep.bmsCnt == 0) 
    { 
        Sleep.bmsCnt = ACTIVE_BMS_INTERVAL_TIME; 
        bms_active(); 
    } 
 
    if (Sleep.bmsCnt) 
    { 
        Sleep.bmsCnt--; 
    } 
} 
 
/*******************************************************************************
//  Function   : system_sleep_task
//  Description: 系统休眠处理任务
//  Param      : None
//  Return     : None
//  Others     : 达到休眠时间后关闭相关外设进入STOP模式，唤醒后重新初始化外设
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_sleep_task() 
{ 
    if (Sleep.state != ENTER_SLEEP) 
    { 
        Sleep.sleepCnt = 0; 
        return; 
    } 
 
    if (++Sleep.sleepCnt >= SYSTEM_SLEEP_DELAY_TIME) 
    { 
        Sleep.sleepCnt = 0; 

        bsp_bms_usart_ReceiveStop(); 
        bsp_bms_usart_Deinit(); 
        bsp_timer_Deinit(); 

        led_All_Off(); 
        SwitchPinPullDown(); 
 
        bsp_rcc_SystemStop(); 
 
        SwitchPinPullUp(); 
        bsp_timer_Init(); 
        sleep_ClrQuickFlg(); 
        bsp_bms_usart_rx_OutputPulse(); 
        bsp_bms_usart_Init(); 
        bsp_bms_usart_HalfDuplexRx(); 
        bsp_bms_usart_ReceiveStart(); 
    } 
} 
 
/*******************************************************************************
//  Function   : sleep_1ms_task
//  Description: 系统休眠管理周期任务 - 1ms
//  Param      : None
//  Return     : None
//  Others     : 执行BMS连接维护，非DEBUG模式下同时执行系统休眠处理
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void sleep_1ms_task(void) 
{ 
    bms_connect_task(); 
#ifndef DEBUG_MODE 
    system_sleep_task(); 
#endif 
}
