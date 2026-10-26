
/**
 ***********************************************************************************************************************
 * @file    app_bms.c
 * @brief   BMS 应用层管理，实现电池参数配置、数据恢复、采样调度及 SOC/SOH 更新
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "app_bms.h"
#include "app_system.h"
#include "bms.h"

#include <float.h>


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** BMS 全局电池运行数据，由采样、保护及 SOC/SOH 模块共同维护 */
bms_battery_t bms_battery = {0};


/** 默认电池配置参数 */
const battery_config_t battery_config =
{
    .design_capacity_mah  = 5000.0f,    /* 设计容量，单位 mAh */
    .full_voltage_v       = 4.20f,      /* 满充电压，单位 V */
    .cutoff_voltage_v     = 2.50f,      /* 放电截止电压，单位 V */
    .rest_current_ma      = 50.0f,      /* 静置电流阈值，单位 mA */
    .rest_dvdt_v_per_s    = 0.0002f,    /* 静置电压变化率阈值，单位 V/s */
    .rest_time_ms         = 1800000U,   /* OCV 修正所需静置时间，单位 ms */
    .charge_efficiency    = 0.99f,      /* 充电库仑效率 */
    .discharge_efficiency = 1.00f       /* 放电库仑效率 */
};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_bms_persist_valid
//  Description: 校验电池掉电保存数据的有效性
//  Param      : data   - 待校验的电池历史数据
//               config - 当前电池配置参数
//  Return     : true：数据有效；false：参数或数据范围异常
//  Others     : 检查 SOC、SOH、剩余容量及 SOC 与容量的一致性；
//               循环次数和 SOH 在恢复后根据累计放电量重新计算
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
static bool app_bms_persist_valid(const battery_soc_t *data, const battery_config_t *config)
{
    double expected_soc;
    double soc_diff;

    /* 检查配置参数及持久化数据的有效范围。 */
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

    /*
     * 根据剩余容量反算 SOC，并与保存的 SOC 比较。
     *
     * expected_soc = remaining_capacity / design_capacity × 100%
     *
     * 两者偏差超过允许范围时，认为掉电数据不一致。
     */
    expected_soc = (double)data->remaining_capacity_mah *
                   (double)BMS_SOC_PERCENT_FULL /
                   (double)config->design_capacity_mah;

    soc_diff = (double)data->soc - expected_soc;

    if (soc_diff < 0.0)
    {
        soc_diff = -soc_diff;
    }

    return (soc_diff <= BMS_PERSIST_SOC_TOLERANCE_PERCENT);
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_bms_Restore
//  Description: 恢复掉电保存的电池 SOC、SOH 及累计电量数据
//  Param      : saved - 从 Flash 读取的电池历史数据
//  Return     : true：恢复成功；false：数据无效或模块初始化失败
//  Others     : 应在电池参数配置成功后调用；恢复失败时尝试恢复原运行数据
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
bool app_bms_Restore(const battery_soc_t *saved)
{
    uint32_t previous_charge;
    uint32_t previous_discharge;
    float previous_soc;
    float restored_soc;

    /* 校验掉电数据及当前电池配置。 */
    if (!app_bms_persist_valid(saved, bms_battery.configInfo))
    {
        return false;
    }

    /* 备份当前运行数据，供恢复失败时回退。 */
    previous_charge    = bms_battery.soc_info.charge_mah_total;
    previous_discharge = bms_battery.soc_info.discharge_mah_total;
    previous_soc       = bms_battery.soc_info.soc;

    /* 根据保存的剩余容量重新计算初始 SOC。 */
    restored_soc = (float)((double)saved->remaining_capacity_mah *
                           (double)BMS_SOC_PERCENT_FULL /
                           (double)bms_battery.configInfo->design_capacity_mah);

    /* 恢复累计充入和放出电量。 */
    bms_battery.soc_info.charge_mah_total    = saved->charge_mah_total;
    bms_battery.soc_info.discharge_mah_total = saved->discharge_mah_total;

    /* 重新初始化 SOC 和 SOH 估算模块。 */
    if (!bms_soc_init(&bms_battery, restored_soc)
    ||  !bms_soh_init(&bms_battery))
    {
        /* 初始化失败时尝试恢复此前的运行数据。 */
        bms_battery.soc_info.charge_mah_total    = previous_charge;
        bms_battery.soc_info.discharge_mah_total = previous_discharge;

        (void)bms_soc_init(&bms_battery, previous_soc);
        (void)bms_soh_init(&bms_battery);

        return false;
    }

    /* 保存已验证的历史数据快照。 */
    bms_battery.soc_persist = *saved;

    return true;
}


/*******************************************************************************
//  Function   : app_bms_Save
//  Description: 生成电池 SOC、SOH 及累计电量的掉电保存快照
//  Param      : output - 电池数据快照输出缓冲区
//  Return     : true：生成成功；false：未初始化或运行数据异常
//  Others     : 仅生成数据快照，不执行 Flash 写入；由上层负责持久化保存
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
bool app_bms_Save(battery_soc_t *output)
{
    battery_soc_t snapshot;

    /* 输出缓冲区及电池配置必须有效。 */
    if ((output == NULL) || (bms_battery.configInfo == NULL))
    {
        return false;
    }

    /* 保存前更新等效循环次数及 SOH。 */
    if (!bms_soh_update(&bms_battery))
    {
        return false;
    }

    snapshot = bms_battery.soc_info;

    /* 检查当前 SOC 数据是否满足保存要求。 */
    if (!app_bms_persist_valid(&snapshot, bms_battery.configInfo))
    {
        return false;
    }

    bms_battery.soc_persist = snapshot;
    *output = snapshot;

    return true;
}


