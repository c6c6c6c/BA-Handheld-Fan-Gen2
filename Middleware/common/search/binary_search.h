/**
 ***********************************************************************************************************************
 * @file    binary_search.h
 * @brief   单调数组二分区间查找接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */
#ifndef BINARY_SEARCH_H
#define BINARY_SEARCH_H

#include <stdint.h>

/**
 * @brief 二分查找执行状态
 */
typedef enum
{
    BINARY_SEARCH_STATUS_OK = 0,              // 查找到精确值或相邻区间
    BINARY_SEARCH_STATUS_PARAM_ERROR,         // 输入参数无效
    BINARY_SEARCH_STATUS_ABOVE_RANGE,         // 目标值大于数组首元素
    BINARY_SEARCH_STATUS_BELOW_RANGE          // 目标值小于数组末元素
} binary_search_status_t;

/*******************************************************************************
//  Function   : BinarySearch_U16Descending
//  Description: 在uint16_t单调递减数组中查找目标值所在的相邻区间
//  Param      : pTable-单调递减数组；length-数组长度；target-目标值
//               pLowIndex-较大数值端索引；pHighIndex-较小数值端索引
//  Return     : 二分查找执行状态
//  Others     : 精确匹配时两个输出索引相同；越界时索引钳位到对应端点
//  Author     : Leo Chen 2026-08-29
*******************************************************************************/
binary_search_status_t BinarySearch_U16Descending(const uint16_t *pTable,
                                                        uint16_t  length,
                                                        uint16_t  target,
                                                        uint16_t *pLowIndex,
                                                        uint16_t *pHighIndex);

#endif /* BINARY_SEARCH_H */
