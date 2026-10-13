/**
 ***********************************************************************************************************************
 * @file    spray_track.c
 * @brief   超声波雾化片谐振频率搜索及频率漂移跟踪模块
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-25
 ***********************************************************************************************************************
 */
#include "spray_track.h"
#include "spray_track_private.h"

/* 变量 --------------------------------------------------------------------------------------------------------------*/
const spray_track_param_t g_sprayTrackParam =        // 追频算法只读常量参数表
{
   .centerFrequencyHz  = 108000UL,  // 雾化片标称中心频率，单位Hz
   .minimumFrequencyHz = 101000UL,  // 最大扫频下限，单位Hz
   .maximumFrequencyHz = 115000UL,  // 最大扫频上限，单位Hz
   .coarseStepHz       = 1000UL,    // 粗扫步进，单位Hz
   .fineStepHz         = 50UL,      // 精扫及漂移跟踪步进，单位Hz
   .settleSamples      = 400UL,     // 切换频率后等待稳定的ADC原始采样数量
   .driftCheckSamples  = 3000UL,    // 漂移检查周期，2kHz采样频率下约192ms
   .fineHalfSteps      = 8U,        // 精扫半区间步数，范围为粗扫峰值正负8步
   .riseConfirm        = 2U,        // 判定上升沿所需的AD连续上升次数
   .targetRatio        = 100U,      // 锁频目标AD占谐振峰值AD的百分比
   .rescanRatio        = 70U,       // 触发重新精扫的AD下限百分比
   .driftLowRatio      = 95U,       // 漂移跟踪滞回区间下限百分比
   .driftHighRatio     = 105U,      // 漂移跟踪滞回区间上限百分比
   .lockSettleCount    = 10U,       // 锁频后建立稳定参考AD所需的检查次数
   .maxBackoffSteps    = 10U,       // 工作频率允许偏离谐振峰值的最大步数
   .errorFaultCount    = 3U,        // 连续进入错误状态后置故障位的次数
   .defauleduty        = 50U        // 开始默认占空比20%
};


/* 公共函数 ----------------------------------------------------------------------------------------------------------*/

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
uint8_t SprayTrack_Init(spray_track_runtime_t      *pHandle,
                        const spray_track_param_t  *pParam,
                        const spray_track_config_t *pConfig)
{
    if ((pHandle                    == 0)
    ||  (pParam                     == 0)
    ||  (pConfig                    == 0)
    ||  (pConfig->setFrequency      == 0)
    ||  (pConfig->setDuty           == 0)
    ||  (pParam->centerFrequencyHz  == 0U)
    ||  (pParam->minimumFrequencyHz == 0U)
    ||  (pParam->maximumFrequencyHz == 0U)
    ||  (pParam->coarseStepHz       == 0U)
    ||  (pParam->fineStepHz         == 0U)
    ||  (pParam->fineHalfSteps      == 0U)
    ||  (pParam->riseConfirm        == 0U)
    ||  (pParam->targetRatio        == 0U)
    ||  (pParam->errorFaultCount    == 0U)
    ||  (pParam->minimumFrequencyHz >= pParam->maximumFrequencyHz)
    ||  (pParam->centerFrequencyHz < pParam->minimumFrequencyHz)
    ||  (pParam->centerFrequencyHz > pParam->maximumFrequencyHz)
    ||  (pParam->targetRatio > 100U))
    {
        return 0U;
    }

    pHandle->pParam                     = pParam;
    pHandle->config                     = *pConfig;
    pHandle->minimumFrequencyHz         = pHandle->pParam->minimumFrequencyHz;
    pHandle->maximumFrequencyHz         = pHandle->pParam->maximumFrequencyHz;
    pHandle->status.state               = SPRAY_TRACK_STATE_IDLE;
    pHandle->status.currentFrequencyHz  = pHandle->pParam->centerFrequencyHz;
    pHandle->status.peakFrequencyHz     = 0U;
    pHandle->status.lockFrequencyHz     = 0U;
    pHandle->status.peakAd              = 0U;
    pHandle->status.lockTargetAd        = 0U;
    pHandle->status.referenceAd         = 0U;
    pHandle->status.locked              = 0U;
    pHandle->status.rescanCount         = 0U;
    pHandle->status.fault               = 0U;
    pHandle->status.errorCount          = 0U;
    pHandle->scanEndFrequencyHz         = 0U;
    pHandle->settleSequence             = 0U;
    pHandle->lastSequence               = 0U;
    pHandle->checkSequence              = 0U;
    pHandle->testOriginalFrequencyHz    = 0U;
    pHandle->previousAd                 = 0U;
    pHandle->testOriginalAd             = 0U;
    pHandle->referenceSum               = 0U;
    pHandle->riseCount                  = 0U;
    pHandle->riseFound                  = 0U;
    pHandle->referenceCount             = 0U;
    pHandle->testDirection              = 0;
    pHandle->initialized                = 1U;
    return 1U;
}

