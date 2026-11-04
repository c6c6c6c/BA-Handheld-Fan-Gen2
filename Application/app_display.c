
/**
 ***********************************************************************************************************************
 * @file    app_display.c
 * @brief   AiP650E 显示管理，实现风机档位、SOC 电量、OTA 状态及亮度控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 * @note
 *          1. APP_PROJECT 支持风机档位、SOC 电量及功能图标显示。
 *          2. BOOT_PROJECT 支持 OTA 升级状态显示。
 *          3. 通过段码表将数字和字符转换为 AiP650E 显示数据。
 *          4. 根据控制参数变化更新亮度及显示状态。
 *          5. 定时强制刷新显示 RAM，增强显示可靠性。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_display.h"
#include "timer_base.h"

#ifdef APP_PROJECT
#include "app_system.h"
#include "app_bms.h"
#include "button.h"
#endif

#ifdef BOOT_PROJECT
#include "param.h"
#endif


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** 数字显示无效索引，用于关闭对应数码管 */
#define APP_DISPLAY_NUM_BLANK          0xFFU

/** 显示寄存器强制刷新周期，单位 ms */
#define APP_DISPLAY_REFRESH_TIME_MS    1000U

/** OTA 升级状态显示切换周期，单位 ms */
#define APP_DISPLAY_BOOT_SWITCH_MS     500U


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief AiP650E 显示驱动实例
 * @note  默认使用七段显示模式、4 级亮度，并开启显示输出。
 */
aip650e_t aip650e =
{
    .ctrl =
    {
        .bit =
        {
            .segment        = AIP650_7_SEGMENT,     /**< 七段显示模式 */
            .brightness     = BRIGHTNESS_4_LEVEL,   /**< 默认亮度等级 */
            .sleeping       = false,                /**< 禁止睡眠 */
            .displayEnabled = true                  /**< 使能显示 */
        }
    },
    .show = {0xFFU, 0xFFU, 0xFFU, 0xFFU}            /**< COM1~COM4 显示缓存 */
};

/** 应用层显示状态，保存数字及功能图标信息 */
app_display_t display;


/* Private constants ------------------------------------------------------------------------------------------------*/

/**
 * @brief DIG1 数字及字符段码表
 * @note  索引 0~9 对应数字 0~9；
 *        索引 10~16 对应 F、U、P、d、n、E、r。
 */
static const uint8_t aip650e_seglist1[] =
{
    SEG1_A | SEG1_B | SEG1_C | SEG1_D | SEG1_E | SEG1_F,           /**< 0 */
    SEG1_B | SEG1_C,                                               /**< 1 */
    SEG1_A | SEG1_B | SEG1_D | SEG1_E | SEG1_G,                    /**< 2 */
    SEG1_A | SEG1_B | SEG1_C | SEG1_D | SEG1_G,                    /**< 3 */
    SEG1_B | SEG1_C | SEG1_F | SEG1_G,                             /**< 4 */
    SEG1_A | SEG1_C | SEG1_D | SEG1_F | SEG1_G,                    /**< 5 */
    SEG1_A | SEG1_C | SEG1_D | SEG1_E | SEG1_F | SEG1_G,           /**< 6 */
    SEG1_A | SEG1_B | SEG1_C,                                      /**< 7 */
    SEG1_A | SEG1_B | SEG1_C | SEG1_D | SEG1_E | SEG1_F | SEG1_G,  /**< 8 */
    SEG1_A | SEG1_B | SEG1_C | SEG1_D | SEG1_F | SEG1_G,           /**< 9 */
    SEG1_A | SEG1_F | SEG1_G | SEG1_E,                             /**< F */
    SEG1_B | SEG1_C | SEG1_D | SEG1_E | SEG1_F,                    /**< U */
    SEG1_A | SEG1_B | SEG1_E | SEG1_F | SEG1_G,                    /**< P */
    SEG1_B | SEG1_C | SEG1_D | SEG1_E | SEG1_G,                    /**< d */
    SEG1_C | SEG1_E | SEG1_G,                                      /**< n */
    SEG1_A | SEG1_D | SEG1_E | SEG1_F | SEG1_G,                    /**< E */
    SEG1_E | SEG1_G                                                /**< r */
};


/**
 * @brief DIG2 数字及字符段码表
 * @note  索引与 DIG1 段码表保持一致，
 *        使用 DIG2 对应的硬件段线映射。
 */
