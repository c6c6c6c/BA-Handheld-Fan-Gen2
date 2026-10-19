/**
 ***********************************************************************************************************************
 * @file    mist_ctrl.h
 * @brief   雾化片追频控制接口、配置参数及运行状态
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-25
 ***********************************************************************************************************************
 */
#ifndef MIST_CTRL_H
#define MIST_CTRL_H

#include <stdint.h>

/* 命名约定：N=采样数/步数，Cnt=次数，Pct=百分比，Seq=采样序号。 */

/* 类型定义 ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Description: 追频算法运行状态
*******************************************************************************/
typedef enum
{
    MIST_STATE_IDLE = 0,          // 空闲状态，追频算法未运行
    MIST_STATE_COARSE,            // 粗扫状态，搜索谐振上升沿及粗略峰值
    MIST_STATE_FINE,              // 精扫状态，在粗扫峰值附近搜索真实峰值
    MIST_STATE_BACKOFF,           // 退峰状态，从谐振峰值向高频侧寻找目标工作点
    MIST_STATE_WAIT_REF,          // 漂移等待状态，锁频后等待稳定并建立参考AD
    MIST_STATE_MONITOR,           // 漂移监控状态，监测反馈AD是否超出滞回区间
    MIST_STATE_TEST,              // 漂移试探状态，单步调整频率并判断修正效果
    MIST_STATE_ERROR              // 错误状态，扫描或状态机运行异常
} mist_state_t;


/*******************************************************************************
//  Description: PWM频率及占空比设置回调函数类型
*******************************************************************************/
typedef void (*mist_freq_t)(uint32_t freq);
typedef void (*mist_duty_t)(uint16_t duty);

/*******************************************************************************
//  Description: 追频算法只读参数表
*******************************************************************************/
typedef struct
{
    uint32_t centerFreq;  // 标称中心频率 Hz
    uint32_t minFreq;     // 扫频下限 Hz
    uint32_t maxFreq;     // 扫频上限 Hz
    uint32_t coarseStep;  // 粗扫步进 Hz
    uint32_t fineStep;    // 精扫及漂移跟踪步进 Hz
    uint32_t settleN;     // 切频后稳定等待采样数
    uint32_t driftN;      // 漂移检查采样周期
    uint8_t  fineN;       // 精扫半区间步数
    uint8_t  riseCnt;     // 连续上升确认次数
    uint8_t  targetPct;   // 锁频目标占峰值比例 %
    uint8_t  rescanPct;   // 重新精扫触发比例 %
    uint8_t  driftLo;     // 漂移跟踪滞回下限 %
    uint8_t  driftHi;     // 漂移跟踪滞回上限 %
    uint8_t  lockCnt;     // 锁频参考值稳定检查次数
    uint8_t  backoffN;    // 最大频率回退步数
    uint8_t  faultCnt;    // 连续错误故障判定次数
    uint8_t  duty;        // 默认 PWM 占空比，单位依驱动约定
} mist_cfg_t;


/*******************************************************************************
//  Description: 追频算法硬件回调接口
*******************************************************************************/
typedef struct
{
    mist_freq_t setFreq;  // PWM输出频率设置回调函数
    mist_duty_t setDuty;  // PWM输出占空比设置回调函数
} mist_api_t;


/*******************************************************************************
//  Description: 雾化片追频运行状态
*******************************************************************************/
typedef struct
{
    mist_state_t state;      // 当前追频状态
    uint32_t     freq;       // 当前 PWM 频率 Hz
    uint32_t     peakFreq;   // 谐振峰值频率 Hz
    uint32_t     lockFreq;   // 锁定工作频率 Hz
    uint16_t     peakAd;     // 谐振峰值 AD
    uint16_t     targetAd;   // 锁频目标 AD
    uint16_t     refAd;      // 漂移跟踪参考 AD
    uint8_t      locked;     // 锁频标志：0-未锁定，1-已锁定
    uint8_t      rescanCnt;  // 自动重新扫描次数
    uint8_t      fault;      // 故障标志：0-正常，1-故障
    uint8_t      errCnt;     // 连续错误次数
} mist_stat_t;


/*******************************************************************************
//  Description: 雾化片追频算法运行上下文
*******************************************************************************/
typedef struct
{
    const mist_cfg_t *cfg;       // 只读算法配置参数
    mist_api_t       api;        // 追频算法接口
    mist_stat_t      stat;       // 当前运行状态

    uint32_t         minFreq;    // 粗扫频率下限 Hz
    uint32_t         maxFreq;    // 粗扫频率上限 Hz
    uint32_t         endFreq;    // 当前扫描结束频率 Hz

    uint32_t         settleSeq;  // 最近一次切频的 ADC 采样序号
    uint32_t         lastSeq;    // 上一次算法执行的 ADC 采样序号
    uint32_t         checkSeq;   // 上一次漂移检查的 ADC 采样序号

    uint32_t         oldFreq;    // 漂移试探前的频率 Hz
    uint16_t         prevAd;     // 上一次滤波 AD 值
    uint16_t         oldAd;      // 漂移试探前的 AD 值

    uint32_t         refSum;     // 锁频参考 AD 累加值

    uint8_t          riseCnt;    // AD 连续上升次数
    uint8_t          riseOk;     // 谐振上升沿检测标志
    uint8_t          refCnt;     // 参考 AD 累计次数
    int8_t           dir;        // 试探方向：-1 降频，1 升频
    uint8_t          ready;      // 初始化标志
} mist_ctrl_t;

/* 常量参数 ----------------------------------------------------------------------------------------------------------*/

extern const mist_cfg_t g_mistCfg;   // 追频算法只读常量参数表

/* 函数声明 ----------------------------------------------------------------------------------------------------------*/

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
uint8_t mist_ctrl_init(mist_ctrl_t *ctrl, const mist_cfg_t *cfg, const mist_api_t *api);

/*******************************************************************************
//  Function   : mist_ctrl_start
//  Description: 启动新一轮谐振频率粗略扫描
//  Param      : ctrl - 追频算法实例指针
//               seq - 当前ADC原始采样序号
//  Return     : 无
//  Others     : 从扫描范围的高频端开始，逐步向低频方向扫描
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_ctrl_start(mist_ctrl_t *ctrl, uint32_t seq);

/*******************************************************************************
//  Function   : mist_ctrl_stop
//  Description: 停止谐振频率搜索及频率漂移跟踪
//  Param      : ctrl - 追频算法实例指针
//  Return     : 无
//  Others     : 通过回调将占空比设为0，不直接操作PWM计数器
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
void mist_ctrl_stop(mist_ctrl_t *ctrl);

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
void mist_ctrl_task(mist_ctrl_t *ctrl, uint16_t ad, uint32_t seq);

/*******************************************************************************
//  Function   : mist_ctrl_status
//  Description: 获取当前谐振频率跟踪状态
//  Param      : ctrl - 追频算法实例指针
//  Return     : 状态只读指针；ctrl为空时返回0
//  Others     : 主要用于应用层状态监控及调试器观察
//  Author     : Leo Chen 2026-08-25
*******************************************************************************/
const mist_stat_t *mist_ctrl_status(const mist_ctrl_t *ctrl);

#endif /* MIST_CTRL_H */
