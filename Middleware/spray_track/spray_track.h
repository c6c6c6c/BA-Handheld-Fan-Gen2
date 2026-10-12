/**
 ***********************************************************************************************************************
 * @file    spray_track.h
 * @brief   超声波雾化片谐振频率搜索及频率漂移跟踪接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-25
 ***********************************************************************************************************************
 */
#ifndef __SPRAY_TRACK_H
#define __SPRAY_TRACK_H

#include <stdint.h>

/* 类型定义 ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Description: 追频算法运行状态
*******************************************************************************/
typedef enum
{
    SPRAY_TRACK_STATE_IDLE = 0,                // 空闲状态，追频算法未运行
    SPRAY_TRACK_STATE_COARSE,                  // 粗扫状态，搜索谐振上升沿及粗略峰值
    SPRAY_TRACK_STATE_FINE,                    // 精扫状态，在粗扫峰值附近搜索真实峰值
    SPRAY_TRACK_STATE_BACKOFF,                 // 退峰状态，从谐振峰值向高频侧寻找目标工作点
    SPRAY_TRACK_STATE_DRIFT_WAIT_REF,          // 漂移等待状态，锁频后等待稳定并建立参考AD
    SPRAY_TRACK_STATE_DRIFT_MONITOR,           // 漂移监控状态，监测反馈AD是否超出滞回区间
    SPRAY_TRACK_STATE_DRIFT_TEST,              // 漂移试探状态，单步调整频率并判断修正效果
    SPRAY_TRACK_STATE_ERROR                    // 错误状态，扫描或状态机运行异常
} spray_track_state_t;


/*******************************************************************************
//  Description: PWM频率设置回调函数类型
*******************************************************************************/
typedef void (*spray_track_set_frequency_t)(uint32_t frequencyHz);
typedef void (*spray_track_set_duty_t)(uint16_t duty);

/*******************************************************************************
//  Description: 追频算法只读参数表
*******************************************************************************/
typedef struct
{
    uint32_t centerFrequencyHz;                // 雾化片标称中心频率，单位Hz
    uint32_t minimumFrequencyHz;               // 最大扫频下限，单位Hz
    uint32_t maximumFrequencyHz;               // 最大扫频上限，单位Hz
    uint32_t coarseStepHz;                     // 粗扫步进，单位Hz
    uint32_t fineStepHz;                       // 精扫及漂移跟踪步进，单位Hz
    uint32_t settleSamples;                    // 切换频率后等待稳定的ADC原始采样数量
    uint32_t driftCheckSamples;                // 漂移检查周期对应的ADC采样数量
    uint8_t  fineHalfSteps;                    // 精扫半区间步数
    uint8_t  riseConfirm;                      // 判定上升沿所需的AD连续上升次数
    uint8_t  targetRatio;                      // 锁频目标AD占谐振峰值AD的百分比
    uint8_t  rescanRatio;                      // 触发重新精扫的AD下限百分比
    uint8_t  driftLowRatio;                    // 漂移跟踪滞回区间下限百分比
    uint8_t  driftHighRatio;                   // 漂移跟踪滞回区间上限百分比
    uint8_t  lockSettleCount;                  // 锁频后建立稳定参考AD所需的检查次数
    uint8_t  maxBackoffSteps;                  // 工作频率允许偏离谐振峰值的最大步数
    uint8_t  errorFaultCount;                  // 连续进入错误状态后置故障位的次数
    uint8_t  defauleduty;
} spray_track_param_t;


/*******************************************************************************
//  Description: 追频算法配置参数
*******************************************************************************/
typedef struct
{
    spray_track_set_frequency_t setFrequency;  // PWM输出频率设置回调函数
    spray_track_set_duty_t setDuty;            // PWM输出频率设置回调函数
} spray_track_config_t;


/*******************************************************************************
//  Description: 追频算法运行实例
*******************************************************************************/
typedef struct
{
    spray_track_state_t state;                  // 当前追频算法状态
    uint32_t currentFrequencyHz;                // 当前设置的PWM频率，单位Hz
    uint32_t peakFrequencyHz;                   // 最近一次搜索到的谐振峰值频率，单位Hz
    uint32_t lockFrequencyHz;                   // 最近一次锁定的工作频率，单位Hz
    uint16_t peakAd;                            // 最近一次搜索到的谐振峰值AD
    uint16_t lockTargetAd;                      // 根据峰值AD计算得到的目标工作AD
    uint16_t referenceAd;                       // 漂移跟踪参考AD
    uint8_t  locked;                            // 锁频状态：0-未锁频，1-已锁频
    uint8_t  rescanCount;                       // 自动重新扫描次数
    uint8_t  fault;                             // 故障位：0-正常，1-连续错误达到故障次数
    uint8_t  errorCount;                        // 连续进入错误状态计数
} spray_track_status_t;

