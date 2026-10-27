
/**
 ***********************************************************************************************************************
 * @file    app_bms.c
 * @brief   BMS 应用层管理，实现电池数据采样、充放电状态判断、保护及 SOC/SOH 估算
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_bms.h"
#include "app_system.h"
#include "bms.h"
#include "param.h"

#include <float.h>


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** BMS 全局电池运行数据，由采样、保护及 SOC/SOH 模块共同维护 */
bms_battery_t bms_battery = {0};


/* Constants ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief 电池默认配置参数
 * @note  定义电池额定容量、充放电电压范围、静置判断条件
 *        及库仑积分效率参数。
 */
const battery_config_t battery_config =
{
    .design_capacity_mah  = 5000.0f,    /**< 电池设计容量，单位 mAh */
    .full_voltage_v       = 4.15f,      /**< 电池满充电压，单位 V */
    .cutoff_voltage_v     = 3.00f,      /**< 电池放电截止电压，单位 V */
    .rest_current_ma      = 50.0f,      /**< 静置电流判断阈值，单位 mA */
    .rest_dvdt_v_per_s    = 0.02f,      /**< 静置电压变化率阈值，单位 V/s */
    .rest_time_ms         = 10000U,     /**< OCV 修正要求的静置时间，单位 ms */
    .charge_efficiency    = 3.35f,      /**< 充电库仑积分效率系数 */
    .discharge_efficiency = 1.00f       /**< 放电库仑积分效率系数 */
};


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_bms_Restore
//  Description: 从 Flash 恢复电池 SOC/SOH 及累计电量数据
//  Param      : None
//  Return     : None
//  Others     : 从 SOC 参数分区读取 battery_soc_t 历史数据，
//               并恢复至 bms_battery.soc_info；
//               数据有效性由 Flash 参数管理模块判断
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_bms_Restore(void)
{
    param_soc_read((uint8_t *)&bms_battery.soc_info, sizeof(battery_soc_t));
}


/*******************************************************************************
//  Function   : app_bms_Save
//  Description: 将当前电池 SOC/SOH 及累计电量数据保存至 Flash
//  Param      : None
//  Return     : None
//  Others     : 将 bms_battery.soc_info 写入 SOC 参数分区；
//               Flash 擦除及编程操作由参数管理模块完成
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_bms_Save(void)
{
    param_soc_save((uint8_t *)&bms_battery.soc_info, sizeof(battery_soc_t));
}


/*******************************************************************************
//  Function   : app_bms_Init
//  Description: 初始化 BMS 应用层功能模块
//  Param      : None
//  Return     : None
//  Others     : 加载默认电池配置，恢复 Flash 历史数据，
//               初始化 ADC 换算模块及 IP5326 KEY 控制模块；
//               SOC 初始化标志默认设置为 false
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_bms_Init(void)
{
    /* 绑定电池配置参数并清除 SOC 初始化标志。 */
    bms_battery.configInfo = &battery_config;
    bms_battery.soc_Init   = false;

    /* 恢复 Flash 保存的电池历史数据。 */
    app_bms_Restore();

    /* 初始化电池 ADC 数据换算模块。 */
    bms_convert_Init();

    /* 初始化 IP5326 KEY 控制状态机。 */
    bms_key_Init();
}


/*******************************************************************************
//  Function   : app_bms_key_ctrl
//  Description: 根据系统休眠状态控制 IP5326 KEY 请求
//  Param      : None
//  Return     : None
//  Others     : 系统休眠时请求 IP5326 待机；
//               系统正常运行且上次完成待机操作时请求唤醒；
//               正常运行期间每隔约 10s 再次发送唤醒请求
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_bms_key_ctrl(void)
{
    static timer_base_t _time = {0};

    /* 系统休眠时请求 IP5326 进入待机，并跳过唤醒处理。 */
    if (System.sleep_flg)
    {
        bms_key_Request(BMS_KEY_STANDBY);
        return;
    }
    else
    {
        /* 上次完成待机操作后，请求唤醒并复位计时器。 */
        if (bms_key_GetStatus()->lastComplete == BMS_KEY_STANDBY)
        {
            bms_key_Request(BMS_KEY_WAKEUP);
            timer_base_Rst(&_time);
        }
    }

    /*
     * 正常运行期间周期性请求唤醒。
     * 原电流方向判断暂时禁用，当前不限制充放电状态。
     */
    // if (bms_battery.current_ma < 0)
    {
        if (timer_base_Count(&_time) > 10000U)
        {
            bms_key_Request(BMS_KEY_WAKEUP);
            timer_base_Rst(&_time);
        }
    }
}


