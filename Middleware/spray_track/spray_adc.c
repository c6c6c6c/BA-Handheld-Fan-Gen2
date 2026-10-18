/**
 ***********************************************************************************************************************
 * @file    spray_adc.c
 * @brief   喷雾驱动ADC采集及滤波模块实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-24
 ***********************************************************************************************************************
 */

#include "spray_adc.h"

/* Variable ----------------------------------------------------------------------------------------------------------*/

SprayAd_Runtime_t s_sprayAd;

/* Private Function --------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : SprayAd_Filter
//  Description: 对当前ADC缓冲区数据执行平均滤波
//  Param      : 无
//  Return     : 无
//  Others     : 同时统计当前滤波窗口内最大值和最小值
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
static void SprayAd_Filter(void)
{
    uint8_t i;
    uint8_t count;

    uint16_t value;
    uint16_t minValue;
    uint16_t maxValue;

    uint32_t sum;

    count = s_sprayAd.buffer.count;

    if (count < SPRAY_ADC_FILTER_SIZE)
    {
        s_sprayAd.filterValid = 0U;
        return;
    }

    sum      = 0U;
    minValue = SPRAY_ADC_MAX_VALUE;
    maxValue = 0U;

    for (i = 0U; i < SPRAY_ADC_FILTER_SIZE; i++)
    {
        value = s_sprayAd.buffer.data[i];

        sum += value;

        if (value < minValue)
        {
            minValue = value;
        }

        if (value > maxValue)
        {
            maxValue = value;
        }
    }
		sum -= minValue;
		sum -= maxValue;

    s_sprayAd.filteredAd =
        (uint16_t)(sum / (SPRAY_ADC_FILTER_SIZE - 2));

    s_sprayAd.minAd = minValue;
    s_sprayAd.maxAd = maxValue;

    s_sprayAd.filterValid = 1U;
}

/* Public Function ---------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : SprayAd_Init
//  Description: 初始化喷雾ADC采集及滤波模块
//  Param      : 无
//  Return     : 无
//  Others     : ADC和定时器硬件初始化应由BSP层完成
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
void SprayAd_Init(void)
{
    uint8_t i;

    for (i = 0U; i < SPRAY_ADC_BUFFER_SIZE; i++)
    {
        s_sprayAd.buffer.data[i] = 0U;
    }

    s_sprayAd.buffer.writeIndex = 0U;
    s_sprayAd.buffer.count      = 0U;
    s_sprayAd.buffer.sequence   = 0U;

    s_sprayAd.filteredAd        = 0U;
    s_sprayAd.minAd             = 0U;
    s_sprayAd.maxAd             = 0U;

    s_sprayAd.lastSequence      = 0U;
    s_sprayAd.filterValid       = 0U;
}

/*******************************************************************************
//  Function   : SprayAd_PushSample
//  Description: 将ADC原始采样值写入采样缓冲区
//  Param      : adValue - ADC原始采样值
//  Return     : 无
//  Others     : 可在ADC转换完成中断中调用，本函数只执行快速数据缓存操作
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
void SprayAd_PushSample(uint16_t adValue)
{
    s_sprayAd.buffer.data[s_sprayAd.buffer.writeIndex] = adValue;

    s_sprayAd.buffer.writeIndex++;

    if (s_sprayAd.buffer.writeIndex >= SPRAY_ADC_BUFFER_SIZE)
    {
        s_sprayAd.buffer.writeIndex = 0U;
    }

    if (s_sprayAd.buffer.count < SPRAY_ADC_BUFFER_SIZE)
    {
        s_sprayAd.buffer.count++;
    }

    s_sprayAd.buffer.sequence++;
}

/*******************************************************************************
//  Function   : SprayAd_Task
//  Description: 执行喷雾ADC数据滤波处理
//  Param      : 无
//  Return     : 无
//  Others     : 建议在1ms周期任务中调用，不在ADC中断中执行复杂滤波
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
void SprayAd_Task(void)
{
    uint32_t sequence;

    sequence = s_sprayAd.buffer.sequence;

    /*
     * ADC没有产生新数据，无需重复执行滤波。
     */
    if (sequence == s_sprayAd.lastSequence)
    {
        return;
    }

    s_sprayAd.lastSequence = sequence;

    SprayAd_Filter();
}

/*******************************************************************************
//  Function   : SprayAd_GetFilteredValue
//  Description: 获取当前滤波后的喷雾反馈AD值
//  Param      : 无
//  Return     : 当前滤波后的AD反馈值
//  Others     : 本函数不启动ADC转换，仅返回后台处理后的最新结果
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
uint16_t SprayAd_GetFilteredValue(void)
{
    return s_sprayAd.filteredAd;
}

/*******************************************************************************
//  Function   : SprayAd_IsValid
//  Description: 获取当前ADC滤波数据是否有效
//  Param      : 无
//  Return     : 0-无效，1-有效
//  Others     : 缓冲区获得足够采样数据后置为有效
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
uint8_t SprayAd_IsValid(void)
{
    return s_sprayAd.filterValid;
}

/*******************************************************************************
//  Function   : SprayAd_GetSequence
//  Description: 获取ADC当前采样序号
//  Param      : 无
//  Return     : ADC累计采样序号
//  Others     : 可用于判断PWM切频后是否产生新的ADC采样数据
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
uint32_t SprayAd_GetSequence(void)
{
    return s_sprayAd.buffer.sequence;
}

/*******************************************************************************
//  Function   : SprayAd_ResetFilter
//  Description: 清除ADC历史滤波数据
//  Param      : 无
//  Return     : 无
//  Others     : PWM发生较大频率切换时可调用，避免旧频率数据参与新频率反馈计算
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
void SprayAd_ResetFilter(void)
{
    s_sprayAd.buffer.writeIndex = 0U;
    s_sprayAd.buffer.count      = 0U;

    s_sprayAd.filteredAd        = 0U;
    s_sprayAd.minAd             = 0U;
    s_sprayAd.maxAd             = 0U;

    s_sprayAd.filterValid       = 0U;
}

/*******************************************************************************
//  Function   : SprayAd_GetRuntime
//  Description: 获取喷雾ADC模块运行状态
//  Param      : 无
//  Return     : ADC运行变量只读指针
//  Others     : 主要用于调试及状态监控
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
const SprayAd_Runtime_t *SprayAd_GetRuntime(void)
{
    return &s_sprayAd;
}

/*******************************************************************************
//  Function   : bsp_adc_ConvCpltCallback
//  Description: ADC conversion complete callback
//  Param      : Value: ADC conversion result
//  Return     : None
//  Others     : Weak function, user can override it
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_adc_ConvCpltCallback(uint16_t Value)
{
	
}


/*******************************************************************************
//  Function   : bsp_adc_ConvCpltCallback
//  Description: ADC conversion complete callback
//  Param      : Value: ADC conversion result
//  Return     : None
//  Others     : Weak function, user can override it
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
uint16_t SprayAd_GetadDiff()
{
	return s_sprayAd.maxAd - s_sprayAd.minAd;
}
