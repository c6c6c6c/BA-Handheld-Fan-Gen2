/**
 ***********************************************************************************************************************
 * @file    bms_soc.c
 * @brief   BMS SOC 估算模块，实现库仑计量、静置 OCV 校正及满充/空电端点修正
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-13
 ***********************************************************************************************************************
 * @note
 *          1. SOC 主估算采用库仑计量法，规定正电流表示放电，负电流表示充电。
 *          2. 电池静置且电压稳定达到指定时间后，使用 OCV-SOC 曲线缓慢修正累计误差。
 *          3. 满充和空电端点修正优先级高于 OCV 修正。
 *          4. 默认 OCV 表应根据实际电芯型号、温度及测试条件重新标定。
 ***********************************************************************************************************************
 */

#include "bms_soc.h"
#if BMS_MODULE_SOC_ENABLE


/* Constant ----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 默认 OCV-SOC 结构体标定表
 *
 * 每行依次保存开路电压（V）与整数 SOC（千分比）。
 * 20 -> 2.0%，44 -> 4.4%，500 -> 50.0%，980 -> 98.0%。
 * 查表插值后除以 BMS_SOC_PERMILLE_FULL，返回 0.0~1.0 的浮点 SOC。
 *
 * @note
 * 电压和 SOC 均必须严格递增；实际产品需按电芯特性重新标定。
 */
static const BmsSocOcvPoint default_ocv_soc[] =
{
    {3.5125f,  20U},
    {3.6515f,  44U},
    {3.6820f,  68U},
    {3.6900f,  92U},
    {3.6965f, 116U},
    {3.7095f, 140U},
    {3.7287f, 164U},
    {3.7460f, 188U},
    {3.7575f, 212U},
    {3.7690f, 236U},
    {3.7755f, 260U},
    {3.7795f, 284U},
    {3.7835f, 308U},
    {3.7870f, 332U},
    {3.7914f, 356U},
    {3.7963f, 380U},
    {3.8020f, 404U},
    {3.8080f, 428U},
    {3.8150f, 452U},
    {3.8222f, 476U},
    {3.8310f, 500U},
    {3.8405f, 524U},
    {3.8525f, 548U},
    {3.8695f, 572U},
    {3.8915f, 596U},
    {3.9069f, 620U},
    {3.9200f, 644U},
    {3.9330f, 668U},
    {3.9470f, 692U},
    {3.9615f, 716U},
    {3.9765f, 740U},
    {3.9925f, 764U},
    {4.0090f, 788U},
    {4.0265f, 812U},
    {4.0453f, 836U},
    {4.0645f, 860U},
    {4.0850f, 884U},
    {4.1055f, 908U},
    {4.1270f, 932U},
    {4.1490f, 956U},
    {4.1725f, 980U}
};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : absf_local
//  Description: float 绝对值计算
//  Param      : x - 待计算绝对值的浮点数
//  Return     : x 的绝对值
//  Others     : None
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
static float absf_local(float x)
{
    return (x < 0.0f) ? -x : x;
}


