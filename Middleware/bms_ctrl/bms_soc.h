/**
 ***********************************************************************************************************************
 * @file    bms_soc.h
 * @brief   BMS SOC 估算模块接口定义，实现库仑计量、静置 OCV 校正及满充/空电端点修正
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-13
 ***********************************************************************************************************************
 * @note
 *          1. 电流方向约定：正值表示放电，负值表示充电。
 *          2. 电压输入为单节电芯电压。
 *          3. bms_soc_update() 应按固定周期调用。
 *          4. 默认 OCV-SOC 曲线仅作为初始参考，实际产品应根据电芯特性重新标定。
 ***********************************************************************************************************************
 */

#ifndef BMS_SOC_H
#define BMS_SOC_H

#include <stddef.h>
#include <stdint.h>
#include "bms_config.h"

/* 表内 SOC 使用整数千分比，1000 表示 100%；估算器仍使用 0.0~1.0 浮点值。 */


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 单个 OCV-SOC 标定点，将同一测量点的电压与电量配对保存
 */
typedef struct
{
    float    voltage_v;          /**< 单节电芯开路电压，单位 V */
    uint16_t soc_permille;       /**< SOC 千分比，范围 0~1000，例如 44 表示 4.4% */
} BmsSocOcvPoint;


/**
 * @brief SOC 估算配置参数
 */
typedef struct
{
    float capacity_ah;               /**< 电池额定容量，单位 Ah */

    float rest_current_a;            /**< 静置最大允许电流，单位 A */
    float rest_time_s;               /**< 静置持续确认时间，单位 s */
    float rest_dvdt_v_per_s;         /**< 静置最大允许电压变化速率，单位 V/s */
    float ocv_correction_tau_s;      /**< OCV 修正时间常数，单位 s */

    float full_voltage_v;            /**< 满充端点电压阈值，单位 V */
    float full_terminate_current_a;  /**< 满充终止电流阈值，单位 A */
    float empty_voltage_v;           /**< 空电端点电压阈值，单位 V */
    float empty_min_discharge_a;     /**< 空电判定最小放电电流，单位 A */
    float endpoint_hold_s;           /**< 满充/空电端点持续确认时间，单位 s */

    const BmsSocOcvPoint *ocv_table; /**< OCV-SOC 结构体表，电压和整数 SOC 均须严格递增 */
    size_t ocv_count;                /**< OCV-SOC 曲线数据点数量 */
} BmsSocConfig;


/**
 * @brief SOC 估算器运行状态
 */
typedef struct
{
    BmsSocConfig cfg;            /**< SOC 配置参数副本 */

    float soc;                   /**< 当前 SOC，范围 0.0~1.0 */
    
    float rest_timer_s;          /**< 当前连续静置时间，单位 s */
    float full_timer_s;          /**< 满充条件连续成立时间，单位 s */
    float empty_timer_s;         /**< 空电条件连续成立时间，单位 s */
    float last_voltage_v;        /**< 上一次电芯电压，单位 V */
    int voltage_initialized;     /**< 历史电压是否已初始化，0：未初始化，非 0：有效 */
} BmsSocEstimator;



/** ADCxConvertedDatas[] 使用逻辑通道索引，不可使用 LL_ADC_CHANNEL_xxx 作为数组下标 */
#define BMS_BATTERY_ADC_FULL_SCALE              4095U          /* 12 位右对齐 ADC 满量程码值 */
#define BMS_BATTERY_VREFINT_V                   1.2            /* VREFINT 典型电压，单位 V */
#define BMS_BATTERY_PGA_GAIN                    16.0           /* OPA2 PGA 增益，与 bsp_pga2_Init() 保持一致 */
#define BMS_BATTERY_PGA_ZERO_RATIO              0.5            /* 零电流时 PGA 输出相对 VCCA 的比例 */
#define BMS_BATTERY_SHUNT_OHM                   0.01           /* 电流采样电阻 R18，单位 Ω */
#define BMS_BATTERY_CURRENT_SIGN                1.0            /* 电流方向修正系数，正值表示放电 */



/* Function declarations ---------------------------------------------------------------------------------------------*/


/*******************************************************************************
//  Function   : bms_soc_default_config
//  Description: 初始化 SOC 默认配置
//  Param      : cfg         - SOC 配置结构体
//               capacity_ah - 电池额定容量，单位 Ah
//  Return     : None
//  Others     : 填充默认 OCV-SOC 曲线及静置、满充、空电相关参数，实际产品需按电芯重新标定
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_soc_default_config(BmsSocConfig *cfg, float capacity_ah);


/*******************************************************************************
//  Function   : bms_soc_init
//  Description: 初始化 SOC 估算器
//  Param      : est         - SOC 估算器
//               cfg         - SOC 配置参数
//               initial_soc - 初始 SOC，建议范围 0.0~1.0
//  Return     : 1：初始化成功；0：参数或配置非法
//  Others     : initial_soc 会自动限制在 0.0~1.0 范围
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
int bms_soc_init(BmsSocEstimator *est, const BmsSocConfig *cfg, float initial_soc);


/*******************************************************************************
//  Function   : bms_soc_update
//  Description: 更新一次 SOC 估算结果
//  Param      : est            - SOC 估算器
//               cell_voltage_v - 当前单节电芯电压，单位 V
//               current_a      - 当前电池电流，单位 A；正值放电，负值充电
//               dt_s           - 距离上次更新的时间间隔，单位 s
//  Return     : 本周期产生的 BmsSocEvent 事件
//  Others     : 执行库仑计量，并根据状态执行 OCV、满充或空电校正
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
BmsSocEvent bms_soc_update(BmsSocEstimator *est, float cell_voltage_v,
                           float current_a, float dt_s);


/*******************************************************************************
//  Function   : bms_soc_from_ocv
//  Description: 根据静置电压估算 SOC
//  Param      : cfg       - SOC 配置参数
//               voltage_v - 单节电芯开路电压，单位 V
//  Return     : OCV-SOC 曲线插值得到的浮点 SOC，范围 0.0~1.0
//  Others     : 表中 SOC 为整数千分比，插值后转换为 0.0~1.0；电压越界时返回端点值
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
float bms_soc_from_ocv(const BmsSocConfig *cfg, float voltage_v);


#endif /* BMS_SOC_H */
