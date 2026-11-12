/**
 ***********************************************************************************************************************
 * @file    app_io_ctrl.c
 * @brief   APP IO 控制管理，实现逻辑控制状态更新及电池、USB 输入通路输出控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_io_ctrl.h"
#include "app_system.h"
#include "fan_protocol.h"

/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_io_update
//  Description: 更新 IO 输出状态，并恢复下一周期的默认逻辑控制值
//  Param      : None
//  Return     : None
//  Others     : 将当前逻辑控制值复制到实际输出控制状态；
//               随后将逻辑控制值恢复为默认值：USB 输入通路关闭、电池通路使能；
//               其他模块如需改变下一周期输出，应在下一次调用本函数前更新逻辑控制值
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
static void app_io_update(void)
{
    /* 保存本周期需要输出的控制状态。 */
    System.io_real.bit.vbat_ctrl = System.io_logic.bit.vbat_ctrl;
    System.io_real.bit.vin_ctrl  = System.io_logic.bit.vin_ctrl;
	
	//协议支持禁止充电
	if(protocol.IsConnect && protocol_data.ctrl.prohibit_charging)
	{
		System.io_real.bit.vin_ctrl = true;
	}


    /* 恢复下一周期的默认逻辑控制值。 */
    System.io_logic.bit.vin_ctrl  = false;
    System.io_logic.bit.vbat_ctrl = true;
}

/*******************************************************************************
//  Function   : app_io_output
//  Description: 根据实际输出控制状态设置电池及 USB 输入通路使能引脚
//  Param      : None
//  Return     : None
//  Others     : vbat_ctrl 为 true 时输出高电平，使能电池通路；
//               vin_ctrl 为 true 时输出高电平，使能 USB 输入通路；
//               对应控制值为 false 时输出低电平，关闭相应通路
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
static void app_io_output(void)
{
    /* 电池通路控制：高电平使能，低电平关闭。 */
    if (System.io_real.bit.vbat_ctrl)
    {
        LL_GPIO_SetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    }

    /* USB 输入通路控制：高电平使能，低电平关闭。 */
    if (System.io_real.bit.vin_ctrl)
    {
        LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    }
}

/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_io_ctrl_task
//  Description: 执行 APP IO 控制周期任务
//  Param      : None
//  Return     : None
//  Others     : 先更新实际输出控制状态并恢复默认逻辑控制值，再设置 GPIO 输出；
//               应在本周期其他模块完成逻辑控制值更新后调用
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
void app_io_ctrl_task(void)
{
    app_io_update();
    app_io_output();
}