static const uint8_t aip650e_seglist2[] =
{
    SEG2_A | SEG2_B | SEG2_C | SEG2_D | SEG2_E | SEG2_F,           /**< 0 */
    SEG2_B | SEG2_C,                                               /**< 1 */
    SEG2_A | SEG2_B | SEG2_D | SEG2_E | SEG2_G,                    /**< 2 */
    SEG2_A | SEG2_B | SEG2_C | SEG2_D | SEG2_G,                    /**< 3 */
    SEG2_B | SEG2_C | SEG2_F | SEG2_G,                             /**< 4 */
    SEG2_A | SEG2_C | SEG2_D | SEG2_F | SEG2_G,                    /**< 5 */
    SEG2_A | SEG2_C | SEG2_D | SEG2_E | SEG2_F | SEG2_G,           /**< 6 */
    SEG2_A | SEG2_B | SEG2_C,                                      /**< 7 */
    SEG2_A | SEG2_B | SEG2_C | SEG2_D | SEG2_E | SEG2_F | SEG2_G,  /**< 8 */
    SEG2_A | SEG2_B | SEG2_C | SEG2_D | SEG2_F | SEG2_G,           /**< 9 */
    SEG2_A | SEG2_F | SEG2_G | SEG2_E,                             /**< F */
    SEG2_B | SEG2_C | SEG2_D | SEG2_E | SEG2_F,                    /**< U */
    SEG2_A | SEG2_B | SEG2_E | SEG2_F | SEG2_G,                    /**< P */
    SEG2_B | SEG2_C | SEG2_D | SEG2_E | SEG2_G,                    /**< d */
    SEG2_C | SEG2_E | SEG2_G,                                      /**< n */
    SEG2_A | SEG2_D | SEG2_E | SEG2_F | SEG2_G,                    /**< E */
    SEG2_E | SEG2_G                                                /**< r */
};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_display_dev
//  Description: 执行 AiP650E 底层显示刷新及控制状态同步
//  Param      : None
//  Return     : None
//  Others     : 正常情况下仅更新发生变化的显示数据和控制参数；
//               每隔 1000ms 强制重写控制字及全部显示 RAM
//  Author     : Chen Liang 2026-10-22
*******************************************************************************/
static void app_display_dev(void)
{
    static timer_base_t _time = {0};
    uint8_t i;

    /* 更新发生变化的 DIG 显示数据。 */
    (void)aip650_write_segments(aip650e.show);

    /* 同步显示控制参数。 */
    if (aip650_ctrl_data() != aip650e.ctrl.data)
    {
        (void)aip650_WriteControl(&aip650e.ctrl);
    }

    /* 周期性强制刷新全部显示数据。 */
    if (timer_base_Count(&_time) >= APP_DISPLAY_REFRESH_TIME_MS)
    {
        /* 重新写入显示控制寄存器。 */
        (void)aip650_WriteControl(&aip650e.ctrl);

        /* 重新写入全部 COM 显示缓存。 */
        for (i = 0U; i < AIP650E_COM_COUNT; i++)
        {
            (void)aip650_write_digit(i, aip650e.show[i]);
        }

        /* 重置强制刷新计时器。 */
        timer_base_Rst(&_time);
    }
}


/*******************************************************************************
//  Function   : app_display_convert
//  Description: 将应用层显示状态转换为 AiP650E 显示数据
//  Param      : None
//  Return     : None
//  Others     : DIG1、DIG2 根据索引查表生成数字或字符段码；
//               DIG3 生成雾化、百分比、风机及制冷图标数据；
//               DIG4 控制数字 100 的百位显示
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
static void app_display_convert(void)
{
    /* 转换 DIG1 段码，非法索引关闭对应数码管。 */
    aip650e.show[0] =
        display.bit.showNum[1] >= sizeof(aip650e_seglist1)
        ? 0U
        : aip650e_seglist1[display.bit.showNum[1]];

    /* 转换 DIG2 段码，非法索引关闭对应数码管。 */
    aip650e.show[1] =
        display.bit.showNum[2] >= sizeof(aip650e_seglist2)
        ? 0U
        : aip650e_seglist2[display.bit.showNum[2]];

    /* 控制百位数字 1 对应的独立笔段。 */
    aip650e.show[3] =
        display.bit.showNum[0] == 1U ? 0x03U : 0U;

    /* 组合 DIG3 功能图标。 */
    aip650e.show[2] = 0U;

    aip650e.show[2] |= ((display.bit.mist != 0U))           << 0;
    aip650e.show[2] |= ((display.bit.percentageIcon != 0U)) << 1;
    aip650e.show[2] |= ((display.bit.fanIcon != 0U))        << 2;
    aip650e.show[2] |= (display.bit.cooling & 0x07U)        << 3;
}




