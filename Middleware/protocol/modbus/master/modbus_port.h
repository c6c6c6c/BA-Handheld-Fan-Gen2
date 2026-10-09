/**
 ***********************************************************************************************************************
 * @file    modbus_port.h
 * @brief   Modbus RTU master polling queue implementation.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-19
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */
#ifndef __MODBUS_PORT_H
#define __MODBUS_PORT_H

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "common_port.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Exported functions prototypes --------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : ModbusPort_1msTask
//  Description: 执行Modbus物理端口1ms周期任务
//  Param      : 无
//  Return     : 无
//  Others     : 内部调用common_port不定长接收计时接口
*******************************************************************************/
void ModbusPort_1msTask(void);

extern const common_port_api_t s_modbusPortApi;

#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_PORT_H */
