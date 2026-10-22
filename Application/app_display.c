
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
#include "aip650e.h"


/* Private variables -------------------------------------------------------------------------------------------------*/

/** COM1~COM4 显示数据缓存，仅在内容变化时更新硬件 */
uint8_t s_displayData[AIP650E_COM_COUNT] = {0xff,0xff,0xff,0xff};

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
}


/*******************************************************************************
//  Function   : app_display_task
//  Description: AiP650E 显示周期刷新任务
//  Param      : None
//  Return     : None
//  Others     : 检查显示缓存是否变化，仅在有待更新数据时执行硬件刷新
//  Author     : Chen Liang 2026-10-21
*******************************************************************************/
void app_display_task(void)
{
	aip650e_ctrl_t ctrl;
    ctrl.segment        = AIP650_7_SEGMENT;
    ctrl.brightness     = BRIGHTNESS_4_LEVEL;
    ctrl.sleeping       = false;
    ctrl.displayEnabled = true;

    aip650_WriteControl(&ctrl);
    aip650_write_segments(s_displayData);
}