/*******************************************************************************
//  Function   : app_display_showNum
//  Description: 设置应用层数字显示内容
//  Param      : num       - 待显示数字，范围 0~100
//               IsDisplay - true：显示；false：关闭
//  Return     : None
//  Others     : 小于 10 时关闭十位；
//               等于 100 时显示独立百位数字 1；
//               数值无效或关闭显示时清除数字显示
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
static void app_display_showNum(uint8_t num, bool IsDisplay)
{
    if (IsDisplay && (num <= 100U))
    {
        /* 百位数字 1 显示控制。 */
        display.bit.showNum[0] = (num == 100U);

        /* 十位小于 10 时关闭显示。 */
        display.bit.showNum[1] = num >= 10U
                               ? (num / 10U) % 10U
                               : APP_DISPLAY_NUM_BLANK;

        /* 设置个位数字。 */
        display.bit.showNum[2] = num % 10U;
    }
    else
    {
        /* 清除数字显示。 */
        display.bit.showNum[0] = 0U;
        display.bit.showNum[1] = APP_DISPLAY_NUM_BLANK;
        display.bit.showNum[2] = APP_DISPLAY_NUM_BLANK;
    }
}

#ifdef APP_PROJECT
/*******************************************************************************
//  Function   : app_display_fanmode
//  Description: 设置风机档位显示内容
//  Param      : fanmode - 当前风机档位
//  Return     : None
//  Others     : DIG1 显示 F，DIG2 显示档位数字；
//               显示格式为 F0~F9
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
static void app_display_fanmode(uint8_t fanmode)
{
    /* 关闭独立百位。 */
    display.bit.showNum[0] = 0U;

    /* DIG1 显示字符 F。 */
    display.bit.showNum[1] = 10U;

    /* DIG2 显示当前档位。 */
    display.bit.showNum[2] = fanmode % 10U;
}


/*******************************************************************************
//  Function   : app_display_logic
//  Description: 根据 Application 运行状态更新显示内容及亮度
//  Param      : None
//  Return     : None
//  Others     : 开机时显示风机档位及功能图标；
//               风机或雾化按键长按时显示 SOC 百分比；
//               有显示操作时恢复亮度，10s 无操作后降低亮度
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
static void app_display_logic(void)
{
    static timer_base_t _timer = {0};
	static timer_base_t _timer_soc = {0};

    if (System.logic.power)
    {
        /* 更新风机及雾化功能图标。 */
        display.bit.percentageIcon = false;
        display.bit.fanIcon        = (System.logic.fan_mode != 0U);
        display.bit.mist           = (System.logic.mist_duty != 0U);

        /* 长按风机或雾化按键时显示当前 SOC。 */
        if ((botton_getEvent(BUTTON_ITEM_FAN) == BUTTON_EVENT_LONG_PRESS_HOLD) ||
            (botton_getEvent(BUTTON_ITEM_MIST) == BUTTON_EVENT_LONG_PRESS_HOLD))
        {
            /* 显示电池 SOC 百分比。 */
            app_display_showNum(bms_battery.soc_info.soc + 0.5,true);

            /* 进入 SOC 显示模式。 */
            System.logic.display_flg   = true;
            display.bit.percentageIcon = true;
            display.bit.fanIcon        = false;
        }
        else
        {
            /* 默认显示当前风机档位。 */
            if(System.logic.fan_mode > FAN_MODE_0 && System.logic.fan_mode < FAN_MODE_TYPE_NUM)
            {
                app_display_fanmode(FAN_MODE_TYPE_NUM - System.logic.fan_mode);
            }
            else
            {
                app_display_fanmode(FAN_MODE_0);
            }
        }
    }
    else
    {
        /* 设备关闭时清除显示内容。 */
        display.bit.percentageIcon = false;
        display.bit.fanIcon        = false;
        display.bit.mist           = false;

        app_display_showNum(0U, false);
    }

    /* 有显示操作时恢复亮度并重置计时器。 */
    if (System.logic.display_flg)
    {
        timer_base_Rst(&_timer);

        System.logic.display_flg = false;

        aip650e.ctrl.bit.brightness = BRIGHTNESS_4_LEVEL;
    }
    /* 无操作超过 10s 后降低显示亮度。 */
    else if (timer_base_Count(&_timer) > 10000U)
    {
        aip650e.ctrl.bit.brightness = BRIGHTNESS_1_LEVEL;
    }
	
	if(System.logic.power     == 0 
	&& System.logic.fan_mode  == FAN_MODE_0
	&& System.logic.mist_duty == 0
	&& System.logic.usb_present
	&& ((bms_battery.state == BAT_STATE_FULL) || (bms_battery.state == BAT_STATE_CHARGING)))
	{
		app_display_showNum(bms_battery.soc_info.soc + 0.5,true);
		display.bit.percentageIcon = true;
		
		if(bms_battery.soc_info.soc + 0.5  < 100.0)
		{
			if (timer_base_Count(&_timer_soc) < 500U)
			{
				app_display_showNum(0U, false);
			}
			else if (timer_base_Count(&_timer_soc) > 1000U)
			{
				timer_base_Rst(&_timer_soc);
			}
		}
			
		aip650e.ctrl.bit.brightness = BRIGHTNESS_1_LEVEL;
		
		System.logic.display_flg = true;
	}
	
	
}

