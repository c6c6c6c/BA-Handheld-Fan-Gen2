/**
 ***********************************************************************************************************************
 * @file    queue.c
 * @brief   
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2024-1-25
 ***********************************************************************************************************************
 * @copyright Copyright © 1999-2024 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "queue.h"

/**
 * @brief  队列初始化
 * @param  queue : 要初始化的队列
 */
void QueueInit(queue_t *queue, uint8_t *buf, uint16_t len)
{
    memset(buf, 0, len);
    queue->Buffer     = buf;
    queue->BufferSize = len;
    queue->Head       = 0;
    queue->Tail       = 0;
}

/**
 * @brief  清空队列
 * @param  queue : 要执行清空操作的队列
 */
void QueueClean(queue_t *queue)
{
    queue->Head = 0;
    queue->Tail = 0;
}

/**
 * @brief  获取队列中的数据长度
 * @param  queue : 要检查的队列
 * @return 循环队列中未处理的数据长度
 */
uint16_t QueueLength(queue_t *queue)
{
    return ((queue->Tail >= queue->Head) ? (queue->Tail - queue->Head) : (queue->BufferSize - queue->Head + queue->Tail));
}

/**
 * @brief  检查队列是否为空
 * @param  queue : 要检查的队列
 * @return 1 : 循环队列为空; 0 : 循环队列不为空;
 */
uint8_t IsQueueEmpty(queue_t *queue)
{
    return (queue->Tail == queue->Head);
}

/**
 * @brief  检查队列是否已满
 * @param  queue : 要检查的队列
 * @return 1 : 循环队列已满; 0 ： 循环队列未满;
 */
uint8_t IsQueueFull(queue_t *queue)
{
    return ((queue->Tail + 1 == queue->Head) || ((queue->Tail + 1 == queue->BufferSize) && (queue->Head == 0)));
}

/**
 * @brief  数据入队操作
 * @param  queue : 要执行入队操作的队列
 * @param  buf : 要入队的数据所在的缓存区
 * @param  len : 要入队的数据长度
 * @return 实际入队的数据长度
 */
uint16_t QueueDataIn(queue_t *queue, uint8_t *buf, uint16_t len)
{
    uint16_t i;
    uint16_t temp;

    for (i = 0; i < len; i++)
    {
        temp = queue->Tail + 1;
        if (temp >= queue->BufferSize)
        {
            temp = 0;
        }
        if (temp == queue->Head)
        {
            break;
        }

        queue->Buffer[queue->Tail] = *buf++;
        queue->Tail = temp;
    }

    return i;
}

/**
 * @brief  数据出队操作
 * @param  queue : 要执行出队操作的队列
 * @param  buf : 存放出队值的缓存地址
 * @param  len : 要读出的数据长度
 * @return 实际读出的数据长度
 */
uint16_t QueueDataOut(queue_t *queue, uint8_t *buf, uint16_t len)
{
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        if (queue->Head == queue->Tail)
        {
            break;
        }

        *buf++ = queue->Buffer[queue->Head];
        if (++queue->Head >= queue->BufferSize)
        {
            queue->Head = 0;
        }
    }

    return i;
}
