
/**
 ***********************************************************************************************************************
 * @file    app_boot.c
 * @brief   Bootloader 应用层管理，实现 OTA 状态检测、外设初始化及 Application 跳转
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2026-11-02
 ***********************************************************************************************************************
 * @note
 *          1. 启动时检查 Flash 保存的 OTA 状态。
 *          2. OTA 升级未完成时进入 Bootloader 模式。
 *          3. 初始化 GPIO、显示模块及 UART1 通信协议。
 *          4. 周期执行显示刷新和 OTA 通信处理。
 *          5. OTA 升级成功后等待 200ms，再尝试跳转 Application。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_boot.h"
#include "app_display.h"

#include "bsp_gpio.h"
#include "bsp_uart1.h"
#include "bsp_flash.h"

#include "timer_base.h"
#include "uart1_port.h"
#include "fan_protocol.h"
#include "bootloader.h"
#include "param.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** OTA 升级成功后跳转 Application 的等待时间，单位 ms */
#define BOOT_JUMP_DELAY_MS    200U


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** Bootloader 全局运行状态 */
boot_t Boot;


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : system_bsp_init
//  Description: 初始化 Bootloader 使用的底层硬件驱动
//  Param      : None
//  Return     : None
//  Others     : 当前仅初始化系统 GPIO；
//               UART1 由风扇通信协议模块负责初始化
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
static void system_bsp_init(void)
{
    /* 初始化 Bootloader 使用的 GPIO。 */
    bsp_gpio_Init();
}


/*******************************************************************************
//  Function   : system_data_init
//  Description: 初始化 Bootloader 应用层功能模块
//  Param      : None
//  Return     : None
//  Others     : 初始化 AiP650E 显示驱动；
//               初始化 UART1 风扇通信协议模块
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
static void system_data_init(void)
{
    /* 初始化 AiP650E 显示模块。 */
    app_display_Init();

    /* 初始化 UART1 通信协议模块。 */
    fan_protocol_Init();
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : boot_init
//  Description: 初始化 Bootloader 系统及 OTA 升级运行环境
//  Param      : None
//  Return     : None
//  Others     : 启动时首先读取 OTA 状态；
//               OTA 已成功时尝试跳转 Application；
//               其他状态下初始化固件升级控制参数；
//               最后完成 GPIO、显示及 UART1 协议初始化
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
void boot_init(void)
{
    /* OTA 已成功时，尝试跳转 Application。 */
    if (param_ota_state() == OTC_STATUS_SUCCESS)
    {
        JumpToApplication();
    }
    else
    {
		if(param_ota_state() == OTC_STATUS_START)
		{
			protocol.response = PROTO_ENTER_OTA_RESPONSE;
		}
        /* 初始化 OTA 固件烧录地址及控制参数。 */
        bootloader_Init();
    }

    /* 初始化 Bootloader 底层硬件。 */
    system_bsp_init();

    /* 初始化 Bootloader 应用层模块。 */
    system_data_init();
}


/*******************************************************************************
//  Function   : boot_run
//  Description: 执行 Bootloader 主循环及周期任务调度
//  Param      : None
//  Return     : None
//  Others     : 每 1ms 执行显示刷新和 UART1 协议处理；
//               OTA 未完成时保持 Bootloader 运行；
//               OTA 成功后等待 200ms，再尝试跳转 Application
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
void boot_run(void)
{
    static timer_base_t _timer_logic = {0};
    static timer_base_t _timer_jump  = {0};
    static bool jump_pending = false;

    /* 执行 Bootloader 1ms 周期任务。 */
    if (timer_base_Timing(&_timer_logic, 1U))
    {
        /* 执行 AiP650E 显示逻辑及硬件刷新。 */
        app_display_task();

        /* 处理 UART1 OTA 命令接收及协议应答。 */
        fan_protocol_1ms_task();
    }

    /* 检查 OTA 是否已经完成。 */
    if (param_ota_state() == OTC_STATUS_SUCCESS)
    {
		bsp_flash_Lock();
        /* 首次检测到升级成功时启动跳转延时。 */
        if (jump_pending == false)
        {
            jump_pending = true;
            timer_base_Rst(&_timer_jump);
        }

        /* 达到等待时间后跳转 Application。 */
        if (timer_base_Count(&_timer_jump) >= BOOT_JUMP_DELAY_MS)
        {
            JumpToApplication();
        }
    }
    else
    {
		bsp_flash_Unlock();
        /* OTA 未完成，不允许跳转 Application。 */
        jump_pending = false;
    }
}
