
/**
 ***********************************************************************************************************************
 * @file    app_mist.c
 * @brief   雾化片应用层控制，实现 PWM 输出管理及自动追频调度
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

/** 雾化片追频控制器实例，保存追频状态及运行参数 */
mist_ctrl_t mist_ctrl;


/** 雾化片 PWM 硬件操作接口，由 TIM16 提供频率及占空比控制 */
const mist_api_t mist_api =
{
    .setFreq = bsp_tim16_ch1_pwm_setfreq,
    .setDuty = bsp_tim16_ch1_pwm_setduty
};


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_mist_Init
//  Description: 初始化雾化片追频控制模块
//  Param      : None
//  Return     : None
//  Others     : 绑定追频配置及 PWM 操作接口，初始化成功后停止追频；
//               初始化结束时将 PWM 占空比设置为 0，避免雾化片误启动
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void app_mist_Init(void)
{
    /* 初始化追频控制器，绑定算法参数及底层 PWM 接口。 */
    if (mist_ctrl_init(&mist_ctrl, &g_mistCfg, &mist_api))
    {
        /* 初始化成功后保持追频停止状态。 */
        mist_ctrl_stop(&mist_ctrl);
    }

    /* 关闭 PWM 有效输出，确保初始化后不驱动雾化片。 */
    bsp_tim16_ch1_pwm_setduty(0);
}


/*******************************************************************************
//  Function   : app_mist_stop
//  Description: 停止雾化片自动追频
//  Param      : None
//  Return     : None
//  Others     : 仅在追频控制器非空闲状态时执行停止操作，避免重复调用
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void app_mist_stop(void)
{
    /* 已经处于空闲状态时不重复执行停止操作。 */
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
//  Others     : 仅在追频控制器处于空闲状态时启动，防止重复初始化追频过程
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void app_mist_start(void)
{
    /* 空闲状态下允许启动追频。 */
    if (mist_ctrl_status(&mist_ctrl)->state == MIST_STATE_IDLE)
    {
        mist_ctrl_start(&mist_ctrl, 0);
    }
}


/*******************************************************************************
//  Function   : app_mist_task
//  Description: 执行雾化片 PWM 控制及自动追频周期任务
//  Param      : None
//  Return     : None
//  Others     : 根据系统控制模式选择固定 PWM 输出、自动追频或停止输出；
//               自动追频模式下使用雾化片 ADC 滤波值及采样序号更新算法
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void app_mist_task(void)
{
    /*
     * 强制控制模式：
     * 由上层直接指定 PWM 占空比和输出频率。
     * 当前功能暂未启用。
     */
    if (0) // System.Isforce
    {
        // mist_api.setDuty(System.forceDuty);
        // mist_api.setFreq(System.forceFre);
    }
    /*
     * 自动追频模式：
     * 启动追频控制器，使用最新 ADC 采样值和更新序号执行追频算法。
     * 当前功能暂未启用。
     */
    else if (0) // System.Istrak
    {
        app_mist_start();

        mist_ctrl_task(&mist_ctrl, get_mist_val(), get_mist_seq());

        // mist_api.setFreq(System.trak_duty);
    }
    else
    {
        /* 非工作模式下停止追频，并将 PWM 输出频率设置为 0。 */
        app_mist_stop();
        mist_api.setFreq(0);
    }
}
