
/**
 ***********************************************************************************************************************
 * @file    bsp_exti.c
 * @brief   EXTI 外部中断及低功耗唤醒配置，实现电源按键和 USB_IN 唤醒事件管理
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
//  Description: USB_IN 外部中断事件回调
//  Param      : None
//  Return     : None
//  Others     : 弱定义函数，可由应用层重新实现；
//               用于处理 USB_IN 外部中断事件；
//               建议仅设置事件标志，避免在中断上下文执行耗时操作
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
__weak void bsp_exti_usb_Callback(void){}

/*******************************************************************************
//  Function   : bsp_exti_sleep_Init
//  Description: 初始化电源按键及 USB_IN 低功耗唤醒 EXTI
//  Param      : None
//  Return     : None
//  Others     : POWER_SW、USB_IN 均配置为 EXTI Event 模式；
//               检测到指定触发边沿时产生外部事件；
//               可配合 WFE 指令实现低功耗唤醒；
//               Event 模式不产生 CPU 中断请求
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void bsp_exti_sleep_Init(void)
{
    LL_EXTI_InitTypeDef EXTI_InitStruct = {0};

    /* 使能 EXTI Line，并选择 Event 模式。 */
    EXTI_InitStruct.LineCommand = ENABLE;
    EXTI_InitStruct.Mode        = LL_EXTI_MODE_EVENT;

    /*
     * POWER_SW 电源按键唤醒配置：
     *
     * 1. 配置对应 EXTI Line。
     * 2. 设置电源按键触发边沿。
     * 3. 检测到有效边沿后产生 EXTI Event。
     * 4. 可用于唤醒处于相应低功耗模式的 MCU。
     */
    EXTI_InitStruct.Line        = POWER_SW_EXTI_LINE;
    EXTI_InitStruct.Trigger     = POWER_SW_EXTI_TRIGGER;
    LL_EXTI_Init(&EXTI_InitStruct);


    /*
     * USB_IN 外部事件唤醒配置：
     *
     * 1. 配置 USB_IN 对应 EXTI Line。
     * 2. 设置 USB_IN 触发边沿。
     * 3. 检测到有效边沿后产生 EXTI Event。
     * 4. 当前仅用于事件唤醒，不触发 USB 中断回调。
     */
    EXTI_InitStruct.Line        = USB_IN_EXTI_LINE;
    EXTI_InitStruct.Trigger     = USB_IN_EXTI_TRIGGER;
    LL_EXTI_Init(&EXTI_InitStruct);

    /*
     * 配置 GPIO 与 EXTI Line 的映射关系：
     *
     * POWER_SW -> POWER_SW_EXTI_CONFIG_PORT / LINE
     * USB_IN   -> USB_IN_EXTI_CONFIG_PORT / LINE
     *
     * GPIO 端口映射需要依赖 SYSCFG 外设时钟。
     */
    LL_EXTI_SetEXTISource(POWER_SW_EXTI_CONFIG_PORT, POWER_SW_EXTI_CONFIG_LINE);
    LL_EXTI_SetEXTISource(USB_IN_EXTI_CONFIG_PORT, USB_IN_EXTI_CONFIG_LINE);
}


