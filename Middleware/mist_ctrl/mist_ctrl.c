/**
 ***********************************************************************************************************************
 * @file    mist_ctrl.c
 * @brief   雾化片追频控制、初始化及状态机调度
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-25
 ***********************************************************************************************************************
 */
#include "mist_ctrl.h"
#include "mist_process.h"

/* 变量 --------------------------------------------------------------------------------------------------------------*/
const mist_cfg_t g_mistCfg = // 追频算法只读常量参数表
{
	   .centerFreq = 108000UL,   // 雾化片标称中心频率，单位Hz
	   .minFreq    = 101000UL,   // 扫频下限，单位Hz
	   .maxFreq    = 115000UL,   // 扫频上限，单位Hz
	   .coarseStep = 1000UL,     // 粗扫步进，单位Hz
	   .fineStep   = 50UL,       // 精扫及漂移跟踪步进，单位Hz
	   .settleN    = 400UL,      // 切换频率后等待稳定的ADC原始采样数量
	   .driftN     = 3000UL,     // 漂移检查间隔，按ADC累计采样数计
	   .fineN      = 8U,         // 精扫半区间步数，范围为粗扫峰值正负8步
	   .riseCnt    = 2U,         // 判定上升沿所需的AD连续上升次数
	   .targetPct  = 100U,       // 锁频目标AD占谐振峰值AD的百分比
	   .rescanPct  = 70U,        // 触发重新精扫的AD下限百分比
	   .driftLo    = 95U,        // 漂移跟踪滞回区间下限百分比
	   .driftHi    = 105U,       // 漂移跟踪滞回区间上限百分比
	   .lockCnt    = 10U,        // 锁频后建立稳定参考AD所需的检查次数
	   .backoffN   = 10U,        // 工作频率允许偏离谐振峰值的最大步数
	   .faultCnt   = 3U,         // 连续进入错误状态后置故障位的次数
	   .duty       = 50U         // 默认占空比，单位由PWM驱动约定
};


/* 公共函数 ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : mist_ctrl_init
//  Description: 初始化谐振频率搜索及频率漂移跟踪模块
//  Param      : ctrl - 追频算法实例指针
//               cfg - 只读算法参数表指针
//               api - PWM频率和占空比回调接口
//  Return     : 0-初始化失败，1-初始化成功
//  Others     : 初始化完成后模块保持空闲，调用mist_ctrl_start后开始扫描
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
uint8_t mist_ctrl_init(mist_ctrl_t *ctrl, const mist_cfg_t *cfg, const mist_api_t *api)
{
    if ((ctrl            == 0)
    ||  (cfg             == 0)
    ||  (api             == 0)
    ||  (api->setFreq    == 0)
    ||  (api->setDuty    == 0)
    ||  (cfg->centerFreq == 0U)
    ||  (cfg->minFreq    == 0U)
    ||  (cfg->maxFreq    == 0U)
    ||  (cfg->coarseStep == 0U)
    ||  (cfg->fineStep   == 0U)
    ||  (cfg->fineN      == 0U)
    ||  (cfg->riseCnt    == 0U)
    ||  (cfg->targetPct  == 0U)
    ||  (cfg->faultCnt   == 0U)
    ||  (cfg->minFreq    >= cfg->maxFreq)
    ||  (cfg->centerFreq <  cfg->minFreq)
    ||  (cfg->centerFreq >  cfg->maxFreq)
    ||  (cfg->targetPct  >  100U))
    {
        return 0U;
    }

    ctrl->cfg            = cfg;
    ctrl->api            = *api;
    ctrl->minFreq        = ctrl->cfg->minFreq;
    ctrl->maxFreq        = ctrl->cfg->maxFreq;
    ctrl->stat.state     = MIST_STATE_IDLE;
    ctrl->stat.freq      = ctrl->cfg->centerFreq;
    ctrl->stat.peakFreq  = 0U;
    ctrl->stat.lockFreq  = 0U;
    ctrl->stat.peakAd    = 0U;
    ctrl->stat.targetAd  = 0U;
    ctrl->stat.refAd     = 0U;
    ctrl->stat.locked    = 0U;
    ctrl->stat.rescanCnt = 0U;
    ctrl->stat.fault     = 0U;
    ctrl->stat.errCnt    = 0U;
    ctrl->endFreq        = 0U;
    ctrl->settleSeq      = 0U;
    ctrl->lastSeq        = 0U;
    ctrl->checkSeq       = 0U;
    ctrl->oldFreq        = 0U;
    ctrl->prevAd         = 0U;
    ctrl->oldAd          = 0U;
    ctrl->refSum         = 0U;
    ctrl->riseCnt        = 0U;
    ctrl->riseOk         = 0U;
    ctrl->refCnt         = 0U;
    ctrl->dir            = 0;
    ctrl->ready          = 1U;
    return 1U;
}

/*******************************************************************************
//  Function   : mist_ctrl_start
//  Description: 启动新一轮谐振频率粗略扫描
//  Param      : ctrl - 追频算法实例指针
//               seq - 当前ADC原始采样序号
//  Return     : 无
//  Others     : 从扫描范围高频端开始，逐步向低频方向扫描
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_ctrl_start(mist_ctrl_t *ctrl, uint32_t seq)
{
    if ((ctrl == 0) || (ctrl->ready == 0U))
    {
        return;
    }
    ctrl->stat.state    = MIST_STATE_COARSE;
    ctrl->stat.locked   = 0U;
    ctrl->stat.peakAd   = 0U;
    ctrl->stat.peakFreq = 0U;
    ctrl->prevAd        = 0U;
    ctrl->riseCnt       = 0U;
    ctrl->riseOk        = 0U;
    ctrl->lastSeq       = seq;
    mist_process_set_freq(ctrl, ctrl->maxFreq, seq);
    /* 启动仅设置扫描频率，占空比由调用方控制。 */
}

