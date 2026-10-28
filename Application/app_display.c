
/**
 ***********************************************************************************************************************
 * @file    app_display.c
 * @brief   AiP650E 显示应用层管理，实现风机档位、SOC 电量、功能图标及亮度控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_display.h"
#include "app_system.h"
#include "app_bms.h"

#include "button.h"
#include "timer_base.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** 数字显示无效值，用于关闭对应数码管 */
#define APP_DISPLAY_NUM_BLANK          0xFFU

/** 显示寄存器周期强制刷新时间，单位 ms */
#define APP_DISPLAY_REFRESH_TIME_MS    1000U


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief AiP650E 显示驱动实例
 * @note  默认使用七段显示模式、4 级亮度，并开启显示输出
 */
aip650e_t aip650e =
{
    .ctrl =
    {
        .bit =
        {
            .segment        = AIP650_7_SEGMENT,     /**< 七段显示模式 */
            .brightness     = BRIGHTNESS_4_LEVEL,   /**< 默认亮度等级 4 */
            .sleeping       = false,                /**< 禁用睡眠模式 */
            .displayEnabled = true                  /**< 开启显示输出 */
        }
    },
    .show = {0xFFU, 0xFFU, 0xFFU, 0xFFU}            /**< COM1~COM4 初始显示数据 */
};

/** 应用层显示状态，保存数字及功能图标控制信息 */
app_display_t display;


/* Private constants -------------------------------------------------------------------------------------------------*/

/**
 * @brief DIG1 数字及字符段码表
 * @note  索引 0~9 对应数字，索引 10 对应字符 F；
 *        段码根据 DIG1 实际硬件段线映射生成。
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
    SEG1_A | SEG1_F | SEG1_G | SEG1_E                              /**< F */
};


/**
 * @brief DIG2 数字及字符段码表
 * @note  索引 0~9 对应数字，索引 10 对应字符 F；
 *        段码根据 DIG2 实际硬件段线映射生成。
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
    SEG2_A | SEG2_F | SEG2_G | SEG2_E                              /**< F */
};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_display_dev
//  Description: 执行 AiP650E 底层显示刷新及控制寄存器同步
//  Param      : None
//  Return     : None
//  Others     : 正常情况下仅更新发生变化的显示数据及控制参数；
//               每隔 1000ms 强制重写系统控制字及全部显示 RAM，
//               用于恢复芯片异常复位后可能丢失的显示配置
//  Author     : Chen Liang 2026-10-22
*******************************************************************************/
static void app_display_dev(void)
{
    static timer_base_t _time = {0};
    uint8_t i;

    /* 更新发生变化的 DIG 显示数据。 */
    (void)aip650_write_segments(aip650e.show);

    /* 控制参数变化时重新写入系统控制寄存器。 */
    if (aip650_ctrl_data() != aip650e.ctrl.data)
    {
        (void)aip650_WriteControl(&aip650e.ctrl);
    }

    /* 每隔 1000ms 强制刷新全部显示数据及控制寄存器。 */
    if (timer_base_Count(&_time) >= APP_DISPLAY_REFRESH_TIME_MS)
    {
        /* 重新写入显示模式、亮度及显示使能配置。 */
        (void)aip650_WriteControl(&aip650e.ctrl);

        /* 重新写入 DIG1~DIG4，避免芯片复位导致显示内容丢失。 */
        for (i = 0U; i < AIP650E_COM_COUNT; i++)
        {
            (void)aip650_write_digit(i, aip650e.show[i]);
        }

        /* 复位周期强制刷新计时器。 */
        timer_base_Rst(&_time);
    }
}


/*******************************************************************************
//  Function   : app_display_convert
//  Description: 将应用层显示状态转换为 AiP650E 显示 RAM 数据
//  Param      : None
//  Return     : None
//  Others     : DIG1、DIG2 根据数字或字符索引查表生成段码；
//               DIG3 根据雾化、风机、百分比及制冷状态组合图标数据；
//               DIG4 用于控制百位数字 1 的独立显示笔段
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
static void app_display_convert(void)
{
    /* DIG1、DIG2 根据显示索引转换为对应的数码管段码。 */
    aip650e.show[0] =
        display.bit.showNum[1] >= sizeof(aip650e_seglist1)
        ? 0U
        : aip650e_seglist1[display.bit.showNum[1]];

    aip650e.show[1] =
        display.bit.showNum[2] >= sizeof(aip650e_seglist2)
        ? 0U
        : aip650e_seglist2[display.bit.showNum[2]];

    /* 根据百位显示标志控制 DIG4 对应笔段。 */
    aip650e.show[3] =
        display.bit.showNum[0] == 1U ? 0x03U : 0U;

    /* 清空 DIG3 图标缓存，重新组合功能状态。 */
    aip650e.show[2] = 0U;

    aip650e.show[2] |= ((display.bit.mist != 0U))           << 0;  /* 雾化图标 */
    aip650e.show[2] |= ((display.bit.percentageIcon != 0U)) << 1;  /* 百分比图标 */
    aip650e.show[2] |= ((display.bit.fanIcon != 0U))        << 2;  /* 风机图标 */
    aip650e.show[2] |= (display.bit.cooling & 0x07U)        << 3;  /* 制冷状态 */
}


