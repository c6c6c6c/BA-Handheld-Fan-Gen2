
/**
 ***********************************************************************************************************************
 * @file    app_display.c
 * @brief   AiP650E 显示应用层管理，实现显示逻辑、段码转换及硬件刷新
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_display.h"
#include "app_system.h"
#include "timer_base.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** 数字显示无效值，用于关闭对应数码管 */
#define APP_DISPLAY_NUM_BLANK          0xFFU

/** 显示寄存器周期强制刷新时间，单位 ms */
#define APP_DISPLAY_REFRESH_TIME_MS    1000U


/* Private variables -------------------------------------------------------------------------------------------------*/

/**
 * @brief AiP650E 显示驱动实例
 * @note  默认使用七段显示模式、4 级亮度，并开启显示输出
 */
static aip650e_t aip650e =
{
    .ctrl =
    {
        .bit =
        {
            .segment        = AIP650_7_SEGMENT,     /**< 七段显示模式 */
            .brightness     = BRIGHTNESS_4_LEVEL,   /**< 亮度等级 4 */
            .sleeping       = false,                /**< 禁用睡眠 */
            .displayEnabled = true                  /**< 开启显示 */
        }
    },
    .show = {0xFFU, 0xFFU, 0xFFU, 0xFFU}            /**< COM1~COM4 初始显示缓存 */
};

/** 应用层显示状态，保存数字及功能图标控制信息 */
app_display_t display;


/* Private constants -------------------------------------------------------------------------------------------------*/

/** DIG1 数字 0~9 对应的段码表 */
static const uint8_t aip650e_seglist1[10] =
{
    0x7EU, 0x30U, 0x6BU, 0x79U, 0x35U,
    0x5DU, 0x5FU, 0x38U, 0x7FU, 0x7DU
};

/** DIG2 数字 0~9 对应的段码表 */
static const uint8_t aip650e_seglist2[10] =
{
    0x77U, 0x30U, 0x2FU, 0x3BU, 0x78U,
    0x5BU, 0x5FU, 0x31U, 0x7FU, 0x7BU
};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_display_dev
//  Description: 执行 AiP650E 底层显示刷新及控制状态同步
//  Param      : None
//  Return     : None
//  Others     : 正常情况下仅更新变化的显示数据及控制参数；
//               每隔 1000ms 强制重新写入控制字及全部显示 RAM
//  Author     : Chen Liang 2026-10-22
*******************************************************************************/
static void app_display_dev(void)
{
    static timer_base_t _time = {0};
    uint8_t i;

    /* 更新发生变化的 DIG 显示数据。 */
    (void)aip650_write_segments(aip650e.show);

    /* 目标控制字发生变化时同步至 AiP650E。 */
    if (aip650_ctrl_data() != aip650e.ctrl.data)
    {
        (void)aip650_WriteControl(&aip650e.ctrl);
    }

    /* 每隔 1000ms 强制重新写入控制寄存器及全部显示 RAM。 */
    if (timer_base_Count(&_time) >= APP_DISPLAY_REFRESH_TIME_MS)
    {
        (void)aip650_WriteControl(&aip650e.ctrl);

        for (i = 0U; i < AIP650E_COM_COUNT; i++)
        {
            (void)aip650_write_digit(i, aip650e.show[i]);
        }

        timer_base_Rst(&_time);
    }
}


