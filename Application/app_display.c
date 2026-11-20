
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
//  Description: 同步显示缓存与 AiP650E 控制状态
//  Param      : None
//  Return     : None
//  Others     : 优先写入显示段码与变化的控制参数；每 1000ms 强制重新写入控制寄存器及所有 COM 显示 RAM
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
//  Description: 将数字索引及功能图标转换为 AiP650E 段码
//  Param      : None
//  Return     : None
//  Others     : DIG1/DIG2 查表，非法索引写 0；DIG4 表示独立百位；DIG3 包含雾化、风机、百分比及制冷图标位
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
    aip650e.show[3] = display.bit.showNum[0] == 1U ? 0x03U : 0U;

    /* 组合 DIG3 功能图标。 */
    aip650e.show[2] = 0U;

    aip650e.show[2] |= ((display.bit.mist           != 0U)) << 0;
    aip650e.show[2] |= ((display.bit.fanIcon        != 0U)) << 1;
    aip650e.show[2] |= ((display.bit.percentageIcon != 0U)) << 2;
    aip650e.show[2] |= (display.bit.cooling & 0x07U)        << 3;
}




/*******************************************************************************
//  Function   : app_display_showNum
//  Description: 更新数码管数字显示缓存
//  Param      : num - 0～199 的显示数值；IsDisplay - 是否显示数字
//  Return     : None
//  Others     : num 为 uint8_t；实际允许显示 0～199；达到 100 时开启独立百位 1；小于 10 时隐藏十位；IsDisplay 为 false 时清空数字显示
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
static void app_display_showNum(uint8_t num, bool IsDisplay)
{
    if (IsDisplay && (num <= 199U))
    {
        /* 对 100～199 的数值开启独立百位数字 1。 */
        display.bit.showNum[0] = (num >= 100U);

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
//  Param      : fanmode - 风机显示档位
//  Return     : None
//  Others     : DIG1 显示 F，DIG2 显示 fanmode 对 10 取模的结果；关闭独立百位
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
//  Function   : power_off_charging_detection
//  Description: 检测关机状态下是否需要显示充电电量
//  Param      : None
//  Return     : true - 设备关机、风机与雾化关闭、USB 已连接且电池处于充电或充满状态；false - 不满足条件
//  Others     : 只负责判定显示条件，不直接修改电池状态或显示缓存
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
static bool power_off_charging_detection()
{
    if(System.logic.power     == 0 
    && System.logic.fan_mode  == FAN_MODE_0
    && System.logic.mist_duty == 0
    && System.logic.usb_present
    && ((bms_battery.state == BAT_STATE_FULL) || (bms_battery.state == BAT_STATE_CHARGING)))
    {
        return true;
    }
    return false;
}

/*******************************************************************************
//  Function   : power_off_low_battery_detection
//  Description: 检测开机状态下的低电量显示条件
//  Param      : None
//  Return     : true - 设备处于开机状态且 SOC 小于 10%；false - 不满足条件
//  Others     : 仅根据系统电源标志和 SOC 判断，不执行关机或电池保护
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
static bool power_off_low_battery_detection()
{
    if(System.logic.power == 1 && bms_battery.soc_info.soc < 10.5)
    {
        return true;
    }
    return false;
}

/*******************************************************************************
//  Function   : app_display_logic
//  Description: 根据设备状态更新 APP 数码管内容及亮度
//  Param      : None
//  Return     : None
//  Others     : 开机默认显示风机档位，按键长按时显示 SOC；关机充电或开机低电量时按定时器交替显示 SOC 和空白；有显示标志时亮度恢复至 4 级，超过 10s 无操作调至 1 级；充电/低电量提示分支最终将亮度设为 1 级
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
        if ((botton_getEvent(BUTTON_ITEM_FAN) == BUTTON_EVENT_LONG_PRESS_HOLD) 
		||  (botton_getEvent(BUTTON_ITEM_MIST) == BUTTON_EVENT_LONG_PRESS_HOLD))
        {
            /* 使用 SOC 四舍五入结果更新数字显示缓存。 */
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
        /* 设备关机时先清除显示内容；充电提示分支可在后面覆盖。 */
        display.bit.percentageIcon = false;
        display.bit.fanIcon        = false;
        display.bit.mist           = false;

        app_display_showNum(0U, false);
    }

    /* 有显示操作时恢复亮度并重置计时器。 */
    if (System.logic.display_flg)
    {
        timer_base_Rst(&_timer);

        System.logic.display_flg    = false;
        aip650e.ctrl.bit.brightness = BRIGHTNESS_4_LEVEL;
    }
    /* 无操作超过 10s 后降低显示亮度。 */
    else if (timer_base_Count(&_timer) > 10000U)
    {
        aip650e.ctrl.bit.brightness = BRIGHTNESS_1_LEVEL;
    }

	if(bms_battery.soc_Init == false)return;
	
    /*
     * 关机充电或开机低电量提示：
     * 先设置 SOC 数字和百分比图标；SOC 小于 100 时，
     * 计时器的前 500ms 关闭数字，后续阶段显示 SOC，形成闪烁。
     * 此分支同时将亮度设为 1 级，并持续设置显示刷新标志。
     */
    if(power_off_charging_detection() || power_off_low_battery_detection())
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

		if(!power_off_low_battery_detection())
		{
			aip650e.ctrl.bit.brightness = BRIGHTNESS_1_LEVEL;
		}
        
        System.logic.display_flg = true;
    }
	
	
}

