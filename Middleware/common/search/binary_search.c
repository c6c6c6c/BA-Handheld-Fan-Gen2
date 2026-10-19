/**
 ***********************************************************************************************************************
 * @file    binary_search.c
 * @brief   单调数组二分区间查找接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */
#include "binary_search.h"


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
                                                        uint16_t *pHighIndex)
{
    uint16_t low;
    uint16_t high;
    uint16_t middle;

    if ((pTable     == 0)
    ||  (pLowIndex  == 0)
    ||  (pHighIndex == 0)
    ||  (length < 2U))
    {
        return BINARY_SEARCH_STATUS_PARAM_ERROR;
    }

    if (target > pTable[0])
    {
        *pLowIndex  = 0U;
        *pHighIndex = 0U;
        return BINARY_SEARCH_STATUS_ABOVE_RANGE;
    }

    if (target < pTable[length - 1U])
    {
        *pLowIndex  = length - 1U;
        *pHighIndex = length - 1U;
        return BINARY_SEARCH_STATUS_BELOW_RANGE;
    }

    low  = 0U;
    high = length - 1U;

    while (low <= high)
    {
        middle = low + ((high - low) / 2U);

        if (target == pTable[middle])
        {
            *pLowIndex  = middle;
            *pHighIndex = middle;
            return BINARY_SEARCH_STATUS_OK;
        }

        if (target > pTable[middle])
        {
            if (middle == 0U)
            {
                break;
            }
            high = middle - 1U;
        }
        else
        {
            low = middle + 1U;
        }
    }

    /* 此时high位于较大数值端，low位于较小数值端。 */
    *pLowIndex  = high;
    *pHighIndex = low;

    return BINARY_SEARCH_STATUS_OK;
}
