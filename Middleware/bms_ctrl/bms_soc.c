/**
 ***********************************************************************************************************************
 * @file    bms_soc.c
 * @brief   电池 SOC 估算：库仑计量、静置 OCV 校正和充满/放空端点修正
 ***********************************************************************************************************************
 */

#include "bms_soc.h"

#include <float.h>


/*
 * 默认 OCV-SOC 标定曲线。
 *
 * voltage_v   : 静置状态下的单节电池开路电压，单位 V
 * soc_permille: SOC 千分比，1000 表示 100.0%
 *
 * 注意：
 * 1. 当前曲线仅用于估算参考，量产前应使用实际电芯测试数据重新标定。
 * 2. OCV 只适合在电池充分静置、端电压稳定后用于 SOC 修正。
 * 3. 不应直接使用大电流充放电状态下的端电压作为 OCV。
 */
static const BmsSocOcvPoint s_ocv_table[] =
{
    {3.5125f,  20U}, {3.6515f,  44U}, {3.6820f,  68U}, {3.6900f,  92U},
    {3.6965f, 116U}, {3.7095f, 140U}, {3.7287f, 164U}, {3.7460f, 188U},
    {3.7575f, 212U}, {3.7690f, 236U}, {3.7755f, 260U}, {3.7795f, 284U},
    {3.7835f, 308U}, {3.7870f, 332U}, {3.7914f, 356U}, {3.7963f, 380U},
    {3.8020f, 404U}, {3.8080f, 428U}, {3.8150f, 452U}, {3.8222f, 476U},
    {3.8310f, 500U}, {3.8405f, 524U}, {3.8525f, 548U}, {3.8695f, 572U},
    {3.8915f, 596U}, {3.9069f, 620U}, {3.9200f, 644U}, {3.9330f, 668U},
    {3.9470f, 692U}, {3.9615f, 716U}, {3.9765f, 740U}, {3.9925f, 764U},
    {4.0090f, 788U}, {4.0265f, 812U}, {4.0453f, 836U}, {4.0645f, 860U},
    {4.0850f, 884U}, {4.1055f, 908U}, {4.1270f, 932U}, {4.1490f, 956U},
    {4.1725f, 980U}, {4.2000f, 1000U}
};

#define BMS_SOC_OCV_POINT_COUNT    (sizeof(s_ocv_table) / sizeof(s_ocv_table[0]))


/*
 * SOC 模块运行时状态。
 *
 * 本工程当前只绑定一个 bms_battery 实例。
 * remaining_capacity_mah 采用 double 保存，提高库仑积分累计精度。
 * charge/discharge_fraction_mah 用于保存不足 1 mAh 的小数部分，
 * 这些运行变量不属于掉电保存数据。
 */
struct
{
    bms_battery_t          *owner;                   /* 当前绑定的电池对象。 */
    const battery_config_t *config;                  /* 初始化时绑定的配置对象。 */
    uint32_t                rest_ms;                 /* 满足静置条件的累计时间，单位 ms。 */
    uint32_t                empty_ms;                /* 满足放空条件的累计时间，单位 ms。 */
    double                  remaining_capacity_mah;  /* 当前剩余容量估算值，单位 mAh。 */
    float                   last_voltage_v;          /* 上一次有效采样的电池电压，单位 V。 */
    double                  charge_fraction_mah;     /* 充电累计量不足 1 mAh 的小数部分。 */
    double                  discharge_fraction_mah;  /* 放电累计量不足 1 mAh 的小数部分。 */
    bool                    has_last_voltage;        /* last_voltage_v 是否已经有效。 */
} s_soc;