/*******************************************************************************
//  Function   : clamp01
//  Description: 将 SOC 限制在 0.0 ~ 1.0 范围
//  Param      : x - 待限制数值
//  Return     : 限制在 0.0~1.0 范围内的结果
//  Others     : None
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
static float clamp01(float x)
{
    if (x < 0.0f)
    {
        return 0.0f;
    }

    if (x > 1.0f)
    {
        return 1.0f;
    }

    return x;
}


/*******************************************************************************
//  Function   : bms_soc_default_config
//  Description: 初始化 BMS SOC 默认配置参数
//  Param      : cfg         - SOC 配置结构体
//               capacity_ah - 电池额定容量，单位 Ah
//  Return     : None
//  Others     : 配置默认静置判定、OCV校正、满充/空电端点及 OCV-SOC 曲线
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_soc_default_config(BmsSocConfig *cfg, float capacity_ah)
{
    if (cfg == NULL)
    {
        return;
    }
	
    cfg->capacity_ah              = capacity_ah;                             /* 电池额定容量。库仑计量会根据该值将积分得到的 Ah 换算为 SOC。*/
    cfg->rest_current_a           = BMS_SOC_DEFAULT_REST_CURRENT_A;          /* 静置状态最大允许电流。 |I| <= 50mA 时才可能被认为处于静置状态。*/
    cfg->rest_time_s              = BMS_SOC_DEFAULT_REST_TIME_S;             /* 静置持续时间。单位：s 当前设置 1800s = 30min。*/
    cfg->rest_dvdt_v_per_s        = BMS_SOC_DEFAULT_REST_DVDT_V_PER_S;       /* 静置时允许的最大电压变化速率。单位：V/s 当前约允许 1Hz 采样时存在 2mV 左右的电压噪声。*/
    cfg->ocv_correction_tau_s     = BMS_SOC_DEFAULT_CORRECTION_TAU_S;        /* OCV 修正时间常数。 单位：s. 静置状态下不会直接跳变到 OCV SOC，而是通过一阶缓慢修正减小显示突变。*/
    cfg->full_voltage_v           = BMS_SOC_DEFAULT_FULL_VOLTAGE_V;          /* 满充端点电压阈值。*/
    cfg->full_terminate_current_a = BMS_SOC_DEFAULT_FULL_TERMINATE_CURRENT_A;/* 满充终止电流阈值。充电电流绝对值小于该值时，配合满充电压判断是否达到充满状态。 */
    cfg->empty_voltage_v          = BMS_SOC_DEFAULT_EMPTY_VOLTAGE_V;         /* 空电端点电压阈值。*/
    cfg->empty_min_discharge_a    = BMS_SOC_DEFAULT_EMPTY_MIN_DISCHARGE_A;   /* 空电判定最小放电电流。防止静置时由于瞬时低电压误判为空电。*/
    cfg->endpoint_hold_s          = BMS_SOC_DEFAULT_ENDPOINT_HOLD_S;         /* 满充 / 空电端点条件持续确认时间。*/
    cfg->ocv_table                = default_ocv_soc;                         /* 默认 OCV-SOC 标定表。*/

    cfg->ocv_count = sizeof(default_ocv_soc) / sizeof(default_ocv_soc[0]);
}


/*******************************************************************************
//  Function   : config_is_valid
//  Description: 检查 SOC 配置参数是否合法
//  Param      : c - SOC 配置结构体
//  Return     : 1：配置合法；0：配置非法
//  Others     : 同时检查 OCV-SOC 表长度、指针及单调递增关系
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
static int config_is_valid(const BmsSocConfig *c)
{
    size_t k;

    /*
     * 基础参数检查。
     */
    if ((c                           == NULL) 
    ||  (c->capacity_ah              <= 0.0f) 
    ||  (c->ocv_table                == NULL)
    ||  (c->ocv_count                <  2U) 
    ||  (c->rest_time_s              <= 0.0f) 
    ||  (c->rest_current_a           <  0.0f) 
    ||  (c->rest_dvdt_v_per_s        <  0.0f)
    ||  (c->ocv_correction_tau_s     <  0.0f) 
    ||  (c->full_terminate_current_a <  0.0f) 
    ||  (c->empty_min_discharge_a    <  0.0f) 
    ||  (c->endpoint_hold_s          <= 0.0f))
    {
        return 0;
    }

    /*
     * OCV-SOC 表必须严格递增，SOC 千分比不得超过 1000。
     *
     * 这样后续二分查找和线性插值才有明确意义。
     */
    if (c->ocv_table[0].soc_permille > BMS_SOC_PERMILLE_FULL)
    {
        return 0;
    }

    for (k = 1U; k < c->ocv_count; ++k)
    {
        if ((c->ocv_table[k].soc_permille >  BMS_SOC_PERMILLE_FULL) 
		||  (c->ocv_table[k].soc_permille <= c->ocv_table[k - 1U].soc_permille) 
		||  (c->ocv_table[k].voltage_v    <= c->ocv_table[k - 1U].voltage_v))
        {
            return 0;
        }
    }

    return 1;
}


