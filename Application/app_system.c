
/**
 ***********************************************************************************************************************
 * @file    app_system.c
 * @brief   系统初始化及主循环调度，管理风机、按键、BMS、显示、通信及应用逻辑
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
#include "app_logic.h"

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

/** 系统全局运行状态，保存设备状态、工作模式及调度相关信息 */
system_t System;

/** UART1 接收数据缓冲区 */
uint8_t rx_buffer[200];

/** UART1 发送数据缓冲区 */
uint8_t tx_buffer[200];


/* Debug variables ---------------------------------------------------------------------------------------------------*/

/** 电池供电通路控制标志，true=开启，false=关闭 */
bool vbat_ctrl = true;

/** USB 输入通路控制标志，true=开启，false=关闭 */
bool vin_ctrl = true;


/* Function implementations ------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : system_loop_delay
//  Description: 统计系统主循环相邻两次执行的时间间隔
//  Param      : None
//  Return     : None
//  Others     : 使用系统毫秒计时器计算主循环间隔，
//               并更新 System.systemLoopDelay，单位 ms
//  Author     : Chen Liang 2026-10-26
*******************************************************************************/
void system_loop_delay(void)
{
    static uint32_t lastDelay = 0U;
    static uint32_t currDelay = 0U;

    /* 获取当前系统时间并计算相邻两次主循环的时间间隔。 */
    currDelay = getSystemMs();

    System.systemLoopDelay = (uint32_t)(currDelay - lastDelay);

    /* 保存本次时间，供下一次计算使用。 */
    lastDelay = currDelay;
}


/*******************************************************************************
//  Function   : system_bsp_init
//  Description: 初始化系统使用的底层硬件驱动
//  Param      : None
//  Return     : None
//  Others     : 初始化 GPIO、PGA2、ADC、TIM3 采样定时器、
//               TIM1 风机 PWM 及 TIM16 雾化片 PWM
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_bsp_init(void)
{
    /* 初始化系统 GPIO。 */
    bsp_gpio_Init();

    /* 外部中断初始化，当前暂未启用。 */
    // bsp_exti_Init();

    /* 初始化 PGA2 模拟放大器。 */
    bsp_pga2_Init();

    /* 初始化 ADC 并启动采样。 */
    bsp_adc_Init();
    bsp_adc_Start();

    /* 初始化 TIM3 并启动定时器。 */
    bsp_tim3_Init();
    bsp_tim3_Start();

    /* 初始化风扇 PWM 定时器。 */
    bsp_tim1_Init();

    /* 初始化雾化片 PWM 定时器。 */
    bsp_tim16_Init();

    /* UART1 底层初始化由通信端口模块管理。 */
    // bsp_uart1_Init();
}


/*******************************************************************************
//  Function   : system_data_init
//  Description: 初始化系统应用层功能模块
//  Param      : None
//  Return     : None
//  Others     : 初始化风扇、按键、BMS、ADC 滤波、UART1 通信及显示模块
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_data_init(void)
{
    /* 初始化风扇控制模块。 */
    fan_Init();

    /* 初始化按键处理模块。 */
    button_Init();

    /* 初始化电池管理应用模块。 */
    app_bms_Init();

    /* 初始化 ADC 采样滤波模块。 */
    app_sampling_Init();

    /* 初始化 UART1 通信端口及收发缓冲区。 */
    s_uart1PortApi.Init(rx_buffer, 200, tx_buffer, 200);

    /* 初始化 AiP650E 显示模块。 */
    app_display_Init();
}


/*******************************************************************************
//  Function   : system_init
//  Description: 执行系统整体初始化
//  Param      : None
//  Return     : None
//  Others     : 先初始化底层硬件，再初始化应用层功能模块
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_init(void)
{
    system_bsp_init();
    system_data_init();
}


/*******************************************************************************
//  Function   : system_run
//  Description: 执行系统主循环及周期任务调度
//  Param      : None
//  Return     : None
//  Others     : 实时处理雾化片 PWM 配置及 ADC 滤波；
//               按 1ms 周期调度风扇、按键、BMS、UART1、显示及应用逻辑任务；
//               同时更新主循环时间间隔及电源通路控制 GPIO
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_run(void)
{
    static timer_base_t _timer_logic;

    /* 处理 ADC 采样滤波及相关状态更新。 */
    app_sampling_task();

    /* 执行 1ms 周期任务。 */
    if (timer_base_Timing(&_timer_logic, 1))
    {
        /* 风扇档位、PWM 渐变及转速反馈处理。 */
        fan_1ms_task();

        /* 按键采样及事件处理。 */
        button_1ms_task();

        /* BMS 电池采样、SOC 及相关控制处理。 */
        app_bms_1ms_task();

        /* UART1 通信端口周期处理。 */
        uart1_port_1msTask();

        /* AiP650E 显示逻辑及硬件刷新。 */
        app_display_task();

        /* 应用层设备运行逻辑。 */
        add_logic_1ms_task();
    }

    /* 更新主循环执行间隔。 */
    system_loop_delay();

    /* 根据电池通路控制标志更新 VBAT_EN。 */
    if (vbat_ctrl)
    {
        LL_GPIO_SetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    }

    /* 根据 USB 输入通路控制标志更新 VIN_EN。 */
    if (vin_ctrl)
    {
        LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    }
}
