
/**
 ***********************************************************************************************************************
 * @file    app_system.c
 * @brief   系统初始化及主循环管理，实现风扇、雾化、BMS、显示、通信及低功耗任务调度
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
#include "app_protect.h"
#include "app_display.h"
#include "app_logic.h"
#include "app_mist.h"

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
#include "bsp_flash.h"

#include "fan.h"
#include "sleep.h"
#include "button.h"
#include "timer_base.h"

#include "uart1_port.h"
#include "fan_protocol.h"
#include "bootloader.h"
#include "crc.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief 系统全局运行状态
 * @note  保存设备运行模式、控制参数、固件信息及主循环调度状态。
 */
system_t System;


/* Debug variables ---------------------------------------------------------------------------------------------------*/


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : system_loop_delay
//  Description: 统计系统主循环相邻两次执行的时间间隔
//  Param      : None
//  Return     : None
//  Others     : 使用系统毫秒计时器获取当前时间；
//               计算相邻两次调用之间的时间差；
//               将结果保存至 System.systemLoopDelay，单位 ms；
//               uint32_t 无符号减法支持计数器自然回绕
//  Author     : Chen Liang 2026-10-26
*******************************************************************************/
static void system_loop_delay(void)
{
    static uint32_t lastDelay = 0U;
    uint32_t currDelay;

    /* 获取当前系统毫秒计数。 */
    currDelay = getSystemMs();

    /* 计算相邻两次主循环执行的时间间隔。 */
    System.systemLoopDelay = (uint32_t)(currDelay - lastDelay);

    /* 保存当前时间，供下一次调用计算。 */
    lastDelay = currDelay;
}


/*******************************************************************************
//  Function   : Unique_ID_read
//  Description: 读取 MCU 唯一设备标识 UID
//  Param      : None
//  Return     : None
//  Others     : 通过底层 Flash 接口读取芯片 UID；
//               将读取结果保存至 System.unique_id；
//               可用于通信协议的设备信息应答
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
static void Unique_ID_read(void)
{
    /* 读取芯片唯一标识并保存至系统状态。 */
    bsp_flash_ReadUID(System.unique_id);
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : system_bsp_init
//  Description: 初始化系统底层硬件驱动模块
//  Param      : None
//  Return     : None
//  Others     : 依次初始化 GPIO、EXTI、PGA2、ADC、TIM3、TIM1 和 TIM16；
//               配置按键及 USB_IN 的外部事件唤醒功能；
//               ADC 与 TIM3 初始化完成后立即启动；
//               TIM1 用于风机 PWM，TIM16 用于雾化片 PWM
//  Author     : Chen Liang 2026-06-06
*******************************************************************************/
void system_bsp_init(void)
{
    /* 初始化板级 GPIO 及默认输出状态。 */
    bsp_gpio_Init();

    /* 初始化电源按键及 USB_IN 外部事件唤醒。 */
    bsp_exti_Init();

    /* 初始化 PGA2 模拟信号放大模块。 */
    bsp_pga2_Init();

    /* 初始化 ADC 并启动采样。 */
    bsp_adc_Init();
    bsp_adc_Start();

    /* 初始化 TIM3 并启动定时器。 */
    bsp_tim3_Init();
    bsp_tim3_Start();

    /* 初始化风机 PWM 定时器 TIM1。 */
    bsp_tim1_Init();

    /* 初始化雾化片 PWM 定时器 TIM16。 */
    bsp_tim16_Init();
}


/*******************************************************************************
//  Function   : system_data_init
//  Description: 初始化系统应用层功能模块
//  Param      : None
//  Return     : None
//  Others     : 读取 MCU UID 及固件编译时间；
//               初始化风机、按键、BMS、ADC 采样及显示模块；
//               初始化 UART1 通信协议；
//               初始化雾化片 PWM 控制及自动追频模块
//  Author     : Chen Liang 2026-06-06
*******************************************************************************/
void system_data_init(void)
{
    /* 初始化风机控制模块。 */
    fan_Init();

    /* 初始化按键检测及事件处理模块。 */
    button_Init();

    /* 初始化电池管理应用模块。 */
    app_bms_Init();

    /* 初始化 ADC 采样滤波及缓存管理模块。 */
    app_sampling_Init();

    /* 初始化 AiP650E 显示管理模块。 */
    app_display_Init();

    /* 初始化 UART1 风扇通信协议模块。 */
    fan_protocol_Init();

    /* 初始化雾化片自动追频及 PWM 控制模块。 */
    app_mist_Init();
}



/*******************************************************************************
//  Function   : system_init
//  Description: 执行系统整体初始化
//  Param      : None
//  Return     : None
//  Others     : 先初始化 BSP 底层硬件；
//               再初始化应用层功能模块及通信协议；
//               确保系统主循环运行前完成基础初始化
//  Author     : Chen Liang 2026-06-06
*******************************************************************************/
void system_init(void)
{

    /* 读取芯片 UID。 */
    Unique_ID_read();

    /* 初始化板级硬件驱动。 */
    system_bsp_init();

    /* 初始化系统应用层功能模块。 */
    system_data_init();
	
	/* 计算 Application Flash 区域软件校验码。 */
    System.app_flash_crc  = CRC32_IEEE_Calculate((const uint8_t *)APP_BASE_ADDRESS,APP_MAX_SIZE);

    /* 计算 Bootloader Flash 区域软件校验码。 */
    System.boot_flash_crc = CRC32_IEEE_Calculate((const uint8_t *)MCU_BASE_ADDRESS,APP_FLASH_OFFSET);
}


/*******************************************************************************
//  Function   : system_run
//  Description: 执行系统主循环及 1ms 周期任务调度
//  Param      : None
//  Return     : None
//  Others     : 主循环持续处理 ADC 数据及 USB 电压突降检测；
//               每 1ms 执行风机、按键、BMS、通信、显示、
//               应用逻辑、雾化追频及低功耗管理任务；
//               更新主循环间隔统计；
//               根据调试控制标志更新电池和 USB 电源通路
//  Author     : Chen Liang 2026-11-04
*******************************************************************************/
void system_run(void)
{
    static timer_base_t _timer_logic = {0};

    /* 处理 ADC 采样缓存及滤波结果。 */
    app_sampling_task();

    /*  */
    app_logic_fan();

    /* 按 1ms 周期调度系统任务。 */
    if (timer_base_Timing(&_timer_logic, 1U))
    {
        /* 更新风机 PWM、档位及转速反馈。 */
        fan_1ms_task();

        /* 执行按键扫描及事件处理。 */
        button_1ms_task();

        /* 更新 BMS 状态、SOC/SOH 及保护逻辑。 */
        app_bms_1ms_task();

        /* 更新 AiP650E 显示内容及硬件状态。 */
        app_display_task();

        /* 执行系统应用层功能控制逻辑。 */
        app_logic_1ms_task();
		
		/* 系统保护控制逻辑。 */
		app_protect_task();

        /* 处理 UART1 协议接收、解析及应答。 */
        fan_protocol_1ms_task();

        /* 更新雾化片 PWM 输出及自动追频状态。 */
        app_mist_1ms_task();

        /* 执行系统低功耗状态管理及休眠控制。 */
        sleep_1ms_task();
    }

    /* 统计系统主循环相邻两次执行的时间间隔。 */
    system_loop_delay();

    app_io_ctrl_task();

}