#endif /* APP_PROJECT */


#ifdef BOOT_PROJECT

/*******************************************************************************
//  Function   : boot_display_logic
//  Description: 根据 OTA 状态更新 Bootloader 显示内容
//  Param      : None
//  Return     : None
//  Others     : START 状态显示 UP；PROGRAMMING 状态按 500ms 切换空白与 UP；其他状态保留已有数字内容；关闭 APP 图标并固定亮度为 3 级
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
            /* OTA START 状态：显示字符 UP。 */
            display.bit.showNum[0] = 0U;
            display.bit.showNum[1] = 11U;   /* U */
            display.bit.showNum[2] = 12U;   /* P */
            break;
        }

        case OTC_STATUS_PROGRAMMING:
        {
            /* 前 500ms：关闭数字及百分比图标（当前代码显示空白）。 */
            if (timer_base_Count(&_timer) < APP_DISPLAY_BOOT_SWITCH_MS)
            {
                app_display_showNum(0U, false);

                display.bit.percentageIcon = false;
            }
            /* 后 500ms：显示 UP，百分比图标保持前一阶段的关闭状态。 */
            else if (timer_base_Count(&_timer) < (APP_DISPLAY_BOOT_SWITCH_MS * 2U))
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

/*******************************************************************************
//  Function   : test_display_logic
//  Description: 测试
//  Param      : None
//  Return     : None
//  Others     : None
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
#if 0
static void test_display_logic(void)
{
    static timer_base_t _timer = {0};
    static int Cnt = 0;

    if(System.logic.power == 0)
    {
        display.bit.percentageIcon = false;
        display.bit.fanIcon        = false;
        display.bit.mist           = false;
        display.bit.cooling        = false;
        app_display_showNum(0U, false);
        timer_base_Rst(&_timer);
        Cnt = 0;
    }
    else
    {
        if(timer_base_Timing(&_timer, 200))
        {
            Cnt++;
            display.bit.percentageIcon = false;
            display.bit.fanIcon        = false;
            display.bit.mist           = false;
            display.bit.cooling        = false;
            app_display_showNum(0U, false);
            switch(Cnt)
            {
                case 0:case 1:case 2:
                break;
                case 3:case 4:case 5:case 6:case 7:case 8:case 9:case 10:case 11:case 12:
                    app_display_showNum((Cnt - 3) * 11, true);
                break;
                case 13:
                    app_display_showNum(100, true); 
                break;
                case 14:
                    display.bit.fanIcon = true;
                break;
                case 15:
                    display.bit.mist = true;
                break;
                case 16:
                    display.bit.percentageIcon = true;
                break;
                case 17:
                    display.bit.cooling = 1;
                break;
                case 18:
                    display.bit.cooling = 2;
                break;
                case 19:
                    display.bit.cooling = 4;
                break;      
                default:
                    Cnt = 0;
                break;         
            }

        }
    }
	
	app_display_showNum(188,true);
	display.bit.cooling = 7;
	display.bit.percentageIcon = true;
	display.bit.mist = true;
	display.bit.fanIcon = true;
}
#endif
/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_display_Init
//  Description: 初始化 AiP650E 显示应用模块
//  Param      : None
//  Return     : None
//  Others     : 初始化显示驱动并写入默认控制参数
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
//  Description: 执行显示应用层周期任务
//  Param      : None
//  Return     : None
//  Others     : 按编译工程选择 APP 或 BOOT 显示逻辑；随后转换段码并刷新硬件
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
