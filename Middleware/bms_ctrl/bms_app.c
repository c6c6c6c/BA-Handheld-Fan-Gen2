/**
 ***********************************************************************************************************************
 * @file    bms_status.c
 * @brief   BMS 状态识别实现，完成 IP5326 LED 状态检测及 1/2/4 灯模式识别
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-14
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bms_app.h"
#include "bms.h"


bms_battery_t bms_battery = {0};

void bms_app_Init(void)
{
    bms_convert_Init();
    bms_key_Init();
}






void bms_app_1ms_task(void)
{
    double adcValue[BMS_ADC_NUM] = {0.0};
    bms_key_1ms_task();

    if(sample_accum_get(adcValue))
    {

        //获取电池电压、电流、USB输入电压
        bms_battery.voltage_v  = bms_battery_voltage(adcValue[BMS_ADC_VOLTAGE]);
        bms_battery.current_ma = bms_battery_current(adcValue[BMS_ADC_CURRENT], bms_battery.voltage_v);
        bms_battery.usb_in_v   = bms_usb_in_voltage(adcValue[BMS_ADC_USB_VOLTAGE]);

        //根据电池电流判断当前充放电状态
        bms_battery.state = bms_battery_state(bms_battery.current_ma);

        //执行 BMS 软件保护检查
        bms_protect_check(&bms_battery);

        //根据电池电压、电流、USB输入电压及保护状态计算 SOC、SOH、剩余容量等参数
        bms_calculate_soc_soh(&bms_battery);
    }
}