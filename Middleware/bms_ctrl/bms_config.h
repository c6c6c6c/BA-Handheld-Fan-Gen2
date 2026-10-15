/**
 ***********************************************************************************************************************
 * @file    bms_config.h
 * @brief   BMS 模块配置，包含 IO 适配、电池参数及软件保护配置
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-13
 ***********************************************************************************************************************
 * @note
 *          1. 本文件统一管理 BMS 模块相关配置及板级 IO 适配。
 *          2. GPIO 引脚映射统一来自 bsp_gpio.h，避免底层初始化与业务层使用不一致。
 *          3. VIN_EN / VBAT_EN / CTL 等控制统一通过宏封装。
 *          4. 软件保护阈值需根据实际电池及产品参数配置。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_CONFIG_H_
#define _BMS_CONFIG_H_

#include <stdbool.h>
#include <stdint.h>

#include "bsp_gpio.h"


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 电池工作状态
 */
typedef enum
{
    BAT_STATE_IDLE = 0,          /**< 空闲状态 */
    BAT_STATE_CHARGING,          /**< 充电状态 */
    BAT_STATE_DISCHARGING,       /**< 放电状态 */
    BAT_STATE_FULL,              /**< 满电状态 */
    BAT_STATE_FAULT              /**< 故障状态 */
} battery_state_t;


/**
 * @brief 电池掉电保存数据
 */
typedef struct
{
    uint32_t cycle_count;             /**< 电池循环次数 */
    float soc;                        /**< SOC，范围 0.0~100.0% */
    float soh;                        /**< SOH，范围 0.0~100.0% */
    float remaining_capacity_mah;     /**< 当前剩余容量，单位 mAh */
    uint32_t charge_mah_total;        /**< 累计充入电量，单位 mAh */
    uint32_t discharge_mah_total;     /**< 累计放出电量，单位 mAh */
    uint32_t checksum;                /**< 掉电保存数据校验值 */
} battery_persist_t;


/**
 * @brief 电池配置参数
 */
typedef struct
{
    float design_capacity_mah;        /**< 电池设计容量，单位 mAh */
    float full_voltage_v;             /**< 满充电压，单位 V */
    float cutoff_voltage_v;           /**< 放电截止电压，单位 V */
    float rest_current_ma;            /**< 静置电流阈值，单位 mA */
    float rest_dvdt_v_per_s;          /**< 静置电压变化率阈值，单位 V/s */
    uint32_t rest_time_ms;            /**< OCV 修正所需静置时间，单位 ms */
    float charge_efficiency;          /**< 充电库仑效率 */
    float discharge_efficiency;       /**< 放电库仑效率 */
} battery_config_t;


/**
 * @brief BMS 实时及保持故障位域
 * @note  只按成员名访问，不依赖编译器的位域排列顺序。
 */
typedef union
{
    uint16_t data;
    struct
    {
        uint16_t batteryLow   : 1;     /**< 电池欠压 */
        uint16_t batteryHigh  : 1;     /**< 电池过压 */
        uint16_t usbHigh      : 1;     /**< USB 输入过压 */
        uint16_t usbLow       : 1;     /**< USB 输入欠压 */
        uint16_t tempHigh     : 1;     /**< 温度过高 */
        uint16_t currentHigh  : 1;     /**< 电流过大 */
    }bit;
} bms_protect_fault_t;

/**
 * @brief 电池实时运行数据
 */
typedef struct
{
    battery_config_t     *configInfo;                 /**< 电池配置参数 */
    float                 usb_in_v;                   /**< usb输入电压，单位 V */
    float                 voltage_v;                  /**< 电池电压，单位 V */
    float                 current_ma;                 /**< 电池电流，单位 mA */
    float                 soc;                        /**< 当前 SOC，范围 0.0~100.0% */
    float                 soh;                        /**< 当前 SOH，范围 0.0~100.0% */
    float                 remaining_capacity_mah;     /**< 当前剩余容量，单位 mAh */
    uint32_t              charge_mah_total;           /**< 累计充入电量，单位 mAh */
    uint32_t              discharge_mah_total;        /**< 累计放出电量，单位 mAh */
    battery_state_t       state;                      /**< 当前电池工作状态 */
    battery_persist_t     persist;                    /**< 掉电保存数据 */
    bms_protect_fault_t   fault_real;                 /**< 实时故障 */
    bms_protect_fault_t   fault_hold;                 /**< 保持故障 */

} bms_battery_t;



/* IO Adapt ----------------------------------------------------------------------------------------------------------*/

/** VIN 输入通路控制 */
#define BMS_IO_VIN_ENABLE()         LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN)
#define BMS_IO_VIN_DISABLE()        LL_GPIO_ResetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN)

/** VBAT 电池通路控制 */
#define BMS_IO_VBAT_ENABLE()        LL_GPIO_SetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN)
#define BMS_IO_VBAT_DISABLE()       LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN)

/** BMS CTL 电平控制 */
#define BMS_IO_CTL_HIGH()           LL_GPIO_SetOutputPin(BMS_CTL_PORT, BMS_CTL_PIN)
#define BMS_IO_CTL_LOW()            LL_GPIO_ResetOutputPin(BMS_CTL_PORT, BMS_CTL_PIN)

/** USB 输入状态检测 */
#define BMS_IO_USB_IS_PRESENT()     (LL_GPIO_IsInputPinSet(BMS_USB_IN_PORT, BMS_USB_IN_PIN) != 0U)


/* Control Parameters ------------------------------------------------------------------------------------------------*/


/* Software Protection ------------------------------------------------------------------------------------------------*/







#endif /* _BMS_CONFIG_H_ */
