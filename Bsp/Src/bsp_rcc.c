/**
 ***********************************************************************************************************************
 * @file    bsp_rcc.c
 * @brief   系统复位及 STOP 低功耗控制实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-12
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bsp_rcc.h"

/* 轮询次数上限，不是毫秒值。 */
#define BSP_RCC_READY_TIMEOUT  1000000U

/*******************************************************************************
//  Function   : bsp_rcc_SystemReset
//  Description: 请求 MCU 软件系统复位
//  Param      : None
//  Return     : None
//  Others     : 通过 SYSRESETREQ 复位 MCU，调用后不返回；复位前数据保存由调用方负责
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_rcc_SystemReset(void)
{
    __disable_irq();
    __DSB();
    NVIC_SystemReset();
}

/*******************************************************************************
//  Function   : bsp_rcc_SystemStop
//  Description: 进入 STOP 模式，唤醒后恢复运行时钟
//  Param      : None
//  Return     : None
//  Others     : 仅在线程模式调用，调用前需配置 EXTI Event 唤醒并处理业务外设；
//               使用 DLPR + WFE，暂停 SysTick，唤醒后恢复项目 72 MHz HSI 时钟；
//               保留进入前的中断屏蔽状态，IWDG 启用后仍可能在休眠中超时复位
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_rcc_SystemStop(void)
{
    uint32_t primask;
    uint32_t systick_ctrl;
    uint32_t sleep_config;
    uint32_t pwr_clock_enabled;
    uint32_t timeout;

    /* STOP 接口需返回到调用线程，不支持在中断处理期间进入深睡眠。 */
    if (__get_IPSR() != 0U)
    {
        APP_ErrorHandler();
        return;
    }

    primask = __get_PRIMASK();
    __disable_irq();
    systick_ctrl = SysTick->CTRL;
    sleep_config = SCB->SCR;
    pwr_clock_enabled = LL_APB1_GRP1_IsEnabledClock(LL_APB1_GRP1_PERIPH_PWR);

    /* 暂停系统节拍，清除已挂起的 SysTick，避免系统节拍阻止休眠。 */
    CLEAR_BIT(SysTick->CTRL, SysTick_CTRL_ENABLE_Msk | SysTick_CTRL_TICKINT_Msk);
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);

    /*
     * PY32F032 STOP 使用 DLPR，而不是低功耗运行模式。
     * 选择唤醒时先等待 MR 稳定，再启动 HSI，Flash 在 HSI 稳定后启动。
     */
    LL_PWR_SetPowerMode(LL_PWR_MODE_RUN);
    LL_PWR_SetVoltageRegulatorMode(LL_PWR_VOLTAGE_REGULATOR_MODE_DLPR);
    LL_PWR_SetStopWakeUpHSIOnMode(LL_PWR_STOPWAKEUP_HSION_AFTER_MR);
    LL_PWR_SetStopWakeUpFlashOnMode(LL_PWR_STOPWAKEUP_FLASHON_AFTER_HSI);

    LL_LPM_DisableSleepOnExit();
    /* 屏蔽中断期间允许新挂起的中断产生事件，事件模式 EXTI 也可直接唤醒。 */
    LL_LPM_EnableEventOnPend();
    LL_LPM_EnableDeepSleep();
    __DSB();
    __ISB();

    /* 清除旧事件锁存，再等待新事件；已有待处理中断时直接跳过休眠。 */
    __SEV();
    __WFE();
    if ((SCB->ICSR & SCB_ICSR_ISRPENDING_Msk) == 0U)
    {
        __DSB();
        __WFE();
    }

    /* 先退出深睡眠配置，保持中断屏蔽，避免外设 ISR 使用尚未恢复的时钟。 */
    LL_LPM_EnableSleep();
    LL_PWR_SetPowerMode(LL_PWR_MODE_RUN);
    LL_PWR_SetVoltageRegulatorMode(LL_PWR_VOLTAGE_REGULATOR_MODE_MR);
    timeout = BSP_RCC_READY_TIMEOUT;
    while (LL_PWR_IsActiveFlag_MR_RDY() == 0U)
    {
        if (timeout-- == 0U)
        {
            APP_ErrorHandler();
            return;
        }
    }

    /* 与 main.c 的时钟配置一致：升频前先设置 Flash 等待周期。 */
    if (LL_SetFlashLatency(72000000U) != SUCCESS)
    {
        APP_ErrorHandler();
        return;
    }
    LL_RCC_HSI_Enable();
    timeout = BSP_RCC_READY_TIMEOUT;
    while (LL_RCC_HSI_IsReady() == 0U)
    {
        if (timeout-- == 0U)
        {
            APP_ErrorHandler();
            return;
        }
    }
    LL_RCC_SetHSIDiv(LL_RCC_HSI_DIV_1);
    LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
    LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_1);
    LL_RCC_HSI_SetCalibFreq(LL_RCC_HSICALIBRATION_72MHz);
    LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_HSISYS);
    timeout = BSP_RCC_READY_TIMEOUT;
    while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_HSISYS)
    {
        if (timeout-- == 0U)
        {
            APP_ErrorHandler();
            return;
        }
    }
    SystemCoreClockUpdate();

    /* 保留原 SysTick 的周期设置及使能状态；休眠时间不补计入软件节拍。 */
    SysTick->VAL = 0U;
    SysTick->CTRL = systick_ctrl & (SysTick_CTRL_ENABLE_Msk |
                                  SysTick_CTRL_TICKINT_Msk |
                                  SysTick_CTRL_CLKSOURCE_Msk);
    MODIFY_REG(SCB->SCR, SCB_SCR_SLEEPONEXIT_Msk | SCB_SCR_SEVONPEND_Msk,
               sleep_config & (SCB_SCR_SLEEPONEXIT_Msk | SCB_SCR_SEVONPEND_Msk));
    if (pwr_clock_enabled == 0U)
    {
        LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_PWR);
    }
    __DSB();
    __ISB();
    __set_PRIMASK(primask);
}
