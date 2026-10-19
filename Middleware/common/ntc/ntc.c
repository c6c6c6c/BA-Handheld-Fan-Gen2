/**
 ***********************************************************************************************************************
 * @file    ntc.c
 * @brief   NTC 温度查表实现
 * @author  Leo Chen
 * @version 1.0.1
 * @date    2026-08-29
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */
#include "ntc.h"
#include "ntc_table.h"
#include "binary_search.h"
/*
 * NTC 型号：TSM2B104F3951RZA
 * ADC：12 位，满量程有效码 4095
 * 固定上拉电阻：10 kΩ
 *
 * 分压电路：
 * 3.3V -- 10 kΩ -- ADC -- NTC -- GND
 *
 * 数组顺序：-40°C ~ 125°C
 * 温度升高时，NTC 阻值和 ADC 值均递减。
 */
const ntc_table_t g_ntc_TSM =
{
    .len     = (uint16_t)(sizeof(TSM2B104F3951RZA_NTC_50) /
                          sizeof(TSM2B104F3951RZA_NTC_50[0])),
    .step    = 1.0f,
    .rPull   = 10000U,
    .unit    = 50U,
    .scale   = 200U,
    .maxTemp = 125.0f,
    .minTemp = -40.0f,
    .pTable  = TSM2B104F3951RZA_NTC_50
};

/*
 * NTC型号：WMF52D-103F3435FEB25-B
 * ADC：12位，满量程有效码4095
 * 固定上拉电阻：10kΩ
 *
 * 分压电路：
 * 3.3V -- 10kΩ -- ADC -- NTC -- GND
 *
 * 数组顺序：-40℃～125℃
 * 温度升高时，NTC阻值和ADC采样值均递减。
 */
const ntc_table_t g_ntc_WMF52D =
{
    .len     = (uint16_t)(sizeof(WMF52D_103F3435FEB25_B_NTC_4) /
                          sizeof(WMF52D_103F3435FEB25_B_NTC_4[0])),
    .step    = 1.0f,
    .rPull   = 10000U,
    .unit    = 4U,
    .scale   = 2500U,
    .maxTemp = 125.0f,
    .minTemp = -40.0f,
    .pTable  = WMF52D_103F3435FEB25_B_NTC_4
};


/*******************************************************************************
//  Function   : NtcTable_AdcToTableValue
//  Description: 将 ADC 值直接换算为查表单位，并进行四舍五入。
//  Param      : adcValue-ADC 采样值；rPull-上拉电阻值；unit-查表单位；scale-rPull / unit 预计算值
//  Return     : 查表单位下的 NTC 电阻值
//  Others     : tableValue = ADC * rPull / ((4095 - ADC) * unit)
//               scale 非 0 时优先走 32 位快速路径，减少 Cortex-M0+ 上的软件除法开销。
//  Author     : Leo Chen 2026-08-29
*******************************************************************************/
static uint32_t NtcTable_AdcToTableValue(uint16_t adcValue, uint32_t rPull, uint16_t unit, uint32_t scale)
{
    uint32_t denominator;
    uint32_t numerator32;
    uint64_t numerator;
    uint64_t divisor;
    uint64_t tableValue;

    denominator = (uint32_t)NTC_ADC_FULL_SCALE_CODE - (uint32_t)adcValue;

    if (scale != 0U)
    {
        if (scale <= (UINT32_MAX / (uint32_t)NTC_ADC_OPEN_THRESHOLD))
        {
            numerator32 = ((uint32_t)adcValue * scale) + (denominator / 2U);
            return numerator32 / denominator;
        }
    }

    divisor = (uint64_t)denominator * (uint64_t)unit;
    numerator = ((uint64_t)adcValue * (uint64_t)rPull) + (divisor / 2ULL);
    tableValue = numerator / divisor;

    if (tableValue > (uint64_t)UINT32_MAX)
    {
        tableValue = (uint64_t)UINT32_MAX;
    }

    return (uint32_t)tableValue;
}

