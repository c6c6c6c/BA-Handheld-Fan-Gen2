/**
 ***********************************************************************************************************************
 * @file    ntc.h
 * @brief   NTC 温度查表转换接口
 * @author  Leo Chen
 * @version 1.0.1
 * @date    2026-08-29
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef NTC_H
#define NTC_H

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include <stdint.h>

/* Defines -----------------------------------------------------------------------------------------------------------*/

/* 12 位 ADC 满量程有效码 */
#define NTC_ADC_FULL_SCALE_CODE              4095U

/* NTC 开路判定阈值，ADC 采样值大于或等于该值时判定为开路 */
#define NTC_ADC_OPEN_THRESHOLD               4090U

/* NTC 短路判定阈值，ADC 采样值小于或等于该值时判定为短路 */
#define NTC_ADC_SHORT_THRESHOLD              5U

#if (NTC_ADC_OPEN_THRESHOLD >= NTC_ADC_FULL_SCALE_CODE)
#error "NTC_ADC_OPEN_THRESHOLD must be less than NTC_ADC_FULL_SCALE_CODE"
#endif

#if (NTC_ADC_SHORT_THRESHOLD >= NTC_ADC_OPEN_THRESHOLD)
#error "NTC_ADC_SHORT_THRESHOLD must be less than NTC_ADC_OPEN_THRESHOLD"
#endif

/* Typedefs ----------------------------------------------------------------------------------------------------------*/

/**
 * @brief NTC 温度与电阻查表参数
 */
typedef struct
{
    uint16_t        len;       /* 查表数据长度 */
    float           step;      /* 温度步进，单位：℃ */
    uint32_t        rPull;     /* 上拉电阻值，单位：Ω */
    uint16_t        unit;      /* 每个表值代表的电阻，单位：Ω */
    uint32_t        scale;     /* 电阻换算系数 rPull/unit，设置为 0 时使用通用转换公式 */
    float           maxTemp;   /* 最高温度，单位：℃ */
    float           minTemp;   /* 最低温度，单位：℃ */
    const uint16_t *pTable;    /* NTC 电阻查表数据，要求随温度升高严格递减 */
} ntc_table_t;

/**
 * @brief NTC 温度转换状态
 */
typedef enum
{
    NTC_STATUS_OK = 0,                  /* 温度转换成功 */
    NTC_STATUS_PARAM_ERROR,             /* 输入参数无效 */
    NTC_STATUS_OPEN_CIRCUIT,            /* NTC 开路 */
    NTC_STATUS_SHORT_CIRCUIT,           /* NTC 短路 */
    NTC_STATUS_BELOW_RANGE,             /* 温度低于查表范围 */
    NTC_STATUS_ABOVE_RANGE              /* 温度高于查表范围 */
} ntc_status_t;

/* Exported Variables ------------------------------------------------------------------------------------------------*/

/* TSM2B104F3951RZA NTC 查表参数实例 */
extern const ntc_table_t g_ntc_TSM;

extern const ntc_table_t g_ntc_WMF52D;

/* Function Prototypes -----------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : NtcTable_ConvertAdc
//  Description: 使用二分查找和线性插值，将 NTC ADC 采样值转换为温度
//  Param      : pNtcTable     - NTC 查表参数
//               adcValue     - ADC 采样值
//               pTemperatureC- 温度输出地址，单位：℃
//  Return     : NTC 温度转换状态
//  Others     : 温度越界时输出对应的端点温度；NTC 开路或短路时返回相应的故障状态
//  Author     : Leo Chen 2026-08-29
*******************************************************************************/
ntc_status_t NtcTable_ConvertAdc(const ntc_table_t *pNtcTable,
                                       uint16_t           adcValue,
                                       float             *pTemperatureC);

#endif /* NTC_H */