/*******************************************************************************
//  Description: 追频算法运行状态信息
*******************************************************************************/
typedef struct
{
    const spray_track_param_t *pParam;             // 当前实例使用的只读算法参数表
    spray_track_config_t config;                   // 追频算法配置参数
    spray_track_status_t status;                   // 追频算法当前运行状态
    uint32_t minimumFrequencyHz;                   // 粗扫允许的最小频率，单位Hz
    uint32_t maximumFrequencyHz;                   // 粗扫允许的最大频率，单位Hz
    uint32_t scanEndFrequencyHz;                   // 当前扫描阶段的结束频率，单位Hz
    uint32_t settleSequence;                       // 最近一次切换频率时的ADC采样序号
    uint32_t lastSequence;                         // 上一次执行算法时的ADC采样序号
    uint32_t checkSequence;                        // 上一次漂移检查时的ADC采样序号
    uint32_t testOriginalFrequencyHz;              // 漂移试探前的原始频率，单位Hz
    uint16_t previousAd;                           // 上一次参与判断的滤波AD值
    uint16_t testOriginalAd;                       // 漂移试探前的原始AD值
    uint32_t referenceSum;                         // 锁频收敛阶段的参考AD累加值
    uint8_t riseCount;                             // AD连续上升次数
    uint8_t riseFound;                             // 谐振上升沿已找到标志
    uint8_t referenceCount;                        // 参考AD累计次数
    int8_t  testDirection;                         // 漂移试探方向：-1降频，1升频
    uint8_t initialized;                           // 模块初始化完成标志
} spray_track_runtime_t;
/* 常量参数 ----------------------------------------------------------------------------------------------------------*/

extern const spray_track_param_t g_sprayTrackParam;   // 追频算法只读常量参数表

/* 函数声明 ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : SprayTrack_Init
//  Description: 初始化谐振频率搜索及频率漂移跟踪模块
//  Param      : pHandle - 追频算法实例指针
//               pParam - 只读算法参数表指针
//               pConfig - PWM频率设置回调配置
//  Return     : 0-初始化失败，1-初始化成功
//  Others     : 初始化完成后模块保持空闲，调用SprayTrack_Start后开始扫描
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
uint8_t SprayTrack_Init(spray_track_runtime_t *pHandle,
                        const spray_track_param_t *pParam,
                        const spray_track_config_t *pConfig);

/*******************************************************************************
//  Function   : SprayTrack_Start
//  Description: 启动新一轮谐振频率粗略扫描
//  Param      : pHandle - 追频算法实例指针
//               sampleSequence - 当前ADC原始采样序号
//  Return     : 无
//  Others     : 从扫描范围的高频端开始，逐步向低频方向扫描
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrack_Start(spray_track_runtime_t *pHandle, uint32_t sampleSequence);

/*******************************************************************************
//  Function   : SprayTrack_Stop
//  Description: 停止谐振频率搜索及频率漂移跟踪
//  Param      : pHandle - 追频算法实例指针
//  Return     : 无
//  Others     : 本函数只停止追频状态机，不停止PWM外设输出
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrack_Stop(spray_track_runtime_t *pHandle);

/*******************************************************************************
//  Function   : SprayTrack_Task
//  Description: 执行一次非阻塞追频算法任务
//  Param      : pHandle - 追频算法实例指针
//               filteredAd - 最新的雾化片反馈滤波AD值
//               sampleSequence - 当前ADC原始采样序号
//  Return     : 无
//  Others     : 在SprayAd_Task之后循环调用，重复的ADC采样数据将被忽略
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrack_Task(spray_track_runtime_t *pHandle, 
										 uint16_t               filteredAd, 
										 uint32_t               sampleSequence);

/*******************************************************************************
//  Function   : SprayTrack_GetStatus
//  Description: 获取当前谐振频率跟踪状态
//  Param      : pHandle - 追频算法实例指针
//  Return     : 追频算法状态只读指针
//  Others     : 主要用于应用层状态监控及调试器观察
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
const spray_track_status_t *SprayTrack_GetStatus(const spray_track_runtime_t *pHandle);

#endif /* __SPRAY_TRACK_H */
