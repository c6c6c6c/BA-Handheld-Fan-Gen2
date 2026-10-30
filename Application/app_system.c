
/**
 ***********************************************************************************************************************
 * @file    app_system.c
 * @brief   系统初始化及主循环调度，管理风扇、按键、BMS、显示、通信及应用逻辑
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
#include "bsp_flash.h"

#include "fan.h"
#include "button.h"
#include "timer_base.h"

#include "uart1_port.h"
#include "fan_protocol.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** 系统全局运行状态，保存设备状态、故障信息及调度参数 */
system_t System;


/* Debug variables ---------------------------------------------------------------------------------------------------*/

/** 电池供电通路控制标志，true=开启，false=关闭 */
bool vbat_ctrl = true;

/** USB 输入通路控制标志，true=开启，false=关闭 */
bool vin_ctrl = true;


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : system_loop_delay
//  Description: 统计系统主循环相邻两次执行的时间间隔
//  Param      : None
//  Return     : None
//  Others     : 使用系统毫秒计时器计算相邻两次主循环的时间差，
//               并更新 System.systemLoopDelay，单位 ms
//  Author     : Chen Liang 2026-10-26
*******************************************************************************/
static void system_loop_delay(void)
{
    static uint32_t lastDelay = 0U;
    uint32_t currDelay;

    /* 获取当前系统时间。 */
    currDelay = getSystemMs();

    /* 计算主循环执行间隔。 */
    System.systemLoopDelay = (uint32_t)(currDelay - lastDelay);

    /* 保存本次时间，供下一次计算使用。 */
    lastDelay = currDelay;
}


/*******************************************************************************
//  Function   : Unique_ID_read
//  Description: 读取 MCU 唯一设备标识 UID
//  Param      : None
//  Return     : None
//  Others     : 通过 Flash 驱动读取芯片 UID，
//               并将结果保存至 System.unique_id
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
static void Unique_ID_read(void)
{
    /* 读取芯片唯一设备标识。 */
    bsp_flash_ReadUID(System.unique_id);
}


/*******************************************************************************
//  Function   : compile_prarm_read
//  Description: 解析并保存固件编译日期及编译时间
//  Param      : None
//  Return     : None
//  Others     : 使用编译器提供的 __DATE__ 和 __TIME__ 宏；
//               __DATE__ 格式为 Mmm dd yyyy；
//               __TIME__ 格式为 hh:mm:ss；
//               解析结果保存至 System.compile_param
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
static void compile_prarm_read(void)
{
    const char compile_date[] = __DATE__;
    const char compile_time[] = __TIME__;
    uint8_t month = 0U;

    /* 根据月份英文缩写解析月份。 */
    switch (compile_date[0])
    {
        case 'J':
            if (compile_date[1] == 'a')
            {
                month = 1U;
            }
            else
            {
                month = (compile_date[2] == 'n') ? 6U : 7U;
            }
            break;

        case 'F':
            month = 2U;
            break;

        case 'M':
            month = (compile_date[2] == 'r') ? 3U : 5U;
            break;

        case 'A':
            month = (compile_date[1] == 'p') ? 4U : 8U;
            break;

        case 'S':
            month = 9U;
            break;

        case 'O':
            month = 10U;
            break;

        case 'N':
            month = 11U;
            break;

        case 'D':
            month = 12U;
            break;

        default:
            month = 0U;
            break;
    }

    /* 解析编译年份，格式 yyyy。 */
    System.compile_param.year = (uint16_t)(
          (uint16_t)(compile_date[7]  - '0') * 1000U
        + (uint16_t)(compile_date[8]  - '0') * 100U
        + (uint16_t)(compile_date[9]  - '0') * 10U
        + (uint16_t)(compile_date[10] - '0')
    );

    /* 保存编译月份。 */
    System.compile_param.month = month;

    /* 解析编译日期，兼容个位日期前面的空格。 */
    System.compile_param.day = (uint8_t)(
          ((compile_date[4] == ' ') ? 0U :
            (uint8_t)(compile_date[4] - '0') * 10U)
        + (uint8_t)(compile_date[5] - '0')
    );

    /* 解析编译小时。 */
    System.compile_param.hour = (uint8_t)(
          (uint8_t)(compile_time[0] - '0') * 10U
        + (uint8_t)(compile_time[1] - '0')
    );

    /* 解析编译分钟。 */
    System.compile_param.minute = (uint8_t)(
          (uint8_t)(compile_time[3] - '0') * 10U
        + (uint8_t)(compile_time[4] - '0')
    );

    /* 解析编译秒数。 */
    System.compile_param.second = (uint8_t)(
          (uint8_t)(compile_time[6] - '0') * 10U
        + (uint8_t)(compile_time[7] - '0')
    );
}


