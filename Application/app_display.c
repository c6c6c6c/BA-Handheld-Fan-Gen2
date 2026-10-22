
/**
 ***********************************************************************************************************************
 * @file    app_display.c
 * @brief   AiP650E 显示应用层管理，实现数码管、指示灯、RGB 及显示刷新控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_display.h"

#include "timer_base.h"


/* Private variables -------------------------------------------------------------------------------------------------*/
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
            .brightness     = BRIGHTNESS_4_LEVEL,   /**< 亮度等级 4 */
            .sleeping       = false,                /**< 禁用睡眠 */
            .displayEnabled = true                  /**< 开启显示 */
        }
    },
    .show = {0xFFU, 0xFFU, 0xFFU, 0xFFU}        /**< COM1~COM4 初始显示数据 */
};

const uint8_t aip650e_seglist1[10] = {0x7E, 0x30, 0x6B, 0x79, 0x35, 0x5D, 0x5F, 0x38, 0x7F, 0x7D };
const uint8_t aip650e_seglist2[10] = {0x77, 0x30, 0x2F, 0x3B, 0x78, 0x5B, 0x5F, 0x31, 0x7F, 0x7B};

app_display_t display;

/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : app_display_Init
//  Description: 初始化 AiP650E 显示应用模块
//  Param      : None
//  Return     : true：初始化成功；false：芯片初始化或亮度配置失败
//  Others     : 清空显示缓存，初始化 AiP650E 并设置默认亮度
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_display_Init(void)
{
    /* 初始化 AiP650E 底层驱动。 */
    aip650_init();
	aip650_WriteControl(&aip650e.ctrl);
}



/*******************************************************************************
//  Function   : app_display_Init
//  Description: 初始化 AiP650E 显示应用模块
//  Param      : None
//  Return     : true：初始化成功；false：芯片初始化或亮度配置失败
//  Others     : 清空显示缓存，初始化 AiP650E 并设置默认亮度
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_display_convert(void)
{
	aip650e.show[0] = display.bit.showNum[1] > 9 ? 0 : aip650e_seglist1[display.bit.showNum[1]];
	aip650e.show[1] = display.bit.showNum[2] > 9 ? 0 : aip650e_seglist2[display.bit.showNum[2]];
	aip650e.show[3] = display.bit.showNum[0] == 1 ? 0x03 : 0;
	
	aip650e.show[2] = 0;

	aip650e.show[2] |= ((display.bit.mist != 0))           << 0;
	aip650e.show[2] |= ((display.bit.percentageIcon != 0)) << 1;
	aip650e.show[2] |= ((display.bit.percentageIcon != 0)) << 2;
	aip650e.show[2] |= ((display.bit.cooling != 0))        << 3;
	
}

/*******************************************************************************
//  Function   : app_display_task
//  Description: AiP650E 显示周期刷新及通信异常恢复
//  Param      : None
//  Return     : None
//  Others     : 正常情况下仅更新变化的显示数据； 300ms 后强制编写一次
//               重新写入控制寄存器及全部 COM 显示数据
//  Author     : Chen Liang 2026-10-22
*******************************************************************************/
void app_display_task(void)
{
    static timer_base_t _time = {0};
    uint8_t i;

   app_display_convert();
   aip650_write_segments(aip650e.show);

   if(aip650_ctrl_data() != aip650e.ctrl.data)
   {
        (void)aip650_WriteControl(&aip650e.ctrl);
   }

   if(timer_base_Count(&_time) > 1000U) 
    {
        (void)aip650_WriteControl(&aip650e.ctrl);

        for (i = 0U; i < AIP650E_COM_COUNT; i++)
        {
            aip650_write_digit(i, aip650e.show[i]);
        }
        timer_base_Rst(&_time);
    }
}


