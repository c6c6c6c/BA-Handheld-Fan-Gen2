/**
 ***********************************************************************************************************************
 * @file    modbus_register.h
 * @brief   Modbus holding register frame handler.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-19
 ***********************************************************************************************************************
 */
#ifndef __MODBUS_REGISTER_H
#define __MODBUS_REGISTER_H

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "modbus_queue.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Macro -------------------------------------------------------------------------------------------------------------*/

#define MODBUS_REGISTER_CRC_SIZE              2U    /* Modbus RTU帧尾CRC长度 */
#define MODBUS_REGISTER_READ_REQ_LEN          8U     /* 读多个保持寄存器请求帧长度 */
#define MODBUS_REGISTER_WRITE_RSP_LEN         8U     /* 写多个保持寄存器正常响应帧长度 */
#define MODBUS_REGISTER_EXCEPTION_RSP_LEN     5U     /* Modbus异常响应帧长度 */
#define MODBUS_REGISTER_EXCEPTION_MASK        0x80U  /* Modbus异常响应功能码掩码 */
#define MODBUS_RTU_MAX_REGISTERS              128

/* Exported functions prototypes --------------------------------------------------------------------------------------*/

#ifdef __cplusplus
}
#endif
/*******************************************************************************
//  Function   : ModbusRegister_BuildRequest
//  Description: 根据Modbus功能码构建对应的请求帧
//  Param      : pMessage    - Modbus消息指针
//               pTxBuffer   - 发送缓冲区
//               txBufferSize- 发送缓冲区大小
//               pTxLength   - 返回请求帧长度
//  Return     : Modbus RTU处理状态
//  Others     : 当前支持03和10功能码
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
modbus_err_t ModbusRegister_BuildRequest(const modbus_item_t *pMessage,
                                         uint8_t             *pTxBuffer,
                                         uint16_t            *pTxLength);
/*******************************************************************************
//  Function   : ModbusRegister_ParseResponse
//  Description: 根据Modbus功能码构建对应的请求帧
//  Param      : pMessage    - Modbus消息指针
//               pTxBuffer   - 发送缓冲区
//               txBufferSize- 发送缓冲区大小
//               pTxLength   - 返回请求帧长度
//  Return     : Modbus RTU处理状态
//  Others     : 当前支持03和10功能码
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
modbus_err_t ModbusRegister_ParseResponse(modbus_item_t *pMessage,
                                          uint8_t       *pRxBuffer,
                                          uint16_t       rxLength);
#endif /* __MODBUS_REGISTER_H */