/*******************************************************************************
//  Function   : system_bsp_init
//  Description: 初始化系统底层硬件驱动模块
//  Param      : None
//  Return     : None
//  Others     : 依次初始化 GPIO、PGA2、ADC、TIM3、TIM1 和 TIM16；
//               ADC 及 TIM3 初始化完成后立即启动
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
}


/*******************************************************************************
//  Function   : system_data_init
//  Description: 初始化系统应用层功能模块
//  Param      : None
//  Return     : None
//  Others     : 读取芯片 UID 与固件编译时间；
//               初始化风扇、按键、BMS、ADC 采样、显示及 UART 协议模块
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_data_init(void)
{
    /* 读取芯片 UID 及当前固件编译时间。 */
    Unique_ID_read();
    compile_prarm_read();

    /* 初始化风扇控制模块。 */
    fan_Init();

    /* 初始化按键处理模块。 */
    button_Init();

    /* 初始化电池管理应用模块。 */
    app_bms_Init();

    /* 初始化 ADC 采样滤波模块。 */
    app_sampling_Init();

    /* 初始化 AiP650E 显示模块。 */
    app_display_Init();

    /* 初始化风扇 UART1 通信协议模块。 */
    fan_protocol_Init();
}


/*******************************************************************************
//  Function   : system_test
//  Description: 检测 USB 输入电压突降并执行风扇 PWM 渐变复位
//  Param      : None
//  Return     : None
//  Others     : 比较相邻两次 USB 输入电压，检测超过 2V 的下降；
//               当设备开机且风扇档位不低于 3 档时，
//               调用 fan_pwm_ramp_rst() 复位风扇 PWM 渐变状态；
//               当前保留原有瞬时电压判断与 200ms 计时逻辑
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void system_test(void)
{
    static double last_usb_v = 0.0;
    static bool boost_flg = false;
    static timer_base_t _timer_boost = {0};

    /* 检测 USB 输入电压是否出现超过 2V 的突降。 */
    boost_flg = (last_usb_v - bms_battery.usb_in_v) > 2.0;

    /* 保存当前 USB 输入电压。 */
    last_usb_v = bms_battery.usb_in_v;

    if (boost_flg == false)
    {
        /* 未检测到电压突降时复位计时器。 */
        timer_base_Rst(&_timer_boost);
    }
    else
    {
        /* 检测到突降后检查计时器状态。 */
        if (timer_base_Count(&_timer_boost) > 200U)
        {
            boost_flg = false;
        }
    }

    /* 高档风扇运行期间检测到突降时复位 PWM 渐变。 */
    if (boost_flg &&
        System.logic.power &&
        (System.logic.fan_mode >= FAN_MODE_3))
    {
        fan_pwm_ramp_rst();
    }
}


/*******************************************************************************
//  Function   : system_init
//  Description: 执行系统整体初始化
//  Param      : None
//  Return     : None
//  Others     : 先初始化 BSP 底层硬件，
//               再初始化系统应用层及通信协议模块
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
//  Others     : 主循环持续处理 ADC 滤波及 USB 电压突降检测；
//               每 1ms 调度风扇、按键、BMS、UART1、显示、
//               应用逻辑及风扇通信协议任务；
//               同时统计主循环间隔并更新电源通路控制 GPIO
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void system_run(void)
{
    static timer_base_t _timer_logic = {0};

    /* 处理 ADC 采样滤波及相关状态更新。 */
    app_sampling_task();

    /* 执行 USB 输入电压突降检测。 */
    system_test();

    /* 执行 1ms 周期任务。 */
    if (timer_base_Timing(&_timer_logic, 1U))
    {
        /* 风扇档位、PWM 渐变及转速反馈处理。 */
        fan_1ms_task();

        /* 按键采样及事件处理。 */
        button_1ms_task();

        /* BMS 数据采样、SOC/SOH 及保护处理。 */
        app_bms_1ms_task();

        /* UART1 通信接口周期处理。 */
        uart1_port_1msTask();

        /* AiP650E 显示逻辑及硬件刷新。 */
        app_display_task();

        /* 应用层设备运行逻辑。 */
        add_logic_1ms_task();

        /* 风扇通信协议接收及应答处理。 */
        fan_protocol_1ms_task();
    }

    /* 更新主循环执行间隔。 */
    system_loop_delay();

    /* 根据控制标志更新电池供电通路。 */
    if (vbat_ctrl)
    {
        LL_GPIO_SetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    }

    /* 根据控制标志更新 USB 输入通路。 */
    if (vin_ctrl)
    {
        LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    }
}