#endif /* APP_PROJECT */


#ifdef BOOT_PROJECT

/*******************************************************************************
//  Function   : boot_display_logic
//  Description: 根据 OTA 升级状态更新 Bootloader 显示内容
//  Param      : None
//  Return     : None
//  Others     : OTA 开始时持续显示 UP；
//               OTA 编程中每隔 500ms 在 0% 和 UP 之间切换；
//               关闭其他功能图标，仅保留升级状态相关显示；
//               当前尚未定义 SUCCESS 等其他状态的显示内容
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
static void boot_display_logic(void)
{
    static timer_base_t _timer = {0};

    /* 关闭 Application 专用功能图标。 */
    display.bit.percentageIcon = false;
    display.bit.fanIcon        = false;
    display.bit.mist           = false;
    display.bit.cooling        = 0U;

    /* 根据 Flash 保存的 OTA 状态更新显示内容。 */
    switch (param_ota_state())
    {
        case OTC_STATUS_START:
        {
            /* OTA 开始：持续显示 UP。 */
            display.bit.showNum[0] = 0U;
            display.bit.showNum[1] = 11U;   /* U */
            display.bit.showNum[2] = 12U;   /* P */
            break;
        }

        case OTC_STATUS_PROGRAMMING:
        {
            /* 前 500ms：显示 0%。 */
            if (timer_base_Count(&_timer) < APP_DISPLAY_BOOT_SWITCH_MS)
            {
                app_display_showNum(22,true);

                display.bit.percentageIcon = true;
            }
            /* 后 500ms：显示 UP。 */
            else if (timer_base_Count(&_timer) <
                     (APP_DISPLAY_BOOT_SWITCH_MS * 2U))
            {
                display.bit.showNum[0] = 0U;
                display.bit.showNum[1] = 11U;
                display.bit.showNum[2] = 12U;
            }
            else
            {
                /* 一个显示周期结束，重新开始计时。 */
                timer_base_Rst(&_timer);
            }
            break;
        }

        default:
        {
            /* 其他 OTA 状态暂不修改数字显示内容。 */
            break;
        }
    }
	aip650e.ctrl.bit.brightness = BRIGHTNESS_3_LEVEL;
}

#endif /* BOOT_PROJECT */


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_display_Init
//  Description: 初始化 AiP650E 显示应用模块
//  Param      : None
//  Return     : None
//  Others     : 初始化底层两线通信及显示 RAM；
//               写入默认显示模式、亮度及显示使能配置
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_display_Init(void)
{
    /* 初始化 AiP650E 底层驱动。 */
    aip650_init();

    /* 写入默认显示控制参数。 */
    (void)aip650_WriteControl(&aip650e.ctrl);
}


/*******************************************************************************
//  Function   : app_display_task
//  Description: 执行 AiP650E 显示应用周期任务
//  Param      : None
//  Return     : None
//  Others     : 根据当前工程模式执行相应显示逻辑；
//               完成段码转换并同步至显示硬件
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
void app_display_task(void)
{
#ifdef APP_PROJECT
    /* Application 设备运行状态显示。 */
    app_display_logic();
#endif

#ifdef BOOT_PROJECT
    /* Bootloader OTA 升级状态显示。 */
    boot_display_logic();
#endif

    /* 将数字、字符及图标转换为段码。 */
    app_display_convert();

    /* 更新 AiP650E 显示数据及控制寄存器。 */
    app_display_dev();
}
