/**
 ***********************************************************************************************************************
 * @file    queue.h
 * @brief   
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2024-1-25
 ***********************************************************************************************************************
 * @copyright Copyright © 1999-2024 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _RING_QUEUE_H_
#define _RING_QUEUE_H_

#include <stdint.h>
#include <string.h>

/* Typedef ----------- -----------------------------------------------------------------------------------------------*/

typedef struct _QUEUE_STRUCT_TYPE
{
    uint8_t *Buffer;
    uint16_t BufferSize;
    uint16_t Head;
    uint16_t Tail;
} queue_t;

/* Function declaration ----------------------------------------------------------------------------------------------*/

void     QueueInit   (queue_t *queue, uint8_t *buf, uint16_t len);
void     QueueClean  (queue_t *queue);
uint16_t QueueLength (queue_t *queue);
uint8_t  IsQueueEmpty(queue_t *queue);
uint8_t  IsQueueFull (queue_t *queue);
uint16_t QueueDataIn (queue_t *queue, uint8_t *buf, uint16_t len);
uint16_t QueueDataOut(queue_t *queue, uint8_t *buf, uint16_t len);

#endif
