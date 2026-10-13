/**
 ***********************************************************************************************************************
 * @file    queue.c
 * @brief   循环队列实现，提供队列初始化、清空、长度查询及数据入队/出队操作
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2024-01-25
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2024 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "queue.h"


/*******************************************************************************
//  Function   : QueueInit
//  Description: 初始化循环队列
//  Param      : queue - 队列对象
//               buf   - 队列数据缓冲区
//               len   - 缓冲区长度
//  Return     : None
//  Others     : 初始化时清空缓冲区，并将 Head/Tail 复位为 0
//  Author     : Su Jiawei 2024-01-25
*******************************************************************************/
void QueueInit(queue_t *queue, uint8_t *buf, uint16_t len)
{
    memset(buf, 0, len);

    queue->Buffer     = buf;
    queue->BufferSize = len;
    queue->Head       = 0;
    queue->Tail       = 0;
}


/*******************************************************************************
//  Function   : QueueClean
//  Description: 清空循环队列
//  Param      : queue - 队列对象
//  Return     : None
//  Others     : 仅复位 Head/Tail，不清除缓冲区中的原始数据
//  Author     : Su Jiawei 2024-01-25
*******************************************************************************/
void QueueClean(queue_t *queue)
{
    queue->Head = 0;
    queue->Tail = 0;
}


/*******************************************************************************
//  Function   : QueueLength
//  Description: 获取循环队列当前有效数据长度
//  Param      : queue - 队列对象
//  Return     : 当前未处理的数据字节数
//  Others     : 根据 Head/Tail 的相对位置计算有效数据长度
//  Author     : Su Jiawei 2024-01-25
*******************************************************************************/
uint16_t QueueLength(queue_t *queue)
{
    return ((queue->Tail >= queue->Head) ?
            (queue->Tail - queue->Head) :
            (queue->BufferSize - queue->Head + queue->Tail));
}


/*******************************************************************************
//  Function   : IsQueueEmpty
//  Description: 检查循环队列是否为空
//  Param      : queue - 队列对象
//  Return     : 1：队列为空；0：队列非空
//  Others     : Head == Tail 时表示队列为空
//  Author     : Su Jiawei 2024-01-25
*******************************************************************************/
uint8_t IsQueueEmpty(queue_t *queue)
{
    return (queue->Tail == queue->Head);
}


/*******************************************************************************
//  Function   : IsQueueFull
//  Description: 检查循环队列是否已满
//  Param      : queue - 队列对象
//  Return     : 1：队列已满；0：队列未满
//  Others     : 采用预留一个存储单元的方式区分队列满和队列空
//  Author     : Su Jiawei 2024-01-25
*******************************************************************************/
uint8_t IsQueueFull(queue_t *queue)
{
    return ((queue->Tail + 1 == queue->Head) ||
            ((queue->Tail + 1 == queue->BufferSize) && (queue->Head == 0)));
}


/*******************************************************************************
//  Function   : QueueDataIn
//  Description: 向循环队列写入数据
//  Param      : queue - 队列对象
//               buf   - 待写入数据缓冲区
//               len   - 请求写入的数据长度
//  Return     : 实际写入的数据长度
//  Others     : 当队列空间不足时停止写入，不覆盖尚未读取的数据
//  Author     : Su Jiawei 2024-01-25
*******************************************************************************/
uint16_t QueueDataIn(queue_t *queue, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint16_t temp;

    for (i = 0; i < len; i++)
    {
        /* 计算下一个写入位置，并处理环形回绕。 */
        temp = queue->Tail + 1;
        if (temp >= queue->BufferSize)
        {
            temp = 0;
        }

        /* 下一个位置与 Head 重合时表示队列已满。 */
        if (temp == queue->Head)
        {
            break;
        }

        queue->Buffer[queue->Tail] = *buf++;
        queue->Tail = temp;
    }

    return i;
}


/*******************************************************************************
//  Function   : QueueDataOut
//  Description: 从循环队列读取数据
//  Param      : queue - 队列对象
//               buf   - 接收数据缓冲区
//               len   - 请求读取的数据长度
//  Return     : 实际读取的数据长度
//  Others     : 当队列为空时停止读取
//  Author     : Su Jiawei 2024-01-25
*******************************************************************************/
uint16_t QueueDataOut(queue_t *queue, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        /* Head == Tail 表示队列中已经没有可读数据。 */
        if (queue->Head == queue->Tail)
        {
            break;
        }

        *buf++ = queue->Buffer[queue->Head];

        /* Head 到达缓冲区末尾后回绕到 0。 */
        if (++queue->Head >= queue->BufferSize)
        {
            queue->Head = 0;
        }
    }

    return i;
}