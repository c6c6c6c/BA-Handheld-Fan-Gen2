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
    uint32_t cycle_count;             /**< 等效完整放电循环数，整数部分 */
    float    soc;                     /**< SOC，范围 0.0~100.0% */
    float    soh;                     /**< SOH，范围 0.0~100.0% */
    float    remaining_capacity_mah;  /**< 当前剩余容量，单位 mAh */
    uint32_t charge_mah_total;        /**< 累计充入电量，单位 mAh */
    uint32_t discharge_mah_total;     /**< 累计放出电量，单位 mAh */
} battery_soc_t;


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
    const battery_config_t *configInfo;               /**< 电池配置参数，运行期间只读 */

    battery_state_t         state;                      /**< 当前电池工作状态 */
  
    double                  usb_in_v;                   /**< usb输入电压，单位 V */
    double                  voltage_v;                  /**< 电池电压，单位 V */
    double                  current_ma;                 /**< 电池电流，单位 mA */

    battery_soc_t           soc_info;                    /**< soc 参数 */
    battery_soc_t           soc_persist;                /**< 掉电保存数据 */

    bms_protect_fault_t     fault_real;                 /**< 实时故障 */
    bms_protect_fault_t     fault_hold;                 /**< 保持故障 */

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
#define SETBIT(x, bit)              ((x) |= (1U << (bit)))
#define GETBIT(x, bit)              (((x) >> (bit)) & 0x01U)
#define CLRBIT(x, bit)              ((x) &= ~(1U << (bit)))

/* SOC 估算参数：默认 OCV 曲线需要按所选电芯实测标定。 */
#define BMS_SOC_PERCENT_FULL               100.0f     /* SOC 百分比满量程 */
#define BMS_SOC_PERMILLE_PER_PERCENT       10.0f      /* 千分比换算成百分比的系数 */
#define BMS_SOC_MS_PER_SECOND              1000.0f    /* 秒到毫秒换算 */
#define BMS_SOC_MS_PER_HOUR                3600000.0f /* 小时到毫秒换算 */
#define BMS_SOC_OCV_CORRECTION_TAU_MS      600000.0f  /* 静置 OCV 柔性修正时间常数 */
#define BMS_SOC_ENDPOINT_HOLD_MS           10000U     /* 放空端点连续确认时间 */
#define BMS_SOC_MAX_UPDATE_MS              1000U      /* 超过此采样间隔不外推库仑电量 */

/* SOH 经验模型示例：累计放出一次设计容量记作一个等效完整循环。 */
#define BMS_SOH_REFERENCE_CYCLES           500U       /* 参考循环次数，需按实际电芯寿命数据标定 */
#define BMS_SOH_AT_REFERENCE_PERCENT       80.0f      /* 达到参考循环次数时的假定 SOH，单位 % */
#define BMS_SOH_INITIAL_PERCENT            100.0f     /* 零循环时的估算 SOH，单位 % */
#define BMS_SOH_MIN_PERCENT                0.0f       /* SOH 显示下限 */

/* 掉电恢复时的 SOC/剩余容量一致性容差。 */
#define BMS_PERSIST_SOC_TOLERANCE_PERCENT  0.1f

/* Software Protection ------------------------------------------------------------------------------------------------*/







#endif /* _BMS_CONFIG_H_ */