/***********************************************************************************************************************
//  Function   : bms_soc_clamp
//  Description: 将输入值限制在 0 到 maximum 范围内
//  Param      : value   - 待限制的输入值
//               maximum - 允许的最大值
//  Return     : 限幅后的结果
//  Others     : value < 0 返回 0；value > maximum 返回 maximum
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
static float bms_soc_clamp(float value, float maximum)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }

    return (value > maximum) ? maximum : value;
}


/***********************************************************************************************************************
//  Function   : bms_soc_abs
//  Description: 计算 float 类型数值的绝对值
//  Param      : value - 输入值
//  Return     : value 的绝对值
//  Others     : SOC 模块内部使用，避免额外依赖数学库
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
float bms_soc_abs(float value)
{
    return (value < 0.0f) ? -value : value;
}


/***********************************************************************************************************************
//  Function   : bms_soc_add_time
//  Description: 对毫秒计时器执行带饱和保护的累加
//  Param      : elapsed - 当前已累计时间，单位 ms
//               delta   - 本次需要增加的时间，单位 ms
//  Return     : 累加后的时间
//  Others     : 累加结果超过 UINT32_MAX 时保持为 UINT32_MAX，避免整数溢出
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
static uint32_t bms_soc_add_time(uint32_t elapsed, uint32_t delta)
{
    return (elapsed > UINT32_MAX - delta) ? UINT32_MAX : elapsed + delta;
}


/***********************************************************************************************************************
//  Function   : bms_soc_add_total
//  Description: 累加充电或放电总量，并保存不足 1 mAh 的小数部分
//  Param      : total     - 已累计的整数 mAh 总量
//               fraction  - 不足 1 mAh 的小数累计量
//               delta_mah - 本次需要增加的电量，单位 mAh
//  Return     : None
//  Others     : total 达到 UINT32_MAX 后停止累计；内部带整数溢出保护
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
static void bms_soc_add_total(uint32_t *total, double *fraction, double delta_mah)
{
    uint32_t whole;

    /* 已达到最大累计值，或者本次没有有效增量。 */
    if ((*total == UINT32_MAX) || (delta_mah <= 0.0f))
    {
        return;
    }

    /* 先将本次增量加入小数累计量。 */
    *fraction += delta_mah;

    /* 不足 1 mAh 时继续保留在 RAM 中，等待后续累计。 */
    if (*fraction < 1.0f)
    {
        return;
    }

    /* 防止 total + fraction 转换后的整数部分发生 uint32_t 溢出。 */
    if (*fraction >= (double)(UINT32_MAX - *total))
    {
        *total = UINT32_MAX;
        *fraction = 0.0f;
        return;
    }

    /* 取出完整的整数 mAh 累加到掉电保存计数中。 */
    whole = (uint32_t)*fraction;

    *total    += whole;
    *fraction -= (double)whole;
}


