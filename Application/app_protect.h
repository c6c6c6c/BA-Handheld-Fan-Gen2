
/**
 ***********************************************************************************************************************
 * @file    app_protect.h
 * @brief   APP 软件保护配置及接口，定义电池、USB、温度、电流保护参数与故障状态
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-16
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _APP_PROTECT_H_
#define _APP_PROTECT_H_

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "bms_config.h"


/* Protection Parameters ---------------------------------------------------------------------------------------------*/

/**
 * @brief BMS 采样数据超时时间
 * @note  单位 ms。
 *        用于判断 ADC 采样数据是否长时间未更新。
 *        设置为 0 表示不启用采样超时检测。
 */
#define PROTECT_SAMPLE_TIMEOUT_MS        0U

/**
 * @brief 电池欠压保护阈值
 * @note  单位 V。
 *        当电池电压低于配置阈值时，可触发欠压保护。
 *        设置为 0 表示不启用该保护。
 */
#define PROTECT_BATTERY_LOW_V            0.0f

/**
 * @brief 电池过压保护阈值
 * @note  单位 V。
 *        当电池电压高于配置阈值时，可触发过压保护。
 *        设置为 0 表示不启用该保护。
 */
#define PROTECT_BATTERY_HIGH_V           0.0f

/**
 * @brief USB 输入欠压保护阈值
 * @note  单位 V。
 *        当 USB 输入电压低于配置阈值时，可触发欠压保护。
 *        设置为 0 表示不启用该保护。
 */
#define PROTECT_USB_LOW_V                0.0f

/**
 * @brief USB 输入过压保护阈值
 * @note  单位 V。
 *        当 USB 输入电压高于配置阈值时，可触发过压保护。
 *        设置为 0 表示不启用该保护。
 */
#define PROTECT_USB_HIGH_V               0.0f

/**
 * @brief 主控板 NTC 过温保护阈值
 * @note  单位 0.1°C。
 *        例如 650 表示 65.0°C。
 *        设置为 0 表示不启用该保护。
 */
#define PROTECT_TEMP_HIGH_DECI_C         0

/**
 * @brief 电池过流保护阈值
 * @note  单位 mA。
 *        根据电池电流绝对值判断是否过流，
 *        因此充电及放电电流均可能触发该保护。
 *        设置为 0 表示不启用该保护。
 */
#define PROTECT_CURRENT_HIGH_MA          0.0f

/**
 * @brief 软件保护支持的故障位数量
 * @note  当前配置值为 7。
 *        具体故障种类由保护检测模块实现。
 */
#define PROTECT_FAULT_NUM                7U

/**
 * @brief 软件保护恢复检测时间参数
 * @note  单位 ms。
 *        当前配置为 10ms。
 *        具体用法及恢复条件由 app_protect.c 实现。
 */
#define PROTECT_RECOVER_TIME_MS          10U

/**
 * @brief 实时故障确认时间
 * @note  单位 ms。
 *        故障条件连续满足指定时间后，
 *        可将其确认为有效故障。
 *        当前配置为 3ms。
 */
#define PROTECT_DETECT_TIME_MS           3U


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief BMS 软件保护故障状态联合体
 * @note  用于表示电池、USB、温度及电流相关故障。
 *
 *        data：故障状态的 16 位整体表示。
 *        bit ：各故障状态的独立位域表示。
 *
 *        各故障位：
 *        batteryLow  - 电池欠压
 *        batteryHigh - 电池过压
 *        usbHigh     - USB 输入过压
 *        usbLow      - USB 输入欠压
 *        tempHigh    - 主控板温度过高
 *        currentHigh - 电池电流过大
 *
 *        位域排列顺序由编译器实现决定。
 *        跨平台通信时不应直接依赖位域的物理位顺序。
 */
typedef union
{
    uint16_t data;                         /**< 故障状态整体数据 */

    struct
    {
        uint16_t batteryLow   : 1;         /**< 电池欠压故障 */
        uint16_t batteryHigh  : 1;         /**< 电池过压故障 */
        uint16_t usbHigh      : 1;         /**< USB 输入过压故障 */
        uint16_t usbLow       : 1;         /**< USB 输入欠压故障 */
        uint16_t tempHigh     : 1;         /**< 主控板 NTC 过温故障 */
        uint16_t currentHigh  : 1;         /**< 电池充放电过流故障 */
    }bit;

} protect_fault_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_protect_task
//  Description: 执行 APP 软件保护检测及故障状态更新
//  Param      : None
//  Return     : None
//  Others     : 根据电池、USB、温度及电流采样数据检测保护条件；
//               结合故障确认时间和恢复时间参数处理故障状态；
//               更新实时故障及故障保持状态；
//               具体保护执行方式由 app_protect.c 实现
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
void app_protect_task(void);


#endif /* _APP_PROTECT_H_ */