/*******************************************************************************
//  Function   : NtcTable_ConvertAdc
//  Description: 使用二分查找和线性插值将 NTC ADC 值转换为温度。
//  Param      : pNtcTable-NTC 查表参数；adcValue-ADC 采样值；pTemperatureC-温度输出地址
//  Return     : NTC 温度转换状态
//  Others     : 输出温度单位为 °C；越界时输出对应端点温度。
//  Author     : Leo Chen 2026-08-29
*******************************************************************************/
ntc_status_t NtcTable_ConvertAdc( const ntc_table_t *pNtcTable,
                                        uint16_t     adcValue,
                                        float       *pTemperatureC)
{
    uint16_t lowIndex;
    uint16_t highIndex;
    uint16_t resHigh;
    uint16_t resLow;
    uint32_t tableValue;
    float ratio;
    binary_search_status_t searchStatus;

    if ((pNtcTable          == 0)
    ||  (pNtcTable->pTable  == 0)
    ||  (pTemperatureC      == 0)
    ||  (pNtcTable->len     < 2U)
    ||  (pNtcTable->step <= 0.0f)
    ||  (pNtcTable->rPull  == 0U)
    ||  (pNtcTable->unit   == 0U)
    ||  (pNtcTable->maxTemp < pNtcTable->minTemp))
    {
        return NTC_STATUS_PARAM_ERROR;
    }

    /* uint16_t 输入可能大于 12 位 ADC 有效范围，统一按开路/高端异常处理。 */
    if (adcValue >= NTC_ADC_OPEN_THRESHOLD)
    {
        *pTemperatureC = pNtcTable->minTemp;
        return NTC_STATUS_OPEN_CIRCUIT;
    }

    if (adcValue <= NTC_ADC_SHORT_THRESHOLD)
    {
        *pTemperatureC = pNtcTable->maxTemp;
        return NTC_STATUS_SHORT_CIRCUIT;
    }

    /* 由于开路阈值小于满量程，到达这里时分母一定大于 0。 */
    tableValue = NtcTable_AdcToTableValue(adcValue,
                                          pNtcTable->rPull,
                                          pNtcTable->unit,
                                          pNtcTable->scale);

    if (tableValue > (uint32_t)UINT16_MAX)
    {
        *pTemperatureC = pNtcTable->minTemp;
        return NTC_STATUS_BELOW_RANGE;
    }

    searchStatus = BinarySearch_U16Descending(pNtcTable->pTable,
                                              pNtcTable->len,
                                              (uint16_t)tableValue,
                                              &lowIndex,
                                              &highIndex);

    if (searchStatus == BINARY_SEARCH_STATUS_ABOVE_RANGE)
    {
        *pTemperatureC = pNtcTable->minTemp;
        return NTC_STATUS_BELOW_RANGE;
    }

    if (searchStatus == BINARY_SEARCH_STATUS_BELOW_RANGE)
    {
        *pTemperatureC = pNtcTable->maxTemp;
        return NTC_STATUS_ABOVE_RANGE;
    }

    if (searchStatus != BINARY_SEARCH_STATUS_OK)
    {
        return NTC_STATUS_PARAM_ERROR;
    }

    /* 防御性检查，避免异常搜索结果造成数组越界。 */
    if ((lowIndex  >= pNtcTable->len)
    ||  (highIndex >= pNtcTable->len)
    ||  (lowIndex  >  highIndex))
    {
        return NTC_STATUS_PARAM_ERROR;
    }

    if (lowIndex == highIndex)
    {
        *pTemperatureC = pNtcTable->minTemp + ((float)lowIndex * pNtcTable->step);
        return NTC_STATUS_OK;
    }

    resHigh = pNtcTable->pTable[lowIndex];
    resLow = pNtcTable->pTable[highIndex];

    if (resHigh <= resLow)
    {
        return NTC_STATUS_PARAM_ERROR;
    }

    /*
     * 注意：这里必须用 tableValue 插值，不能使用原始 adcValue。
     * resHigh、resLow、tableValue 三者量纲一致，均为“电阻 / 查表单位”。
     */
    ratio = ((float)resHigh - (float)tableValue) /
            ((float)resHigh - (float)resLow);

    /* 理论上 ratio 应位于 [0, 1]；钳位可抵御异常输入或未来修改带来的浮点越界。 */
    if (ratio < 0.0f)
    {
        ratio = 0.0f;
    }
    else if (ratio > 1.0f)
    {
        ratio = 1.0f;
    }
    else
    {
        /* 无需处理。 */
    }

    *pTemperatureC = pNtcTable->minTemp + (((float)lowIndex + ratio) * pNtcTable->step);

    /* 最终输出再钳位一次，确保配置轻微不一致时也不会超出声明温区。 */
    if (*pTemperatureC < pNtcTable->minTemp)
    {
        *pTemperatureC = pNtcTable->minTemp;
    }
    else if (*pTemperatureC > pNtcTable->maxTemp)
    {
        *pTemperatureC = pNtcTable->maxTemp;
    }
    else
    {
        /* 无需处理。 */
    }

    return NTC_STATUS_OK;
}