/*******************************************************************************
//  Function   : app_display_showNum
//  Description: 设置应用层数字显示内容
//  Param      : num       - 待显示数字，范围 0~100
//               IsDisplay - true：显示数字；false：关闭数字显示
//  Return     : None
//  Others     : 小于 10 时关闭十位，等于 100 时启用独立百位笔段；
//               数字超出范围或关闭显示时，将各位设置为无效显示值
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
static void app_display_showNum(uint8_t num, bool IsDisplay)
{
    if (IsDisplay && (num <= 100U))
    {
        /* 百位仅支持显示数字 1。 */
        display.bit.showNum[0] = (num == 100U);

        /* 十位不足两位数时关闭对应数码管。 */
        display.bit.showNum[1] = num >= 10U
                               ? (num / 10U) % 10U
                               : APP_DISPLAY_NUM_BLANK;

        /* 个位始终显示。 */
        display.bit.showNum[2] = num % 10U;
    }
    else
    {
        /* 关闭全部数字显示。 */
        display.bit.showNum[0] = 0U;
        display.bit.showNum[1] = APP_DISPLAY_NUM_BLANK;
        display.bit.showNum[2] = APP_DISPLAY_NUM_BLANK;
    }
}


/*******************************************************************************
//  Function   : app_display_fanmode
//  Description: 设置风机档位显示内容
//  Param      : fanmode - 当前风机档位
//  Return     : None
//  Others     : DIG1 显示字符 F，DIG2 显示档位数字；
//               最终显示形式为 F0~F9，实际有效档位由应用层控制
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
static void app_display_fanmode(uint8_t fanmode)
{
    /* 关闭独立百位显示。 */
    display.bit.showNum[0] = 0U;

    /* DIG1 显示字符 F，对应段码表索引 10。 */
    display.bit.showNum[1] = 10U;

    /* DIG2 显示当前风机档位。 */
    display.bit.showNum[2] = fanmode % 10U;
}


/*******************************************************************************
//  Function   : app_display_logic
//  Description: 根据设备运行状态更新应用层显示内容及亮度
//  Param      : None
//  Return     : None
//  Others     : 开机时显示风机档位及功能图标；
//               风机或雾化按键长按时切换为电池 SOC 百分比显示；
//               触发显示操作后提高亮度，无操作超过 10s 后降低亮度
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
static void app_display_logic(void)
{
    static timer_base_t _timer = {0};

    if (System.logic.power)
    {
        /* 根据设备运行状态更新功能图标。 */
        display.bit.percentageIcon = false;
        display.bit.fanIcon        = (System.logic.fan_mode  != 0U);
        display.bit.mist           = (System.logic.mist_duty != 0U);

        /*
         * 风机或雾化按键处于长按保持状态时，
         * 显示当前电池 SOC 百分比。
         */
        if ((botton_getEvent(BUTTON_ITEM_FAN)  == BUTTON_EVENT_LONG_PRESS_HOLD)
        ||  (botton_getEvent(BUTTON_ITEM_MIST) == BUTTON_EVENT_LONG_PRESS_HOLD))
        {
            /* 将 SOC 四舍五入后转换为整数百分比显示。 */
            app_display_showNum(bms_battery.soc_info.soc + 0.5, true);

            /* 标记显示操作并开启百分比图标。 */
            System.logic.display_flg   = true;
            display.bit.percentageIcon = true;
			display.bit.fanIcon        = false;
			display.bit.mist           = false;
        }
        else
        {
            /* 默认显示当前风机档位，格式为 F1~F5。 */
            app_display_fanmode(System.logic.fan_mode);
        }
    }
    else
    {
        /* 设备关机时清除数字及功能图标。 */
        display.bit.percentageIcon = false;
        display.bit.fanIcon        = false;
        display.bit.mist           = false;

        app_display_showNum(0U, false);
    }

    /*
     * 检测显示操作事件：
     * 有操作时恢复亮度等级 2，并重置空闲计时器；
     * 连续 10s 无显示操作时降低至亮度等级 1。
     */
    if (System.logic.display_flg)
    {
        timer_base_Rst(&_timer);

        System.logic.display_flg = false;

        aip650e.ctrl.bit.brightness = BRIGHTNESS_2_LEVEL;
    }
    else if (timer_base_Count(&_timer) > 10000U)
    {
        aip650e.ctrl.bit.brightness = BRIGHTNESS_1_LEVEL;
    }
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_display_Init
//  Description: 初始化 AiP650E 显示应用模块
//  Param      : None
//  Return     : None
//  Others     : 初始化底层两线通信及显示 RAM，
//               写入默认显示模式、亮度及显示使能参数
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_display_Init(void)
{
    /* 初始化 AiP650E 底层驱动。 */
    aip650_init();

    /* 写入默认显示控制字。 */
    (void)aip650_WriteControl(&aip650e.ctrl);
}


/*******************************************************************************
//  Function   : app_display_task
//  Description: 执行 AiP650E 显示应用周期任务
//  Param      : None
//  Return     : None
//  Others     : 依次处理设备显示逻辑、段码转换及硬件寄存器刷新
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
void app_display_task(void)
{
    app_display_logic();
    app_display_convert();
    app_display_dev();
}
