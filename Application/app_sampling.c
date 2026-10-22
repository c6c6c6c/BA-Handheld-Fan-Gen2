
/**
 ***********************************************************************************************************************
 * @file    app_sampling.c
 * @brief   应用层 ADC 采样分发、平均滤波及 NTC 温度转换
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_sampling.h"
#include "app_system.h"

#include "bms_convert.h"
#include "bsp_adc.h"

#include "filter.h"
#include "ntc.h"


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** 各 ADC 通道的平均滤波器，按 filter_list_t 索引访问 */
static avg_filter_t avgFilterList[FILTER_LIST_NUM];


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_adc_ChannelConvCpltCallback
//  Description: ADC 一组通道转换完成回调函数
//  Param      : Value - ADC 各通道转换结果数组
//               Size  - ADC 转换数据数量
//  Return     : None
//  Others     : 在 ADC 采样完成中断中调用，将数据分发至 BMS 累计采样器及各通道滤波器
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
void bsp_adc_ChannelConvCpltCallback(volatile uint32_t *Value, uint16_t Size)
{
    uint16_t adcDat[BMS_ADC_NUM];

    /* 检查 ADC 采样数据地址及通道数量。 */
    if ((Value == 0) || (Size < ADC_CH_NUM))
    {
        return;
    }

    /* 提取 BMS 电流、VREFINT 及 USB 电压采样值。 */
    adcDat[BMS_ADC_CURRENT]     = Value[ADC_CH_PGA2];
    adcDat[BMS_ADC_VOLTAGE]     = Value[ADC_CH_VREFINT];
    adcDat[BMS_ADC_USB_VOLTAGE] = Value[ADC_CH_USB];

    sample_accum_push(adcDat);

    /* 将其余 ADC 采样值分别写入对应的平均滤波器。 */
    AvgFilter_PushFromIsr(&avgFilterList[FILTER_LIST_MIST],  Value[ADC_CH_WH]);
    AvgFilter_PushFromIsr(&avgFilterList[FILTER_LIST_NTC],   Value[ADC_CH_NTC]);
    AvgFilter_PushFromIsr(&avgFilterList[FILTER_LIST_MOTOR], Value[ADC_CH_MOTOR]);
    AvgFilter_PushFromIsr(&avgFilterList[FILTER_LIST_SW],    Value[ADC_CH_SW]);
}


/*******************************************************************************
//  Function   : app_sampling_Init
//  Description: 初始化应用层 ADC 采样滤波模块
//  Param      : None
//  Return     : None
//  Others     : 分别初始化 NTC、雾化片、电机及 ADC 按键的平均滤波器
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
void app_sampling_Init(void)
{
    AvgFilter_Init(&avgFilterList[FILTER_LIST_NTC]);
    AvgFilter_Init(&avgFilterList[FILTER_LIST_MOTOR]);
    AvgFilter_Init(&avgFilterList[FILTER_LIST_MIST]);
    AvgFilter_Init(&avgFilterList[FILTER_LIST_SW]);
}


/*******************************************************************************
//  Function   : app_sampling_task
//  Description: 执行应用层 ADC 平均滤波及 NTC 温度更新
//  Param      : None
//  Return     : None
//  Others     : 仅在 NTC 滤波序号变化时执行温度换算，并更新系统 NTC 故障状态
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_sampling_task(void)
{
    static uint32_t ntc_sequence = 0U;
    ntc_status_t status;

    /* 处理各 ADC 通道的累计采样及平均滤波。 */
    AvgFilter_Process(&avgFilterList[FILTER_LIST_NTC]);
    AvgFilter_Process(&avgFilterList[FILTER_LIST_MIST]);
    AvgFilter_Process(&avgFilterList[FILTER_LIST_MOTOR]);
    AvgFilter_Process(&avgFilterList[FILTER_LIST_SW]);

    /* NTC 产生新的滤波结果后才进行温度换算。 */
    if (AvgFilter_GetSequence(&avgFilterList[FILTER_LIST_NTC]) != ntc_sequence)
    {
        ntc_sequence = AvgFilter_GetSequence(&avgFilterList[FILTER_LIST_NTC]);

        /* 根据 NTC 查表参数将 ADC 滤波值转换为实际温度。 */
        status = NtcTable_ConvertAdc(&g_ntc_TSM,
                                     AvgFilter_GetFilteredValue(&avgFilterList[FILTER_LIST_NTC]),
                                     &System.ntc_temp);

        /* NTC 换算失败时置位系统 NTC 故障标志。 */
        System.err.ntc_err = (status != NTC_STATUS_OK);
    }
}


/*******************************************************************************
//  Function   : get_mist_val
//  Description: 获取雾化片 ADC 滤波结果
//  Param      : None
//  Return     : 雾化片 ADC 滤波值
//  Others     : 返回最近一次完成平均滤波的结果
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
uint16_t get_mist_val(void)
{
    return AvgFilter_GetFilteredValue(&avgFilterList[FILTER_LIST_MIST]);
}


/*******************************************************************************
//  Function   : get_mist_seq
//  Description: 获取雾化片 ADC 滤波结果更新序号
//  Param      : None
//  Return     : 雾化片滤波结果更新序号
//  Others     : 序号变化表示产生了新的滤波结果，可用于避免重复处理
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
uint32_t get_mist_seq(void)
{
    return AvgFilter_GetSequence(&avgFilterList[FILTER_LIST_MIST]);
}


/*******************************************************************************
//  Function   : get_sw_val
//  Description: 获取 ADC 按键滤波结果
//  Param      : None
//  Return     : ADC 按键滤波值
//  Others     : 返回最近一次完成平均滤波的结果
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
uint16_t get_sw_val(void)
{
    return AvgFilter_GetFilteredValue(&avgFilterList[FILTER_LIST_SW]);
}


/*******************************************************************************
//  Function   : get_sw_seq
//  Description: 获取 ADC 按键滤波结果更新序号
//  Param      : None
//  Return     : ADC 按键滤波结果更新序号
//  Others     : 序号变化表示产生了新的滤波结果，可用于按键状态更新
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
uint32_t get_sw_seq(void)
{
    return AvgFilter_GetSequence(&avgFilterList[FILTER_LIST_SW]);
}


/*******************************************************************************
//  Function   : get_motor_val
//  Description: 获取电机电流 ADC 滤波结果
//  Param      : None
//  Return     : 电机电流 ADC 滤波值
//  Others     : 返回最近一次完成平均滤波的结果，尚未换算为实际电流
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
uint16_t get_motor_val(void)
{
    return AvgFilter_GetFilteredValue(&avgFilterList[FILTER_LIST_MOTOR]);
}


/*******************************************************************************
//  Function   : get_motor_seq
//  Description: 获取电机电流 ADC 滤波结果更新序号
//  Param      : None
//  Return     : 电机电流滤波结果更新序号
//  Others     : 序号变化表示产生了新的滤波结果，可用于检测新采样数据
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
uint32_t get_motor_seq(void)
{
    return AvgFilter_GetSequence(&avgFilterList[FILTER_LIST_MOTOR]);
}
