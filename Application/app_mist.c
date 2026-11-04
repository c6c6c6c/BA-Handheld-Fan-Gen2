
/**
 ***********************************************************************************************************************
 * @file    app_mist.c
 * @brief   雾化片应用层控制，实现 PWM 输出管理、脉冲雾化及自动追频调度
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_mist.h"
#include "app_system.h"
#include "app_sampling.h"

#include "mist_ctrl.h"
#include "bsp_tim16.h"


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief 雾化片自动追频控制器实例
 * @note  保存追频算法的配置、工作状态、目标频率及采样信息。
 */
mist_ctrl_t mist_ctrl;


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : set_mist_freq
//  Description: 更新系统雾化片 PWM 目标频率
//  Param      : Fre - 目标 PWM 频率，单位 Hz
//  Return     : None
//  Others     : 作为追频算法的频率设置回调函数；
//               仅更新系统逻辑层中的目标频率；
//               实际 PWM 输出由 app_mist_1ms_task() 统一更新
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
static void set_mist_freq(uint32_t Fre){System.logic.mist_freq = Fre;}


/*******************************************************************************
//  Function   : set_mist_Duty
//  Description: 更新系统雾化片 PWM 目标占空比
//  Param      : Duty - 目标 PWM 占空比参数
//  Return     : None
//  Others     : 作为追频算法的占空比设置回调函数；
//               仅更新系统逻辑层中的目标占空比；
//               实际 PWM 输出由 app_mist_1ms_task() 统一更新
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
static void set_mist_Duty(uint16_t Duty){System.logic.mist_duty = Duty;}


/* Private constants -------------------------------------------------------------------------------------------------*/

/**
 * @brief 雾化片追频算法硬件操作接口
 * @note  将追频算法输出的频率及占空比写入系统逻辑状态；
 *        PWM 硬件寄存器由周期任务通过 TIM16 驱动统一更新。
 */
const mist_api_t mist_api =
{
    .setFreq = set_mist_freq,
    .setDuty = set_mist_Duty
};


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_mist_Init
//  Description: 初始化雾化片自动追频及 PWM 控制模块
//  Param      : None
//  Return     : None
//  Others     : 绑定追频算法配置参数及 PWM 操作接口；
//               初始化成功后将追频控制器设置为停止状态；
//               初始化结束时将 TIM16 PWM 占空比设置为 0，
//               避免系统启动过程中误驱动雾化片
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void app_mist_Init(void)
{
    /* 初始化追频控制器，绑定算法配置及 PWM 操作接口。 */
    if (mist_ctrl_init(&mist_ctrl, &g_mistCfg, &mist_api))
    {
        /* 初始化成功后保持追频停止状态。 */
        mist_ctrl_stop(&mist_ctrl);
    }

    /* 将 PWM 占空比设置为 0，关闭雾化片有效输出。 */
    bsp_tim16_ch1_pwm_setduty(0);
}


/*******************************************************************************
//  Function   : app_mist_stop
//  Description: 停止雾化片自动追频
//  Param      : None
//  Return     : None
//  Others     : 检查当前追频状态；
//               仅在控制器不处于空闲状态时执行停止操作；
//               避免重复调用追频停止接口
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void app_mist_stop(void)
{
    /* 控制器未处于空闲状态时执行追频停止。 */
    if (mist_ctrl_status(&mist_ctrl)->state != MIST_STATE_IDLE)
    {
        mist_ctrl_stop(&mist_ctrl);
    }
}


/*******************************************************************************
//  Function   : app_mist_start
//  Description: 启动雾化片自动追频
//  Param      : None
//  Return     : None
//  Others     : 检查当前追频状态；
//               仅在控制器处于空闲状态时启动追频；
//               防止重复启动导致追频状态被重新初始化
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void app_mist_start(void)
{
    /* 控制器处于空闲状态时启动追频。 */
    if (mist_ctrl_status(&mist_ctrl)->state == MIST_STATE_IDLE)
    {
        mist_ctrl_start(&mist_ctrl, 0);
    }
}