/*******************************************************************************
//  Function   : bms_soc_init
//  Description: 初始化 SOC 估算器
//  Param      : est         - SOC 估算器对象
//               cfg         - SOC 配置参数
//               initial_soc - 初始 SOC，范围建议 0.0 ~ 1.0
//  Return     : 1：初始化成功；0：参数非法
//  Others     : 初始 SOC 会自动限制在 0.0 ~ 1.0 范围内
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
int bms_soc_init(BmsSocEstimator *est,
                 const BmsSocConfig *cfg,
                 float initial_soc)
{
    if ((est == NULL) || !config_is_valid(cfg))
    {
        return 0;
    }

    /*
     * 保存配置副本。
     */
    est->cfg = *cfg;

    /*
     * 初始化 SOC。
     */
    est->soc = clamp01(initial_soc);

    /*
     * 清零各状态计时器。
     */
    est->rest_timer_s  = 0.0f;
    est->full_timer_s  = 0.0f;
    est->empty_timer_s = 0.0f;

    /*
     * 初始化电压稳定性判断状态。
     */
    est->last_voltage_v      = 0.0f;
    est->voltage_initialized = 0;

    return 1;
}


/*******************************************************************************
//  Function   : bms_soc_from_ocv
//  Description: 根据 OCV 电压查询对应 SOC
//  Param      : cfg       - SOC 配置参数
//               voltage_v - 当前开路电压，单位 V
//  Return     : 根据 OCV-SOC 曲线得到的浮点 SOC，范围 0.0~1.0
//  Others     : 二分查找定位区间，整数千分比 SOC 插值后转换为浮点比例
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
float bms_soc_from_ocv(const BmsSocConfig *cfg,
                       float voltage_v)
{
    size_t lo;
    size_t hi;
    float fraction;

    if (!config_is_valid(cfg))
    {
        return 0.0f;
    }

    /*
     * 低于 OCV 表最低电压时，
     * 返回最低标定 SOC。
     */
    if (voltage_v <= cfg->ocv_table[0].voltage_v)
    {
        return (float)cfg->ocv_table[0].soc_permille / (float)BMS_SOC_PERMILLE_FULL;
    }

    /*
     * 高于 OCV 表最高电压时，
     * 返回最高标定 SOC。
     */
    if (voltage_v >= cfg->ocv_table[cfg->ocv_count - 1U].voltage_v)
    {
        return (float)cfg->ocv_table[cfg->ocv_count - 1U].soc_permille / (float)BMS_SOC_PERMILLE_FULL;
    }

    /*
     * 使用二分查找定位：
     *
     * ocv_table[lo].voltage_v <= voltage < ocv_table[hi].voltage_v
     */
    lo = 0U;
    hi = cfg->ocv_count - 1U;

    while ((hi - lo) > 1U)
    {
        size_t mid = lo + (hi - lo) / 2U;

        if (cfg->ocv_table[mid].voltage_v <= voltage_v)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }

    /*
     * 在相邻 OCV 点之间做线性插值。
     */
    fraction = (voltage_v - cfg->ocv_table[lo].voltage_v) / (cfg->ocv_table[hi].voltage_v - cfg->ocv_table[lo].voltage_v);

    /* 先用浮点数插值保留精度，再将整数千分比转换为 0.0~1.0 的比例。 */
    return ((float)cfg->ocv_table[lo].soc_permille +
            fraction * ((float)cfg->ocv_table[hi].soc_permille -
                        (float)cfg->ocv_table[lo].soc_permille)) /
           (float)BMS_SOC_PERMILLE_FULL;
}


