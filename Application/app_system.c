/**
 ***********************************************************************************************************************
 * @file    task.c
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
#include "app_sampling.h"
#include "app_bms.h"
#include "app_display.h"

#include "bms.h"

#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "bsp_exti.h"
#include "bsp_uart1.h"
#include "bsp_pga2.h"
#include "bsp_tim1.h"
#include "bsp_tim16.h"
#include "bsp_wdg.h"
#include "bsp_tim3.h"

#include "fan.h"
#include "button.h" 
#include "timer_base.h"

#include "uart1_port.h"
/* Macro -------------------------------------------------------------------------------------------------------------*/


/* Variables ---------------------------------------------------------------------------------------------------------*/
system_t     System;              /* 系统运行状态、工作模式、计时及动画状态 */
/* External variables ------------------------------------------------------------------------------------------------*/

uint8_t rx_buffer[200];
uint8_t tx_buffer[200];

/* Function implementations ------------------------------------------------------------------------------------------*/
void system_loop_delay()
{
	static uint32_t lastDelay = 0;
	static uint32_t currDelay = 0;
	
	currDelay = getSystemMs(); 
	System.systemLoopDelay = currDelay - lastDelay;
	lastDelay = currDelay;
}

/*******************************************************************************
//  Function   : system_bsp_init
//  Description: 初始化系统使用的底层硬件驱动
//  Param      : None
//  Return     : None
//  Others     : 包括看门狗、GPIO、定时器、BMS 串口及 Modbus 串口
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_bsp_init(void)
{
    bsp_gpio_Init();
//    bsp_exti_Init();

    bsp_pga2_Init();

    // adc 采样 初始化 - 开启采样
    bsp_adc_Init();
    bsp_adc_Start();
    bsp_tim3_Init();
    bsp_tim3_Start();

    // FAN PWM 初始化
    bsp_tim1_Init();

    //雾化片 PWM 初始化
    bsp_tim16_Init();

    //串口初始化
    //bsp_uart1_Init();
}
/*******************************************************************************
//  Function   : system_data_init
//  Description: 初始化系统功能模块及通信协议模块
//  Param      : None
//  Return     : None
//  Others     : 初始化风机、参数、按键、LED、BMS 通信及 Modbus RTU 从机
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_data_init(void)
{

    fan_Init();
    button_Init();
    app_bms_Init();
    app_sampling_Init();
    s_uart1PortApi.Init(rx_buffer,200,tx_buffer,200);
	
    app_display_Init();

}
/*******************************************************************************
//  Function   : system_init
//  Description: 执行系统整体初始化
//  Param      : None
//  Return     : None
//  Others     : 按当前程序顺序依次初始化功能模块和底层硬件
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_init(void)
{    
    system_bsp_init();  
    system_data_init();
	
}
/*******************************************************************************
//  Function   : system_run
//  Description: 执行系统主循环调度任务
//  Param      : None
//  Return     : None
//  Others     : 每次调用处理通信和 Modbus；按 1ms、10ms 周期调度业务任务
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
uint32_t freq = 100000;
uint16_t duty = 200;
void system_run(void)
{
    static timer_base_t _timer_logic;

    static uint32_t old_freq = 0U;
    static uint16_t old_duty = 0U;

    if ((freq != old_freq) || (duty != old_duty))
    {
        if (bsp_tim16_ch1_pwm_set(freq, duty) == SUCCESS)
        {
            old_freq = freq;
            old_duty = duty;
        }
    }

    app_sampling_task();
	
    if(timer_base_Timing(&_timer_logic, 1))
    {
        fan_1ms_task();  
        button_1ms_task();
        app_bms_1ms_task();
        uart1_port_1msTask();
        app_display_task();
    }
	 system_loop_delay();
}