/*******************************************************************************
//  Function   : bsp_exti_usb_Init
//  Description: 初始化 USB_IN 外部中断及低功耗唤醒
//  Param      : None
//  Return     : None
//  Others     : USB_IN 配置为 Interrupt + Event 模式；
//               Event 用于低功耗唤醒；
//               Interrupt 用于触发 CPU 中断并通知应用层；
//               当前函数通过条件编译禁用，暂不参与实际初始化
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
#if 0
static void bsp_exti_usb_Init(void)
{
    LL_EXTI_InitTypeDef EXTI_InitStruct = {0};

    /*
     * 配置 USB_IN 为 Interrupt + Event 模式：
     *
     * Event：
     * 检测到指定边沿后产生外部事件，
     * 可配合 WFE 实现低功耗唤醒。
     *
     * Interrupt：
     * 检测到指定边沿后产生中断请求，
     * 在 NVIC 允许的情况下进入 EXTI4_15_IRQHandler()。
     */
    EXTI_InitStruct.LineCommand = ENABLE;
    EXTI_InitStruct.Mode        = LL_EXTI_MODE_IT_EVENT;
    EXTI_InitStruct.Line        = USB_IN_EXTI_LINE;
    EXTI_InitStruct.Trigger     = USB_IN_EXTI_TRIGGER;

    LL_EXTI_Init(&EXTI_InitStruct);

    /*
     * 配置 USB_IN GPIO 端口与 EXTI Line 的映射。
     *
     * 应确保对应 GPIO 已配置为输入模式，
     * 并且 SYSCFG 外设时钟已使能。
     */
    LL_EXTI_SetEXTISource(USB_IN_EXTI_CONFIG_PORT, USB_IN_EXTI_CONFIG_LINE);

    /*
     * 配置 USB_IN 外部中断的 NVIC：
     *
     * 1. USB_IN 使用 EXTI4~15 共用中断入口。
     * 2. 设置中断抢占优先级为 2。
     * 3. 使能 EXTI4_15_IRQn 中断。
     *
     * 中断优先级应与系统其他外设的中断配置协调。
     */
    NVIC_SetPriority(EXTI4_15_IRQn, 2);
    NVIC_EnableIRQ(EXTI4_15_IRQn);
}
#endif

/*******************************************************************************
//  Function   : bsp_exti_Init
//  Description: 初始化板级 EXTI 外部事件及唤醒功能
//  Param      : None
//  Return     : None
//  Others     : 使能 SYSCFG 时钟，支持 GPIO 与 EXTI Line 映射；
//               初始化 POWER_SW 和 USB_IN 低功耗唤醒事件；
//               当前仅配置 Event 模式，不启用 USB_IN 中断模式
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_exti_Init(void)
{
    /*
     * 使能 SYSCFG 外设时钟：
     *
     * 用于配置 GPIO 端口与 EXTI Line 的映射关系，
     * 应在 LL_EXTI_SetEXTISource() 调用前完成。
     */
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);

    /* 初始化 POWER_SW 和 USB_IN 低功耗唤醒事件。 */
    bsp_exti_sleep_Init();
}


/*******************************************************************************
//  Function   : EXTI4_15_IRQHandler
//  Description: EXTI Line4~Line15 共用中断服务函数
//  Param      : None
//  Return     : None
//  Others     : 用于处理 USB_IN 对应的外部中断请求；
//               检测 Pending 标志后调用应用层弱定义回调；
//               随后清除 EXTI Pending 标志；
//               当前 USB_IN 配置为 Event 模式，因此正常情况下
//               不会通过该配置触发此中断函数
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void EXTI4_15_IRQHandler(void)
{
    /*
     * USB_IN 外部中断处理：
     *
     * 当 USB_IN 配置为中断模式、NVIC 已使能，
     * 并且检测到有效触发边沿时，
     * 可进入 EXTI4_15_IRQHandler()。
     */
    if (LL_EXTI_IsActiveFlag(USB_IN_EXTI_LINE) != RESET)
    {
        /*
         * 通知应用层处理 USB_IN 事件。
         *
         * 回调函数可用于：
         * 1. 设置 USB 插入或拔出事件标志。
         * 2. 通知电源管理状态机。
         * 3. 请求应用层执行后续处理。
         *
         * 应避免在回调中执行耗时操作。
         */
        bsp_exti_usb_Callback();

        /*
         * 清除 USB_IN 外部中断 Pending 标志。
         *
         * 中断标志未清除可能导致 CPU 退出中断后
         * 立即再次进入中断服务函数。
         */
        LL_EXTI_ClearFlag(USB_IN_EXTI_LINE);

        /*
         * 后续 USB 事件处理建议由应用层完成。
         *
         * 不建议在中断中直接执行：
         * 1. 长时间延时。
         * 2. 复杂电源管理逻辑。
         * 3. Flash 擦写等耗时操作。
         */

        /* 示例：
         *
         * g_usb_insert_flag = 1U;
         */
    }
}
