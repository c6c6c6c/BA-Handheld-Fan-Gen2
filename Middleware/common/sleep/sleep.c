/** 
 *********************************************************************************************************************** 
 * @file    sleep.c 
 * @brief   系统休眠管理，实现低功耗进入、时钟切换、外部唤醒及外设恢复
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

/** 系统休眠控制状态，包含延时计数及退出休眠锁定标志。 */
sleep_t sleep_ctrl;

/** 预置 PLL 参数结构体，当前文件的手动时钟切换流程未直接引用。 */
LL_UTILS_PLLInitTypeDef UTILS_PLLInitStruct = {LL_RCC_PLL_MUL9};
/** 预置总线时钟参数结构体，当前文件的手动时钟切换流程未直接引用。 */
LL_UTILS_ClkInitTypeDef UTILS_ClkInitStruct = {LL_RCC_SYSCLK_DIV_1, LL_RCC_APB1_DIV_1};

/*******************************************************************************
//  Function   : sleep_can_enter
//  Description: 判断当前系统是否满足休眠条件
//  Param      : None
//  Return     : bool，true 表示电源按键未按下且 USB 未接入；false 表示不满足
//  Others     : 依据电源按键当前状态及 System.logic.usb_present 判断；不检查其他业务状态
//  Author     : Chen Liang 2026-11-19
*******************************************************************************/
static bool sleep_can_enter(void)
{
    return (botton_getCurrState(BUTTON_ITEM_POWER) != BUTTON_PRESS) && (System.logic.usb_present == 0U);
}


/*******************************************************************************
//  Function   : bsp_exti_usb_Callback
//  Description: 处理 USB_IN 外部中断唤醒通知
//  Param      : None
//  Return     : None
//  Others     : 收到 USB 中断通知后清除 sleep_ctrl.lock_sleep；当前为应用层实现，不是弱定义
//  Author     : Chen Liang 2026-11-19
*******************************************************************************/
void bsp_exti_usb_Callback(void)
{
    sleep_ctrl.lock_sleep = 0;
}

/*******************************************************************************
//  Function   : bsp_exti_power_Callback
//  Description: 处理电源按键外部中断唤醒通知
//  Param      : None
//  Return     : None
//  Others     : 收到电源按键中断通知后清除 sleep_ctrl.lock_sleep；用于请求退出休眠循环
//  Author     : Chen Liang 2026-11-19
*******************************************************************************/
void bsp_exti_power_Callback(void)
{
     sleep_ctrl.lock_sleep = 0;
}

/*******************************************************************************
//  Function   : enter_sleep
//  Description: 执行一次深度休眠等待
//  Param      : None
//  Return     : None
//  Others     : 设置电压调节器为 DLPR，置位 SLEEPDEEP 后执行 WFI；
                 唤醒返回后清除深度休眠选择；不在本函数内恢复主系统时钟
//  Author     : Chen Liang 2026-11-19
*******************************************************************************/
void enter_sleep(void)
{
  /* 稳压器切到 DLPR 模式, 进一步降低休眠电流 */
  LL_PWR_SetVoltageRegulatorMode(LL_PWR_VOLTAGE_REGULATOR_MODE_DLPR);

  /* 设置 SLEEPDEEP 位，允许 WFI 进入所配置的深度低功耗模式。 */
  LL_LPM_EnableDeepSleep();

  /* 等待中断唤醒 */
  __WFI();

  /* 清除深度睡眠选择，后续 WFI 不再默认进入深度睡眠。 */
  LL_LPM_EnableSleep();
}

/*******************************************************************************
//  Function   : init_systemclock_pll_off
//  Description: 将系统时钟切换至 8MHz HSI 并关闭 PLL
//  Param      : None
//  Return     : None
//  Others     : 等待 HSI 就绪后切换 SYSCLK 为 HSISYS；关闭 PLL，设置 AHB/APB1 分频和 8MHz 软件时基
//  Author     : Chen Liang 2026-11-19
*******************************************************************************/
void init_systemclock_pll_off(void)
{
  LL_RCC_HSI_Enable();
  LL_RCC_HSI_SetCalibFreq(LL_RCC_HSICALIBRATION_8MHz);
  while(LL_RCC_HSI_IsReady() != 1)
  {
  }

  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSISYS);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSISYS)
  {
  }
	
	RCC->CR &=~(1<<24);//关闭PLL
	
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_Init1msTick(8000000);
  LL_SetSystemCoreClock(8000000);
}

