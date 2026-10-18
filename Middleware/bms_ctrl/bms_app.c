/**
 ***********************************************************************************************************************
 * @file    bms_app.c
 * @brief   BMS 应用调度，采集电池数据并调用保护和 SOC 模块
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-14
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bms_app.h"
#include "bms.h"

#include <float.h>


/* BMS 全局电池运行数据，由采集、保护及 SOC 模块共同维护。 */
bms_battery_t bms_battery = {0};


const battery_config_t battery_config =
{
    .design_capacity_mah  = 5000.0f,    /* 标称容量：5000 mAh */
    .full_voltage_v       = 4.20f,      /* 充电截止电压：4.20 V */
    .cutoff_voltage_v     = 2.50f,      /* 放电截止电压：2.50 V */

    .rest_current_ma      = 50.0f,      /* 静置判定电流：|I| <= 50 mA */
    .rest_dvdt_v_per_s    = 0.0002f,    /* 静置电压变化率：<= 0.2 mV/s */
    .rest_time_ms         = 1800000U,   /* 静置 30 min 后允许 OCV 修正 */

    .charge_efficiency    = 0.99f,      /* 充电库仑效率：99% */
    .discharge_efficiency = 1.00f,      /* 放电库仑效率：100% */
};


/***********************************************************************************************************************
//  Function   : bms_app_persist_valid
//  Description: 检查掉电数据的 SOC、SOH 及剩余容量范围
//  Param      : data   - 用户读取的掉电数据
//               config - 当前电池配置
//  Return     : true：有效；false：无效
//  Others     : SOH 和循环数在恢复时按累计放电量重新计算，不依赖保存值
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
static bool bms_app_persist_valid(const battery_soc_t *data, const battery_config_t *config)
{
    double expected_soc;
    double soc_diff;

    if ((data == NULL)
    ||  (config == NULL)
    || !(config->design_capacity_mah > 0.0f)
    || !(config->design_capacity_mah < FLT_MAX)
    || !(data->soc >= 0.0f)
    || !(data->soc <= BMS_SOC_PERCENT_FULL)
    || !(data->soh >= BMS_SOH_MIN_PERCENT)
    || !(data->soh <= BMS_SOH_INITIAL_PERCENT)
    || !(data->remaining_capacity_mah >= 0.0f)
    || !(data->remaining_capacity_mah <= config->design_capacity_mah))
    {
        return false;
    }

    expected_soc = (double)data->remaining_capacity_mah * (double)BMS_SOC_PERCENT_FULL / (double)config->design_capacity_mah;
    soc_diff     = (double)data->soc - expected_soc;
    if (soc_diff < 0.0)
    {
        soc_diff = -soc_diff;
    }

    return (soc_diff <= BMS_PERSIST_SOC_TOLERANCE_PERCENT);
}


/***********************************************************************************************************************
//  Function   : bms_app_Restore
//  Description: 检查掉电数据后恢复累计电量和 SOC/SOH 模块
//  Param      : saved - Flash 读取结果
//  Return     : true：恢复成功；false：数据无效或初始化失败
//  Others     : 无效数据不会覆盖已初始化的默认运行状态
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
bool bms_app_Restore(const battery_soc_t *saved)
{
    uint32_t previous_charge;
    uint32_t previous_discharge;
    float previous_soc;
    float restored_soc;

    if (!bms_app_persist_valid(saved, bms_battery.configInfo))
    {
        return false;
    }

    previous_charge    = bms_battery.soc_info.charge_mah_total;
    previous_discharge = bms_battery.soc_info.discharge_mah_total;
    previous_soc       = bms_battery.soc_info.soc;
    restored_soc       = (float)((double)saved->remaining_capacity_mah *
                                 (double)BMS_SOC_PERCENT_FULL /
                                 (double)bms_battery.configInfo->design_capacity_mah);

    bms_battery.soc_info.charge_mah_total    = saved->charge_mah_total;
    bms_battery.soc_info.discharge_mah_total = saved->discharge_mah_total;

    if (!bms_soc_init(&bms_battery, restored_soc)
    ||  !bms_soh_init(&bms_battery))
    {
        bms_battery.soc_info.charge_mah_total    = previous_charge;
        bms_battery.soc_info.discharge_mah_total = previous_discharge;
        (void)bms_soc_init(&bms_battery, previous_soc);
        (void)bms_soh_init(&bms_battery);
        return false;
    }

    bms_battery.soc_persist = *saved;
    return true;
}


/***********************************************************************************************************************
//  Function   : bms_app_Save
//  Description: 生成由调用方写入 Flash 的掉电数据快照
//  Param      : output - 接收完整快照的缓冲区
//  Return     : true：已生成；false：对象未初始化或数值无效
//  Others     : 不执行 Flash 操作；写入失败时调用方不得将其视为已持久化
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
bool bms_app_Save(battery_soc_t *output)
{
    battery_soc_t snapshot;

    if ((output == NULL) || (bms_battery.configInfo == NULL))
    {
        return false;
    }

    if (!bms_soh_update(&bms_battery))
    {
        return false;
    }

    snapshot = bms_battery.soc_info;

    if (!bms_app_persist_valid(&snapshot, bms_battery.configInfo))
    {
        return false;
    }

    bms_battery.soc_persist = snapshot;
    *output = snapshot;
    return true;
}


/***********************************************************************************************************************
//  Function   : bms_app_ConfigureBattery
//  Description: 配置电池参数并初始化 SOC、SOH 模块
//  Param      : config              - 电池配置参数
//               initial_soc_percent - 初始 SOC，单位 %
//  Return     : true：配置成功；false：配置或初始 SOC 无效
//  Others     : 任一估算模块初始化失败时恢复原 configInfo
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
bool bms_app_ConfigureBattery(const battery_config_t *config, float initial_soc_percent)
{
    const battery_config_t *previous = bms_battery.configInfo;
    float previous_soc = bms_battery.soc_info.soc;

    /* 临时绑定新的电池配置，用于 SOC 模块参数校验及初始化。 */
    bms_battery.configInfo = config;

    /* SOC 初始化失败时不更改原估算器状态。 */
    if (!bms_soc_init(&bms_battery, initial_soc_percent))
    {
        bms_battery.configInfo = previous;
        return false;
    }

    /* SOH 配置无效时恢复原绑定及此前 SOC。 */
    if (!bms_soh_init(&bms_battery))
    {
        bms_battery.configInfo = previous;
        if (previous != NULL)
        {
            (void)bms_soc_init(&bms_battery, previous_soc);
        }
        return false;
    }

    return true;
}

