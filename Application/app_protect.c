
/**
 ***********************************************************************************************************************
 * @file    app_protect.c
 * @brief   APP 软件保护管理，实现电池、USB、温度、电流故障检测及故障状态保持
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_protect.h"
#include "app_bms.h"
#include "app_system.h"


/* Private variables -------------------------------------------------------------------------------------------------*/


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : protect_batteryLow_check
//  Description: 检查电池欠压保护条件
//  Param      : voltage_v - 当前电池电压，单位 V
//  Return     : true  - 电池电压达到欠压保护条件
//               false - 电池电压正常或欠压保护未启用
//  Others     : 当 PROTECT_BATTERY_LOW_V 大于 0 时启用检测；
//               电池电压小于或等于保护阈值时返回 true
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static bool protect_batteryLow_check(float voltage_v)
{
    return (PROTECT_BATTERY_LOW_V > 0.0f) &&
           (voltage_v <= PROTECT_BATTERY_LOW_V);
}


/*******************************************************************************
//  Function   : protect_batteryHigh_check
//  Description: 检查电池过压保护条件
//  Param      : voltage_v - 当前电池电压，单位 V
//  Return     : true  - 电池电压达到过压保护条件
//               false - 电池电压正常或过压保护未启用
//  Others     : 当 PROTECT_BATTERY_HIGH_V 大于 0 时启用检测；
//               电池电压大于或等于保护阈值时返回 true
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static bool protect_batteryHigh_check(float voltage_v)
{
    return (PROTECT_BATTERY_HIGH_V > 0.0f) &&
           (voltage_v >= PROTECT_BATTERY_HIGH_V);
}


/*******************************************************************************
//  Function   : protect_usbLow_check
//  Description: 检查 USB 输入欠压保护条件
//  Param      : usb_v - 当前 USB 输入电压
//  Return     : true  - 满足当前 USB 欠压检测表达式
//               false - 不满足检测表达式或保护未启用
//  Others     : 当 PROTECT_USB_LOW_V 大于 0 时启用检测；
//               当前代码使用 usb_v >= PROTECT_USB_LOW_V 判断；
//               该比较方向与通常的欠压判定相反，需单独确认；
//               usb_v 的实际单位应与保护阈值保持一致
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
static bool protect_usbLow_check(uint16_t usb_v)
{
    return (PROTECT_USB_LOW_V > 0U) &&
           (usb_v >= PROTECT_USB_LOW_V);
}


/*******************************************************************************
//  Function   : protect_usbHigh_check
//  Description: 检查 USB 输入过压保护条件
//  Param      : usb_v - 当前 USB 输入电压
//  Return     : true  - USB 输入电压达到过压保护条件
//               false - USB 输入电压正常或过压保护未启用
//  Others     : 当 PROTECT_USB_HIGH_V 大于 0 时启用检测；
//               USB 输入电压大于或等于阈值时返回 true；
//               usb_v 的实际单位应与保护阈值保持一致
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
static bool protect_usbHigh_check(uint16_t usb_v)
{
    return (PROTECT_USB_HIGH_V > 0U) &&
           (usb_v >= PROTECT_USB_HIGH_V);
}


/*******************************************************************************
//  Function   : protect_tempHigh_check
//  Description: 检查主控板 NTC 过温保护条件
//  Param      : temperature_deci_c - 当前温度，单位 0.1°C
//  Return     : true  - 当前温度达到过温保护条件
//               false - 当前温度正常或过温保护未启用
//  Others     : 当 PROTECT_TEMP_HIGH_DECI_C 大于 0 时启用检测；
//               当前温度大于或等于保护阈值时返回 true；
//               例如 600 表示 60.0°C
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static bool protect_tempHigh_check(int16_t temperature_deci_c)
{
    return (PROTECT_TEMP_HIGH_DECI_C > 0) &&
           (temperature_deci_c >= PROTECT_TEMP_HIGH_DECI_C);
}


/*******************************************************************************
//  Function   : protect_currentHigh_check
//  Description: 检查电池充放电过流保护条件
//  Param      : current_ma - 当前电池电流，单位 mA
//  Return     : true  - 电池电流绝对值达到过流保护条件
//               false - 电池电流正常或过流保护未启用
//  Others     : 当 PROTECT_CURRENT_HIGH_MA 大于 0 时启用检测；
//               不区分电流方向，统一使用绝对值进行比较；
//               电流绝对值大于或等于保护阈值时返回 true
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static bool protect_currentHigh_check(float current_ma)
{
    /* 充电为负、放电为正，过流保护只关注电流绝对值。 */
    float current_abs_ma = (current_ma < 0.0f) ? -current_ma : current_ma;

    return (PROTECT_CURRENT_HIGH_MA > 0.0f) &&
           (current_abs_ma >= PROTECT_CURRENT_HIGH_MA);
}


