/**
 ***********************************************************************************************************************
 * @file    fan.h
 * @brief   
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _FAN_H_
#define _FAN_H_

#include "main.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/
//#define GetBatteryVoltage()       3550 //放置报错
/* Typedef -----------------------------------------------------------------------------------------------------------*/

/*********************************************************
  * Description： 风扇模式定义
*********************************************************/
typedef enum
{
    FAN_MODE_0,
    FAN_MODE_1,
    FAN_MODE_2,
    FAN_MODE_3,
    FAN_MODE_4,
    FAN_MODE_5,
    FAN_MODE_BOOST,
    FAN_MODE_TYPE_NUM,
} fan_mode_enum_t;

/*********************************************************
  * Description： 风扇结构体定义
*********************************************************/
typedef struct
{
    fan_mode_enum_t mode;      //风扇模式
    uint32_t        nowCapVal; //当前捕获值
    uint32_t        lastCapVal;//上次捕获值
    uint32_t        pulseVal;  //脉冲间隔值
    uint32_t        timeout;   //超时计数值
    uint32_t        speed;     //风扇转速
    bool            timeoutFlg;//风扇超时标志
} fan_t;

/* Function declarations ---------------------------------------------------------------------------------------------*/

void fan_IRQCallback(void);
void fan_Init(void);
void fan_1ms_task(void);
void fan_SetSpeedMode(uint8_t mode);
fan_mode_enum_t fan_GetSpeedMode(void);
uint32_t fan_getSpeed(void);
void fan_SetTimeoutFlg(void);
void fan_CleanTimeoutFlg(void);
#endif
