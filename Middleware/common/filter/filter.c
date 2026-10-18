/**
 ***********************************************************************************************************************
 * @file    filter.c
 * @brief   ADC环形采样缓存及滑动平均滤波实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-28
 ***********************************************************************************************************************
 */
#include "filter.h"


/*******************************************************************************
//  Function   : AvgFilter_Init
//  Description: 初始化ADC环形采样缓存及滑动平均滤波运行状态
//  Param      : pFilter-滤波器实例
//  Return     : None
//  Others     : 必须在开启ADC采样中断前调用
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
void AvgFilter_Init(avg_filter_t *pFilter)
{
    uint16_t index;

    if (pFilter == 0)
    {
        return;
    }

    for (index = 0U; index < CIRCLE_BUFFER_SIZE; index++)
    {
        pFilter->buf.data[index] = 0U;
    }

    pFilter->buf.idx      = 0U;
    pFilter->buf.cnt      = 0U;
    pFilter->buf.sequence = 0U;

    pFilter->filter_val   = 0U;
    pFilter->min          = 0U;
    pFilter->max          = 0U;
    pFilter->valid        = 0U;
    pFilter->lastSequence = 0U;
}

/*******************************************************************************
//  Function   : AvgFilter_PushFromIsr
//  Description: 在ADC中断中向环形缓存写入一个原始采样值
//  Param      : pFilter-滤波器实例；adcValue-ADC原始采样值
//  Return     : None
//  Others     : 中断中只写入数据和更新索引，不执行求和、极值或除法运算
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
void AvgFilter_PushFromIsr(avg_filter_t *pFilter,uint16_t val)
{
    if (pFilter == 0)
    {
        return;
    }

    pFilter->buf.data[pFilter->buf.idx] = val;
    pFilter->buf.idx ++;
    pFilter->buf.idx %= CIRCLE_BUFFER_SIZE;

    if(pFilter->buf.cnt < CIRCLE_BUFFER_SIZE)
    {
        pFilter->buf.cnt ++;
    }

    /* 必须最后更新采样序号，用于主循环判断窗口数据是否被中断改写。 */
    pFilter->buf.sequence++;
}

/*******************************************************************************
//  Function   : AvgFilter_Process
//  Description: 对当前ADC缓冲区执行去极值平均滤波
//  Param      : pFilter-滤波器实例
//  Return     : 1-滤波结果更新成功；0-未更新
//  Others     : 去除一个最大值和一个最小值后计算平均值
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
uint8_t AvgFilter_Process(avg_filter_t *pFilter)
{
    uint8_t  i;
    uint16_t value;
    uint16_t minimum;
    uint16_t maximum;
    uint32_t sum;
    uint32_t sequenceBefore;
    uint32_t sequenceAfter;

    if (pFilter == 0)
    {
        return 0U;
    }

    sequenceBefore = pFilter->buf.sequence;

    if (sequenceBefore == pFilter->lastSequence)
    {
        return 0U;
    }

    if (pFilter->buf.cnt < CIRCLE_BUFFER_SIZE)
    {
        pFilter->valid = 0U;
        pFilter->lastSequence = sequenceBefore;
        return 0U;
    }

    value = pFilter->buf.data[0];
    minimum = value;
    maximum = value;
    sum = value;

    for (i = 1U; i < CIRCLE_BUFFER_SIZE; i++)
    {
        value = pFilter->buf.data[i];

        sum += value;

        if (value < minimum)
        {
            minimum = value;
        }

        if (value > maximum)
        {
            maximum = value;
        }
    }

    /*
     * 计算过程中如果产生了新的ADC采样，
     * 说明缓冲区可能已被修改，本次结果作废。
     */
    sequenceAfter = pFilter->buf.sequence;

    if (sequenceAfter != sequenceBefore)
    {
        return 0U;
    }

    sum -= minimum;
    sum -= maximum;

    pFilter->filter_val =
        (uint16_t)(sum / (CIRCLE_BUFFER_SIZE - 2U));

    pFilter->min = minimum;
    pFilter->max = maximum;
    pFilter->valid = 1U;
    pFilter->lastSequence = sequenceBefore;

    return 1U;
}


/*******************************************************************************
//  Function   : AvgFilter_GetFilteredValue
//  Description: 获取最近一次有效的ADC滤波结果
//  Param      : pFilter-滤波器实例
//  Return     : 滤波结果，无有效结果时返回0
//  Others     : 本函数不会触发ADC转换或执行滤波
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
uint16_t AvgFilter_GetFilteredValue(const avg_filter_t *pFilter)
{
    if ((pFilter == 0) || (pFilter->valid == 0U))
    {
        return 0U;
    }

    return pFilter->filter_val;
}

/*******************************************************************************
//  Function   : AvgFilter_GetSequence
//  Description: 获取ADC累计采样序号
//  Param      : pFilter-滤波器实例
//  Return     : ADC累计采样序号，参数无效时返回0
//  Others     : 可用于判断PWM切频后是否产生新的ADC采样
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
uint32_t AvgFilter_GetSequence(const avg_filter_t *pFilter)
{
    if (pFilter == 0)
    {
        return 0U;
    }

    return pFilter->buf.sequence;
}