/*******************************************************************************
//  Function   : app_bms_ConfigureBattery
//  Description: 配置电池参数并初始化 SOC、SOH 估算模块
//  Param      : config              - 电池配置参数
//               initial_soc_percent - 初始 SOC，单位 %
//  Return     : true：配置成功；false：配置参数或初始 SOC 无效
//  Others     : 初始化失败时恢复原电池配置，并尝试恢复此前 SOC 状态
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
bool app_bms_ConfigureBattery(const battery_config_t *config, float initial_soc_percent)
{
    const battery_config_t *previous = bms_battery.configInfo;
    float previous_soc = bms_battery.soc_info.soc;

    /* 临时绑定新配置，用于 SOC/SOH 模块初始化。 */
    bms_battery.configInfo = config;

    if (!bms_soc_init(&bms_battery, initial_soc_percent))
    {
        bms_battery.configInfo = previous;
        return false;
    }

    if (!bms_soh_init(&bms_battery))
    {
        /* SOH 初始化失败时恢复此前的配置及 SOC。 */
        bms_battery.configInfo = previous;

        if (previous != NULL)
        {
            (void)bms_soc_init(&bms_battery, previous_soc);
        }

        return false;
    }

    return true;
}


/*******************************************************************************
//  Function   : app_bms_Init
//  Description: 初始化 BMS 应用层相关功能模块
//  Param      : None
//  Return     : None
//  Others     : 初始化 ADC 数据换算模块及 IP5326 KEY 控制模块
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_bms_Init(void)
{
    bms_convert_Init();
    bms_key_Init();
}


/*******************************************************************************
//  Function   : app_bms_key_ctrl
//  Description: 根据系统休眠状态及电池电流控制 IP5326 KEY
//  Param      : None
//  Return     : None
//  Others     : 1. 系统进入休眠时请求 IP5326 待机
//               2. 系统退出休眠且上次完成待机操作时请求唤醒
//               3. 检测到电池充电时，每隔 10s 请求一次唤醒
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_bms_key_ctrl(void)
{
    static timer_base_t _time = {0};

    /* 系统休眠时，请求 IP5326 进入待机状态。 */
    if (System.sleep_flg)
    {
        bms_key_Request(BMS_KEY_STANDBY);
		return;
    }
    else
    {
        /* 上一次完成待机操作时，请求唤醒并复位计时器。 */
        if (bms_key_GetStatus()->lastComplete == BMS_KEY_STANDBY)
        {
            bms_key_Request(BMS_KEY_WAKEUP);
            timer_base_Rst(&_time);
        }
    }

    /* 电池电流小于 0，表示处于放电方向。 */
    if (bms_battery.current_ma < 0)
    {
        /* 每隔 10s 请求一次 KEY 唤醒操作。 */
        if (timer_base_Count(&_time) > 10000)
        {
            bms_key_Request(BMS_KEY_WAKEUP);
            timer_base_Rst(&_time);
        }
    }
}

/*******************************************************************************
//  Function   : app_bms_1ms_task
//  Description: BMS 应用层 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 周期推进 KEY 状态机；ADC 累计采样完成后更新电池参数、
//               充放电状态、软件保护及 SOC/SOH 估算结果
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_bms_1ms_task(void)
{
    double adcValue[BMS_ADC_NUM] = {0.0};

    /* 推进 IP5326 KEY 非阻塞状态机。 */
    bms_key_1ms_task();

    /* 更新 IP5326 LED 反馈状态。 */
    bms_status_check(bms_battery.state);

    /* 仅在一组 ADC 累计采样完成后执行数据换算。 */
    if (sample_accum_get(adcValue))
    {
        /* 将 ADC 采样值换算为电池电压、电流及 USB 输入电压。 */
        bms_battery.voltage_v  = bms_battery_voltage(adcValue[BMS_ADC_VOLTAGE]);
        bms_battery.current_ma = bms_battery_current(adcValue[BMS_ADC_CURRENT], 3.3);
        bms_battery.usb_in_v   = bms_usb_in_voltage(adcValue[BMS_ADC_USB_VOLTAGE]);

        /*
         * 根据电池电流判断充放电状态。
         * 状态判断输入单位为 A，需将 mA 转换为 A。
         */
        bms_battery.state = bms_battery_state(bms_battery.current_ma);

        /* 检查电池欠压、过压、USB 输入、温度及电流保护。 */
        bms_protect_check(&bms_battery);

        /*
         * 根据采样间隔进行库仑积分。
         * SOC 更新成功后，继续更新等效循环次数及 SOH。
         */
        if (bms_soc_update(&bms_battery, SAMPLE_ACCUM_COUNT))
        {
            (void)bms_soh_update(&bms_battery);
        }
    }
	
	app_bms_key_ctrl();
}