/*******************************************************************************
//  Function   : mist_ctrl_stop
//  Description: 停止谐振频率搜索及频率漂移跟踪
//  Param      : ctrl - 追频算法实例指针
//  Return     : 无
//  Others     : 通过回调将占空比设为0，不直接操作PWM计数器
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_ctrl_stop(mist_ctrl_t *ctrl)
{
    if (ctrl == 0)
    {
        return;
    }
    ctrl->stat.state  = MIST_STATE_IDLE;
    ctrl->stat.locked = 0U;
    ctrl->api.setDuty(0); // 停止时将占空比设为0
}

/*******************************************************************************
//  Function   : mist_ctrl_task
//  Description: 执行一次非阻塞追频算法任务
//  Param      : ctrl - 追频算法实例指针
//               ad - 最新的雾化片反馈滤波AD值
//               seq - 当前ADC原始采样序号
//  Return     : 无
//  Others     : 在反馈AD滤波更新后循环调用，重复的ADC采样数据将被忽略
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_ctrl_task(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq)
{
    if ((ctrl == 0)
     || (ctrl->ready  == 0U)
     || (ctrl->stat.state == MIST_STATE_IDLE)
     || (seq == ctrl->lastSeq))
    {
        return;
    }
    ctrl->lastSeq = seq;
    if (mist_process_is_ready(ctrl, seq) == 0U)
    {
        return;
    }

    switch (ctrl->stat.state)
    {
        /* 粗扫状态：从高频向低频扫描，寻找谐振上升沿及粗略峰值 */
        case MIST_STATE_COARSE:
            mist_process_coarse(ctrl, ad, seq);
            break;

        /* 精扫状态：在粗扫峰值附近进行小范围扫描，确定真实谐振峰值 */
        case MIST_STATE_FINE:
            mist_process_fine(ctrl, ad, seq);
            break;

        /* 退峰状态：从谐振峰值向高频侧移动，寻找目标AD对应的工作频率 */
        case MIST_STATE_BACKOFF:
            mist_process_backoff(ctrl, ad, seq);
            break;

        /* 漂移等待状态：锁频后等待系统稳定，并采集漂移跟踪参考AD值 */
        case MIST_STATE_WAIT_REF:
            mist_process_wait_ref(ctrl, ad, seq);
            break;

        /* 漂移监控状态：周期检查反馈AD是否处于允许的滞回范围内 */
        case MIST_STATE_MONITOR:
            mist_process_monitor(ctrl, ad, seq);
            break;

        /* 漂移试探状态：调整一个频率步进，判断频率修正方向是否正确 */
        case MIST_STATE_TEST:
            mist_process_test_drift(ctrl, ad, seq);
            break;


        case MIST_STATE_ERROR:
            if (ctrl->stat.errCnt < ctrl->cfg->faultCnt)
            {
                ctrl->stat.errCnt++;
            }

            if (ctrl->stat.errCnt >= ctrl->cfg->faultCnt)
            {
                ctrl->stat.fault = 1U;
            }

            mist_ctrl_start(ctrl, seq);
            break;

        /* 未知状态：状态机运行异常，切换至错误状态 */
        default:
            ctrl->stat.state = MIST_STATE_ERROR;
            break;
    }

}

/*******************************************************************************
//  Function   : mist_ctrl_status
//  Description: 获取当前谐振频率跟踪状态
//  Param      : ctrl - 追频算法实例指针
//  Return     : 追频算法状态只读指针
//  Others     : 主要用于应用层状态监控及调试器观察
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
const mist_stat_t *mist_ctrl_status(const mist_ctrl_t *ctrl)
{
    if (ctrl == 0)
    {
        return 0;
    }
    return &ctrl->stat;
}