/*******************************************************************************
//  Function   : init_systemclock_pll_on
//  Description: 使能 PLL 并恢复 72MHz 系统时钟
//  Param      : None
//  Return     : None
//  Others     : 使用 8MHz HSI 作为 PLL 输入，倍频系数为 9；设置 Flash 等待周期；
                 切换 SYSCLK 到 PLL 并恢复软件时基与 SysTick 中断
//  Author     : Chen Liang 2026-11-19
*******************************************************************************/
void init_systemclock_pll_on(void)
{
  /* Enable and initialize HSI */
  LL_RCC_HSI_Enable();
  LL_RCC_HSI_SetCalibFreq(LL_RCC_HSICALIBRATION_8MHz);
  while(LL_RCC_HSI_IsReady() != 1)
  {
  }
  /* Configure HSISYS as system clock */
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSISYS);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSISYS)
  {
  }
  /* PLL multiplication factor of 9 using HSI (8MHz) */
  LL_RCC_PLL_Disable();
  while(LL_RCC_PLL_IsReady() != 0)
  {
  }
  LL_RCC_PLL_SetMainSource(LL_RCC_PLLSOURCE_HSI);
  LL_RCC_PLL_SetMulFactor(LL_RCC_PLL_MUL9);

  LL_RCC_PLL_Enable();
  while(LL_RCC_PLL_IsReady() != 1)
  {
  }
  
  /* Set flash latency */
  LL_FLASH_SetLatency(LL_FLASH_LATENCY_2);
  while(LL_FLASH_GetLatency() != LL_FLASH_LATENCY_2)
  {
  }
  
  /* Configure AHB prescaler */
  LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);

  /* Configure PLL as system clock and initialize */
  LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
  while(LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL)
  {
  }

  /* Configure APB1 prescaler and initialize */
  LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
  LL_Init1msTick(72000000);

  /* Update the SystemCoreClock global variable(which can be updated also through SystemCoreClockUpdate function) */
  LL_SetSystemCoreClock(72000000);
  LL_SYSTICK_EnableIT();
}

/*******************************************************************************
//  Function   : enter_stop_mode
//  Description: 循环进入低功耗，直至接收到外部退出休眠请求
//  Param      : None
//  Return     : None
//  Others     : 检查线程模式与中断允许状态，保存 PRIMASK、SysTick 及调试配置；屏蔽中断并切换 8MHz 时钟；每次以 WFI 进入深度休眠，在短暂开放中断后检查 lock_sleep；仅在 SYSTEM_WDG_EN 启用时安排 LPTIM 单次定时唤醒和看门狗刷新；退出循环后恢复 72MHz 时钟及先前保存的 SysTick、调试与中断状态
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
	
	init_systemclock_pll_off();
    sleep_ctrl.lock_sleep = true;

    do
    {
#if SYSTEM_WDG_EN
        /* 3. 看门狗启用时，重置 LPTIM 单次计数并启动下一次定时唤醒。 */
        lptim_stop();
        LL_LPTIM_ClearFLAG_ARRM(LPTIM);
        NVIC_ClearPendingIRQ(LPTIM1_IRQn);
        bsp_wdg_Refresh();
        lptim_start();
#endif

        /*
         * 4. 当前调用 enter_sleep()，以 DLPR + SLEEPDEEP + WFI 进入低功耗。
         *    下方 bsp_rcc_SystemStop() 仅为已注释的备用调用。
         *    此时 PRIMASK 为 1，中断不会在此处正常进入 ISR；
         *    唤醒行为还取决于芯片对挂起中断和 WFI 的具体支持。
         *    系统时钟恢复至 72MHz 的操作位于退出休眠循环之后。
         */
		enter_sleep();

#if SYSTEM_WDG_EN
        /* 5. 从 WFI 返回后再次刷新看门狗；此时尚未恢复 72MHz 系统时钟。 */
        bsp_wdg_Refresh();
#endif

        /*
         * 6. 短暂解除中断屏蔽，使挂起的中断有机会执行回调。
         *    USB/电源按键回调通过清除 lock_sleep 请求退出循环。
         *    随后再次屏蔽中断，检查是否需要继续进入低功耗。
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

	init_systemclock_pll_on();
    /* 8. 恢复调试配置和原 SysTick 控制位；休眠时间未补计入软件节拍。 */
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
//  Function   : sleep_1ms_task
//  Description: 执行系统休眠条件判断及外设停止、恢复流程
//  Param      : None
//  Return     : None
//  Others     : 电源按键释放且 USB 未接入持续达到 3000 次任务调用后开始休眠；休眠前调整 GPIO 并停止 ADC/PGA/定时器/串口协议；休眠返回后重新初始化这些外设；计数按任务调用次数递增，实际延时取决于调用周期
//  Author     : Chen Liang 2026-11-19
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