/*******************************************************************************
//  Function   : app_display_convert
//  Description: 将应用层显示状态转换为 AiP650E 显示 RAM 数据
//  Param      : None
//  Return     : None
//  Others     : 根据显示数字及功能图标状态生成 DIG1~DIG4 段码；
//               数字超出 0~9 范围时关闭对应数码管
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
static void app_display_convert(void)
{
    /* 将显示数字转换为对应的数码管段码。 */
    aip650e.show[0] = display.bit.showNum[1] > 9U
                    ? 0U
                    : aip650e_seglist1[display.bit.showNum[1]];

    aip650e.show[1] = display.bit.showNum[2] > 9U
                    ? 0U
                    : aip650e_seglist2[display.bit.showNum[2]];

    /* 控制百位数字 1 对应的显示笔段。 */
    aip650e.show[3] = display.bit.showNum[0] == 1U ? 0x03U : 0U;

    /* 根据功能状态组合 DIG3 指示灯及图标数据。 */
    aip650e.show[2] = 0U;

    aip650e.show[2] |= ((display.bit.mist != 0U))           << 0; /* 雾化指示 */
    aip650e.show[2] |= ((display.bit.percentageIcon != 0U)) << 1; /* 百分比图标位 1 */
    aip650e.show[2] |= ((display.bit.percentageIcon != 0U)) << 2; /* 百分比图标位 2 */
    aip650e.show[2] |= (display.bit.cooling & 0x07U)        << 3; /* 制冷状态三位编码 */
}

/*******************************************************************************
//  Function   : app_display_showNum
//  Description: 设置应用层数码管显示数值
//  Param      : num       - 待显示数值，范围 0~100
//               IsDisplay - true：显示数字；false：关闭数字显示
//  Return     : None
//  Others     : 小于 10 时关闭十位；等于 100 时使用独立百位笔段；
//               数字无效或关闭显示时，将各位设置为无效显示值
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
static void app_display_showNum(uint8_t num, bool IsDisplay)
{
    if (IsDisplay && (num <= 100U))
    {
        /* 百位仅支持数字 1。 */
        display.bit.showNum[0] = (num == 100U);

        /* 十位不足两位数时关闭显示。 */
        display.bit.showNum[1] = num >= 10U
                               ? (num / 10U) % 10U
                               : APP_DISPLAY_NUM_BLANK;

        /* 个位始终显示。 */
        display.bit.showNum[2] = num % 10U;
    }
    else
    {
        /* 关闭百位、十位和个位显示。 */
        display.bit.showNum[0] = 0U;
        display.bit.showNum[1] = APP_DISPLAY_NUM_BLANK;
        display.bit.showNum[2] = APP_DISPLAY_NUM_BLANK;
    }
}


/*******************************************************************************
//  Function   : app_display_logic
//  Description: 根据系统运行状态更新应用层显示内容
//  Param      : None
//  Return     : None
//  Others     : 系统开启时显示风机档位对应的百分比数值；
//               系统关闭时清除数字及相关功能图标
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
static void app_display_logic(void)
{
    if (System.dev.power)
    {
        /* 系统开启时显示百分比及风机图标。 */
        display.bit.percentageIcon = true;
        display.bit.fanIcon        = true;

        /* 风机档位每级对应 20，最大支持显示 100。 */
        app_display_showNum(System.dev.fan_dev * 20U, true);
    }
    else
    {
        /* 系统关闭时清除百分比、风机图标和数字显示。 */
        display.bit.percentageIcon = false;
        display.bit.fanIcon        = false;

        app_display_showNum(0U, false);
    }
}


/* Exported functions ------------------------------------------------------------------------------------------------*/


/*******************************************************************************
//  Function   : app_display_Init
//  Description: 初始化 AiP650E 显示应用模块
//  Param      : None
//  Return     : None
//  Others     : 初始化底层两线通信及显示 RAM，写入默认显示控制参数
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_display_Init(void)
{
    /* 初始化 AiP650E 底层通信及显示 RAM。 */
    aip650_init();

    /* 写入默认显示模式、亮度和显示使能配置。 */
    (void)aip650_WriteControl(&aip650e.ctrl);
}


/*******************************************************************************
//  Function   : app_display_task
//  Description: 执行 AiP650E 显示应用周期任务
//  Param      : None
//  Return     : None
//  Others     : 依次完成显示逻辑更新、段码转换及硬件刷新
//  Author     : Chen Liang 2026-10-23
*******************************************************************************/
void app_display_task(void)
{
    app_display_logic();
    app_display_convert();
    app_display_dev();
}
