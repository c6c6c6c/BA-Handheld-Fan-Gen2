/**
 ***********************************************************************************************************************
 * @file    app_io_ctrl.h
 * @brief   APP IO 控制接口，定义电池、USB 输入通路控制宏及 IO 控制状态
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-16
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _APP_IO_CTRL_H_
#define _APP_IO_CTRL_H_

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include <stdint.h>
#include "bsp_gpio.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/

/** USB/VIN 输入通路控制：高电平使能，低电平关闭。 */
#define BMS_IO_VIN_ENABLE()         LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN)
#define BMS_IO_VIN_DISABLE()        LL_GPIO_ResetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN)

/** VBAT 电池通路控制：高电平使能，低电平关闭。 */
#define BMS_IO_VBAT_ENABLE()        LL_GPIO_SetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN)
#define BMS_IO_VBAT_DISABLE()       LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN)

/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 设备 IO 控制状态联合体
 * @note  data：IO 控制状态的 8 位整体表示。
 *        bit ：各通路控制状态的独立位域表示。
 *        控制位为 1 表示使能对应通路，为 0 表示关闭对应通路。
 *        本类型仅保存软件控制状态，不表示 GPIO 引脚反馈。
 *        位域排列及存储布局由编译器决定，不应直接用于跨平台通信。
 */
typedef union
{
    uint8_t data;                       /**< IO 控制状态集合 */
    struct
    {
        uint8_t vbat_ctrl : 1;           /**< 电池通路使能控制 */
        uint8_t vin_ctrl  : 1;           /**< USB/VIN 输入通路使能控制 */
		uint8_t usb_in    : 1;           /**< USB/VIN 输入通路使能控制 */
        uint8_t res       : 6;           /**< 保留位 */
    } bit;
} dev_io_t;

/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_io_ctrl_task
//  Description: 执行 APP IO 控制周期任务
//  Param      : None
//  Return     : None
//  Others     : 将逻辑控制状态复制到实际输出控制状态，并更新 GPIO 输出；
//               每周期将逻辑控制值恢复为默认值：USB 输入通路关闭、电池通路使能；
//               应在本周期其他模块完成逻辑控制值更新后调用
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
void app_io_ctrl_task(void);

#endif /* _APP_IO_CTRL_H_ */