/*******************************************************************************
//  Function   : app_protect_hold
//  Description: 对实时故障执行触发防抖及恢复延时处理
//  Param      : real - 当前实时故障位域指针
//  Return     : 经过故障确认及恢复延时处理后的保持故障状态
//  Others     : 每个故障位独立维护触发计数及恢复计数；
//               实时故障持续满足检测条件后置位保持故障；
//               实时故障消失并持续达到恢复条件后清除保持故障；
//               故障重新出现时立即清零对应恢复计数；
//               实时故障消失时立即清零对应触发计数；
//               输入指针为空时清除所有故障状态及计数器；
//               时间参数按照函数实际调用次数累计
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
static protect_fault_t app_protect_hold(const protect_fault_t *real)
{
    /* 各故障位的连续触发确认计数器。 */
    static uint32_t fault_detect_cnt[PROTECT_FAULT_NUM] = {0U};

    /* 各故障位的连续恢复确认计数器。 */
    static uint32_t fault_recover_cnt[PROTECT_FAULT_NUM] = {0U};

    /* 经防抖及恢复延时处理后的保持故障位域。 */
    static protect_fault_t result = {0};

    uint8_t i;

    /*
     * 输入故障指针无效：
     *
     * 1. 清除所有保持故障。
     * 2. 清除各故障位的触发计数。
     * 3. 清除各故障位的恢复计数。
     */
    if (real == 0)
    {
        result.data = 0U;

        for (i = 0U; i < PROTECT_FAULT_NUM; i++)
        {
            fault_detect_cnt[i]  = 0U;
            fault_recover_cnt[i] = 0U;
        }

        return result;
    }

    /* 依次处理每个故障位，各故障位独立计时。 */
    for (i = 0U; i < PROTECT_FAULT_NUM; i++)
    {
        /* 获取当前保持故障状态及实时故障状态。 */
        uint8_t result_bit = GETBIT(result.data, i);
        uint8_t real_bit   = GETBIT(real->data, i);

        /*
         * ================================================================
         * 实时故障存在：执行故障触发确认
         * ================================================================
         */
        if (real_bit)
        {
            /* 故障重新出现时，立即清零恢复计数器。 */
            fault_recover_cnt[i] = 0U;

            /*
             * 当前故障尚未进入保持状态：
             *
             * 连续累计触发确认计数。
             * 达到配置的确认条件后置位保持故障。
             */
            if (!result_bit)
            {
                if (fault_detect_cnt[i] < PROTECT_DETECT_TIME_MS)
                {
                    fault_detect_cnt[i]++;
                }
                else
                {
                    /* 故障确认完成，置位对应保持故障。 */
                    SETBIT(result.data, i);

                    /* 清除该故障的触发确认计数。 */
                    fault_detect_cnt[i] = 0U;
                }
            }
            else
            {
                /*
                 * 当前故障已经进入保持状态。
                 *
                 * 无须继续累计触发确认次数。
                 */
                fault_detect_cnt[i] = 0U;
            }
        }
        /*
         * ================================================================
         * 实时故障消失：执行故障恢复确认
         * ================================================================
         */
        else
        {
            /* 实时故障消失后，立即清零触发确认计数。 */
            fault_detect_cnt[i] = 0U;

            /*
             * 保持故障仍处于置位状态：
             *
             * 开始累计故障恢复计数。
             * 达到恢复条件后清除对应保持故障位。
             */
            if (result_bit)
            {
                if (fault_recover_cnt[i] < PROTECT_RECOVER_TIME_MS)
                {
                    fault_recover_cnt[i]++;
                }
                else
                {
                    /* 故障恢复完成，清除对应保持故障。 */
                    CLRBIT(result.data, i);

                    /* 清除该故障的恢复确认计数。 */
                    fault_recover_cnt[i] = 0U;
                }
            }
            else
            {
                /*
                 * 实时故障及保持故障均不存在。
                 *
                 * 当前故障位处于正常状态，
                 * 清除恢复确认计数器。
                 */
                fault_recover_cnt[i] = 0U;
            }
        }
    }

    /* 返回最终保持故障状态。 */
    return result;
}


