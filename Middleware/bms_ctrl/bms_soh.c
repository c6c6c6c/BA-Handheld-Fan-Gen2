/**
 ***********************************************************************************************************************
 * @file    bms_soh.c
 * @brief   根据累计等效完整循环数估算电池 SOH
 ***********************************************************************************************************************
 */

#include "bms_soh.h"

#include <float.h>
#include <stddef.h>

#if BMS_SOH_REFERENCE_CYCLES == 0U
#error "BMS_SOH_REFERENCE_CYCLES must be greater than zero"
#endif

/* 与 SOC 模块一致：当前 BMS 应用只维护一个电池对象。 */
static bms_battery_t *s_soh_owner;
static const battery_config_t *s_soh_config;
static uint32_t s_last_discharge_mah_total;


/***********************************************************************************************************************
//  Function   : bms_soh_config_valid
//  Description: 检查等效循环计算所需的设计容量和经验参数
//  Param      : config - 电池配置参数
//  Return     : true：参数有效；false：参数无效
//  Others     : 防止零容量、无穷大及不合理的 SOH 参考百分比参与计算
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
static bool bms_soh_config_valid(const battery_config_t *config)
{
    return (config != NULL)
        && (config->design_capacity_mah > 0.0f)
        && (config->design_capacity_mah < FLT_MAX)
        && (BMS_SOH_INITIAL_PERCENT > 0.0f)
        && (BMS_SOH_INITIAL_PERCENT <= 100.0f)
        && (BMS_SOH_AT_REFERENCE_PERCENT >= 0.0f)
        && (BMS_SOH_AT_REFERENCE_PERCENT <= BMS_SOH_INITIAL_PERCENT)
        && (BMS_SOH_MIN_PERCENT >= 0.0f)
        && (BMS_SOH_MIN_PERCENT <= BMS_SOH_AT_REFERENCE_PERCENT);
}


/***********************************************************************************************************************
//  Function   : bms_soh_update
//  Description: 将累计放电量换算为等效完整循环数，再用经验衰减曲线计算 SOH
//  Param      : battery - 电池运行数据
//  Return     : true：更新成功；false：尚未初始化或配置无效
//  Others     : 仅更新运行数据，不改写 persist 或其校验值
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
bool bms_soh_update(bms_battery_t *battery)
{
    double equivalent_cycles;
    double soh_percent;

    if ((battery == NULL)
    ||  (battery != s_soh_owner)
    ||  (battery->configInfo != s_soh_config)
    ||  (battery->discharge_mah_total < s_last_discharge_mah_total)
    ||  !bms_soh_config_valid(battery->configInfo))
    {
        return false;
    }

    /* 累计放出设计容量对应 1 个等效完整循环，允许由多个浅放电组成。 */
    equivalent_cycles = (double)battery->discharge_mah_total /
                        (double)battery->configInfo->design_capacity_mah;

    /* 对外显示整数循环数；完整精度只用于下面的 SOH 计算。 */
    if (equivalent_cycles >= (double)UINT32_MAX)
    {
        battery->cycle_count = UINT32_MAX;
    }
    else
    {
        battery->cycle_count = (uint32_t)equivalent_cycles;
    }

    /* 从 0 循环时的初始值线性衰减至参考循环处的设定值。 */
    soh_percent = (double)BMS_SOH_INITIAL_PERCENT -
                  ((double)BMS_SOH_INITIAL_PERCENT -
                   (double)BMS_SOH_AT_REFERENCE_PERCENT) *
                  equivalent_cycles / (double)BMS_SOH_REFERENCE_CYCLES;

    if (soh_percent < (double)BMS_SOH_MIN_PERCENT)
    {
        soh_percent = (double)BMS_SOH_MIN_PERCENT;
    }
    else if (soh_percent > (double)BMS_SOH_INITIAL_PERCENT)
    {
        soh_percent = (double)BMS_SOH_INITIAL_PERCENT;
    }

    battery->soh = (float)soh_percent;
    s_last_discharge_mah_total = battery->discharge_mah_total;
    return true;
}


/***********************************************************************************************************************
//  Function   : bms_soh_init
//  Description: 绑定电池对象，并根据已有累计放电量恢复循环数和 SOH
//  Param      : battery - 已设置 configInfo 的电池运行数据
//  Return     : true：初始化成功；false：对象或配置无效
//  Others     : 上层需先恢复有效的 discharge_mah_total，再调用本函数
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************************************/
bool bms_soh_init(bms_battery_t *battery)
{
    if ((battery == NULL) || !bms_soh_config_valid(battery->configInfo))
    {
        return false;
    }

    s_soh_owner  = battery;
    s_soh_config = battery->configInfo;
    s_last_discharge_mah_total = battery->discharge_mah_total;
    return bms_soh_update(battery);
}
