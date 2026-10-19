/**
 ***********************************************************************************************************************
 * @file    app_system.c
 * @brief   System application test.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-24
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */
#include "app_system.h"
#include "app_spray_track.h"
#include "../Middleware/mist_ctrl/mist_ctrl.h"

mist_ctrl_t spray_track;
extern modbus_rtu_t modbus;
/*******************************************************************************
//  Function   : mobdus_ctrl_func
//  Description: Modbus控制参数接收完成回调，根据控制指令启动或停止喷雾追频
//  Param      : pItem - Modbus轮询队列消息项指针
//  Return     : 无
//  Others     : 当前主要处理追频使能控制，UsartExti控制功能待完善
//  Author     : Leo Chen 2026-08-26
*******************************************************************************/
void app_spray_track_Init(void)
{
    static const mist_api_t spray_api =
	{
		.setFreq = bsp_pwm_SetFrequency,
		.setDuty = bsp_pwm_SetDuty
	};

    if(mist_ctrl_init(&spray_track,&g_mistCfg,&spray_api))
	{
        mist_ctrl_stop(&spray_track);
	}
    bsp_pwm_SetDuty(0);

}

/*******************************************************************************
//  Function   : mobdus_show_func
//  Description: 更新Modbus监控参数，采集喷雾ADC、PWM及追频运行状态
//  Param      : pItem - Modbus轮询队列消息项指针
//  Return     : 无
//  Others     : 在监控参数发送前调用，确保上传数据为当前最新状态
//  Author     : Leo Chen 2026-08-26
*******************************************************************************/
void app_spray_track_stop(void)
{
    if(mist_ctrl_status(&spray_track)->state != MIST_STATE_IDLE)
    {
        mist_ctrl_stop(&spray_track);
    }
}

/*******************************************************************************
//  Function   : mobdus_show_func
//  Description: 更新Modbus监控参数，采集喷雾ADC、PWM及追频运行状态
//  Param      : pItem - Modbus轮询队列消息项指针
//  Return     : 无
//  Others     : 在监控参数发送前调用，确保上传数据为当前最新状态
//  Author     : Leo Chen 2026-08-26
*******************************************************************************/
void app_spray_track_start(void)
{
    if(mist_ctrl_status(&spray_track)->state == MIST_STATE_IDLE)
    {
        mist_ctrl_start(&spray_track, 0);
    }
}

/*******************************************************************************
//  Function   : mobdus_comm_config
//  Description: 配置并初始化Modbus主机轮询通信
//  Param      : 无
//  Return     : 无
//  Others     : 配置监控参数写消息和控制参数读消息
//  Author     : Leo Chen 2026-08-26
*******************************************************************************/
void app_spray_track_task(void)
{
    if(System.Isforce)
    {
        bsp_pwm_SetDuty(System.forceDuty);
        bsp_pwm_SetFrequency(System.forceFre);
    }
    else if(System.Istrak)
    {
        app_spray_track_start();
        mist_ctrl_task(&spray_track,
						SprayAd_GetFilteredValue(),
						SprayAd_GetSequence());
        bsp_pwm_SetDuty(System.trak_duty);
    }
    else
    {
        app_spray_track_stop();
        bsp_pwm_SetDuty(0);
    }
}
