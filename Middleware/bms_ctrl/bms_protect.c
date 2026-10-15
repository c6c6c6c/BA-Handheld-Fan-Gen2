/**
 ***********************************************************************************************************************
 * @file    bms_protect.c
 * @brief   BMS 软件保护实现，完成实时故障检测及故障保持
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bms_protect.h"


/* Private variables -------------------------------------------------------------------------------------------------*/




/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_protect_batteryLow_check
//  Description: 检查电池欠压故障
//  Param      : voltage_v - 当前电池电压，单位 V
//  Return     : true：电池欠压；false：正常或欠压保护未配置
//  Others     : 阈值 <= 0 表示该保护未配置
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool bms_protect_batteryLow_check(float voltage_v)
{
    return (BMS_PROTECT_BATTERY_LOW_V > 0.0f) && (voltage_v <= BMS_PROTECT_BATTERY_LOW_V);
}


/*******************************************************************************
//  Function   : bms_protect_batteryHigh_check
//  Description: 检查电池过压故障
//  Param      : voltage_v - 当前电池电压，单位 V
//  Return     : true：电池过压；false：正常或过压保护未配置
//  Others     : 阈值 <= 0 表示该保护未配置
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool bms_protect_batteryHigh_check(float voltage_v)
{
    return (BMS_PROTECT_BATTERY_HIGH_V > 0.0f) && (voltage_v >= BMS_PROTECT_BATTERY_HIGH_V);
}


/*******************************************************************************
//  Function   : bms_protect_usbHigh_check
//  Description: 检查 USB 输入过压故障
//  Param      : usb_present - USB 接入状态
//               usb_mv      - USB 输入电压，单位 mV
//  Return     : true：USB 输入过压；false：正常、未接入或保护未配置
//  Others     : 仅在 USB 已接入时执行过压判断
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool bms_protect_usbLow_check(uint16_t usb_v)
{
    return  (BMS_PROTECT_USB_HIGH_MV > 0U) && (usb_mv >= BMS_PROTECT_USB_LOW_V);
}

/*******************************************************************************
//  Function   : bms_protect_usbHigh_check
//  Description: 检查 USB 输入过压故障
//  Param      : usb_present - USB 接入状态
//               usb_mv      - USB 输入电压，单位 mV
//  Return     : true：USB 输入过压；false：正常、未接入或保护未配置
//  Others     : 仅在 USB 已接入时执行过压判断
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool bms_protect_usbHigh_check(uint16_t usb_v)
{
    return  (BMS_PROTECT_USB_HIGH_MV > 0U) && (usb_mv >= BMS_PROTECT_USB_HIGH_MV);
}


/*******************************************************************************
//  Function   : bms_protect_tempHigh_check
//  Description: 检查主控板 NTC 过温故障
//  Param      : temperature_deci_c - 当前温度，单位 0.1°C
//  Return     : true：过温；false：正常或过温保护未配置
//  Others     : 例如 600 表示 60.0°C
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool bms_protect_tempHigh_check(int16_t temperature_deci_c)
{
    return (BMS_PROTECT_TEMP_HIGH_DECI_C > 0) && (temperature_deci_c >= BMS_PROTECT_TEMP_HIGH_DECI_C);
}


/*******************************************************************************
//  Function   : bms_protect_currentHigh_check
//  Description: 检查电池过流故障
//  Param      : current_ma - 当前电池电流，单位 mA
//  Return     : true：过流；false：正常或过流保护未配置
//  Others     : 不区分充放电方向，统一使用电流绝对值进行判断
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool bms_protect_currentHigh_check(float current_ma)
{
    /* 充电为负、放电为正，过流保护只关注电流绝对值。 */
    float current_abs_ma = (current_ma < 0.0f) ? -current_ma : current_ma;

    return (BMS_PROTECT_CURRENT_HIGH_MA > 0.0f) && (current_abs_ma >= BMS_PROTECT_CURRENT_HIGH_MA);
}


/*******************************************************************************
//  Function   : bms_protect_hold
//  Description: 更新全部故障的保持状态
//  Param      : real     - 当前实时故障
//               previous - 上一次故障保持状态
//               now_ms   - 当前系统时间，单位 ms
//               time_s   - 故障保持时间，单位 s
//  Return     : 更新后的故障保持位域
//  Others     : 各故障独立计时，实时故障消失后仍保持指定时间
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bms_protect_fault_t bms_protect_hold(const bms_protect_fault_t *real,uint32_t time_s)
{
    static uint32_t            s_last_fault_ms[7] = {0};
    static bms_protect_fault_t result             = {0};
    uint8_t                    i;


    /* 输入参数无效时返回无故障状态。 */
    if (real == 0)
    {
        result.data = 0;
        return result;
    }

    for(i = 0 ; i < 7 ; i ++)
    {
        real->data
    }


    return result;
}


/*******************************************************************************
//  Function   : bms_protect_check
//  Description: 更新 BMS 实时故障及故障保持状态
//  Param      : battery            - 电池运行数据
//               usb_mv             - USB 输入电压，单位 mV
//               temperature_deci_c - 当前温度，单位 0.1°C
//               valid_mask         - 当前有效采样通道位
//               sample_age_ms      - 当前采样数据年龄，单位 ms
//               usb_present        - USB 接入状态
//               external_fault     - 外部故障状态
//               now_ms             - 当前系统时间，单位 ms
//  Return     : None
//  Others     : 先检查采样有效性，再执行各保护判断，最后更新故障保持状态
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void bms_protect_check(bms_battery_t *battery)
{
    
    bms_protect_fault_t *real;
    bool                 usb_present = BMS_IO_USB_IS_PRESENT();
    /* 电池运行数据对象无效时不执行保护判断。 */
    if (battery == 0)
    {
        return;
    }

    real = &battery->fault_real;


    /*
     * 当采样整体无效时，不继续使用这些数据做阈值判断，
     * 避免无效 ADC 数据误触发欠压、过压、过温或过流。
     */
    
    real->bit.batteryLow  = bms_protect_batteryLow_check(battery->voltage_v);     /* 电池欠压。 */
    real->bit.batteryHigh = bms_protect_batteryHigh_check(battery->voltage_v);    /* 电池过压。 */

    real->bit.usbLow      = bms_protect_usbLow_check(battery->usb_in_v);          /* USB 输入过压。 */
    real->bit.usbHigh     = bms_protect_usbHigh_check(battery->usb_in_v);         /* USB 输入过压。 */

    real->bit.tempHigh    = bms_protect_tempHigh_check(0);                        /* 主控板 NTC 过温。 */
    real->bit.currentHigh = bms_protect_currentHigh_check(battery->current_ma);   /* 电池充放电过流。 */


    /*
     * 将实时故障转换为保持故障。
     *
     * 即使实时故障刚刚消失，也会继续保持
     * BMS_PROTECT_HOLD_TIME_S 秒。
     */
    battery->fault_hold = bms_protect_hold(real,BMS_PROTECT_HOLD_TIME_S);
}