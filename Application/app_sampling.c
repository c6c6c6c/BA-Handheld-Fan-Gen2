/**
 ***********************************************************************************************************************
 * @file    app_sampling.c
 * @brief   应用层 ADC 采样数据分发及滤波处理
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2026-10-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_sampling.h"

#include "bms_convert.h"
#include "bsp_adc.h"

#include "filter.h"
#include "ntc.h"

/* Variables ---------------------------------------------------------------------------------------------------------*/

/** 各 ADC 采样通道的平均滤波器 */
static avg_filter_t avgFilterList[FILTER_LIST_NUM];


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_adc_ChannelConvCpltCallback
//  Description: ADC 一组通道转换完成回调函数
//  Param      : Value - ADC 各通道转换结果数组
//               Size  - ADC 转换结果数量
//  Return     : None
//  Others     : 在 ADC 转换完成中断中调用，负责将采样数据分发到对应处理模块；
//               BMS 使用累计采样器，NTC、电机及 ADC 按键使用平均滤波器
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
void bsp_adc_ChannelConvCpltCallback(volatile uint32_t *Value, uint16_t Size)
{
    uint16_t adcDat[BMS_ADC_NUM];

    /* 检查 ADC 采样数据是否有效。 */
    if ((Value == 0) || (Size < ADC_CH_NUM))
    {
        return;
    }

    /* BMS 电池电流、电压及 USB 输入电压采样。 */
    adcDat[BMS_ADC_CURRENT]     = Value[ADC_CH_PGA2];
    adcDat[BMS_ADC_VOLTAGE]     = Value[ADC_CH_VREFINT];
    adcDat[BMS_ADC_USB_VOLTAGE] = Value[ADC_CH_USB];

    sample_accum_push(adcDat);

    /* 雾化片 ADC 采样。 */
    //SprayAd_PushSample(Value[ADC_CH_WH]);

    /* NTC 温度采样滤波。 */
    AvgFilter_PushFromIsr(&avgFilterList[FILTER_LIST_NTC], Value[ADC_CH_NTC]);

    /* 电机电流采样滤波。 */
    AvgFilter_PushFromIsr(&avgFilterList[FILTER_LIST_MOTOR], Value[ADC_CH_MOTOR]);

    /* ADC 按键采样滤波。 */
    AvgFilter_PushFromIsr(&avgFilterList[FILTER_LIST_SW], Value[ADC_CH_SW]);
}


/*******************************************************************************
//  Function   : app_sampling_Init
//  Description: 初始化应用层 ADC 采样滤波模块
//  Param      : None
//  Return     : None
//  Others     : 初始化 NTC、电机及 ADC 按键对应的平均滤波器
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
void app_sampling_Init(void)
{
    AvgFilter_Init(&avgFilterList[FILTER_LIST_NTC]);
    AvgFilter_Init(&avgFilterList[FILTER_LIST_MOTOR]);
    AvgFilter_Init(&avgFilterList[FILTER_LIST_SW]);
}


/*******************************************************************************
//  Function   : app_sampling_task
//  Description: 应用层 ADC 采样滤波周期任务
//  Param      : None
//  Return     : None
//  Others     : 在主循环或周期任务中调用，处理各通道累计采样数据并更新滤波结果
//  Author     : Chen Liang 2026-10-18
*******************************************************************************/
ntc_table_status_t status;
float ntc_temp; //NTC 2 温度
void app_sampling_task(void)
{
		static uint32_t ntc_sequence = 0;

		AvgFilter_Process(&avgFilterList[FILTER_LIST_NTC]);
		AvgFilter_Process(&avgFilterList[FILTER_LIST_MOTOR]);
		AvgFilter_Process(&avgFilterList[FILTER_LIST_SW]);

		if( AvgFilter_GetSequence(&avgFilterList[FILTER_LIST_NTC]) != ntc_sequence)
		{
			ntc_sequence = AvgFilter_GetSequence(&avgFilterList[FILTER_LIST_NTC]);
			status = NtcTable_ConvertAdc(&g_ntc_TSM,AvgFilter_GetFilteredValue(&avgFilterList[FILTER_LIST_NTC]),&ntc_temp);
		}
}
