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

#include "bms_status.h"
#include <string.h>


/* Private variables -------------------------------------------------------------------------------------------------*/

/** BMS 当前状态 */
bms_status_t s_status;


/*******************************************************************************
//  Function   : bms_status_detect
//  Description: 检测单路 LED 当前状态
//  Param      : det     - LED 检测数据
//               level   - 当前 GPIO 电平
//  Return     : LED 常亮、常灭、0.5Hz、1Hz 或 UNKNOWN
//  Others     : 每 2s 统计一次边沿数量；无边沿时根据当前电平判断常亮或常灭
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
static led_state_t bms_status_detect(led_detect_t *det, uint8_t level)
{
    /* 统计窗口内的 LED 跳变次数。 */
	if(level)
	{
		det->count = 100;
	}
	
	det->count = det->count == 0 ? det->count : det->count - 1;
	
	det->level = det->count != 0;
	
	
    if (det->level != det->last_level)
    {
		det->last_level = det->level;
		det->edge_count++;
    }

    if (timer_base_Timing(&det->_timer, BMS_STATUS_DETECT_WINDOW_MS))
    {
        if (det->edge_count == 0U)
        {
            det->state = (det->level != 0U) ? LED_STATE_ON : LED_STATE_OFF;
        }
        else if (det->edge_count > 2U)
        {
            det->state = LED_STATE_BLINK_1HZ;
        }
        else
        {
            det->state = LED_STATE_BLINK_0_5HZ;
        }
        det->edge_count = 0U;
    }

    return det->state;
}


/*******************************************************************************
//  Function   : bms_status_mode1_check
//  Description: 根据单灯模式识别 BMS 状态
//  Param      : IsCharge - 非 0：充电；0：放电
//  Return     : 当前识别到的 BMS 状态
//  Others     : 根据 LED1 状态区分充电、充满、放电及低电量
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
#if BMS_STATUS_LED_MODE == BMS_STATUS_MODE_1_LED
static ip5326_status_t bms_status_mode1_check(battery_state_t state)
{
    if (s_status.led1.state == LED_STATE_UNKNOWN)
    {
        return IP5326_STATUS_UNKNOWN;
    }

    if (s_status.led1.state == LED_STATE_OFF)
    {
        return IP5326_STATUS_IDLE;
    }

    if ((state == BAT_STATE_CHARGING) || (state == BAT_STATE_FULL))
    {
        if (s_status.led1.state == LED_STATE_BLINK_0_5HZ)
        {
            return IP5326_STATUS_CHARGING;
        }

        if (s_status.led1.state == LED_STATE_ON)
        {
            return IP5326_STATUS_FULL;
        }
    }
    else if (state == BAT_STATE_DISCHARGING)
    {
        if (s_status.led1.state == LED_STATE_BLINK_1HZ)
        {
            return IP5326_STATUS_LOW_BATTERY;
        }

        if (s_status.led1.state == LED_STATE_ON)
        {
            return IP5326_STATUS_DISCHARGING;
        }
    }

    return IP5326_STATUS_INVALID;
}
#endif

/*******************************************************************************
//  Function   : bms_status_mode2_check
//  Description: 根据双灯模式识别 BMS 状态
//  Param      : IsCharge - 非 0：充电；0：放电
//  Return     : 当前识别到的 BMS 状态
//  Others     : 根据两路 LED 状态组合判断充放电状态
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
#if BMS_STATUS_LED_MODE == BMS_STATUS_MODE_2_LED
static ip5326_status_t bms_status_mode2_check(battery_state_t state)
{
    if ((s_status.led1.state == LED_STATE_UNKNOWN) ||
        (s_status.led2.state == LED_STATE_UNKNOWN))
    {
        return IP5326_STATUS_UNKNOWN;
    }

    if ((s_status.led1.state == LED_STATE_OFF) && (s_status.led2.state == LED_STATE_OFF))
    {
        return IP5326_STATUS_IDLE;
    }
    else if (s_status.led2.state == LED_STATE_OFF)
    {
        if (s_status.led1.state == LED_STATE_BLINK_0_5HZ)
        {
            return IP5326_STATUS_CHARGING;
        }

        if (s_status.led1.state == LED_STATE_ON)
        {
            return IP5326_STATUS_FULL;
        }
    }
    else if(s_status.led1.state == LED_STATE_OFF)
    {
        if (s_status.led2.state == LED_STATE_BLINK_1HZ)
        {
            return IP5326_STATUS_LOW_BATTERY;
        }

        if (s_status.led2.state == LED_STATE_ON)
        {
            return IP5326_STATUS_DISCHARGING;
        }
    }

    return IP5326_STATUS_INVALID;
}
#endif

/*******************************************************************************
//  Function   : bms_status_mode4_check
//  Description: 根据四灯模式识别 BMS 状态
//  Param      : IsCharge - 非 0：充电；0：放电
//  Return     : 当前识别到的 BMS 状态
//  Others     : 根据 LED1~LED4 的组合状态判断充电、充满、放电及低电量
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
#if BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED
static ip5326_status_t bms_status_mode4_check(battery_state_t state)
{
    led_state_t led[4];
    uint8_t i;
    uint8_t j;
    bool matched;

    led[0] = s_status.led1.state;
    led[1] = s_status.led2.state;
    led[2] = s_status.led3.state;
    led[3] = s_status.led4.state;

    for (i = 0U; i < 4U; i++)
    {
        if (led[i] == LED_STATE_UNKNOWN)
        {
            return IP5326_STATUS_UNKNOWN;
        }
    }

    matched = true;
    for (i = 0U; i < 4U; i++)
    {
        matched = matched && (led[i] == LED_STATE_OFF);
    }

    if (matched)
    {
        return IP5326_STATUS_IDLE;
    }

    if ((state == BAT_STATE_CHARGING) || (state == BAT_STATE_FULL))
    {
        /* 四灯全亮表示充满。 */
        matched = true;
        for (i = 0U; i < 4U; i++)
        {
            matched = matched && (led[i] == LED_STATE_ON);
        }

        if (matched)
        {
            return IP5326_STATUS_FULL;
        }

        /* 充电中：前灯常亮，当前灯 0.5Hz 闪烁，后灯常灭。 */
        for (i = 0U; i < 4U; i++)
        {
            matched = (led[i] == LED_STATE_BLINK_0_5HZ);

            for (j = 0U; j < i; j++)
            {
                matched = matched && (led[j] == LED_STATE_ON);
            }

            for (j = i + 1U; j < 4U; j++)
            {
                matched = matched && (led[j] == LED_STATE_OFF);
            }

            if (matched)
            {
                return IP5326_STATUS_CHARGING;
            }
        }
    }
    else if (state == BAT_STATE_DISCHARGING)
    {
        /* LED1 1Hz 闪烁，其余熄灭表示低电量。 */
        if ((led[0] == LED_STATE_BLINK_1HZ) &&
            (led[1] == LED_STATE_OFF) &&
            (led[2] == LED_STATE_OFF) &&
            (led[3] == LED_STATE_OFF))
        {
            return IP5326_STATUS_LOW_BATTERY;
        }

        /* 正常放电：前 1~4 灯常亮，其余常灭。 */
        for (i = 1U; i <= 4U; i++)
        {
            matched = true;

            for (j = 0U; j < i; j++)
            {
                matched = matched && (led[j] == LED_STATE_ON);
            }

            for (j = i; j < 4U; j++)
            {
                matched = matched && (led[j] == LED_STATE_OFF);
            }

            if (matched)
            {
                return IP5326_STATUS_DISCHARGING;
            }
        }
    }

    return IP5326_STATUS_INVALID;
}
#endif


