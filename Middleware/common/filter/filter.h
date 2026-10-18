/**
 ***********************************************************************************************************************
 * @file    filter.h
 * @brief   ADC滑动平均滤波器数据结构及参数定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-28
 ***********************************************************************************************************************
 */
#ifndef FILTER_H
#define FILTER_H

#include <stdint.h>

/* 滑动滤波环形缓冲区长度，必须为2的整数次幂。 */
#define CIRCLE_BUFFER_SIZE   10U

/* 检查缓冲区长度是否合法。 */
#if (CIRCLE_BUFFER_SIZE < 2U)
#error "CIRCLE_BUFFER_SIZE must be a power of two and not less than 2"
#endif

/**
 * @brief ADC原始采样环形缓冲区
 *
 * 中断服务程序负责写入ADC原始采样值，并更新写入位置、
 * 有效数据数量和累计采样序号。
 */
typedef struct
{
    volatile uint16_t data[CIRCLE_BUFFER_SIZE];  // ADC原始采样数据缓冲区
    volatile uint8_t  idx;                       // 下一次ADC数据写入位置
    volatile uint8_t  cnt;                       // 缓冲区内当前有效数据数量
    volatile uint32_t sequence;                  // ADC累计采样序号，每次采样后加1
} circle_buffer_t;

/**
 * @brief 滑动平均滤波器运行状态
 *
 * 保存ADC原始采样缓冲区、滤波结果、当前窗口极值
 * 以及滤波任务最近一次处理的采样序号。
 */
typedef struct
{
    circle_buffer_t buf;                         // ADC原始采样环形缓冲区
    uint16_t        filter_val;                  // 当前滑动平均滤波结果
    uint16_t        min;                         // 当前滤波窗口中的最小采样值
    uint16_t        max;                         // 当前滤波窗口中的最大采样值
    uint8_t         valid;                       // 滤波结果有效标志：0-无效，1-有效
    uint32_t        lastSequence;                // 最近一次完成滤波处理的采样序号
} avg_filter_t;


/*******************************************************************************
//  Function   : AvgFilter_Init
//  Description: 初始化ADC环形采样缓存及滑动平均滤波运行状态
//  Param      : pFilter-滤波器实例
//  Return     : None
//  Others     : 必须在开启ADC采样中断前调用
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
void AvgFilter_Init(avg_filter_t *pFilter);

/*******************************************************************************
//  Function   : AvgFilter_PushFromIsr
//  Description: 在ADC中断中向环形缓存写入一个原始采样值
//  Param      : pFilter-滤波器实例；adcValue-ADC原始采样值
//  Return     : None
//  Others     : 中断中只写入数据和更新索引，不执行求和、极值或除法运算
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
void AvgFilter_PushFromIsr(avg_filter_t *pFilter,uint16_t val);

/*******************************************************************************
//  Function   : AvgFilter_Process
//  Description: 对当前ADC缓冲区执行去极值平均滤波
//  Param      : pFilter-滤波器实例
//  Return     : 1-滤波结果更新成功；0-未更新
//  Others     : 去除一个最大值和一个最小值后计算平均值
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
uint8_t AvgFilter_Process(avg_filter_t *pFilter);

/*******************************************************************************
//  Function   : AvgFilter_GetFilteredValue
//  Description: 获取最近一次有效的ADC滤波结果
//  Param      : pFilter-滤波器实例
//  Return     : 滤波结果，无有效结果时返回0
//  Others     : 本函数不会触发ADC转换或执行滤波
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
uint16_t AvgFilter_GetFilteredValue(const avg_filter_t *pFilter);

/*******************************************************************************
//  Function   : AvgFilter_GetSequence
//  Description: 获取ADC累计采样序号
//  Param      : pFilter-滤波器实例
//  Return     : ADC累计采样序号，参数无效时返回0
//  Others     : 可用于判断PWM切频后是否产生新的ADC采样
//  Author     : Leo Chen 2026-08-28
*******************************************************************************/
uint32_t AvgFilter_GetSequence(const avg_filter_t *pFilter);
#endif /* FILTER_H */
