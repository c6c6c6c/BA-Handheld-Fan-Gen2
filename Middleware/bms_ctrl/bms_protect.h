/**
 ***********************************************************************************************************************
 * @file    bms_protect.h
 * @brief   BMS 实时故障判断与保持故障记录
 * @note    仅使用 bms_config.h 中已有的 bms_battery_t / bms_protect_fault_t。
 ***********************************************************************************************************************
 */

#ifndef _BMS_PROTECT_H_
#define _BMS_PROTECT_H_

#include "bms_config.h"

/* 启用任意模拟量保护时必须设置大于 0 的采样超时；0 表示尚未配置。 */
#define BMS_PROTECT_SAMPLE_TIMEOUT_MS        0U

/* 尚无经电芯和整机验证的阈值，默认关闭模拟量保护；启用前必须填写对应阈值。 */

#define BMS_PROTECT_BATTERY_LOW_V            0.0f   /* 电池欠压，单位 V */
#define BMS_PROTECT_BATTERY_HIGH_V           0.0f   /* 电池过压，单位 V */


#define BMS_PROTECT_USB_LOW_V                0.0f     /* USB 输入过压，单位 V */
#define BMS_PROTECT_USB_HIGH_V               0.0f     /* USB 输入过压，单位 V */


#define BMS_PROTECT_TEMP_HIGH_DECI_C         0      /* 主控板 NTC 过温，单位 0.1°C */

#define BMS_PROTECT_CURRENT_HIGH_MA          0.0f   /* 电池电流绝对值上限，单位 mA */

#define BMS_PROTECT_HOLD_TIME_S              20U    /* 故障消失后的保持时间，单位 s */




/**
 * @brief 计算电池结构中的 fault_real/fault_hold；外部故障由上层直接传入
 * @note 首次调用前清零 fault_real/fault_hold；按固定周期持续调用，保持计时以 now_ms 为准。
 *       USB 和温度须由上层完成 ADC 换算；本函数不直接操作充放电 GPIO。
 */
void bms_protect_check(bms_battery_t *battery, uint16_t usb_mv,
                       int16_t temperature_deci_c, uint32_t valid_mask,
                       uint32_t sample_age_ms, bool usb_present,
                       bool external_fault, uint32_t now_ms);

#endif /* _BMS_PROTECT_H_ */