/*******************************************************************************
//  Function   : bms_soc_update
//  Description: 更新一次 SOC 估算结果
//  Param      : est       - SOC 估算器
//               voltage_v - 当前电池电压，单位 V
//               current_a - 当前电池电流，单位 A；正值表示放电，负值表示充电
//               dt_s      - 本次更新距离上次更新的时间，单位 s
//  Return     : 本次 SOC 更新产生的事件
//  Others     : SOC 主估算使用库仑计量；满足条件时执行满充、空电端点或 OCV 修正
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
BmsSocEvent bms_soc_update(BmsSocEstimator *est,
                           float voltage_v,
                           float current_a,
                           float dt_s)
{
    BmsSocEvent event = BMS_SOC_EVENT_NONE;
    int voltage_stable;
    float alpha;

    if ((est == NULL) || (dt_s <= 0.0f))
    {
        return event;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Coulomb counting                                                                                                */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * SOC 主估算采用库仑计量法。
     *
     * 电流方向约定：
     *
     * current_a > 0 ：放电
     * current_a < 0 ：充电
     *
     * ΔSOC =
     * I × dt / (3600 × Capacity)
     *
     * 因为正电流表示放电，
     * 所以这里从当前 SOC 中减去积分结果。
     */
    est->soc = clamp01(est->soc - current_a * dt_s / (3600.0f * est->cfg.capacity_ah));


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Rest detection                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 判断电压是否稳定。
     *
     * 第一次采样没有上一帧电压可比较，
     * 因此直接认为电压稳定。
     */
    voltage_stable = !est->voltage_initialized || (absf_local(voltage_v - est->last_voltage_v) <= est->cfg.rest_dvdt_v_per_s * dt_s);


    /*
     * 静置条件：
     *
     * 1. 电流绝对值足够小
     * 2. 电压变化速率足够小
     *
     * 两个条件同时满足时累计 rest_timer_s。
     */
    if ((absf_local(current_a) <= est->cfg.rest_current_a) && voltage_stable)
    {
        est->rest_timer_s += dt_s;
    }
    else
    {
        est->rest_timer_s = 0.0f;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Full endpoint                                                                                                   */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 满充端点条件：
     *
     * 1. 电池电压达到 full_voltage_v
     * 2. 当前处于充电方向或零电流
     * 3. 充电电流绝对值已下降到终止电流以下
     *
     * current_a <= 0 是因为负电流定义为充电。
     */
    if ((voltage_v >= est->cfg.full_voltage_v) 
	&&  (current_a <= 0.0f) 
	&&  (absf_local(current_a) <= est->cfg.full_terminate_current_a))
    {
        est->full_timer_s += dt_s;
    }
    else
    {
        est->full_timer_s = 0.0f;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Empty endpoint                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 空电端点条件：
     *
     * 1. 电池电压低于 empty_voltage_v
     * 2. 当前确实处于放电状态
     * 3. 放电电流达到最小判定值
     *
     * 避免在无负载静置时，
     * 因瞬时低电压误判 SOC = 0。
     */
    if ((voltage_v <= est->cfg.empty_voltage_v) && (current_a >= est->cfg.empty_min_discharge_a))
    {
        est->empty_timer_s += dt_s;
    }
    else
    {
        est->empty_timer_s = 0.0f;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Endpoint / OCV correction                                                                                       */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 修正优先级：
     *
     * FULL
     *   >
     * EMPTY
     *   >
     * OCV
     *
     * 满充/空电属于硬端点，
     * 优先级高于静置 OCV 的柔性校正。
     */
    if (est->full_timer_s >= est->cfg.endpoint_hold_s)
    {
        /*
         * 满充端点成立：
         *
         * 强制 SOC = 100%。
         */
        est->soc = 1.0f;
        event    = BMS_SOC_EVENT_FULL;
    }
    else if (est->empty_timer_s >= est->cfg.endpoint_hold_s)
    {
        /*
         * 空电端点成立：
         *
         * 强制 SOC = 0%。
         */
        est->soc = 0.0f;
        event    = BMS_SOC_EVENT_EMPTY;
    }
    else if (est->rest_timer_s >= est->cfg.rest_time_s)
    {
        float ocv_soc;

        /*
         * 根据当前静置电压查表得到 OCV SOC。
         */
        ocv_soc = bms_soc_from_ocv(&est->cfg, voltage_v);

        /*
         * OCV 修正采用一阶滤波方式：
         *
         * soc_new =
         * soc_old + alpha × (ocv_soc - soc_old)
         *
         * alpha =
         * dt / (tau + dt)
         *
         * tau 越大，修正越慢；
         * tau <= 0 时直接使用 OCV SOC。
         */
        if (est->cfg.ocv_correction_tau_s <= 0.0f)
        {
            alpha = 1.0f;
        }
        else
        {
            alpha = dt_s / (est->cfg.ocv_correction_tau_s + dt_s);
        }

        est->soc = clamp01(est->soc + alpha * (ocv_soc - est->soc));
        event    = BMS_SOC_EVENT_OCV;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Update history                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 保存本次电压，
     * 用于下一周期判断电压变化速度。
     */
    est->last_voltage_v      = voltage_v;
    est->voltage_initialized = 1;

    return event;
}
#endif /* BMS_MODULE_SOC_ENABLE */
