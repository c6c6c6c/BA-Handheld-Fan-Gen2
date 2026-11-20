
/**
 ***********************************************************************************************************************
 * @file    bsp_lptim.c
 * @brief   LPTIM 低功耗定时器驱动，实现 STOP 模式定时唤醒及单次计数控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-11-19
 ***********************************************************************************************************************
 * @note
 *          1. 使用 LSI 作为 LPTIM1 时钟源。
 *          2. 通过预分频器降低计数频率。
 *          3. 使用单次计数模式产生 ARR 匹配中断。
 *          4. 通过 EXTI Line27 实现 STOP 模式唤醒。
 *          5. 每次重新启动前需关闭 LPTIM 并执行恢复延时。
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "bsp_lptim.h"


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : lptim_restart_delay
//  Description: 执行 LPTIM 重新启动前的软件延时
//  Param      : None
//  Return     : None
//  Others     : 使用 volatile 计数变量及 NOP 指令进行延时；
//               对应 HAL 例程中的 APP_DelayNops(900)；
//               用于 LPTIM 关闭后重新启动前的等待；
//               实际延时时间受系统主频及编译器优化影响
//  Author     : Leo Chen 2026-11-19
*******************************************************************************/
static void lptim_restart_delay(void)
{
    volatile uint32_t i = LPTIM_RESTART_DELAY_NOP;

    while (i--)
    {
        __NOP();
    }
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_lptim_Init
//  Description: 初始化 LPTIM1 低功耗定时器及中断唤醒功能
//  Param      : None
//  Return     : None
//  Others     : 使能 EXTI Line27 中断通路并关闭事件通路；
//               使能 LSI 振荡器并等待时钟稳定；
//               选择 LSI 作为 LPTIM1 时钟源；
//               使能 LPTIM1 外设时钟并配置预分频器；
//               配置 ARR 寄存器立即更新模式；
//               设置 LPTIM1 中断优先级并使能 ARR 匹配中断；
//               用于系统 STOP 模式下的周期性定时唤醒
//  Author     : Leo Chen 2026-11-19
*******************************************************************************/
void bsp_lptim_Init(void)
{
    LL_LPTIM_InitTypeDef LPTIM_InitStruct = {0};

    /*
     * 配置 LPTIM1 对应的 EXTI 唤醒通路。
     *
     * PY32F032 使用 EXTI Line27。
     * 使能中断请求，关闭事件请求。
     */
    LL_EXTI_EnableIT(LL_EXTI_LINE_27);
    LL_EXTI_DisableEvent(LL_EXTI_LINE_27);

    /*
     * 配置 LPTIM1 时钟源。
     *
     * 1. 使能 LSI 内部低速振荡器。
     * 2. 等待 LSI 时钟稳定。
     * 3. 将 LPTIM1 时钟源切换为 LSI。
     * 4. 使能 LPTIM1 外设时钟。
     */
    LL_RCC_LSI_Enable();

    while (LL_RCC_LSI_IsReady() != 1)
    {
    }

    LL_RCC_SetLPTIMClockSource(LL_RCC_LPTIM1_CLKSOURCE_LSI);
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_LPTIM1);

    /*
     * 配置 LPTIM1 计数参数。
     *
     * Prescaler ：使用 LPTIM_PRESCALER_VALUE 定义的分频系数。
     * UpdateMode：立即更新 ARR 和比较寄存器配置。
     *
     * 若 LSI = 32768Hz，预分频系数为 32，
     * 则 LPTIM 计数频率为 1024Hz。
     */
    LPTIM_InitStruct.Prescaler  = LPTIM_PRESCALER_VALUE;
    LPTIM_InitStruct.UpdateMode = LL_LPTIM_UPDATE_MODE_IMMEDIATE;

    /* 初始化 LPTIM1，失败时进入系统错误处理。 */
    if (LL_LPTIM_Init(LPTIM, &LPTIM_InitStruct) != SUCCESS)
    {
        APP_ErrorHandler();
    }

    /*
     * 配置 LPTIM1 中断控制器。
     *
     * 中断优先级：0。
     * 允许 LPTIM1 中断请求进入 CPU。
     */
    NVIC_SetPriority(LPTIM1_IRQn, 0);
    NVIC_EnableIRQ(LPTIM1_IRQn);

    /*
     * 使能 ARR 匹配中断。
     *
     * 当计数器达到自动重装载值时，
     * 置位 ARRM 标志并产生中断请求。
     */
    LL_LPTIM_EnableIT_ARRM(LPTIM);
}


/*******************************************************************************
//  Function   : lptim_start
//  Description: 启动 LPTIM1 单次定时计数
//  Param      : None
//  Return     : None
//  Others     : 使能 LPTIM1 外设；
//               设置自动重装载寄存器 ARR；
//               使用单次计数模式启动定时器；
//               ARR 匹配后产生中断，用于唤醒 STOP 模式下的 CPU；
//               具体定时时间由 LSI 频率、预分频系数及 ARR 决定
//  Author     : Leo Chen 2026-11-19
*******************************************************************************/
void lptim_start(void)
{
    /* 使能 LPTIM1 外设。 */
    LL_LPTIM_Enable(LPTIM);

    /*
     * 设置自动重装载值。
     *
     * 当计数频率为 1024Hz，
     * ARR 配置为 1023 时，理论定时周期约为 1s。
     */
    LL_LPTIM_SetAutoReload(LPTIM, LPTIM_AUTORELOAD_VALUE);

    /*
     * 启动单次计数模式。
     *
     * 计数到 ARR 匹配后停止当前计数过程，
     * 并通过 ARR 匹配中断唤醒 CPU。
     */
    LL_LPTIM_StartCounter(LPTIM, LL_LPTIM_OPERATING_MODE_ONESHOT);
}


/*******************************************************************************
//  Function   : lptim_stop
//  Description: 停止 LPTIM1 并准备下一次单次计数
//  Param      : None
//  Return     : None
//  Others     : 关闭 LPTIM1 外设；
//               执行软件恢复延时；
//               避免连续重新启动时内部状态尚未恢复；
//               对应 HAL 例程中的 LPTIM Disable 及 NOP 延时
//  Author     : Leo Chen 2026-11-19
*******************************************************************************/
void lptim_stop(void)
{
    /* 关闭 LPTIM1 外设。 */
    LL_LPTIM_Disable(LPTIM);

    /* 等待 LPTIM 内部状态恢复，为下一次启动做准备。 */
    lptim_restart_delay();
}

/*******************************************************************************
//  Function   : LPTIM1_IRQHandler
//  Description: 清除单次定时到期中断，允许休眠流程继续喂狗
//  Param      : None
//  Return     : None
//  Others     : 不清除休眠锁；定时到期后由 enter_stop_mode() 再次进入 STOP，
//               USB 或电源按键中断才负责请求退出休眠
//  Author     : Chen Liang 2026-11-19
*******************************************************************************/
void LPTIM1_IRQHandler(void)
{
    if (LL_LPTIM_IsEnabledIT_ARRM(LPTIM) && LL_LPTIM_IsActiveFlag_ARRM(LPTIM))
    {
        LL_LPTIM_ClearFLAG_ARRM(LPTIM);
    }
}
