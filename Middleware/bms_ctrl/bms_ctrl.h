/**
 ***********************************************************************************************************************
 * @file    bms_ctrl.h
 * @brief   BMS 通路控制与模块协调接口，可选调度 KEY、状态采集和软件保护
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-13
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_CTRL_H_
#define _BMS_CTRL_H_
#include "bms_config.h"
#include <stdbool.h>

/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 电池 ADC 换算结果
 * @note 电流正值表示放电，负值表示充电；无效采样时对应数值置 0。
 */
typedef struct
{
    double             voltageV;       /**< 电池电压，单位 V */
    double             currentA;       /**< 电池电流，单位 A */
    bms_power_state_t  state;          /**< 电池状态 */
} bms_battery_data_t;

/* 电池 ADC 换算参数 -------------------------------------------------------------------------------------------------*/
/* ADCxConvertedDatas[] 使用逻辑通道索引，不能用 LL_ADC_CHANNEL_OPA2/VREFINT 作为数组下标。 */
#define BMS_BATTERY_ADC_FULL_SCALE              4095U          /**< 12 位右对齐 ADC 满量程码值 */
#define BMS_BATTERY_VREFINT_V                   1.2            /**< VREFINT 典型值，单位 V；精确测量需逐机校准 */
#define BMS_BATTERY_PGA_GAIN                    16.0           /**< OPA2 同相 PGA 增益，与 bsp_pga2_Init() 保持一致 */
#define BMS_BATTERY_PGA_ZERO_RATIO              0.5            /**< 零电流时 OPA2 输出占 VCCA 的比例，目标偏置为 VCCA/2 */
#define BMS_BATTERY_SHUNT_OHM                   0.01           /**< USB 板 R18 标称 10 mΩ；PGA2 跨接此电阻 */
#define BMS_BATTERY_CURRENT_SIGN                1.0            /**< 正数表示放电；若实测极性相反改为 -1.0 */
#define BMS_BATTERY_CURRENT_ADC_INDEX           ADC_CH_PGA2    /**< OPA2 输出对应的 ADC 扫描序号 */
#define BMS_BATTERY_VREFINT_ADC_INDEX           ADC_CH_VREFINT /**< 内部基准对应的 ADC 扫描序号 */

/* Function ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_ctrl_ReadBatteryData
//  Description: 读取 ADC 采样并更新电池电压、电流结构体
//  Param      : data - 输出电池采样结果，不可为 NULL
//  Return     : true - 电压和电流均有效；false - 参数或采样无效
//  Others     : 使用前须初始化并启动 ADC/PGA2；应在同一任务上下文读取
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
bool bms_ctrl_ReadBatteryData(bms_battery_data_t *data);



#endif /* _BMS_CTRL_H_ */
