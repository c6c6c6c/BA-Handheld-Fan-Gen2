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


/***********************************************************************************************
//  Function   : bms_protect_usbLow_check
//  Description: 检查 USB 输入欠压故障
//  Param      : usb_v - 当前 USB 输入电压
//  Return     : true：USB 输入欠压；false：正常或欠压保护未配置
//  Others     : 阈值 <= 0 表示该保护未配置
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************/
bool bms_protect_usbLow_check(uint16_t usb_v)
{
    return  (BMS_PROTECT_USB_LOW_V > 0U) && (usb_v >= BMS_PROTECT_USB_LOW_V);
}

/***********************************************************************************************
//  Function   : bms_protect_usbHigh_check
//  Description: 检查 USB 输入过压故障
//  Param      : usb_v - 当前 USB 输入电压
//  Return     : true：USB 输入过压；false：正常或过压保护未配置
//  Others     : 阈值 <= 0 表示该保护未配置
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************/
bool bms_protect_usbHigh_check(uint16_t usb_v)
{
    return  (BMS_PROTECT_USB_HIGH_V > 0U) && (usb_v >= BMS_PROTECT_USB_HIGH_V);
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


/***********************************************************************************************
//  Function   : bms_protect_hold
//  Description: 对实时故障进行触发防抖及恢复保持
//  Param      : real   - 当前实时故障
//  Return     : 防抖及保持处理后的故障位域
//  Others     : 故障达到检测时间后置位，消失达到保持时间后清除，各故障位独立计时
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************/
bms_protect_fault_t bms_protect_hold(const bms_protect_fault_t *real)
{
    static uint32_t fault_detect_cnt[BMS_PROTECT_FAULT_NUM]  = {0U};
    static uint32_t fault_recover_cnt[BMS_PROTECT_FAULT_NUM] = {0U};
    static bms_protect_fault_t result = {0};

    uint8_t i;

    /* 输入参数无效。 */
    if (real == 0)
    {
        result.data = 0U;

        for (i = 0U; i < BMS_PROTECT_FAULT_NUM; i++)
        {
            fault_detect_cnt[i]  = 0U;
            fault_recover_cnt[i] = 0U;
        }

        return result;
    }


    for (i = 0U; i < BMS_PROTECT_FAULT_NUM; i++)
    {
        uint8_t result_bit = GETBIT(result.data, i);
        uint8_t real_bit   = GETBIT(real->data, i);

        /*
         * ================================================================
         * 实时故障存在
         * ================================================================
         */
        if (real_bit)
        {
            /* 故障存在时，恢复计数立即清零。 */
            fault_recover_cnt[i] = 0U;

            /*
             * 当前保持状态还没有故障：
             * 开始进行故障确认防抖。
             */
            if (!result_bit)
            {
                if (fault_detect_cnt[i] < BMS_PROTECT_DETECT_TIME_MS)
                {
                    fault_detect_cnt[i]++;
                }
                else
                {
                    SETBIT(result.data, i);

                    fault_detect_cnt[i] = 0U;
                }
            }
            else
            {
                /*
                 * 故障已经确认。
                 * 不再需要继续累计触发计数。
                 */
                fault_detect_cnt[i] = 0U;
            }
        }
        else
        {
            /* 无故障时，触发确认计数立即清零。 */
            fault_detect_cnt[i] = 0U;

            /*
             * result 里仍然保持着故障。
             */
            if (result_bit)
            {
                    /*
                     * 开始累计故障恢复时间。
                     */
                    if (fault_recover_cnt[i] < BMS_PROTECT_RECOVER_TIME_MS)
                    {
                        fault_recover_cnt[i]++;
                    }
                    else
                    {
                        CLRBIT(result.data, i);

                        fault_recover_cnt[i] = 0U;
                    }
            }
            else
            {
                /*
                 * real = 0
                 * result = 0
                 *
                 * 当前完全正常，所有计数清零。
                 */
                fault_recover_cnt[i] = 0U;
            }
        }
    }

    return result;
}


/***********************************************************************************************
//  Function   : bms_protect_check
//  Description: 更新 BMS 实时故障及故障保持状态
//  Param      : battery - 电池运行数据
//  Return     : None
//  Others     : 依次执行各保护判断，并更新实时故障及故障保持状态
//  Author     : Chen Liang 2026-10-16
***********************************************************************************************/
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

    real->bit.usbLow      = bms_protect_usbLow_check(battery->usb_in_v);          /* USB 输入欠压。 */
    real->bit.usbHigh     = bms_protect_usbHigh_check(battery->usb_in_v);         /* USB 输入过压。 */

    real->bit.tempHigh    = bms_protect_tempHigh_check(0);                        /* 主控板 NTC 过温。 */
    real->bit.currentHigh = bms_protect_currentHigh_check(battery->current_ma);   /* 电池充放电过流。 */


    /*
     * 将实时故障转换为保持故障。
     *
     * 即使实时故障刚刚消失，也会继续保持
     * BMS_PROTECT_HOLD_TIME_S 秒。
     */
    battery->fault_hold = bms_protect_hold(real);
}