/*******************************************************************************
//  Function   : bms_status_check
//  Description: 更新 LED 状态并识别当前 BMS 工作状态
//  Param      : IsCharge - 非 0：充电；0：放电
//  Return     : None
//  Others     : 根据 BMS_STATUS_LED_MODE 选择 1 灯、2 灯或 4 灯识别逻辑
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_status_check(battery_state_t state)
{
#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_1_LED)

    s_status.led1.state = bms_status_detect(&s_status.led1, (uint8_t)BMS_IO_LED1_IS_HIGH());

    s_status.state = bms_status_mode1_check(state);

#elif (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_2_LED)

    s_status.led1.state = bms_status_detect(&s_status.led1, (uint8_t)BMS_IO_LED1_IS_HIGH());
    s_status.led2.state = bms_status_detect(&s_status.led2, (uint8_t)BMS_IO_LED2_IS_HIGH());

    s_status.state = bms_status_mode2_check(state);

#elif (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

    s_status.led1.state = bms_status_detect(&s_status.led1, (uint8_t)BMS_IO_LED1_IS_HIGH());
    s_status.led2.state = bms_status_detect(&s_status.led2, (uint8_t)BMS_IO_LED2_IS_HIGH());
    s_status.led3.state = bms_status_detect(&s_status.led3, (uint8_t)BMS_IO_LED3_IS_HIGH());
    s_status.led4.state = bms_status_detect(&s_status.led4, (uint8_t)BMS_IO_LED4_IS_HIGH());

    s_status.state = bms_status_mode4_check(state);

#endif
}


/*******************************************************************************
//  Function   : bms_status_GetStatus
//  Description: 获取当前 BMS 状态识别结果
//  Param      : None
//  Return     : 指向内部 bms_status_t 状态结构体的只读指针
//  Others     : 返回对象由模块内部维护，调用方不应修改
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
const bms_status_t *bms_status_GetStatus(void)
{
    return &s_status;
}