/***********************************************************************************************************************
//  Function   : bms_soc_config_valid
//  Description: 检查 SOC 估算所需的电池配置参数是否合法
//  Param      : config - 电池配置参数
//  Return     : true：配置有效；false：配置无效
//  Others     : 主要检查容量、电压、静置条件以及充放电效率参数范围
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
static bool bms_soc_config_valid(const battery_config_t *config)
{
    return (config != NULL)
        && (config->design_capacity_mah  >  0.0f)
        && (config->cutoff_voltage_v     >  0.0f)
        && (config->rest_current_ma      >= 0.0f)
        && (config->rest_dvdt_v_per_s    >= 0.0f)
        && (config->design_capacity_mah  < FLT_MAX)
        && (config->cutoff_voltage_v     < FLT_MAX)
        && (config->full_voltage_v       < FLT_MAX)
        && (config->rest_current_ma      < FLT_MAX)
        && (config->rest_dvdt_v_per_s    < FLT_MAX)
        && (config->full_voltage_v       > config->cutoff_voltage_v)
        && (config->charge_efficiency    >  0.0f)
        && (config->discharge_efficiency >  0.0f)
        && (config->charge_efficiency    <= 10.0f)
        && (config->discharge_efficiency <= 10.0f);
}


/***********************************************************************************************************************
//  Function   : bms_soc_from_ocv
//  Description: 根据默认 OCV 标定曲线插值估算 SOC
//  Param      : voltage_v - 静置时的单节电池电压，单位 V
//  Return     : 估算 SOC，单位 %；无效输入返回 0
//  Others     : 电压越界时返回曲线端点，不将负载下电压直接视为开路电压
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
float bms_soc_from_ocv(float voltage_v)
{
    size_t lo = 0U;
    size_t hi = BMS_SOC_OCV_POINT_COUNT - 1U;
    float fraction;
    float permille;

    /* 排除 0、负数、无穷大及 NaN 等无效输入。 */
    if (!(voltage_v > 0.0f) || !(voltage_v < FLT_MAX))
    {
        return 0.0f;
    }

    /* 低于 OCV 曲线最小电压时，直接使用最低端 SOC。 */
    if (voltage_v <= s_ocv_table[lo].voltage_v)
    {
        return (float)s_ocv_table[lo].soc_permille / BMS_SOC_PERMILLE_PER_PERCENT;
    }

    /* 高于 OCV 曲线最大电压时，直接使用最高端 SOC。 */
    if (voltage_v >= s_ocv_table[hi].voltage_v)
    {
        return (float)s_ocv_table[hi].soc_permille / BMS_SOC_PERMILLE_PER_PERCENT;
    }

    /*
     * 使用二分查找定位 voltage_v 所在的两个 OCV 点。
     * 最终 lo 和 hi 必须相邻：
     *
     *     table[lo].voltage <= voltage_v < table[hi].voltage
     */
    while ((hi - lo) > 1U)
    {
        size_t mid = lo + (hi - lo) / 2U;

        if (s_ocv_table[mid].voltage_v <= voltage_v)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }

    /* 计算 voltage_v 在两个相邻 OCV 点之间的位置比例。 */
    fraction = (voltage_v - s_ocv_table[lo].voltage_v) /
               (s_ocv_table[hi].voltage_v - s_ocv_table[lo].voltage_v);

    /* 对 SOC 千分比执行线性插值。 */
    permille = (float)s_ocv_table[lo].soc_permille +
               fraction * ((float)s_ocv_table[hi].soc_permille -
                           (float)s_ocv_table[lo].soc_permille);

    /* 千分比转换成百分比。 */
    return permille / BMS_SOC_PERMILLE_PER_PERCENT;
}


/***********************************************************************************************************************
//  Function   : bms_soc_init
//  Description: 绑定框架电池对象并初始化 SOC 运行状态
//  Param      : battery             - 已设置 configInfo 的电池对象
//               initial_soc_percent - 上层给出的初始 SOC，单位 %
//  Return     : true：成功；false：配置或参数无效
//  Others     : 不自行猜测初始电量；掉电恢复时由上层传入已校验的保存值
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
bool bms_soc_init(bms_battery_t *battery, float initial_soc_percent)
{
    /* 检查对象、配置及初始 SOC 范围。 */
    if ((battery == NULL)
    || !bms_soc_config_valid(battery->configInfo)
    || !(initial_soc_percent >= 0.0f)
    || !(initial_soc_percent <= BMS_SOC_PERCENT_FULL))
    {
        return false;
    }

    /*
     * 绑定当前电池对象和配置对象。
     * 后续 update 时如果对象或 configInfo 指针发生变化，则认为运行状态已失效。
     */
    s_soc.owner                  = battery;
    s_soc.config                 = battery->configInfo;
    s_soc.rest_ms                = 0U;
    s_soc.empty_ms               = 0U;

    /*
     * 根据初始 SOC 反推出当前剩余容量：
     *
     * remaining_capacity = design_capacity * SOC / 100
     */
    s_soc.remaining_capacity_mah =
        (double)battery->configInfo->design_capacity_mah *
        ((double)initial_soc_percent / (double)BMS_SOC_PERCENT_FULL);

    s_soc.last_voltage_v         = 0.0f;
    s_soc.charge_fraction_mah    = 0.0;
    s_soc.discharge_fraction_mah = 0.0;
    s_soc.has_last_voltage       = false;

    /* 同步更新对外可见的电池运行数据。 */
    battery->soc_info.soc                    = initial_soc_percent;
    battery->soc_info.remaining_capacity_mah = (float)s_soc.remaining_capacity_mah;

    return true;
}


