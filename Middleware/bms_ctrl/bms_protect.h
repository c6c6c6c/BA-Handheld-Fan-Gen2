/**
 ***********************************************************************************************************************
 * @file    bms_protect.h
 * @brief   BMS 软件保护配置及接口定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-16
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_PROTECT_H_
#define _BMS_PROTECT_H_

#include "bms_config.h"


/* Protection Parameters ---------------------------------------------------------------------------------------------*/

/** 采样超时时间，单位 ms；0 表示未配置 */
#define BMS_PROTECT_SAMPLE_TIMEOUT_MS        0U

/** 电池欠压保护阈值，单位 V；0 表示未启用 */
#define BMS_PROTECT_BATTERY_LOW_V            0.0f

/** 电池过压保护阈值，单位 V；0 表示未启用 */
#define BMS_PROTECT_BATTERY_HIGH_V           0.0f

/** USB 输入欠压保护阈值，单位 V；0 表示未启用 */
#define BMS_PROTECT_USB_LOW_V                0.0f

/** USB 输入过压保护阈值，单位 V；0 表示未启用 */
#define BMS_PROTECT_USB_HIGH_V               0.0f

/** 主控板 NTC 过温保护阈值，单位 0.1°C；0 表示未启用 */
#define BMS_PROTECT_TEMP_HIGH_DECI_C         0

/** 电池过流保护阈值，单位 mA；按电流绝对值判断，0 表示未启用 */
#define BMS_PROTECT_CURRENT_HIGH_MA          0.0f

/** 实时故障消失后的保持时间，单位 s */
#define BMS_PROTECT_HOLD_TIME_S              20U

/** 当前支持的故障位数量 */
#define BMS_PROTECT_FAULT_NUM                7U

/** bms_protect_check() 建议调用周期，单位 ms */
#define BMS_PROTECT_RECOVER_TIME_MS          10U

/** 实时故障连续满足该时间后确认，单位 ms */
#define BMS_PROTECT_DETECT_TIME_MS           3U


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_protect_check
//  Description: 更新 BMS 实时故障及故障保持状态
//  Param      : battery - 电池实时运行数据，不可为 NULL
//  Return     : None
//  Others     : 根据 battery 中的电压、电流及相关状态执行保护判断，更新 fault_real
//               和 fault_hold；USB、温度等数据应由上层完成采集和换算，本函数不直接操作 GPIO
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
void bms_protect_check(bms_battery_t *battery);


#endif /* _BMS_PROTECT_H_ */
