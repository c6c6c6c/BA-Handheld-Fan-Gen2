/**
 ***********************************************************************************************************************
 * @file    bsp_exti.c
 * @brief   EXTI 外部中断及低功耗唤醒配置
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bsp_exti.h"
#include "bsp_gpio.h"


/*******************************************************************************
//  Function   : bsp_exti_usb_Callback
//  Description: usb 唤醒中断处理
//  Param      : None
//  Return     : None
//  Others     : 弱定义函数，可由应用层重写；回调中应及时读取捕获值
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
__weak void bsp_exti_usb_Callback(void){}

/*******************************************************************************
//  Function   : bsp_exti_sleep_Init
//  Description: 初始化低功耗睡眠唤醒 EXTI
//  Param      : None
//  Return     : None
//  Others     : FAN_SW、MIST_SW 配置为 EXTI Event 模式，用于低功耗模式下唤醒 MCU，
//               不产生 CPU 中断请求
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void bsp_exti_sleep_Init(void)
{
    LL_EXTI_InitTypeDef EXTI_InitStruct = {0};

    EXTI_InitStruct.LineCommand = ENABLE;
    EXTI_InitStruct.Mode        = LL_EXTI_MODE_EVENT;

    /*
     * FAN_SW：
     * 配置为 EXTI Event 模式。
     * 当检测到指定边沿时产生 EXTI Event，用于睡眠状态唤醒 MCU。
     */
    EXTI_InitStruct.Line        = FAN_SW_EXTI_LINE;
    EXTI_InitStruct.Trigger     = FAN_SW_EXTI_TRIGGER;
    LL_EXTI_Init(&EXTI_InitStruct);

    /*
     * MIST_SW：
     * 配置为 EXTI Event 模式。
     * 当检测到指定边沿时产生 EXTI Event，用于睡眠状态唤醒 MCU。
     */
    EXTI_InitStruct.Line        = MIST_SW_EXTI_LINE;
    EXTI_InitStruct.Trigger     = MIST_SW_EXTI_TRIGGER;
    LL_EXTI_Init(&EXTI_InitStruct);

    /*
     * MIST_SW：
     * 配置为 EXTI Event 模式。
     * 当检测到指定边沿时产生 EXTI Event，用于睡眠状态唤醒 MCU。
     */
    EXTI_InitStruct.Line        = USB_IN_EXTI_LINE;
    EXTI_InitStruct.Trigger     = USB_IN_EXTI_TRIGGER;
    LL_EXTI_Init(&EXTI_InitStruct);

    /*
     * 配置 EXTI Line 与实际 GPIO 端口映射关系。
     *
     * FAN_SW  -> 对应 FAN_SW_EXTI_CONFIG_PORT
     * MIST_SW -> 对应 MIST_SW_EXTI_CONFIG_PORT
     */
    LL_EXTI_SetEXTISource(FAN_SW_EXTI_CONFIG_PORT, FAN_SW_EXTI_CONFIG_LINE);
    LL_EXTI_SetEXTISource(MIST_SW_EXTI_CONFIG_PORT, MIST_SW_EXTI_CONFIG_LINE);
    LL_EXTI_SetEXTISource(USB_IN_EXTI_CONFIG_PORT, USB_IN_EXTI_CONFIG_LINE);
}


/*******************************************************************************
//  Function   : bsp_exti_usb_Init
//  Description: 初始化 USB_IN 外部中断及唤醒功能
//  Param      : None
//  Return     : None
//  Others     : USB_IN 配置为 Interrupt + Event 模式，USB 插入时既可唤醒 MCU，
//               又可进入 EXTI 中断服务函数进行软件处理
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void bsp_exti_usb_Init(void)
{
    LL_EXTI_InitTypeDef EXTI_InitStruct = {0};

    /*
     * USB_IN 使用 IT + EVENT 模式：
     *
     * EVENT：
     * 用于低功耗状态下唤醒 MCU。
     *
     * INTERRUPT：
     * 唤醒后进入 EXTI4_15_IRQHandler()，
     * 由软件进一步处理 USB 插入事件。
     */
    EXTI_InitStruct.LineCommand = ENABLE;
    EXTI_InitStruct.Mode        = LL_EXTI_MODE_IT_EVENT;
    EXTI_InitStruct.Line        = USB_IN_EXTI_LINE;
    EXTI_InitStruct.Trigger     = USB_IN_EXTI_TRIGGER;

    LL_EXTI_Init(&EXTI_InitStruct);

    /*
     * 配置 USB_IN 对应 GPIO 与 EXTI Line 的映射。
     */
    LL_EXTI_SetEXTISource(USB_IN_EXTI_CONFIG_PORT, USB_IN_EXTI_CONFIG_LINE);

    /*
     * USB_IN 当前使用 EXTI4~15 共用中断入口。
     *
     * 中断优先级可根据系统整体中断优先级规划进行调整。
     */
    NVIC_SetPriority(EXTI4_15_IRQn, 2);
    NVIC_EnableIRQ(EXTI4_15_IRQn);
}


/*******************************************************************************
//  Function   : bsp_exti_Init
//  Description: 初始化板级 EXTI
//  Param      : None
//  Return     : None
//  Others     : 配置按键低功耗唤醒及 USB 插入中断/唤醒功能
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_exti_Init(void)
{
    /*
     * SYSCFG 用于配置 GPIO 到 EXTI Line 的映射。
     * 如果芯片 LL_EXTI_SetEXTISource() 依赖 SYSCFG 时钟，
     * 需要提前使能。
     */
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);

    bsp_exti_sleep_Init();
    //bsp_exti_usb_Init();
}


/*******************************************************************************
//  Function   : EXTI4_15_IRQHandler
//  Description: EXTI Line4~Line15 中断服务函数
//  Param      : None
//  Return     : None
//  Others     : 当前主要处理 USB_IN 外部中断；进入中断后必须清除对应 EXTI Pending 标志
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void EXTI4_15_IRQHandler(void)
{
    /*
     * USB_IN 外部中断处理
     *
     * USB 插入后 USB_IN 产生上升沿，
     * EXTI 检测到有效边沿后进入该中断。
     */
    if (LL_EXTI_IsActiveFlag(USB_IN_EXTI_LINE) != RESET)
    {
        /*
         * 必须先清除 Pending 标志。
         *
         * 如果不清除，退出中断后可能立即再次进入中断。
         */
        bsp_exti_usb_Callback();
        LL_EXTI_ClearFlag(USB_IN_EXTI_LINE);
        /*
         * USB 插入事件处理。
         *
         * 建议这里只做轻量级操作，例如：
         *
         * 1. 设置 USB 插入标志
         * 2. 唤醒相关任务
         * 3. 通知状态机
         *
         * 不建议在中断里执行较长延时或复杂业务逻辑。
         */

        /* 示例：
         *
         * g_usb_insert_flag = 1U;
         */
    }
}
