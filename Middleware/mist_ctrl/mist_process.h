/**
 ***********************************************************************************************************************
 * @file    mist_process.h
 * @brief   雾化片追频各阶段处理接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-25
 ***********************************************************************************************************************
 */
#ifndef MIST_PROCESS_H
#define MIST_PROCESS_H

#include "mist_ctrl.h"

/*******************************************************************************
//  Function   : mist_process_set_freq
//  Description: 限制并应用新的雾化片PWM频率
//  Param      : ctrl - 追频算法实例指针；freq - 目标频率，单位Hz
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 频率切换后重新开始ADC稳定等待
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_set_freq(mist_ctrl_t *ctrl, uint32_t freq, uint32_t seq);

/*******************************************************************************
//  Function   : mist_process_is_ready
//  Description: 检查PWM切频后是否完成ADC数据稳定等待
//  Param      : ctrl - 追频算法实例指针；seq - 当前ADC采样序号
//  Return     : 0-未稳定，1-已稳定
//  Others     : 使用无符号差值计算，可兼容采样序号溢出回绕
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
uint8_t mist_process_is_ready(const mist_ctrl_t *ctrl, uint32_t seq);

/*******************************************************************************
//  Function   : mist_process_coarse
//  Description: 执行一次粗扫采样判断及频率步进
//  Param      : ctrl - 追频算法实例指针；ad - 滤波AD值
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 搜索谐振上升沿及粗略峰值
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_coarse(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq);

/*******************************************************************************
//  Function   : mist_process_fine
//  Description: 执行一次精扫采样判断及频率步进
//  Param      : ctrl - 追频算法实例指针；ad - 滤波AD值
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 在精扫窗口内搜索真实谐振峰值
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_fine(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq);

/*******************************************************************************
//  Function   : mist_process_backoff
//  Description: 从谐振峰值向高频侧寻找目标工作点
//  Param      : ctrl - 追频算法实例指针；ad - 滤波AD值
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 达到目标AD比例或最大退峰步数后锁频
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_backoff(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq);

/*******************************************************************************
//  Function   : mist_process_wait_ref
//  Description: 锁频后采集并建立稳定的漂移参考AD
//  Param      : ctrl - 追频算法实例指针；ad - 滤波AD值
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 反馈异常偏低时请求重新精扫
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_wait_ref(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq);

/*******************************************************************************
//  Function   : mist_process_monitor
//  Description: 使用滞回区间监控锁频工作点
//  Param      : ctrl - 追频算法实例指针；ad - 滤波AD值
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 反馈超出滞回范围时启动单步频率试探
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_monitor(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq);

/*******************************************************************************
//  Function   : mist_process_test_drift
//  Description: 评估单步漂移修正效果
//  Param      : ctrl - 追频算法实例指针；ad - 试探后的滤波AD值
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 仅当反馈更接近参考AD时保留当前频率
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_test_drift(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq);

#endif /* MIST_PROCESS_H */
