/**
 ***********************************************************************************************************************
 * @file    modbus_item.h
 * @brief   Modbus RTU master polling queue.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-19
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */
#ifndef __MODBUS_QUEUE_H
#define __MODBUS_QUEUE_H

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include <stdbool.h>
#include <stdint.h>
#include "modbus_def.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Macro -------------------------------------------------------------------------------------------------------------*/
#define MODBUS_ITEM_QUEUE_SIZE  10
/* Typedef -----------------------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Description: Modbus 轮询消息类型，每种类型对应队列中的一个固定槽位
*******************************************************************************/

///* Modbus 轮询消息前置声明 */
typedef struct modbus_item modbus_item_t;

/*******************************************************************************
//  Description: Modbus 消息处理完成回调
//  Param      : pItem - 已完成处理的消息项
//  Return     : 无
*******************************************************************************/
typedef void (*modbus_finish_callback_t)(modbus_item_t *pItem);
typedef void (*modbus_fill_callback_t)  (uint16_t Addr,uint16_t *data);
/*******************************************************************************
//  Description: Modbus 轮询消息项
*******************************************************************************/
struct modbus_item
{
    uint8_t                    funcCode;       //Modbus 功能码
    uint8_t                    slaveAddr;      //从机地址
    uint16_t                   startAddr;      //寄存器起始地址
    uint16_t                   quantity;       //寄存器或线圈数量
    modbus_finish_callback_t   finish;
    union
    {
        uint32_t               identifier;     //唯一标识符
        modbus_fill_callback_t func;
    }fill;
};

/*******************************************************************************
//  Description: Modbus 轮询消息项
*******************************************************************************/
typedef struct 
{
    modbus_item_t item[MODBUS_ITEM_QUEUE_SIZE];
    uint8_t       hand;
    uint8_t       tail;
    uint8_t       user;
}modbus_queue_t;


/* Exported functions prototypes --------------------------------------------------------------------------------------*/
void modbus_queue_Init(modbus_queue_t *pQueue);
bool modbus_queue_add(modbus_queue_t *pQueue , modbus_item_t item);
modbus_item_t* modbus_queue_get(modbus_queue_t *pQueue);
#ifdef __cplusplus
}
#endif

#endif /* __MODBUS_QUEUE_H */