/*******************************************************************************
//  Function   : app_bms_state_update
//  Description: 根据电池电流及 IP5326 反馈更新电池运行状态
//  Param      : None
//  Return     : None
//  Others     : 首先根据电池电流判断充放电状态；
//               当 IP5326 报告充电完成时，将状态覆盖为 BAT_STATE_FAULT；
//               该覆盖行为沿用当前业务逻辑
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void app_bms_state_update(void)
{
    /* 根据当前电池电流判断充放电状态。 */
    bms_battery.state = bms_battery_state(bms_battery.current_ma);

    /* 根据 IP5326 LED 状态进行辅助判断。 */
    if (bms_status_GetStatus()->state == IP5326_STATUS_FULL)
    {
        bms_battery.state = BAT_STATE_FULL;
    }
}


/*******************************************************************************
//  Function   : app_bms_soc_update
//  Description: 执行电池 SOC 初始化、库仑积分及 SOH 更新
//  Param      : None
//  Return     : None
//  Others     : SOC 未初始化时，连续检查电池电压和电流变化；
//               满足稳定条件后根据 OCV 估算初始 SOC；
//               初始化完成后按采样间隔执行库仑积分及 SOH 更新
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void app_bms_soc_update(void)
{
    static int Cnt = 0;
    static double lastVol  = 0.0;
    static double lastCurr = 0.0;

    /* SOC 尚未初始化时，检查连续采样值是否稳定。 */
    if (bms_battery.soc_Init == false)
    {
        if ((bms_soc_abs(lastVol  - bms_battery.voltage_v)  < 0.02)
        &&  (bms_soc_abs(lastCurr - bms_battery.current_ma) < 30.0))
        {
            Cnt++;

            /* 连续满足稳定条件后执行 SOC/SOH 初始化。 */
            if (Cnt > 10)
            {
                /* 根据当前电池电压估算初始 SOC。 */
                bms_battery.soc_info.soc = bms_soc_from_ocv(bms_battery.voltage_v);

                /* 初始化 SOC 状态及 SOH 计算模块。 */
                bms_soc_init(&bms_battery, bms_battery.soc_info.soc);
                bms_soh_init(&bms_battery);

                bms_battery.soc_Init = true;
                Cnt = 0;
            }
        }

        /* 保存本次采样值，供下次判断变化量。 */
        lastVol  = bms_battery.voltage_v;
        lastCurr = bms_battery.current_ma;
    }
    else
    {
        /*
         * 使用本次 ADC 累计采样间隔执行库仑积分。
         * SOC 更新成功后继续更新 SOH。
         */
        if (bms_soc_update(&bms_battery, SAMPLE_ACCUM_COUNT))
        {
            (void)bms_soh_update(&bms_battery);
        }
    }
}


/* Debug variables ---------------------------------------------------------------------------------------------------*/

/** BMS Flash 保存测试触发标志 */
bool Is_bms_save = false;


/*******************************************************************************
//  Function   : app_bms_test
//  Description: 执行 BMS Flash 保存测试
//  Param      : None
//  Return     : None
//  Others     : 当 Is_bms_save 为 true 时，清除触发标志并保存 SOC 数据；
//               用于调试 Flash 持久化功能
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void app_bms_test(void)
{
    if (Is_bms_save)
    {
        Is_bms_save = false;
        app_bms_Save();
    }
}


/*******************************************************************************
//  Function   : app_bms_1ms_task
//  Description: 执行 BMS 应用层 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 推进 IP5326 KEY 状态机并更新 LED 状态；
//               ADC 累计采样完成后更新电压、电流、充放电状态、
//               电池保护及 SOC/SOH；最后处理 Flash 测试与 KEY 控制
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_bms_1ms_task(void)
{
    double adcValue[BMS_ADC_NUM] = {0.0};

    /* 推进 IP5326 KEY 非阻塞控制状态机。 */
    bms_key_1ms_task();

    /* 更新 IP5326 LED 反馈状态。 */
    bms_status_check(bms_battery.state);

    /* ADC 累计采样完成后执行 BMS 数据处理。 */
    if (sample_accum_get(adcValue))
    {
        /* 根据 ADC 数据换算电池电压。 */
        bms_battery.voltage_v  = bms_battery_voltage(adcValue[BMS_ADC_VOLTAGE]);

        /* 根据 ADC 数据换算电池电流，参考电压参数为 3.3V。 */
        bms_battery.current_ma = bms_battery_current(adcValue[BMS_ADC_CURRENT], 3.3);

        /* 根据 ADC 数据换算 USB 输入电压。 */
        bms_battery.usb_in_v   = bms_usb_in_voltage(adcValue[BMS_ADC_USB_VOLTAGE]);

        /* 更新电池充放电状态。 */
        app_bms_state_update();

        /* 执行电池电压、电流、USB 及温度保护检查。 */
        bms_protect_check(&bms_battery);

        /* 执行 SOC 初始化或库仑积分及 SOH 更新。 */
        app_bms_soc_update();
    }

    /* 处理 Flash 数据保存测试。 */
    app_bms_test();

    /* 根据系统休眠状态执行 IP5326 KEY 控制。 */
    app_bms_key_ctrl();
}