/***********************************************************************************************************************
//  Function   : bms_app_Init
//  Description: 初始化 BMS 应用相关模块
//  Param      : None
//  Return     : None
//  Others     : 当前初始化 ADC 换算模块和按键模块
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
void bms_app_Init(void)
{
    bms_convert_Init();
    bms_key_Init();
}


/***********************************************************************************************************************
//  Function   : bms_app_1ms_task
//  Description: BMS 1 ms 周期任务，更新电池采样数据、保护状态、SOC 及 SOH
//  Param      : None
//  Return     : None
//  Others     : 仅在 ADC 累计采样完成后执行换算、保护及电量估算
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
void bms_app_1ms_task(void)
{
    double adcValue[BMS_ADC_NUM] = {0.0};

    /* 执行按键 1 ms 周期任务。 */
    bms_key_1ms_task();

    /*
     * 获取一组完成累计的 ADC 采样值。
     *
     * sample_accum_get() 返回 true 表示当前缓冲区已有一组
     * 完整采样数据，可以执行后续电池参数换算。
     */
    if (sample_accum_get(adcValue))
    {
        /* 将 ADC 数据换算为实际电池电压。 */
        bms_battery.voltage_v  = bms_battery_voltage(adcValue[BMS_ADC_VOLTAGE]);

        /* 将 ADC 数据换算为电池电流。 */
        bms_battery.current_ma = bms_battery_current(adcValue[BMS_ADC_CURRENT], bms_battery.voltage_v);

        /* 将 ADC 数据换算为 USB 输入电压。 */
        bms_battery.usb_in_v   = bms_usb_in_voltage(adcValue[BMS_ADC_USB_VOLTAGE]);

        /*
         * 根据电池电流判断当前电池状态。
         *
         * 电流方向约定：
         * current_ma > 0：放电
         * current_ma < 0：充电
         * bms_battery_state() 的输入单位仍为 A，调用前将 mA 换算为 A。
         */
        bms_battery.state = bms_battery_state(bms_battery.current_ma / BMS_BATTERY_CURRENT_MA_PER_A);

				bms_status_check(bms_battery.state);

        /* 根据当前电压、电流等运行数据执行 BMS 软件保护检查。 */
        bms_protect_check(&bms_battery);

        /*
         * 更新 SOC。
         *
         * ADC 每 SAMPLE_ACCUM_COUNT 个 1 ms 采样周期产生一次有效数据，
         * 因此本次 SOC 积分时间间隔为 SAMPLE_ACCUM_COUNT ms。
         *
         * 当 SOC 模块尚未初始化或采样参数无效时，
         * bms_soc_update() 返回 false，本任务不额外处理。
         */
        if (bms_soc_update(&bms_battery, SAMPLE_ACCUM_COUNT))
        {
            /* SOC 已更新累计放电量，再换算等效完整循环数和 SOH。 */
            (void)bms_soh_update(&bms_battery);
        }
    }
}
