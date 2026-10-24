/**
 ***********************************************************************************************************************
 * @file    bms_convert.h
 * @brief   BMS ADC 累计采样及电池电压、电流、USB 电压换算接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_CONVERT_H_
#define _BMS_CONVERT_H_

#include "bms_config.h"

#include <stdbool.h>
#include <stdint.h>


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** 每组 ADC 累计采样次数 */
#define SAMPLE_ACCUM_COUNT                       128U

/** ADC 累计采样缓冲区数量 */
#define SAMPLE_SUM_COUNT                         2U



/** 12 位 ADC 量化级数 */
#define BMS_BATTERY_ADC_FULL_SCALE               4095U


/* battery voltage conversion --------------------------------------------------------------------------------------------*/

/** MCU 内部参考电压 VREFINT，单位 V */
#define BMS_BATTERY_VREFINT_V                    1.216f

/** 电池电压换算分子，单位 V·ADC码值；固定参数由编译器提前计算 */
#define BMS_BATTERY_VOLTAGE_NUMERATOR            \
                                                ((double)BMS_BATTERY_VREFINT_V * \
                                                 (double)BMS_BATTERY_ADC_FULL_SCALE)


/* battery current conversion --------------------------------------------------------------------------------------------*/

/** PGA2 零电流输出占 VCCA 的比例 */
#define BMS_BATTERY_PGA_ZERO_RATIO               0.5f

/** PGA2 电流采样放大倍数 */
#define BMS_BATTERY_PGA_GAIN                     16.0f

/** 电流采样分流电阻，单位 Ω */
#define BMS_BATTERY_SHUNT_OHM                    0.002f

/** 电流方向修正系数，正值定义为放电 */
#define BMS_BATTERY_CURRENT_SIGN                 1.0f

/** 电流单位换算：1 A = 1000 mA */
#define BMS_BATTERY_CURRENT_MA_PER_A             1000.0

/** 零电流对应的 ADC 码值，保留半码精度。 */
#define BMS_BATTERY_CURRENT_ZERO_ADC             ((double)BMS_BATTERY_ADC_FULL_SCALE * BMS_BATTERY_PGA_ZERO_RATIO)

/** 电流换算系数，单位 mA/(ADC码值·V)；固定参数可由编译器提前计算。 */
#define BMS_BATTERY_CURRENT_MA_PER_CODE_V                                                                     \
                                                ((BMS_BATTERY_CURRENT_MA_PER_A * BMS_BATTERY_CURRENT_SIGN) /  \
                                                ((double)BMS_BATTERY_ADC_FULL_SCALE *  (BMS_BATTERY_PGA_GAIN * BMS_BATTERY_SHUNT_OHM)))


/* USB voltage conversion --------------------------------------------------------------------------------------------*/

/** USB 电压采样上分压电阻，单位 Ω */
#define BMS_USB_R_UP                             1000000.0

/** USB 电压采样下分压电阻，单位 Ω */
#define BMS_USB_R_DOWN                           100000.0

/** USB ADC 参考电压，单位 V */
#define BMS_USB_ADC_REF_VOLTAGE                  3.3

/** USB 输入电压恢复倍率 */
#define BMS_USB_ADC_DIV_RATIO                    ((BMS_USB_R_UP + BMS_USB_R_DOWN) / BMS_USB_R_DOWN)

/** USB ADC 码值到 VBUS 电压的换算系数，单位 V/ADC码值 */
#define BMS_USB_ADC_TO_VBUS_V                    \
                                                 ((BMS_USB_ADC_REF_VOLTAGE * BMS_USB_ADC_DIV_RATIO) / \
                                                 (double)BMS_BATTERY_ADC_FULL_SCALE)


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief BMS ADC 通道索引
 */
typedef enum
{
    BMS_ADC_CURRENT = 0,          /**< 电池电流 PGA2 ADC */
    BMS_ADC_VOLTAGE,              /**< VREFINT ADC，用于反算电池电压 */
    BMS_ADC_USB_VOLTAGE,          /**< USB 输入电压 ADC */
    BMS_ADC_NUM                   /**< BMS ADC 通道总数 */
} bms_adc_t;


