
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


/* Macro -------------------------------------------------------------------------------------------------------------*/


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief 系统全局运行状态
 * @note  保存设备运行模式、控制参数、固件信息及主循环调度状态。
 */
system_t System;


/* Debug variables ---------------------------------------------------------------------------------------------------*/

/** 电池通路调试控制标志，true 表示输出使能电平 */
bool vbat_ctrl = true;

/** USB 输入通路调试控制标志，true 表示输出使能电平 */
bool vin_ctrl = false;


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


/*******************************************************************************
//  Function   : compile_prarm_read
//  Description: 解析并保存固件编译日期及时间
//  Param      : None
//  Return     : None
//  Others     : 使用编译器提供的 __DATE__ 和 __TIME__ 宏；
//               __DATE__ 格式为 Mmm dd yyyy；
//               __TIME__ 格式为 hh:mm:ss；
//               解析年份、月份、日期、小时、分钟及秒数；
//               结果保存至 System.compile_param
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
static void compile_prarm_read(void)
{
    const char compile_date[] = __DATE__;
    const char compile_time[] = __TIME__;
    uint8_t month = 0U;

    /* 根据 __DATE__ 中的英文月份缩写确定月份。 */
    switch (compile_date[0])
    {
        case 'J':
            if (compile_date[1] == 'a')
            {
                /* Jan：一月。 */
                month = 1U;
            }
            else
            {
                /* Jun：六月；Jul：七月。 */
                month = (compile_date[2] == 'n') ? 6U : 7U;
            }
            break;

        case 'F':
            month = 2U;
            break;

        case 'M':
            /* Mar：三月；May：五月。 */
            month = (compile_date[2] == 'r') ? 3U : 5U;
            break;

        case 'A':
            /* Apr：四月；Aug：八月。 */
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
            /* 无法识别月份时保存为 0。 */
            month = 0U;
            break;
    }

    /* 解析编译年份 yyyy。 */
    System.compile_param.year = (uint16_t)(
          (uint16_t)(compile_date[7]  - '0') * 1000U
        + (uint16_t)(compile_date[8]  - '0') * 100U
        + (uint16_t)(compile_date[9]  - '0') * 10U
        + (uint16_t)(compile_date[10] - '0')
    );

    /* 保存编译月份。 */
    System.compile_param.month = month;

    /*
     * 解析编译日期。
     *
     * __DATE__ 对个位日期使用前导空格，
     * 例如 "Nov  4 2026"。
     */
    System.compile_param.day = (uint8_t)(((compile_date[4] == ' ') ? 0U : (uint8_t)(compile_date[4] - '0') * 10U) + (uint8_t)(compile_date[5] - '0'));

    /* 解析编译小时 hh。 */
    System.compile_param.hour   = (uint8_t)((uint8_t)(compile_time[0] - '0') * 10U + (uint8_t)(compile_time[1] - '0'));

    /* 解析编译分钟 mm。 */
    System.compile_param.minute = (uint8_t)((uint8_t)(compile_time[3] - '0') * 10U + (uint8_t)(compile_time[4] - '0'));

    /* 解析编译秒数 ss。 */
    System.compile_param.second = (uint8_t)((uint8_t)(compile_time[6] - '0') * 10U + (uint8_t)(compile_time[7] - '0'));
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
    /* 读取芯片 UID。 */
    Unique_ID_read();

    /* 解析固件编译日期及时间。 */
    compile_prarm_read();

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
//  Function   : system_test
//  Description: 检测 USB 输入电压突降并处理风扇 PWM 渐变复位
//  Param      : None
//  Return     : None
//  Others     : 比较相邻两次检测的 USB 输入电压；
//               当电压下降超过 2V 时置位突降标志；
//               使用软件计时器判断突降事件持续时间；
//               当系统开机且风机档位不低于 3 档时，
//               调用 fan_pwm_ramp_rst() 重置 PWM 渐变控制
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void system_test(void)
{
    static double last_usb_v = 0.0;
    static bool boost_flg = false;
    static timer_base_t _timer_boost = {0};

    /* 检测相邻两次 USB 输入电压是否下降超过 2V。 */
    boost_flg = (last_usb_v - bms_battery.usb_in_v) > 2.0;

    /* 保存当前 USB 输入电压。 */
    last_usb_v = bms_battery.usb_in_v;

    if (boost_flg == false)
    {
        /* 未检测到突降时重置事件计时器。 */
        timer_base_Rst(&_timer_boost);
    }
    else
    {
        /* 检查突降事件的计时状态。 */
        if (timer_base_Count(&_timer_boost) > 200U)
        {
            /* 超过 200ms 后清除突降标志。 */
            boost_flg = false;
        }
    }

    /*
     * 风机高档运行期间，如果检测到 USB 电压突降，
     * 则重新初始化风机 PWM 渐变状态。
     */
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
//  Others     : 先初始化 BSP 底层硬件；
//               再初始化应用层功能模块及通信协议；
//               确保系统主循环运行前完成基础初始化
//  Author     : Chen Liang 2026-06-06
*******************************************************************************/
void system_init(void)
{
    /* 初始化板级硬件驱动。 */
    system_bsp_init();

    /* 初始化系统应用层功能模块。 */
    system_data_init();
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

    /* 执行 USB 输入电压突降检测。 */
    system_test();

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
        add_logic_1ms_task();

        /* 处理 UART1 协议接收、解析及应答。 */
        fan_protocol_1ms_task();

        /* 更新雾化片 PWM 输出及自动追频状态。 */
        app_mist_1ms_task();

        /* 执行系统低功耗状态管理及休眠控制。 */
        sleep_1ms_task();
    }

    /* 统计系统主循环相邻两次执行的时间间隔。 */
    system_loop_delay();

    /*
     * 更新电池通路使能信号：
     *
     * true  ：输出高电平，使能电池通路。
     * false ：输出低电平，关闭电池通路。
     */
    if (vbat_ctrl)
    {
        LL_GPIO_SetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    }

    /*
     * 更新 USB 输入通路使能信号：
     *
     * true  ：输出高电平，使能 USB/VIN 输入通路。
     * false ：输出低电平，关闭 USB/VIN 输入通路。
     */
    if (vin_ctrl)
    {
        LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    }
}