/*******************************************************************************
//  Function   : app_mist_1ms_task
//  Description: 执行雾化片 PWM 控制及自动追频 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 根据雾化工作模式执行对应控制逻辑；
//               PULSATING 模式使用固定频率及周期性占空比控制；
//               CONTINUE 模式通过 ADC 采样值执行自动追频；
//               CLOSE 模式关闭 PWM 占空比并清零雾化时间；
//               当目标频率或占空比发生变化时更新 TIM16 PWM；
//               仅在硬件配置成功后保存最新 PWM 参数
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void app_mist_1ms_task(void)
{
    static uint32_t old_freq = 0U;
    static uint16_t old_duty = 0U;
    static uint16_t Cnt = 0U;

    /*
     * 非连续雾化模式下停止自动追频。
     *
     * 脉冲雾化和关闭模式均不执行追频算法，
     * 避免追频控制器继续修改 PWM 参数。
     */
    if (System.logic.mist_mode != MIST_MODE_CONTINUE)
    {
        app_mist_stop();
    }

    /* 根据当前雾化工作模式执行对应控制。 */
    switch ((uint8_t)System.logic.mist_mode)
    {
        case MIST_MODE_PULSATING:
        {
            /*
             * 脉冲雾化模式：
             *
             * 1. PWM 频率固定为 150kHz。
             * 2. Cnt 小于 1000 时，占空比设置为 200。
             * 3. Cnt 大于或等于 1000 时，占空比设置为 0。
             * 4. 通过周期计数形成间歇式雾化输出。
             *
             * 本任务按 1ms 周期调用时，
             * 对应约 1s 输出、约 1s 停止的工作方式。
             */
            System.logic.mist_freq = 150000U;
            System.logic.mist_duty = Cnt < 1000 ? 200U : 0U;


            /* 更新脉冲雾化周期计数器。 */
            Cnt = Cnt < 2000 ? Cnt + 1 : 0;
            break;
        }

        case MIST_MODE_CONTINUE:
        {
            /*
             * 连续雾化模式：
             *
             * 1. 检查并启动自动追频控制器。
             * 2. 获取雾化片 ADC 采样值。
             * 3. 获取对应的 ADC 采样序号。
             * 4. 调用追频算法更新目标频率及占空比。
             *
             * 追频算法通过 mist_api 回调更新
             * System.logic.mist_freq 和 mist_duty。
             */
            app_mist_start();

            mist_ctrl_task(&mist_ctrl, get_mist_val(), get_mist_seq());
            break;
        }

        case MIST_MODE_CLOSE:
        {
            /*
             * 雾化关闭模式：
             *
             * 1. 将 PWM 占空比设置为 0。
             * 2. 清除累计雾化时间。
             * 3. 不修改当前保存的 PWM 频率。
             */
            System.logic.mist_duty = 0U;
            System.logic.mistTimes = 0;
            break;
        }
    }

    /*
     * 雾化有效输出时间累计。
     *
     * 当目标 PWM 占空比不为 0 时，
     * 每执行一次周期任务累计一次雾化时间。
     *
     * 注意：该统计与脉冲模式分支中的时间累计
     * 同时存在，脉冲开启阶段会累计两次。
     */
    if (System.logic.mist_duty != 0)
    {
        System.logic.mistTimes++;
    }

    /*
     * PWM 参数变化检测：
     *
     * 1. 比较当前目标频率与上一次成功设置的频率。
     * 2. 比较当前目标占空比与上一次成功设置的占空比。
     * 3. 任意参数变化时调用 TIM16 PWM 配置接口。
     * 4. 仅当接口返回 SUCCESS 时更新缓存值。
     *
     * 这样可以避免在参数未变化时重复配置 PWM 寄存器。
     */
    if ((System.logic.mist_freq != old_freq)
    ||  (System.logic.mist_duty != old_duty))
    {
        if (bsp_tim16_ch1_pwm_set(System.logic.mist_freq,System.logic.mist_duty) == SUCCESS)
        {
            /* 记录最新配置成功的 PWM 参数。 */
            old_freq = System.logic.mist_freq;
            old_duty = System.logic.mist_duty;
        }
    }
}
