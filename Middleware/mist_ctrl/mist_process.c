/**
 ***********************************************************************************************************************
 * @file    mist_process.c
 * @brief   雾化片粗扫、精扫、退峰及漂移处理实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-25
 ***********************************************************************************************************************
 */
#include "mist_process.h"

/*******************************************************************************
//  Function   : mist_process_diff
//  Description: 计算两个无符号数之间的绝对差值
//  Param      : a - 第一个数值；b - 第二个数值
//  Return     : 两个数值的绝对差值
//  Others     : 无
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
static uint32_t mist_process_diff(uint32_t a, uint32_t b)
{
    return (a >= b) ? (a - b) : (b - a);
}

/*******************************************************************************
//  Function   : mist_process_down
//  Description: 按步长降低频率，并避免无符号减法下溢
//  Param      : ctrl - 追频算法实例指针
//               step - 降频步长，单位Hz
//  Return     : 不低于配置下限的下一步频率
//  Others     : 无
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
static uint32_t mist_process_down(const mist_ctrl_t *ctrl, uint32_t step)
{
    if (ctrl->stat.freq <= ctrl->minFreq)
    {
        return ctrl->minFreq;
    }
    if ((ctrl->stat.freq - ctrl->minFreq) <= step)
    {
        return ctrl->minFreq;
    }
    return ctrl->stat.freq - step;
}

/*******************************************************************************
//  Function   : mist_process_set_freq
//  Description: 限制并应用新的雾化片PWM频率
//  Param      : ctrl - 追频算法实例指针
//               freq - 请求设置的输出频率，单位Hz
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 频率切换后启动稳定等待，避免旧频率对应的ADC数据参与计算
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_set_freq(mist_ctrl_t *ctrl, uint32_t freq, uint32_t seq)
{
    if (freq < ctrl->minFreq)
    {
        freq = ctrl->minFreq;
    }
    else if (freq > ctrl->maxFreq)
    {
        freq = ctrl->maxFreq;
    }

    ctrl->stat.freq = freq;
    ctrl->settleSeq = seq;
    ctrl->api.setFreq(freq);
}

/*******************************************************************************
//  Function   : mist_process_is_ready
//  Description: 检查PWM切频后是否已经完成ADC数据稳定等待
//  Param      : ctrl - 追频算法实例指针
//               seq - 当前ADC采样序号
//  Return     : 0-未稳定，1-已稳定
//  Others     : 使用无符号数差值计算，可兼容采样序号溢出回绕
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
uint8_t mist_process_is_ready(const mist_ctrl_t *ctrl, uint32_t seq)
{
    return ((seq - ctrl->settleSeq) >= ctrl->cfg->settleN) ? 1U : 0U;
}

/*******************************************************************************
//  Function   : mist_process_begin_fine
//  Description: 以粗扫谐振峰值频率为中心启动精细扫描
//  Param      : ctrl - 追频算法实例指针
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 精扫范围由常量参数表中的fineN参数确定
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
static void mist_process_begin_fine(mist_ctrl_t *ctrl, uint32_t seq)
{
    uint32_t halfSpan;
    uint32_t startFreq;

    halfSpan      = ctrl->cfg->fineStep * ctrl->cfg->fineN;
    startFreq     = ctrl->stat.peakFreq + halfSpan;
    ctrl->endFreq = (ctrl->stat.peakFreq > halfSpan) ?
                                  (ctrl->stat.peakFreq - halfSpan) :
                                   ctrl->minFreq;
    if (ctrl->endFreq < ctrl->minFreq)
    {
        ctrl->endFreq = ctrl->minFreq;
    }

    ctrl->stat.state  = MIST_STATE_FINE;
    ctrl->stat.peakAd = 0U;
    mist_process_set_freq(ctrl, startFreq, seq);
}

/*******************************************************************************
//  Function   : mist_process_rescan
//  Description: 请求围绕最近一次谐振峰值重新执行精细扫描
//  Param      : ctrl - 追频算法实例指针
//               seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 如果不存在有效峰值，则重新执行粗略扫描
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
static void mist_process_rescan(mist_ctrl_t *ctrl, uint32_t seq)
{
    ctrl->stat.locked = 0U;
    ctrl->stat.rescanCnt++;
    if ((ctrl->stat.peakAd == 0U) || (ctrl->stat.peakFreq == 0U))
    {
        mist_ctrl_start(ctrl, seq);
    }
    else
    {
        mist_process_begin_fine(ctrl, seq);
    }
}

/*******************************************************************************
//  Function   : mist_process_coarse
//  Description: 执行一次粗扫采样判断及频率步进
//  Param      : ctrl - 追频算法实例指针
//               ad - 稳定后的滤波AD值；seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 上升沿判断时屏蔽AD值低4位，避免微小波动造成误判
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_coarse(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq)
{
    uint16_t currMask;
    uint16_t prevMask;

    currMask = ad           & 0xFFF0U;
    prevMask = ctrl->prevAd & 0xFFF0U;
    if (currMask > prevMask)
    {
        if (ctrl->riseCnt < ctrl->cfg->riseCnt)
        {
            ctrl->riseCnt++;
        }
        if (ctrl->riseCnt >= ctrl->cfg->riseCnt)
        {
            ctrl->riseOk = 1U;
        }
    }
    else
    {
        ctrl->riseCnt = 0U;
    }
    ctrl->prevAd = ad;

    if ((ctrl->riseOk != 0U) && (ad > ctrl->stat.peakAd))
    {
        ctrl->stat.peakAd   = ad;
        ctrl->stat.peakFreq = ctrl->stat.freq;
    }

    if (ctrl->stat.freq <= ctrl->minFreq)
    {
        if ((ctrl->riseOk != 0U) && (ctrl->stat.peakAd != 0U))
        {
            mist_process_begin_fine(ctrl, seq);
        }
        else
        {
            ctrl->stat.state = MIST_STATE_ERROR;
        }
        return;
    }

    mist_process_set_freq(ctrl, mist_process_down(ctrl, ctrl->cfg->coarseStep), seq);
}

/*******************************************************************************
//  Function   : mist_process_fine
//  Description: 执行一次精扫采样判断及频率步进
//  Param      : ctrl - 追频算法实例指针
//               ad - 稳定后的滤波AD值；seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 完整扫描精扫窗口，并保存窗口内的真实峰值
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_fine(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq)
{
    if (ad > ctrl->stat.peakAd)
    {
        ctrl->stat.peakAd   = ad;
        ctrl->stat.peakFreq = ctrl->stat.freq;
    }

    if (ctrl->stat.freq <= ctrl->endFreq)
    {
        if (ctrl->stat.peakAd == 0U)
        {
            ctrl->stat.state = MIST_STATE_ERROR;
            return;
        }
        ctrl->stat.targetAd = (uint16_t)(((uint32_t)ctrl->stat.peakAd *
                                                  ctrl->cfg->targetPct) / 100U);
        ctrl->stat.state  = MIST_STATE_BACKOFF;
        ctrl->stat.fault  = 0U;
        ctrl->stat.errCnt = 0U;
        mist_process_set_freq(ctrl, ctrl->stat.peakFreq, seq);
        return;
    }

    mist_process_set_freq(ctrl, mist_process_down(ctrl, ctrl->cfg->fineStep), seq);
}

/*******************************************************************************
//  Function   : mist_process_backoff
//  Description: 从峰值点向高频方向退峰并寻找目标工作点
//  Param      : ctrl - 追频算法实例指针
//               ad - 稳定后的滤波AD值；seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 当AD值下降至峰值AD的设定比例时锁定当前频率
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_backoff(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq)
{
    uint32_t maxOffset;

    maxOffset = ctrl->cfg->fineStep * ctrl->cfg->backoffN;
    if ((ad <= ctrl->stat.targetAd)
    ||  (mist_process_diff(ctrl->stat.freq, ctrl->stat.peakFreq) >= maxOffset))
    {
        ctrl->stat.lockFreq = ctrl->stat.freq;
        ctrl->stat.refAd    = ad;
        ctrl->stat.locked   = 1U;
        ctrl->stat.state    = MIST_STATE_WAIT_REF;
        ctrl->refSum        = 0U;
        ctrl->refCnt        = 0U;
        ctrl->checkSeq      = seq;
        return;
    }

    mist_process_set_freq(ctrl, ctrl->stat.freq + ctrl->cfg->fineStep, seq);
}

/*******************************************************************************
//  Function   : mist_process_wait_ref
//  Description: 锁频后采集并计算稳定的漂移跟踪参考AD值
//  Param      : ctrl - 追频算法实例指针
//               ad - 当前滤波AD值；seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 反馈AD异常偏低时立即请求重新精扫
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_wait_ref(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq)
{
    uint16_t rescanAd;

    if ((seq - ctrl->checkSeq) < ctrl->cfg->driftN)
    {
        return;
    }
    ctrl->checkSeq = seq;
    rescanAd       = (uint16_t)(((uint32_t)ctrl->stat.targetAd * ctrl->cfg->rescanPct) / 100U);
    if (ad < rescanAd)
    {
        mist_process_rescan(ctrl, seq);
        return;
    }

    ctrl->refSum += ad;
    ctrl->refCnt++;
    if (ctrl->refCnt >= ctrl->cfg->lockCnt)
    {
        ctrl->stat.refAd = (uint16_t)(ctrl->refSum / ctrl->refCnt);
        ctrl->stat.state = MIST_STATE_MONITOR;
        ctrl->refSum     = 0U;
        ctrl->refCnt     = 0U;
    }
}

/*******************************************************************************
//  Function   : mist_process_monitor
//  Description: 使用滞回区间监控锁频工作点
//  Param      : ctrl - 追频算法实例指针
//               ad - 当前滤波AD值；seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 仅当反馈AD超出滞回区间时启动单步频率试探
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_monitor(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq)
{
    uint16_t low;
    uint16_t high;
    uint16_t rescanAd;
    uint32_t testFreq;

    if ((seq - ctrl->checkSeq) < ctrl->cfg->driftN)
    {
        return;
    }
    ctrl->checkSeq = seq;
    low            = (uint16_t)(((uint32_t)ctrl->stat.refAd * ctrl->cfg->driftLo)  / 100U);
    high           = (uint16_t)(((uint32_t)ctrl->stat.refAd * ctrl->cfg->driftHi) / 100U);
    rescanAd       = (uint16_t)(((uint32_t)ctrl->stat.refAd * ctrl->cfg->rescanPct)    / 100U);

    if (ad < rescanAd)
    {
        mist_process_rescan(ctrl, seq);
        return;
    }
    if ((ad >= low) && (ad <= high))
    {
        return;
    }

    ctrl->oldAd   = ad;
    ctrl->oldFreq = ctrl->stat.freq;
    ctrl->dir     = (ad > high) ? 1 : -1;
    if (ctrl->dir > 0)
    {
        testFreq = ctrl->stat.freq + ctrl->cfg->fineStep;
    }
    else
    {
        testFreq = mist_process_down(ctrl, ctrl->cfg->fineStep);
    }
    ctrl->stat.state = MIST_STATE_TEST;
    mist_process_set_freq(ctrl, testFreq, seq);
}

/*******************************************************************************
//  Function   : mist_process_test_drift
//  Description: 评估单步漂移修正效果，并决定保留或撤销本次频率调整
//  Param      : ctrl - 追频算法实例指针
//               ad - 试探频率稳定后的滤波AD值；seq - 当前ADC采样序号
//  Return     : 无
//  Others     : 仅当调整后AD值更接近参考AD时保留当前频率
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_process_test_drift(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq)
{
    uint32_t oldErr;
    uint32_t newErr;
    uint32_t maxOffset;

    oldErr    = mist_process_diff(ctrl->oldAd, ctrl->stat.refAd);
    newErr    = mist_process_diff(ad, ctrl->stat.refAd);
    maxOffset = ctrl->cfg->fineStep * ctrl->cfg->backoffN;

    if ((newErr >= oldErr) ||
        (mist_process_diff(ctrl->stat.freq, ctrl->stat.peakFreq) > maxOffset))
    {
        mist_process_set_freq(ctrl, ctrl->oldFreq, seq);
    }
    else
    {
        ctrl->stat.lockFreq = ctrl->stat.freq;
        ctrl->checkSeq      = seq;
    }
    ctrl->stat.state = MIST_STATE_MONITOR;
}
