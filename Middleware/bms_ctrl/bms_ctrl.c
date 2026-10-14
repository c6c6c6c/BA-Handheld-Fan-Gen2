/**
 ***********************************************************************************************************************
 * @file    bms_ctrl.c
 * @brief   BMS 通路控制与模块协调，KEY 和状态采集由独立模块实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-14
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bms_ctrl.h"
#include "bms_config.h"
#include "bsp_adc.h"

/* Variables ---------------------------------------------------------------------------------------------------------*/

static bms_battery_data_t s_batteryData;


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_battery_voltage
//  Description: 根据 VREFINT ADC 采样值计算电池电压
//  Param      : None
//  Return     : None
//  Others     : ADC 参考电压为 VCCA，本板 VCCA 与 VBAT 相连，因此通过内部基准 VREFINT
//               的 ADC 采样值反推出 VCCA，即得到电池电压；采样异常时标记数据无效
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
static void bms_battery_voltage(void)
{
    /*
     * 获取内部参考电压 VREFINT 的 ADC 原始采样值。
     *
     * ADC 转换关系：
     *
     *      ADC_VREFINT = VREFINT / VCCA × ADC_FULL_SCALE
     *
     * 其中：
     *      VREFINT        ：MCU 内部固定参考电压
     *      VCCA           ：ADC 参考供电电压
     *      ADC_FULL_SCALE ：ADC 满量程码值，例如 12bit ADC 为 4095
     *
     * 因此可以反推：
     *
     *      VCCA = VREFINT × ADC_FULL_SCALE / ADC_VREFINT
     */
    uint32_t vrefAdc = ADCxConvertedDatas[BMS_BATTERY_VREFINT_ADC_INDEX];

    /*
     * 检查 VREFINT 采样结果是否合法。
     *
     * vrefAdc == 0： 后续计算会发生除 0，因此直接判定无效。
     *
     * vrefAdc > ADC_FULL_SCALE： 超出 ADC 理论最大码值，说明采样数据异常。
     */
    if ((vrefAdc == 0U) || (vrefAdc > BMS_BATTERY_ADC_FULL_SCALE))
    {
        s_batteryData.voltageV     = 0.0;
        return;
    }

    /*
     * 根据内部参考电压反推出 ADC 参考供电电压 VCCA。
     *
     *      VCCA = VREFINT × ADC_FULL_SCALE / ADC_VREFINT
     *
     * 本板 VCCA 与 VBAT 直接相连，因此：
     *
     *      BatteryVoltage = VCCA
     *
     * 例如：
     *
     *      VREFINT        = 1.2V
     *      ADC_FULL_SCALE = 4095
     *      ADC_VREFINT    = 1200
     *
     *      VBAT = 1.2 × 4095 / 1200 ≈ 4.095V
     */
    s_batteryData.voltageV = BMS_BATTERY_VREFINT_V * (double)BMS_BATTERY_ADC_FULL_SCALE / (double)vrefAdc;
}

/*******************************************************************************
 //  Function   : bms_battery_current
 //  Description: 根据 PGA2 ADC 采样值计算电池充放电电流
 //  Param      : None
 //  Return     : None
 //  Others     : PGA2 对分流电阻 R18 两端压差进行放大，并以 VCCA 的固定比例作为零电流基准；
 //               根据 PGA2 输出相对零点的偏移量反推出分流电阻压差，再计算电池电流
 //  Author     : Chen Liang 2026-10-14
 *******************************************************************************/
static void bms_battery_current(void)
{
    /*
     * 获取 PGA2 输出对应的 ADC 原始采样值。
     *
     * ADC 换算关系：
     *
     *      V_PGA = ADC_PGA / ADC_FULL_SCALE × VCCA
     *
     * 其中：
     *      V_PGA          ：PGA2 输出电压
     *      ADC_PGA        ：PGA2 对应 ADC 采样值
     *      ADC_FULL_SCALE ：ADC 满量程码值
     *      VCCA           ：ADC 参考供电电压
     */
    uint32_t pgaAdc = ADCxConvertedDatas[BMS_BATTERY_CURRENT_ADC_INDEX];

    /*
     * 检查 PGA2 ADC 采样值是否合法。
     *
     * pgaAdc == 0：常见于 ADC/PGA2 尚未启动、采样数据未更新或输出异常。
     *
     * pgaAdc >= ADC_FULL_SCALE：PGA2 输出已经接近或达到 ADC 满量程，可能表示 PGA 输出饱和，此时无法可靠反推出实际电流。
     */
    if ((pgaAdc == 0U) || (pgaAdc >= BMS_BATTERY_ADC_FULL_SCALE))
    {
        s_batteryData.currentA = 0.0;
        return;
    }

    /*
     * 根据 PGA2 输出计算分流电阻 R18 中的电流。
     *
     * 第一步：将 ADC 码值归一化。
     *
     *      PGA_RATIO = ADC_PGA / ADC_FULL_SCALE
     *
     * PGA2 零电流输出不是 0V，而是以 VCCA 的固定比例作为零点：
     *
     *      V_ZERO = BMS_BATTERY_PGA_ZERO_RATIO × VCCA
     *
     * 例如 BMS_BATTERY_PGA_ZERO_RATIO = 0.5：
     *
     *      V_ZERO = VCCA / 2
     *
     * 因此 PGA2 相对零点的输出偏移为：
     *
     *      ΔV_PGA = (PGA_RATIO - ZERO_RATIO) × VCCA
     *
     * PGA2 对 R18 两端的差分电压进行了放大：
     *      ΔV_PGA = V_SHUNT × PGA_GAIN
     *
     * 所以：
     *      V_SHUNT = ΔV_PGA / PGA_GAIN
     *
     * 根据欧姆定律：
     *
     *      I = V_SHUNT / R_SHUNT
     *
     * 合并后得到：
     *
     *      I = (ADC_PGA / ADC_FULL_SCALE - ZERO_RATIO) × VCCA / (PGA_GAIN × R_SHUNT)
     *
     * 最后乘 BMS_BATTERY_CURRENT_SIGN，
     * 用于统一电流方向定义：
     *
     *      currentA > 0 ：放电
     *      currentA < 0 ：充电
     */
    s_batteryData.currentA =
        (((double)pgaAdc / (double)BMS_BATTERY_ADC_FULL_SCALE) - BMS_BATTERY_PGA_ZERO_RATIO) *
        s_batteryData.voltageV / (BMS_BATTERY_PGA_GAIN * BMS_BATTERY_SHUNT_OHM) * BMS_BATTERY_CURRENT_SIGN;
}

/*******************************************************************************
 //  Function   : bms_battery_current
 //  Description: 根据 PGA2 ADC 采样值计算电池充放电电流
 //  Param      : None
 //  Return     : None
 //  Others     : PGA2 对分流电阻 R18 两端压差进行放大，并以 VCCA 的固定比例作为零电流基准；
 //               根据 PGA2 输出相对零点的偏移量反推出分流电阻压差，再计算电池电流
 //  Author     : Chen Liang 2026-10-14
 *******************************************************************************/
static void bms_battery_state(void)
{
    if(s_batteryData.currentA > 0)
    {
        s_batteryData.state = BMS_POWER_DISCHARGING;
    }
    else if(s_batteryData.currentA < 0)
    {
        s_batteryData.state = BMS_POWER_CHARGING;
    }
    else
    {
        s_batteryData.state = BMS_POWER_IDLE;
    }
}

/* Exported functions ------------------------------------------------------------------------------------------------*/

