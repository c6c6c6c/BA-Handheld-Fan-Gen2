/**
 ***********************************************************************************************************************
 * @file    spray_track_private.h
 * @brief   超声波雾化片追频算法私有接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-25
 ***********************************************************************************************************************
 */
#ifndef __SPRAY_TRACK_PRIVATE_H
#define __SPRAY_TRACK_PRIVATE_H

#include "spray_track.h"

/* 私有函数声明 ------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : SprayTrackPrivate_ApplyFrequency
//  Description: 限制并应用新的雾化片PWM频率
//  Param      : pHandle - 追频算法实例指针；frequencyHz - 目标频率，单位Hz
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 频率切换后重新开始ADC稳定等待
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_ApplyFrequency(spray_track_runtime_t *pHandle,
                                      uint32_t frequencyHz,
                                      uint32_t sampleSequence);

/*******************************************************************************
//  Function   : SprayTrackPrivate_IsSettled
//  Description: 检查PWM切频后是否完成ADC数据稳定等待
//  Param      : pHandle - 追频算法实例指针；sampleSequence - 当前ADC采样序号
//  Return     : 0-未稳定，1-已稳定
//  Others     : 使用无符号差值计算，可兼容采样序号溢出回绕
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
uint8_t SprayTrackPrivate_IsSettled(const spray_track_runtime_t *pHandle,
                                    uint32_t sampleSequence);

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunCoarse
//  Description: 执行一次粗扫采样判断及频率步进
//  Param      : pHandle - 追频算法实例指针；filteredAd - 滤波AD值
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 搜索谐振上升沿及粗略峰值
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunCoarse(spray_track_runtime_t *pHandle,
                                 uint16_t filteredAd,
                                 uint32_t sampleSequence);

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunFine
//  Description: 执行一次精扫采样判断及频率步进
//  Param      : pHandle - 追频算法实例指针；filteredAd - 滤波AD值
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 在精扫窗口内搜索真实谐振峰值
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunFine(spray_track_runtime_t *pHandle,
                               uint16_t filteredAd,
                               uint32_t sampleSequence);

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunBackoff
//  Description: 从谐振峰值向高频侧寻找目标工作点
//  Param      : pHandle - 追频算法实例指针；filteredAd - 滤波AD值
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 达到目标AD比例或最大退峰步数后锁频
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunBackoff(spray_track_runtime_t *pHandle,
                                  uint16_t filteredAd,
                                  uint32_t sampleSequence);

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunDriftWait
//  Description: 锁频后采集并建立稳定的漂移参考AD
//  Param      : pHandle - 追频算法实例指针；filteredAd - 滤波AD值
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 反馈异常偏低时请求重新精扫
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunDriftWait(spray_track_runtime_t *pHandle,
                                    uint16_t filteredAd,
                                    uint32_t sampleSequence);

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunDriftMonitor
//  Description: 使用滞回区间监控锁频工作点
//  Param      : pHandle - 追频算法实例指针；filteredAd - 滤波AD值
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 反馈超出滞回范围时启动单步频率试探
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunDriftMonitor(spray_track_runtime_t *pHandle,
                                       uint16_t filteredAd,
                                       uint32_t sampleSequence);

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunDriftTest
//  Description: 评估单步漂移修正效果
//  Param      : pHandle - 追频算法实例指针；filteredAd - 试探后的滤波AD值
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 仅当反馈更接近参考AD时保留当前频率
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunDriftTest(spray_track_runtime_t *pHandle,
                                    uint16_t filteredAd,
                                    uint32_t sampleSequence);

#endif /* __SPRAY_TRACK_PRIVATE_H */