/*******************************************************************************
//  Function   : app_protect_check
//  Description: 检测 APP 实时故障并更新保持故障状态
//  Param      : None
//  Return     : None
//  Others     : 根据电池电压检测欠压及过压故障；
//               USB 接入时检测 USB 输入欠压及过压故障；
//               USB 未接入时清除对应实时 USB 故障；
//               根据主控板 NTC 温度检测过温故障；
//               根据电池电流绝对值检测过流故障；
//               最后调用 app_protect_hold() 更新保持故障状态
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
static void app_protect_check(void)
{
    protect_fault_t *real;

    /* 获取 USB 输入是否存在的硬件检测状态。 */
    bool usb_present = BMS_IO_USB_IS_PRESENT();

    /* 获取系统实时软件故障位域。 */
    real = &System.fault_real;

    /*
     * 电池电压保护检测：
     *
     * 根据当前电池电压，分别判断欠压和过压状态。
     * 保护阈值未启用时，对应检测结果为 false。
     */
    real->bit.batteryLow  = protect_batteryLow_check(bms_battery.voltage_v);
    real->bit.batteryHigh = protect_batteryHigh_check(bms_battery.voltage_v);

    /*
     * USB 输入电压保护检测：
     *
     * USB 已插入：
     *     执行 USB 欠压及过压检测。
     *
     * USB 未插入：
     *     清除 USB 相关实时故障，
     *     避免无 USB 输入时误判电压异常。
     */
    if (usb_present)
    {
        real->bit.usbLow  = protect_usbLow_check(bms_battery.usb_in_v);
        real->bit.usbHigh = protect_usbHigh_check(bms_battery.usb_in_v);
    }
    else
    {
        /* USB 未接入，清除对应实时故障。 */
        real->bit.usbLow  = 0;
        real->bit.usbHigh = 0;
    }

    /*
     * 主控板温度保护：
     *
     * 根据 System.ntc_temp 判断是否过温。
     * 温度单位应与 PROTECT_TEMP_HIGH_DECI_C 保持一致。
     */
    real->bit.tempHigh = protect_tempHigh_check(System.ntc_temp);

    /*
     * 电池过流保护：
     *
     * 使用电池实时电流的绝对值进行比较。
     * 充电和放电电流均参与保护检测。
     */
    real->bit.currentHigh = protect_currentHigh_check(bms_battery.current_ma);

    /*
     * 更新保持故障状态：
     *
     * 实时故障需要经过连续检测确认后才能置位；
     * 实时故障消失后，需要经过恢复延时才能清除。
     */
    System.fault_hold = app_protect_hold(real);
}


/*******************************************************************************
//  Function   : app_protect_logic
//  Description: 执行 APP 软件故障保护控制逻辑
//  Param      : None
//  Return     : None
//  Others     : 预留故障处理及保护联锁接口；
//               后续可根据实时故障和保持故障控制风扇、
//               雾化片、BMS 电源通路及其他受保护负载；
//               当前函数尚未实现具体保护动作
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
static void app_protect_logic(void)
{
	protect_fault_t *fault = &System.fault_hold;
	
	if(fault->bit.usbHigh || fault->bit.usbLow)
	{
		System.io_logic.bit.vin_ctrl = true;
	}
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_protect_task
//  Description: 执行 APP 软件保护检测及控制周期任务
//  Param      : None
//  Return     : None
//  Others     : 首先更新实时故障及保持故障状态；
//               然后执行故障保护控制逻辑；
//               调用周期将影响故障确认及恢复延时的实际时间；
//               当前保护控制函数尚未实现具体动作
//  Author     : Chen Liang 2026-10-16
*******************************************************************************/
void app_protect_task(void)
{
    /* 检测并更新实时故障和保持故障。 */
    app_protect_check();

    /* 根据故障状态执行应用层保护控制。 */
    app_protect_logic();
}
