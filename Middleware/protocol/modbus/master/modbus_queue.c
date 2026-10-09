/**
 ***********************************************************************************************************************
 * @file    modbus_queue.c
 * @brief   Modbus RTU master polling queue implementation.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-19
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "modbus_queue.h"

/* Private functions -------------------------------------------------------------------------------------------------*/


/* Exported functions ------------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Function   : ModbusQueue_Init
//  Description: 初始化 Modbus 主机轮询队列
//  Param      : pQueue - 轮询队列指针
//  Return     : 无
//  Others     : 初始化后所有消息槽位均处于禁用状态
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
void modbus_queue_Init(modbus_queue_t *pQueue)
{
    if (pQueue == 0)
    {
        return;
    }

    pQueue->hand = 0;
    pQueue->tail = 0;
    pQueue->user = 0;
}


/*******************************************************************************
//  Function   : ModbusQueue_Config
//  Description: 根据消息类型配置对应的 Modbus 轮询槽位参数
//  Param      : pQueue  - 轮询队列指针
//               type    - 消息类型
//               pConfig - 消息配置参数指针
//  Return     : 1-配置成功，0-参数错误
//  Others     : pData 的有效期必须覆盖消息轮询及回调处理过程
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
static bool modbus_identifier_filter(modbus_queue_t *pQueue , modbus_item_t item)
{
    uint8_t i;

    for(i = 0 ; i < pQueue->user ; i++)
    {
        uint8_t idx = (pQueue->hand + i) % MODBUS_ITEM_QUEUE_SIZE;
        if(pQueue->item[idx].fill.identifier == item.fill.identifier)
        return true;
    }
    return false;
}

/*******************************************************************************
//  Function   : ModbusQueue_Config
//  Description: 根据消息类型配置对应的 Modbus 轮询槽位参数
//  Param      : pQueue  - 轮询队列指针
//               type    - 消息类型
//               pConfig - 消息配置参数指针
//  Return     : 1-配置成功，0-参数错误
//  Others     : pData 的有效期必须覆盖消息轮询及回调处理过程
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
bool modbus_queue_add(modbus_queue_t *pQueue , modbus_item_t item)
{
    if(pQueue->user >= MODBUS_ITEM_QUEUE_SIZE)
    {
        return false;
    }
    if(!modbus_identifier_filter(pQueue,item))
    {
        pQueue->item[pQueue->hand] = item;
        pQueue->hand ++;
        pQueue->user ++;
        pQueue->hand %= MODBUS_ITEM_QUEUE_SIZE;
    }
    return true;
}

/*******************************************************************************
//  Function   : ModbusQueue_Config
//  Description: 根据消息类型配置对应的 Modbus 轮询槽位参数
//  Param      : pQueue  - 轮询队列指针
//               type    - 消息类型
//               pConfig - 消息配置参数指针
//  Return     : 1-配置成功，0-参数错误
//  Others     : pData 的有效期必须覆盖消息轮询及回调处理过程
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
modbus_item_t* modbus_queue_get(modbus_queue_t *pQueue)
{
    modbus_item_t* item = &pQueue->item[pQueue->tail];
    if(pQueue->user == 0)
    {
        return 0;
    }
    pQueue->tail ++;
    pQueue->user --;
    pQueue->tail %= MODBUS_ITEM_QUEUE_SIZE;
    return item;
}