/*******************************************************************************
//  Function   : SprayTrack_Start
//  Description: 启动新一轮谐振频率粗略扫描
//  Param      : pHandle - 追频算法实例指针
//               sampleSequence - 当前ADC原始采样序号
//  Return     : 无
//  Others     : 从扫描范围高频端开始，逐步向低频方向扫描
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrack_Start(spray_track_runtime_t *pHandle, uint32_t sampleSequence)
{
    if ((pHandle == 0) || (pHandle->initialized == 0U))
    {
        return;
    }
    pHandle->status.state           = SPRAY_TRACK_STATE_COARSE;
    pHandle->status.locked          = 0U;
    pHandle->status.peakAd          = 0U;
    pHandle->status.peakFrequencyHz = 0U;
    pHandle->previousAd             = 0U;
    pHandle->riseCount              = 0U;
    pHandle->riseFound              = 0U;
    pHandle->lastSequence           = sampleSequence;
    SprayTrackPrivate_ApplyFrequency(pHandle, pHandle->maximumFrequencyHz, sampleSequence);
    //pHandle->config.setDuty(pHandle->pParam->defauleduty); //开始默认占空比50%
}

/*******************************************************************************
//  Function   : SprayTrack_Stop
//  Description: 停止谐振频率搜索及频率漂移跟踪
//  Param      : pHandle - 追频算法实例指针
//  Return     : 无
//  Others     : 本函数只停止算法状态机，不停止PWM外设输出
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrack_Stop(spray_track_runtime_t *pHandle)
{
    if (pHandle == 0)
    {
        return;
    }
    pHandle->status.state  = SPRAY_TRACK_STATE_IDLE;
    pHandle->status.locked = 0U;
    pHandle->config.setDuty(0); //开始默认占空比50%
}

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
										 uint32_t               sampleSequence)
{
    if ((pHandle == 0)
     || (pHandle->initialized  == 0U)
     || (pHandle->status.state == SPRAY_TRACK_STATE_IDLE) 
     || (sampleSequence == pHandle->lastSequence))
    {
        return;
    }
    pHandle->lastSequence = sampleSequence;
    if (SprayTrackPrivate_IsSettled(pHandle, sampleSequence) == 0U)
		//||  adDiff >= pHandle->pParam->adcDiff)
    {
        return;
    }

    switch (pHandle->status.state)
    {
        /* 粗扫状态：从高频向低频扫描，寻找谐振上升沿及粗略峰值 */
        case SPRAY_TRACK_STATE_COARSE:
            SprayTrackPrivate_RunCoarse(pHandle, filteredAd, sampleSequence);
            break;

        /* 精扫状态：在粗扫峰值附近进行小范围扫描，确定真实谐振峰值 */
        case SPRAY_TRACK_STATE_FINE:
            SprayTrackPrivate_RunFine(pHandle, filteredAd, sampleSequence);
            break;

        /* 退峰状态：从谐振峰值向高频侧移动，寻找目标AD对应的工作频率 */
        case SPRAY_TRACK_STATE_BACKOFF:
            SprayTrackPrivate_RunBackoff(pHandle, filteredAd, sampleSequence);
            break;

        /* 漂移等待状态：锁频后等待系统稳定，并采集漂移跟踪参考AD值 */
        case SPRAY_TRACK_STATE_DRIFT_WAIT_REF:
            SprayTrackPrivate_RunDriftWait(pHandle, filteredAd, sampleSequence);
            break;

        /* 漂移监控状态：周期检查反馈AD是否处于允许的滞回范围内 */
        case SPRAY_TRACK_STATE_DRIFT_MONITOR:
            SprayTrackPrivate_RunDriftMonitor(pHandle, filteredAd, sampleSequence);
            break;

        /* 漂移试探状态：调整一个频率步进，判断频率修正方向是否正确 */
        case SPRAY_TRACK_STATE_DRIFT_TEST:
            SprayTrackPrivate_RunDriftTest(pHandle, filteredAd, sampleSequence);
            break;
				

        case SPRAY_TRACK_STATE_ERROR:
            if (pHandle->status.errorCount < pHandle->pParam->errorFaultCount)
            {
                pHandle->status.errorCount++;
            }

            if (pHandle->status.errorCount >= pHandle->pParam->errorFaultCount)
            {
                pHandle->status.fault = 1U;
            }

            SprayTrack_Start(pHandle, sampleSequence);
            break;

        /* 未知状态：状态机运行异常，切换至错误状态 */
        default:
            pHandle->status.state = SPRAY_TRACK_STATE_ERROR;
            break;
    }

}

/*******************************************************************************
//  Function   : SprayTrack_GetStatus
//  Description: 获取当前谐振频率跟踪状态
//  Param      : pHandle - 追频算法实例指针
//  Return     : 追频算法状态只读指针
//  Others     : 主要用于应用层状态监控及调试器观察
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
const spray_track_status_t *SprayTrack_GetStatus(const spray_track_runtime_t *pHandle)
{
    if (pHandle == 0)
    {
        return 0;
    }
    return &pHandle->status;
}