/***********************************************************************************************************************
//  Function   : bms_soc_update
//  Description: 按实际采样间隔更新库仑计量并按条件修正 SOC
//  Param      : battery    - 电池运行数据，电流单位 mA，正放电、负充电
//               elapsed_ms - 距离上次有效更新的实际时间，单位 ms
//  Return     : true：更新成功；false：尚未初始化、配置改变或采样无效
//  Others     : SOH 无可靠测量依据，本函数不修改 soh；累计电量整数部分以 mAh 保存
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
double val[5];

bool bms_soc_update(bms_battery_t *battery, uint32_t elapsed_ms)
{
    const  battery_config_t *config;
    battery_soc_t           *soc_info;
    double capacity_mah;
    double delta_mah;
    float  alpha;
    bool   voltage_stable;

    /*
     * 检查本次更新条件。
     *
     * 1. battery 必须是初始化时绑定的对象。
     * 2. configInfo 不允许在运行过程中被替换。
     * 3. elapsed_ms 必须在允许范围内。
     * 4. 电压、电流必须是有效有限数值。
     */
    if ((battery == NULL)
    || (battery != s_soc.owner)
    || (battery->configInfo != s_soc.config)
    || !bms_soc_config_valid(battery->configInfo)
    || (elapsed_ms == 0U)
    || (elapsed_ms > BMS_SOC_MAX_UPDATE_MS)
    || !(battery->voltage_v  > 0.0f)
    || !(battery->voltage_v  < FLT_MAX)
    || !(battery->current_ma > -FLT_MAX)
    || !(battery->current_ma < FLT_MAX))
    {
        /*
         * 本次采样无效后重新开始静置/放空条件计时。
         * 同时清除上一电压有效标志，避免下一次使用跨越异常周期的电压计算 dv/dt。
         */
        s_soc.rest_ms          = 0U;
        s_soc.empty_ms         = 0U;
        s_soc.has_last_voltage = false;

        return false;
    }

    config       = battery->configInfo;
    soc_info     = &battery->soc_info;
    capacity_mah = config->design_capacity_mah;

    /*
     * 库仑积分：
     *
     * delta_mAh = |I(mA)| * dt(ms) / 3600000
     */
    delta_mah = (double)bms_soc_abs(battery->current_ma) *
                ((double)elapsed_ms / (double)BMS_SOC_MS_PER_HOUR);

    /*
     * current_ma > 0：充电
     * current_ma < 0：放电
     */
    if (battery->current_ma > 0.0f)
    {
        /* 累计历史充电量。 */
        bms_soc_add_total(&soc_info->charge_mah_total, &s_soc.charge_fraction_mah, delta_mah);

        /*
         * 充入电池的有效容量需要乘以充电效率。
         *
         * effective_charge = delta_mah * charge_efficiency
         */
        s_soc.remaining_capacity_mah += delta_mah * (double)config->charge_efficiency;
    }
    else if (battery->current_ma < 0.0f)
    {
        /* 累计历史放电量。 */
        bms_soc_add_total(&soc_info->discharge_mah_total, &s_soc.discharge_fraction_mah, delta_mah);

        /*
         * 放电时按照放电效率折算剩余容量消耗。
         *
         * effective_discharge = delta_mah / discharge_efficiency
         */
        s_soc.remaining_capacity_mah -= delta_mah / (double)config->discharge_efficiency;
    }

    /* 将剩余容量限制在 0 ~ 设计容量范围内。 */
    if (s_soc.remaining_capacity_mah < 0.0)
    {
        s_soc.remaining_capacity_mah = 0.0;
    }
    else if (s_soc.remaining_capacity_mah > capacity_mah)
    {
        s_soc.remaining_capacity_mah = capacity_mah;
    }

    /*
     * 根据当前剩余容量换算库仑计量 SOC：
     *
     * SOC = remaining_capacity / design_capacity * 100%
     */
    soc_info->soc = (float)(s_soc.remaining_capacity_mah *
                            (double)BMS_SOC_PERCENT_FULL / capacity_mah);

    /*
     * 判断电池端电压是否稳定。
     *
     * |Vnow - Vlast| <= rest_dvdt_v_per_s * dt
     *
     * 只有电压变化速度足够小，才认为满足静置电压稳定条件。
     */
    voltage_stable = s_soc.has_last_voltage
        && (bms_soc_abs(battery->voltage_v - s_soc.last_voltage_v)
        <= config->rest_dvdt_v_per_s * (float)elapsed_ms / BMS_SOC_MS_PER_SECOND);

    /*
     * 静置判定条件：
     *
     * 1. 配置了静置时间。
     * 2. 电池电流绝对值低于静置电流阈值。
     * 3. 电压变化速度满足稳定条件。
     */
    if ((config->rest_time_ms > 0U)
    &&  (bms_soc_abs(battery->current_ma) <= config->rest_current_ma)
    &&   voltage_stable)
    {
        s_soc.rest_ms = bms_soc_add_time(s_soc.rest_ms, elapsed_ms);
    }
    else
    {
        s_soc.rest_ms = 0U;
    }


    soc_info->static_soc = bms_soc_from_ocv(battery->voltage_v);
    /*
     * 放空端点判定：
     *
     * 必须处于放电状态，且存在明显放电电流，
     * 同时电池电压已经低于截止电压。
     */
    if ((battery->state      == BAT_STATE_DISCHARGING)
    &&  (battery->current_ma >  config->rest_current_ma)
    &&  (battery->voltage_v  <= config->cutoff_voltage_v))
    {
        s_soc.empty_ms = bms_soc_add_time(s_soc.empty_ms, elapsed_ms);
    }
    else
    {
        s_soc.empty_ms = 0U;
    }

    /*
     * 满充端点修正。
     *
     * FULL 必须由充电管理状态确认；
     * 仅端电压达到 full_voltage_v 不能单独判断已经充满。
     */
    if ((battery->state == BAT_STATE_FULL) && (battery->voltage_v >= config->full_voltage_v))
    {
        soc_info->soc = BMS_SOC_PERCENT_FULL;
        s_soc.remaining_capacity_mah = capacity_mah;
    }
    /*
     * 放空端点修正。
     *
     * 截止条件持续达到 BMS_SOC_ENDPOINT_HOLD_MS 后，
     * 将 SOC 和剩余容量强制校正为 0。
     */
    else if (s_soc.empty_ms >= BMS_SOC_ENDPOINT_HOLD_MS)
    {
        soc_info->soc = 0.0f;
        s_soc.remaining_capacity_mah = 0.0;
    }
    /*
     * 静置 OCV 修正。
     *
     * 静置时间达到配置值后，根据 OCV 曲线得到参考 SOC，
     * 再通过一阶低通方式逐步修正库仑计量 SOC，避免 SOC 突跳。
     */
    else if ((s_soc.rest_ms >= config->rest_time_ms) && (config->rest_time_ms > 0U))
    {
        /*
         * 一阶滤波系数：
         *
         * alpha = dt / (tau + dt)
         */
        alpha = (float)elapsed_ms / (BMS_SOC_OCV_CORRECTION_TAU_MS + (float)elapsed_ms);

        /*
         * SOCnew = SOCold + alpha * (SOCocv - SOCold)
         */
        soc_info->soc += alpha * (soc_info->static_soc - soc_info->soc);
		//
        soc_info->soc = bms_soc_clamp(soc_info->soc, BMS_SOC_PERCENT_FULL);

        /*
         * OCV 修正 SOC 后，同步反算剩余容量，
         * 保证 SOC 和 remaining_capacity_mah 始终一致。
         */
        s_soc.remaining_capacity_mah = capacity_mah *
                                       ((double)soc_info->soc / (double)BMS_SOC_PERCENT_FULL);
    }

    /* 将内部高精度剩余容量同步到电池运行数据。 */
    soc_info->remaining_capacity_mah = (float)s_soc.remaining_capacity_mah;

    /* 保存本次电压，供下一周期计算电压变化速率。 */
    s_soc.last_voltage_v   = battery->voltage_v;
    s_soc.has_last_voltage = true;

    return true;
}
