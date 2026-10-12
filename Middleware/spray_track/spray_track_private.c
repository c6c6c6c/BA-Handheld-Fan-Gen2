/**
 ***********************************************************************************************************************
 * @file    spray_track_private.c
 * @brief   超声波雾化片追频算法私有接口实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-25
 ***********************************************************************************************************************
 */
#include "spray_track_private.h"

/* 私有函数 ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : SprayTrack_AbsDiff
//  Description: 计算两个无符号数之间的绝对差值
//  Param      : valueA - 第一个数值；valueB - 第二个数值
//  Return     : 两个数值的绝对差值
//  Others     : 无
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
static uint32_t SprayTrack_AbsDiff(uint32_t valueA, uint32_t valueB)
{
    return (valueA >= valueB) ? (valueA - valueB) : (valueB - valueA);
}

/*******************************************************************************
//  Function   : SprayTrack_StepDown
//  Description: Calculate a downward frequency step without unsigned underflow
//  Param      : pHandle - Tracking algorithm instance pointer
//               stepHz - Step width in Hz
//  Return     : Next frequency clamped to the configured minimum frequency
//  Others     : None
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
static uint32_t SprayTrack_StepDown(const spray_track_runtime_t *pHandle, uint32_t stepHz)
{
    if (pHandle->status.currentFrequencyHz <= pHandle->minimumFrequencyHz)
    {
        return pHandle->minimumFrequencyHz;
    }
    if ((pHandle->status.currentFrequencyHz - pHandle->minimumFrequencyHz) <= stepHz)
    {
        return pHandle->minimumFrequencyHz;
    }
    return pHandle->status.currentFrequencyHz - stepHz;
}

/*******************************************************************************
//  Function   : SprayTrackPrivate_ApplyFrequency
//  Description: 限制并应用新的雾化片PWM频率
//  Param      : pHandle - 追频算法实例指针
//               frequencyHz - 请求设置的输出频率，单位Hz
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 频率切换后启动稳定等待，避免旧频率对应的ADC数据参与计算
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_ApplyFrequency(spray_track_runtime_t *pHandle,
                                      uint32_t frequencyHz,
                                      uint32_t sampleSequence)
{
    if (frequencyHz < pHandle->minimumFrequencyHz)
    {
        frequencyHz = pHandle->minimumFrequencyHz;
    }
    else if (frequencyHz > pHandle->maximumFrequencyHz)
    {
        frequencyHz = pHandle->maximumFrequencyHz;
    }

    pHandle->status.currentFrequencyHz = frequencyHz;
    pHandle->settleSequence = sampleSequence;
    pHandle->config.setFrequency(frequencyHz);
}

/*******************************************************************************
//  Function   : SprayTrackPrivate_IsSettled
//  Description: 检查PWM切频后是否已经完成ADC数据稳定等待
//  Param      : pHandle - 追频算法实例指针
//               sampleSequence - 当前ADC采样序号
//  Return     : 0-未稳定，1-已稳定
//  Others     : 使用无符号数差值计算，可兼容采样序号溢出回绕
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
uint8_t SprayTrackPrivate_IsSettled(const spray_track_runtime_t *pHandle, uint32_t sampleSequence)
{
    return ((sampleSequence - pHandle->settleSequence) >= pHandle->pParam->settleSamples) ? 1U : 0U;
}

/*******************************************************************************
//  Function   : SprayTrack_BeginFineScan
//  Description: 以粗扫谐振峰值频率为中心启动精细扫描
//  Param      : pHandle - 追频算法实例指针
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 精扫范围由常量参数表中的fineHalfSteps参数确定
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
static void SprayTrack_BeginFineScan(spray_track_runtime_t *pHandle, uint32_t sampleSequence)
{
    uint32_t halfWindowHz;
    uint32_t startFrequencyHz;

    halfWindowHz     = pHandle->pParam->fineStepHz * pHandle->pParam->fineHalfSteps;
    startFrequencyHz = pHandle->status.peakFrequencyHz + halfWindowHz;
    pHandle->scanEndFrequencyHz = (pHandle->status.peakFrequencyHz > halfWindowHz) ?
                                  (pHandle->status.peakFrequencyHz - halfWindowHz) :
                                   pHandle->minimumFrequencyHz;
    if (pHandle->scanEndFrequencyHz < pHandle->minimumFrequencyHz)
    {
        pHandle->scanEndFrequencyHz = pHandle->minimumFrequencyHz;
    }

    pHandle->status.state  = SPRAY_TRACK_STATE_FINE;
    pHandle->status.peakAd = 0U;
    SprayTrackPrivate_ApplyFrequency(pHandle, startFrequencyHz, sampleSequence);
}

/*******************************************************************************
//  Function   : SprayTrack_RequestFineRescan
//  Description: 请求围绕最近一次谐振峰值重新执行精细扫描
//  Param      : pHandle - 追频算法实例指针
//               sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 如果不存在有效峰值，则重新执行粗略扫描
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
static void SprayTrack_RequestFineRescan(spray_track_runtime_t *pHandle, uint32_t sampleSequence)
{
    pHandle->status.locked = 0U;
    pHandle->status.rescanCount++;
    if ((pHandle->status.peakAd == 0U) || (pHandle->status.peakFrequencyHz == 0U))
    {
        SprayTrack_Start(pHandle, sampleSequence);
    }
    else
    {
        SprayTrack_BeginFineScan(pHandle, sampleSequence);
    }
}

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunCoarse
//  Description: 执行一次粗扫采样判断及频率步进
//  Param      : pHandle - 追频算法实例指针
//               filteredAd - 稳定后的滤波AD值；sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 上升沿判断时屏蔽AD值低4位，避免微小波动造成误判
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunCoarse(spray_track_runtime_t *pHandle,
                                 uint16_t filteredAd,
                                 uint32_t sampleSequence)
{
    uint16_t currentMasked;
    uint16_t previousMasked;

    currentMasked  = filteredAd          & 0xFFF0U;
    previousMasked = pHandle->previousAd & 0xFFF0U;
    if (currentMasked > previousMasked)
    {
        if (pHandle->riseCount < pHandle->pParam->riseConfirm)
        {
            pHandle->riseCount++;
        }
        if (pHandle->riseCount >= pHandle->pParam->riseConfirm)
        {
            pHandle->riseFound = 1U;
        }
    }
    else
    {
        pHandle->riseCount = 0U;
    }
    pHandle->previousAd = filteredAd;

    if ((pHandle->riseFound != 0U) && (filteredAd > pHandle->status.peakAd))
    {
        pHandle->status.peakAd = filteredAd;
        pHandle->status.peakFrequencyHz = pHandle->status.currentFrequencyHz;
    }

    if (pHandle->status.currentFrequencyHz <= pHandle->minimumFrequencyHz)
    {
        if ((pHandle->riseFound != 0U) && (pHandle->status.peakAd != 0U))
        {
            SprayTrack_BeginFineScan(pHandle, sampleSequence);
        }
        else
        {
            pHandle->status.state = SPRAY_TRACK_STATE_ERROR;
        }
        return;
    }

    SprayTrackPrivate_ApplyFrequency(pHandle,
                                     SprayTrack_StepDown(pHandle, pHandle->pParam->coarseStepHz),
                                     sampleSequence);
}

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunFine
//  Description: 执行一次精扫采样判断及频率步进
//  Param      : pHandle - 追频算法实例指针
//               filteredAd - 稳定后的滤波AD值；sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 完整扫描精扫窗口，并保存窗口内的真实峰值
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunFine(spray_track_runtime_t *pHandle,
                               uint16_t filteredAd,
                               uint32_t sampleSequence)
{
    if (filteredAd > pHandle->status.peakAd)
    {
        pHandle->status.peakAd          = filteredAd;
        pHandle->status.peakFrequencyHz = pHandle->status.currentFrequencyHz;
    }

    if (pHandle->status.currentFrequencyHz <= pHandle->scanEndFrequencyHz)
    {
        if (pHandle->status.peakAd == 0U)
        {
            pHandle->status.state = SPRAY_TRACK_STATE_ERROR;
            return;
        }
        pHandle->status.lockTargetAd = (uint16_t)(((uint32_t)pHandle->status.peakAd *
                                                  pHandle->pParam->targetRatio) / 100U);
        pHandle->status.state      = SPRAY_TRACK_STATE_BACKOFF;
        pHandle->status.fault      = 0U;
        pHandle->status.errorCount = 0U;
        SprayTrackPrivate_ApplyFrequency(pHandle, pHandle->status.peakFrequencyHz, sampleSequence);
        return;
    }

    SprayTrackPrivate_ApplyFrequency(pHandle,
                                     SprayTrack_StepDown(pHandle, pHandle->pParam->fineStepHz),
                                     sampleSequence);
}

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunBackoff
//  Description: 从峰值点向高频方向退峰并寻找目标工作点
//  Param      : pHandle - 追频算法实例指针
//               filteredAd - 稳定后的滤波AD值；sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 当AD值下降至峰值AD的设定比例时锁定当前频率
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunBackoff(spray_track_runtime_t *pHandle,
                                  uint16_t filteredAd,
                                  uint32_t sampleSequence)
{
    uint32_t maximumBackoffHz;

    maximumBackoffHz = pHandle->pParam->fineStepHz * pHandle->pParam->maxBackoffSteps;
    if ((filteredAd <= pHandle->status.lockTargetAd) 
    ||  (SprayTrack_AbsDiff(pHandle->status.currentFrequencyHz, pHandle->status.peakFrequencyHz) >= maximumBackoffHz))
    {
        pHandle->status.lockFrequencyHz = pHandle->status.currentFrequencyHz;
        pHandle->status.referenceAd     = filteredAd;
        pHandle->status.locked          = 1U;
        pHandle->status.state           = SPRAY_TRACK_STATE_DRIFT_WAIT_REF;
        pHandle->referenceSum           = 0U;
        pHandle->referenceCount         = 0U;
        pHandle->checkSequence          = sampleSequence;
        return;
    }

    SprayTrackPrivate_ApplyFrequency(pHandle,
                                     pHandle->status.currentFrequencyHz + pHandle->pParam->fineStepHz,
                                     sampleSequence);
}

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunDriftWait
//  Description: 锁频后采集并计算稳定的漂移跟踪参考AD值
//  Param      : pHandle - 追频算法实例指针
//               filteredAd - 当前滤波AD值；sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 反馈AD异常偏低时立即请求重新精扫
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunDriftWait(spray_track_runtime_t *pHandle,
                                    uint16_t filteredAd,
                                    uint32_t sampleSequence)
{
    uint16_t rescanLimit;

    if ((sampleSequence - pHandle->checkSequence) < pHandle->pParam->driftCheckSamples)
    {
        return;
    }
    pHandle->checkSequence = sampleSequence;
    rescanLimit = (uint16_t)(((uint32_t)pHandle->status.lockTargetAd * pHandle->pParam->rescanRatio) / 100U);
    if (filteredAd < rescanLimit)
    {
        SprayTrack_RequestFineRescan(pHandle, sampleSequence);
        return;
    }

    pHandle->referenceSum += filteredAd;
    pHandle->referenceCount++;
    if (pHandle->referenceCount >= pHandle->pParam->lockSettleCount)
    {
        pHandle->status.referenceAd = (uint16_t)(pHandle->referenceSum / pHandle->referenceCount);
        pHandle->status.state       = SPRAY_TRACK_STATE_DRIFT_MONITOR;
        pHandle->referenceSum       = 0U;
        pHandle->referenceCount     = 0U;
    }
}

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunDriftMonitor
//  Description: 使用滞回区间监控锁频工作点
//  Param      : pHandle - 追频算法实例指针
//               filteredAd - 当前滤波AD值；sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 仅当反馈AD超出滞回区间时启动单步频率试探
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunDriftMonitor(spray_track_runtime_t *pHandle,
                                       uint16_t filteredAd,
                                       uint32_t sampleSequence)
{
    uint16_t lowerLimit;
    uint16_t upperLimit;
    uint16_t rescanLimit;
    uint32_t testFrequencyHz;

    if ((sampleSequence - pHandle->checkSequence) < pHandle->pParam->driftCheckSamples)
    {
        return;
    }
    pHandle->checkSequence = sampleSequence;
    lowerLimit  = (uint16_t)(((uint32_t)pHandle->status.referenceAd * pHandle->pParam->driftLowRatio)  / 100U);
    upperLimit  = (uint16_t)(((uint32_t)pHandle->status.referenceAd * pHandle->pParam->driftHighRatio) / 100U);
    rescanLimit = (uint16_t)(((uint32_t)pHandle->status.referenceAd * pHandle->pParam->rescanRatio)    / 100U);

    if (filteredAd < rescanLimit)
    {
        SprayTrack_RequestFineRescan(pHandle, sampleSequence);
        return;
    }
    if ((filteredAd >= lowerLimit) && (filteredAd <= upperLimit))
    {
        return;
    }

    pHandle->testOriginalAd          = filteredAd;
    pHandle->testOriginalFrequencyHz = pHandle->status.currentFrequencyHz;
    pHandle->testDirection           = (filteredAd > upperLimit) ? 1 : -1;
    if (pHandle->testDirection > 0)
    {
        testFrequencyHz = pHandle->status.currentFrequencyHz + pHandle->pParam->fineStepHz;
    }
    else
    {
        testFrequencyHz = SprayTrack_StepDown(pHandle, pHandle->pParam->fineStepHz);
    }
    pHandle->status.state = SPRAY_TRACK_STATE_DRIFT_TEST;
    SprayTrackPrivate_ApplyFrequency(pHandle, testFrequencyHz, sampleSequence);
}

/*******************************************************************************
//  Function   : SprayTrackPrivate_RunDriftTest
//  Description: 评估单步漂移修正效果，并决定保留或撤销本次频率调整
//  Param      : pHandle - 追频算法实例指针
//               filteredAd - 试探频率稳定后的滤波AD值；sampleSequence - 当前ADC采样序号
//  Return     : 无
//  Others     : 仅当调整后AD值更接近参考AD时保留当前频率
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void SprayTrackPrivate_RunDriftTest(spray_track_runtime_t *pHandle,
                                    uint16_t filteredAd,
                                    uint32_t sampleSequence)
{
    uint32_t oldError;
    uint32_t newError;
    uint32_t maximumBackoffHz;

    oldError = SprayTrack_AbsDiff(pHandle->testOriginalAd, pHandle->status.referenceAd);
    newError = SprayTrack_AbsDiff(filteredAd, pHandle->status.referenceAd);
    maximumBackoffHz = pHandle->pParam->fineStepHz * pHandle->pParam->maxBackoffSteps;

    if ((newError >= oldError) ||
        (SprayTrack_AbsDiff(pHandle->status.currentFrequencyHz, pHandle->status.peakFrequencyHz) > maximumBackoffHz))
    {
        SprayTrackPrivate_ApplyFrequency(pHandle, pHandle->testOriginalFrequencyHz, sampleSequence);
    }
    else
    {
        pHandle->status.lockFrequencyHz = pHandle->status.currentFrequencyHz;
        pHandle->checkSequence = sampleSequence;
    }
    pHandle->status.state = SPRAY_TRACK_STATE_DRIFT_MONITOR;
}
