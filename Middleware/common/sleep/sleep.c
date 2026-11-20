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
#include "bsp_wdg.h"
#include "bsp_lptim.h"

#include "fan.h"
#include "button.h"
#include "timer_base.h"

#include "fan_protocol.h"
/* Macro -------------------------------------------------------------------------------------------------------------*/

/* 满足休眠条件后，延迟进入低功耗的时间，单位 ms。 */
#define SYSTEM_SLEEP_DELAY_TIME    (3000U)

/* Variables ---------------------------------------------------------------------------------------------------------*/

sleep_t sleep_ctrl;

LL_UTILS_PLLInitTypeDef UTILS_PLLInitStruct = {LL_RCC_PLL_MUL9};
LL_UTILS_ClkInitTypeDef UTILS_ClkInitStruct = {LL_RCC_SYSCLK_DIV_1, LL_RCC_APB1_DIV_1};

/*******************************************************************************
//  Function   : sleep_can_enter
//  Description: USB_IN 外部中断事件回调
//  Param      : None
//  Return     : None
//  Others     : 弱定义函数，可由应用层重新实现；
//               用于处理 USB_IN 外部中断事件；
//               建议仅设置事件标志，避免在中断上下文执行耗时操作
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static bool sleep_can_enter(void)
{
    return (botton_getCurrState(BUTTON_ITEM_POWER) != BUTTON_PRESS) && (System.logic.usb_present == 0U);
}


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
void bsp_exti_usb_Callback(void)
{
    sleep_ctrl.lock_sleep = 0;
}

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
void bsp_exti_power_Callback(void)
{
     sleep_ctrl.lock_sleep = 0;
}

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
void enter_sleep(void)
{
  /* 稳压器切到 DLPR 模式, 进一步降低休眠电流 */
  LL_PWR_SetVoltageRegulatorMode(LL_PWR_VOLTAGE_REGULATOR_MODE_DLPR);

  /* 置 SLEEPDEEP 位, 进入深度休眠 */
  LL_LPM_EnableDeepSleep();

  /* 等待中断唤醒 */
  __WFI();

  /* 恢复普通 SLEEP 配置 */
  LL_LPM_EnableSleep();
}

/*******************************************************************************
//  Function   : enter_stop_mode
//  Description: 循环进入 STOP，定时唤醒喂狗，外部唤醒后恢复运行
//  Param      : None
//  Return     : None
//  Others     : 在线程模式且全局中断开启时调用，调用前关闭业务外设并配置唤醒源；
//               每次唤醒先恢复 72 MHz 时钟，再允许中断回调执行；
//               LPTIM 唤醒仅喂狗，USB/电源回调清除 lock_sleep 后退出；
//               休眠期间暂停 SysTick，退出时恢复原使能及调试配置
//  Author     : Chen Liang 2026-11-19
*******************************************************************************/
void enter_stop_mode(void)
{
    uint32_t primask;
    uint32_t systick_ctrl;
    uint32_t debug_ctrl;

    /* 1. 退出休眠依赖 EXTI 中断回调，禁止从 ISR 或已屏蔽中断的临界区调用。 */
    primask = __get_PRIMASK();
    if ((__get_IPSR() != 0U) || (primask != 0U))
    {
        APP_ErrorHandler();
        return;
    }

    /* 2. 保存现场并屏蔽中断，避免准备休眠时被回调打断。 */
    __disable_irq();
    systick_ctrl = SysTick->CTRL;
    debug_ctrl = DBGMCU->CR;

    /* 整个休眠循环停用系统节拍，周期唤醒时也不累计软件运行时间。 */
    CLEAR_BIT(SysTick->CTRL, SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    CLEAR_BIT(DBGMCU->CR, DBGMCU_CR_DBG_STOP);
    sleep_ctrl.lock_sleep = true;

    do
    {
#if SYSTEM_WDG_EN
        /* 3. 清理上次定时状态，启动单次唤醒，并在入睡前刷新看门狗。 */
        lptim_stop();
        LL_LPTIM_ClearFLAG_ARRM(LPTIM);
        NVIC_ClearPendingIRQ(LPTIM1_IRQn);
        bsp_wdg_Refresh();
        lptim_start();
#endif

        /*
         * 4. 复用 BSP 的 STOP 流程：保存 PWR/SCB 状态，选择 DLPR，
         *    通过 SEVONPEND + WFE 等待唤醒；有挂起中断时跳过等待。
         *    返回前恢复稳压器、Flash 等待周期及 72 MHz HSI 时钟。
         *    此处 PRIMASK 仍为 1，因此外设 ISR 不会在时钟恢复前运行。
         */
        bsp_rcc_SystemStop();

#if SYSTEM_WDG_EN
        /* 5. 时钟恢复后立即喂狗，避免中断处理时间消耗剩余超时裕量。 */
        bsp_wdg_Refresh();
#endif

        /*
         * 6. 短暂开启中断，让已挂起的 LPTIM/EXTI 服务函数执行。
         *    LPTIM ISR 只清标志；USB/电源回调会清除 lock_sleep。
         *    随后再次关中断，原子判断是否还需休眠，避免丢失唤醒请求。
         */
        __enable_irq();
        __ISB();
        __disable_irq();
    } while (sleep_ctrl.lock_sleep);

#if SYSTEM_WDG_EN
    /* 7. 外部事件要求退出休眠，关闭定时唤醒并清理残留请求。 */
    lptim_stop();
    LL_LPTIM_ClearFLAG_ARRM(LPTIM);
    NVIC_ClearPendingIRQ(LPTIM1_IRQn);
#endif

    /* 8. 恢复调试配置及原 SysTick 使能状态；休眠时间不补计入软件节拍。 */
    DBGMCU->CR = debug_ctrl;
    SysTick->VAL = 0U;
    SysTick->CTRL = systick_ctrl & (SysTick_CTRL_ENABLE_Msk |
                                  SysTick_CTRL_TICKINT_Msk |
                                  SysTick_CTRL_CLKSOURCE_Msk);
    __DSB();
    __ISB();
    __set_PRIMASK(primask);
}

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
	enter_stop_mode();
   

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


