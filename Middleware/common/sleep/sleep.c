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
 
/* 系统正常休眠延时时间 */
#define SYSTEM_SLEEP_DELAY_TIME         (2000U) 

/* AHB 上仅关闭外设时钟，Flash/SRAM 时钟必须保持开启。 */
#define SLEEP_AHB1_PERIPH_CLOCKS         (LL_AHB1_GRP1_PERIPH_DMA1 | LL_AHB1_GRP1_PERIPH_CRC)
 
/* Variables ---------------------------------------------------------------------------------------------------------*/ 
sleep_t sleep_ctrl; 
 

/*******************************************************************************
//  Function   : bms_connect_task
//  Description: BMS通信连接维护任务
//  Param      : None
//  Return     : None
//  Others     : BMS通信断开后按照固定时间间隔主动唤醒BMS
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static bool sleep_enter(void)
{
    if(System.logic.power == 0 && System.logic.usb_present == 0)
    {
        return true;
    }

    return false;
}


/*******************************************************************************
//  Function   : bms_connect_task
//  Description: BMS通信连接维护任务
//  Param      : None
//  Return     : None
//  Others     : BMS通信断开后按照固定时间间隔主动唤醒BMS
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void sleep_1ms_task(void) 
{ 
    if (!sleep_enter())
    {
        sleep_ctrl.count = 0;
        return;
    }

    sleep_ctrl.count ++;

    if(sleep_ctrl.count > SYSTEM_SLEEP_DELAY_TIME)
    {
        gpio_sleep_open();
        bsp_adc_Deinit();
        bsp_pga2_Deinit();
        bsp_tim1_Deinit();
        bsp_tim3_Deinit();
        bsp_tim16_Deinit();
        fan_protocol_Deinit();

        bsp_rcc_SystemStop();

        bsp_gpio_Init();
        bsp_pga2_Init();
        bsp_adc_Init();
        bsp_adc_Start();
        bsp_tim3_Init();
        bsp_tim3_Start();
        bsp_tim1_Init();
        bsp_tim16_Init();
        fan_protocol_Init();

        sleep_ctrl.count = 0;
    }
} 
