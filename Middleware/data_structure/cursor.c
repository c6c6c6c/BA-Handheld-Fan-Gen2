

/**
 ***********************************************************************************************************************
 * @file    cursor.c
 * @brief   数据缓冲区游标管理，实现数据写入、长度查询及游标复位
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-30
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "cursor.h"
#include <string.h>


/* Function implementations ------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : cursor_init
//  Description: 初始化数据缓冲区游标
//  Param      : cursor - 游标控制结构体
//               data   - 数据缓冲区首地址
//               length - 数据缓冲区容量，单位 Byte
//  Return     : None
//  Others     : 绑定目标缓冲区并将当前写入位置清零；
//               不清除缓冲区原有数据
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void cursor_init(cursor_t *cursor, uint8_t *data, uint16_t length)
{
    if (cursor == 0)
    {
        return;
    }

    cursor->data   = data;
    cursor->length = length;
    cursor->pin    = 0U;
}


/*******************************************************************************
//  Function   : cursor_write
//  Description: 向游标管理的缓冲区追加数据
//  Param      : cursor - 游标控制结构体
//               data   - 待写入数据首地址
//               length - 待写入的数据长度，单位 Byte
//  Return     : None
//  Others     : 写入前检查参数及缓冲区剩余容量；
//               写入成功后移动游标，空间不足时不写入数据；
//               不支持部分写入
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void cursor_write(cursor_t *cursor, void *data, uint16_t length)
{
    if ((cursor == 0) || (data == 0) || (cursor->data == 0))
    {
        return;
    }

    /* 检查游标合法性及缓冲区剩余容量，避免整数溢出和越界。 */
    if ((cursor->pin > cursor->length) ||
        (length > (uint16_t)(cursor->length - cursor->pin)))
    {
        return;
    }

    /* 将数据追加到当前游标位置。 */
    memcpy(&cursor->data[cursor->pin], data, length);

    /* 更新已写入的数据长度。 */
    cursor->pin += length;
}


/*******************************************************************************
//  Function   : cursor_read_len
//  Description: 获取当前游标位置
//  Param      : cursor - 游标控制结构体
//  Return     : 当前已写入的数据长度，单位 Byte；
//               参数无效时返回 0
//  Others     : 仅查询游标位置，不修改缓冲区及游标状态
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
uint16_t cursor_read_len(cursor_t *cursor)
{
    if (cursor != NULL)
    {
        return cursor->pin;
    }

    return 0U;
}


/*******************************************************************************
//  Function   : cursor_reset
//  Description: 复位数据缓冲区写入游标
//  Param      : cursor - 游标控制结构体
//  Return     : None
//  Others     : 仅将游标位置清零，不清除缓冲区中的原有数据
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void cursor_reset(cursor_t *cursor)
{
    if (cursor != NULL)
    {
        cursor->pin = 0U;
    }
}
