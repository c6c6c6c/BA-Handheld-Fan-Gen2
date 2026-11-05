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
#include "app_system.h"
#include "sleep.h" 
#include "bms.h"

#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "bsp_exti.h"
#include "bsp_uart1.h"
#include "bsp_pga2.h"
#include "bsp_tim1.h"
#include "bsp_tim16.h"
#include "bsp_tim3.h"
#include "bsp_rcc.h"

#include "fan.h"
#include "button.h"
#include "timer_base.h"

#include "fan_protocol.h"
/* Macro -------------------------------------------------------------------------------------------------------------*/

/* 满足休眠条件后，延迟进入低功耗的时间，单位 ms。 */
#define SYSTEM_SLEEP_DELAY_TIME    (3000U)

/* Variables ---------------------------------------------------------------------------------------------------------*/

sleep_t sleep_ctrl;


/**
 * @brief 判断当前是否允许进入休眠
 *
 * @retval true  系统已关机，且未检测到 USB 接入
 * @retval false 系统运行中，或已检测到 USB 接入
 */
static bool sleep_can_enter(void)
{

    return (System.logic.power == 0U) && (System.logic.usb_present == 0U);
}


/**
 * @brief 系统休眠管理任务，每 1 ms 调用一次
 *
 * @note 连续满足休眠条件达到指定时间后，关闭相关外设并进入低功耗。
 *       唤醒返回后，重新初始化相关外设。
 */
void sleep_1ms_task(void)
{
    if (!sleep_can_enter())
    {
        sleep_ctrl.count = 0U;
        return;
    }

    if (sleep_ctrl.count < SYSTEM_SLEEP_DELAY_TIME)
    {
        ++sleep_ctrl.count;
    }

    if (sleep_ctrl.count < SYSTEM_SLEEP_DELAY_TIME)
    {
        return;
    }

    sleep_ctrl.count = 0U;

    /* 配置休眠期间的 GPIO 状态及唤醒引脚。 */
    gpio_sleep_open();

    /* 关闭采样、定时器及通信模块。 */
    bsp_adc_Deinit();
    bsp_pga2_Deinit();
    bsp_tim1_Deinit();
    bsp_tim3_Deinit();
    bsp_tim16_Deinit();
    fan_protocol_Deinit();

    /*
     * 进入低功耗，唤醒后继续执行。
     * 外设恢复前，应确保系统时钟及时间基准已恢复。
     */
    bsp_rcc_SystemStop();

    /* 恢复 GPIO 和采样模块。 */
    bsp_gpio_Init();
    bsp_pga2_Init();
    bsp_adc_Init();
    bsp_adc_Start();

    /* 恢复定时器及通信模块。 */
    bsp_tim3_Init();
    bsp_tim3_Start();
    bsp_tim1_Init();
    bsp_tim16_Init();
    fan_protocol_Init();
}
