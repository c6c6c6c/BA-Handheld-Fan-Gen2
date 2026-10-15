/**
 ***********************************************************************************************************************
 * @file    bms_ctrl.h
 * @brief   BMS 电池状态采集与控制接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_CTRL_H_
#define _BMS_CTRL_H_

#include "bms_config.h"
#include <stdbool.h>
#include <stdint.h>

/* Macro -------------------------------------------------------------------------------------------------------------*/

/** 每组 ADC 累计采样次数 */
#define SAMPLE_ACCUM_COUNT                       64U


/* 电池 ADC 换算参数 -------------------------------------------------------------------------------------------------*/


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 单组 ADC 累计采样缓冲区
 */
typedef struct
{
    volatile uint32_t sum1;                /**< 第一通道累计采样值 */
    volatile uint32_t sum2;                /**< 第二通道累计采样值 */
    volatile uint32_t count;               /**< 当前累计采样次数 */
    volatile bool     complete;            /**< 当前缓冲区采样完成标志 */
} sample_accum_buf_t;


/**
 * @brief 双缓冲 ADC 累计采样管理
 */
typedef struct
{
    sample_accum_buf_t buf[2];    /**< 双缓冲区，避免处理中断期间覆盖采样数据 */
    volatile uint8_t write_index; /**< 当前中断写入的缓冲区索引 */
} sample_accum_t;




/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_ctrl_ReadBatteryData
//  Description: 读取 ADC 采样并更新电池电压、电流及工作状态
//  Param      : data - 电池数据输出结构体，不可为 NULL
//  Return     : true：数据有效；false：参数错误或采样数据无效
//  Others     : 使用前须完成 ADC/PGA2 初始化并启动采样
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool bms_ctrl_ReadBatteryData(bms_battery_data_t *data);


#endif /* _BMS_CTRL_H_ */