/**
 * @brief 单组 ADC 累计采样缓冲区
 */
typedef struct
{
    volatile uint32_t sum[BMS_ADC_NUM];  /**< 各 ADC 通道累计采样值 */
    volatile uint32_t count;             /**< 当前累计采样次数 */
    volatile bool complete;              /**< 当前缓冲区采样完成标志 */
} sample_accum_buf_t;


/**
 * @brief ADC 双缓冲累计采样管理
 */
typedef struct
{
    sample_accum_buf_t buf[SAMPLE_SUM_COUNT]; /**< 双缓冲区，避免数据处理期间覆盖采样结果 */
    volatile uint8_t write_idx;               /**< 当前采样写入缓冲区索引 */
    volatile uint8_t read_idx;                /**< 当前数据读取缓冲区索引 */
} sample_accum_t;



/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_convert_Init
//  Description: 初始化 BMS ADC 数据换算模块
//  Param      : None
//  Return     : None
//  Others     : 清空 ADC 累计采样缓冲区并复位读写索引
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void bms_convert_Init(void);


/*******************************************************************************
//  Function   : sample_accum_push
//  Description: 向当前写缓冲区加入一次多通道 ADC 采样值
//  Param      : adcValue - ADC 采样值数组，长度至少为 BMS_ADC_NUM
//  Return     : None
//  Others     : 累计达到 SAMPLE_ACCUM_COUNT 后标记当前缓冲区完成，并切换下一写缓冲区；
//               适合在 ADC/DMA 采样完成中断中调用
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void sample_accum_push(uint16_t *adcValue);


/*******************************************************************************
//  Function   : sample_accum_get
//  Description: 获取一组已完成的 ADC 平均采样结果
//  Param      : adcValue - ADC 平均值输出数组，长度至少为 BMS_ADC_NUM
//  Return     : true：成功获取一组数据；false：暂无完成数据或参数异常
//  Others     : 读取完成后释放当前缓冲区，并切换到下一读缓冲区
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool sample_accum_get(double *adcValue);

/*******************************************************************************
//  Function   : bms_battery_voltage
//  Description: 根据 VREFINT ADC 采样值计算电池电压
//  Param      : vrefAdc - VREFINT ADC 采样值
//  Return     : 电池电压，单位 V；采样无效时返回 0
//  Others     : ADC 参考电压为 VCCA，本板 VCCA 与 VBAT 相连，因此通过内部 VREFINT
//               反推出 VCCA，即得到电池电压
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
double bms_battery_voltage(uint32_t vrefAdc);

/*******************************************************************************
//  Function   : bms_battery_current
//  Description: 根据 PGA2 ADC 采样值计算电池充放电电流
//  Param      : pgaAdc - PGA2 输出 ADC 采样值
//               vbat   - 当前电池电压，单位 V
//  Return     : 电池电流，单位 mA；正值表示放电，负值表示充电；采样无效时返回 0
//  Others     : PGA2 放大分流电阻 R18 两端压差，零电流输出为 VCCA 的固定比例
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
double bms_battery_current(uint32_t pgaAdc, double vbat);


/*******************************************************************************
//  Function   : bms_usb_in_voltage
//  Description: 根据 USB ADC 采样值计算 VBUS 输入电压
//  Param      : usbAdc - USB ADC 采样值
//  Return     : USB VBUS 电压，单位 V；采样无效时返回 0
//  Others     : 根据 ADC 参考电压及外部分压比例恢复实际 VBUS 电压
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
double bms_usb_in_voltage(uint32_t usbAdc);

/*******************************************************************************
//  Function   : bms_battery_state
//  Description: 根据电池电流判断当前充放电状态
//  Param      : currentA - 当前电池电流，单位 A
//  Return     : 当前电池工作状态
//  Others     : 使用 ±0.2A 作为空闲判断死区，避免 PGA/ADC 零点噪声导致状态频繁切换
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
battery_state_t bms_battery_state(double currentA);

#endif /* _BMS_CONVERT_H_ */
