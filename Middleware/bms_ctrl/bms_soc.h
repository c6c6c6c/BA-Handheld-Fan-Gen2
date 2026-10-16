/**
 ***********************************************************************************************************************
 * @file    bms_soc.h
 * @brief   BMS SOC 估算接口，使用框架的 bms_battery_t 保存估算结果
 ***********************************************************************************************************************
 */

#ifndef BMS_SOC_H
#define BMS_SOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "bms_config.h"

/** 默认 OCV-SOC 曲线中的单个标定点。 */
typedef struct
{
    float    voltage_v;     /**< 单节电芯开路电压，单位 V */
    uint16_t soc_permille;  /**< SOC 千分比：1000 对应 100% */
} BmsSocOcvPoint;

/*******************************************************************************
//  Function   : bms_soc_from_ocv
//  Description: 根据默认 OCV 曲线查表估算 SOC
//  Param      : voltage_v - 静置电压，单位 V
//  Return     : SOC，单位 %；无效输入返回 0
//  Others     : 仅适用于无负载且电压稳定时；默认曲线应按实际电芯重新标定
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
float bms_soc_from_ocv(float voltage_v);

/*******************************************************************************
//  Function   : bms_soc_init
//  Description: 初始化单实例 SOC 估算器
//  Param      : battery             - 已设置 configInfo 的电池运行数据
//               initial_soc_percent - 上层确定的初始 SOC，单位 %，范围 0~100
//  Return     : true：初始化成功；false：配置或初始 SOC 非法
//  Others     : 不自动猜测初始电量；可传入已校验的掉电保存 SOC
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
bool bms_soc_init(bms_battery_t *battery, float initial_soc_percent);

/*******************************************************************************
//  Function   : bms_soc_update
//  Description: 更新库仑计量、静置 OCV 及端点修正
//  Param      : battery    - 电池运行数据；current_ma 为 mA，正放电、负充电
//               elapsed_ms - 距离上次有效采样的时间，单位 ms
//  Return     : true：更新成功；false：未初始化、配置改变或采样无效
//  Others     : 仅在初始化成功后调用；SOH 由 bms_soh 模块按累计循环数维护
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
bool bms_soc_update(bms_battery_t *battery, uint32_t elapsed_ms);

#endif /* BMS_SOC_H */
