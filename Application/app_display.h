/**
 ***********************************************************************************************************************
 * @file    app_display.h
 * @brief   AiP650E 显示应用层接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 */

#ifndef _APP_DISPLAY_H_
#define _APP_DISPLAY_H_

#include <stdbool.h>
#include <stdint.h>

#include "aip650e.h"
/* Macro -------------------------------------------------------------------------------------------------------------*/

/* Function declarations ---------------------------------------------------------------------------------------------*/
/**
 * @brief 显示状态结构体
 */
typedef union
{
	uint8_t data[AIP650E_COM_COUNT];
	struct
	{      
		uint8_t showNum[3];            /**< 显示数字 */
		uint8_t mist           : 1;    /**< 喷雾指示灯模式 */
		uint8_t cooling        : 3;    /**< 制冷指示灯模式 */
		uint8_t percentageIcon : 1;    /**< 百分比图标 */
		uint8_t fanIcon        : 1;    /**< 风扇图标 */
		uint8_t reserved       : 2;    /**< 保留 */
	}bit;
} app_display_t;


/** 初始化 AiP650E、清空显示，并设置应用默认亮度。 */
void app_display_Init(void);

/** 刷新待更新的显示数据，需在主循环或周期任务中调用。 */
void app_display_task(void);


#endif /* _APP_DISPLAY_H_ */
