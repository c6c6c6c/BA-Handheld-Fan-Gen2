/**
 ***********************************************************************************************************************
 * @file    spray_adc.h
 * @brief   喷雾驱动ADC采集及滤波模块
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-24
 ***********************************************************************************************************************
 */

#ifndef _SPRAY_ADC_H_
#define _SPRAY_ADC_H_

#include <stdint.h>

/* Macro -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief ADC原始数据缓冲区长度
 *
 * @note 建议采用2的整数次幂，便于环形缓冲区管理
 */
#define SPRAY_ADC_BUFFER_SIZE             (16)

/**
 * @brief ADC软件滤波采样点数量
 */
#define SPRAY_ADC_FILTER_SIZE             (16)

/**
 * @brief ADC最大值
 *
 * @note 12位ADC对应4095，如实际ADC分辨率不同需修改
 */
#define SPRAY_ADC_MAX_VALUE               (4095U)

/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief ADC原始数据环形缓冲区
 */
typedef struct
{
    volatile uint16_t data[SPRAY_ADC_BUFFER_SIZE];  // ADC原始采样数据
    volatile uint8_t  writeIndex;                   // 当前写入位置
    volatile uint8_t  count;                        // 当前有效数据数量
    volatile uint32_t sequence;                     // ADC采样序号
} SprayAd_Buffer_t;

/**
 * @brief ADC运行变量
 */
typedef struct
{
    SprayAd_Buffer_t buffer;               // ADC原始采样缓冲区

    uint16_t filteredAd;                    // 当前滤波后的AD值
    uint16_t minAd;                         // 当前滤波窗口最小AD
    uint16_t maxAd;                         // 当前滤波窗口最大AD

    uint32_t lastSequence;                  // 上一次处理的ADC采样序号

    uint8_t  filterValid;                   // 滤波数据有效标志
} SprayAd_Runtime_t;

/* Function ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : SprayAd_Init
//  Description: 初始化喷雾ADC采集及滤波模块
//  Param      : 无
//  Return     : 无
//  Others     : ADC和定时器硬件初始化应由BSP层完成
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
void SprayAd_Init(void);

/*******************************************************************************
//  Function   : SprayAd_Task
//  Description: 执行喷雾ADC数据滤波处理
//  Param      : 无
//  Return     : 无
//  Others     : 建议在1ms周期任务中调用，不在ADC中断中执行复杂滤波
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
void SprayAd_Task(void);

/*******************************************************************************
//  Function   : SprayAd_PushSample
//  Description: 将ADC原始采样值写入采样缓冲区
//  Param      : adValue - ADC原始采样值
//  Return     : 无
//  Others     : 可在ADC转换完成中断中调用，本函数只执行快速数据缓存操作
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
void SprayAd_PushSample(uint16_t adValue);

/*******************************************************************************
//  Function   : SprayAd_GetFilteredValue
//  Description: 获取当前滤波后的喷雾反馈AD值
//  Param      : 无
//  Return     : 当前滤波后的AD反馈值
//  Others     : 本函数不启动ADC转换，仅返回后台处理后的最新结果
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
uint16_t SprayAd_GetFilteredValue(void);

/*******************************************************************************
//  Function   : SprayAd_IsValid
//  Description: 获取当前ADC滤波数据是否有效
//  Param      : 无
//  Return     : 0-无效，1-有效
//  Others     : 缓冲区获得足够采样数据后置为有效
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
uint8_t SprayAd_IsValid(void);

/*******************************************************************************
//  Function   : SprayAd_GetSequence
//  Description: 获取ADC当前采样序号
//  Param      : 无
//  Return     : ADC累计采样序号
//  Others     : 可用于判断PWM切频后是否产生新的ADC采样数据
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
uint32_t SprayAd_GetSequence(void);

/*******************************************************************************
//  Function   : SprayAd_ResetFilter
//  Description: 清除ADC历史滤波数据
//  Param      : 无
//  Return     : 无
//  Others     : PWM发生较大频率切换时可调用，避免旧频率数据参与新频率反馈计算
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
void SprayAd_ResetFilter(void);

/*******************************************************************************
//  Function   : SprayAd_GetRuntime
//  Description: 获取喷雾ADC模块运行状态
//  Param      : 无
//  Return     : ADC运行变量只读指针
//  Others     : 主要用于调试及状态监控
//  Author     : Leo Chen 2026-08-24
*******************************************************************************/
const SprayAd_Runtime_t *SprayAd_GetRuntime(void);

#endif

