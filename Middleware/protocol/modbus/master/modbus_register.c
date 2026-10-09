/**
 ***********************************************************************************************************************
 * @file    modbus_register.c
 * @brief   Modbus holding register frame handler.
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-19
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "modbus_register.h"
#include "crc.h"

/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : ModbusRegister_AppendCrc
//  Description: crc检查
//  Param      : pItem - 已完成处理的消息项
//  Return     : 无
//  Others     : 应在 Modbus 响应处理完成或事务结束后调用
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
static uint16_t ModbusRegister_AppendCrc(uint8_t *pFrame, uint16_t lengthWithoutCrc)
{
    uint16_t crc = CRC_Calculate(pFrame, lengthWithoutCrc);

    pFrame[lengthWithoutCrc]      = (uint8_t)(crc & 0x00FFU);
    pFrame[lengthWithoutCrc + 1U] = (uint8_t)((crc >> 8U) & 0x00FFU);

    return (uint16_t)(lengthWithoutCrc + MODBUS_REGISTER_CRC_SIZE);
}

/*******************************************************************************
//  Function   : ModbusRegister_CheckCrc
//  Description: crc检查
//  Param      : pItem - 已完成处理的消息项
//  Return     : 无
//  Others     : 应在 Modbus 响应处理完成或事务结束后调用
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
static bool ModbusRegister_CheckCrc(const uint8_t *pFrame, uint16_t length)
{
    uint16_t crc;
    uint16_t frameCrc;

    if (length < MODBUS_REGISTER_CRC_SIZE)
    {
        return false;
    }

    crc = CRC_Calculate((uint8_t *)pFrame, (uint16_t)(length - MODBUS_REGISTER_CRC_SIZE));
    frameCrc = (uint16_t)pFrame[length - 2U] | ((uint16_t)pFrame[length - 1U] << 8U);

    return (crc == frameCrc) ? true : false;
}


//----------------------------------------------------------------------------------------------构建发送
/*******************************************************************************
//  Function   : ModbusRegister_BuildReadRequest
//  Description: 构建读保持寄存器请求帧
//  Param      : pMessage    - Modbus消息指针
//               pTxBuffer   - 发送缓冲区
//               txBufferSize- 发送缓冲区大小
//               pTxLength   - 返回请求帧长度
//  Return     : Modbus RTU处理状态
//  Others     : 无
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
static modbus_err_t ModbusRegister_BuildReadRequest(const modbus_item_t *pMessage,
                                                    uint8_t             *pTxBuffer,
                                                    uint16_t            *pTxLength)
{
    if (MODBUS_RTU_TX_BUFFER_SIZE < MODBUS_REGISTER_READ_REQ_LEN)
    {
        return MODBUS_ERR_PARAM;
    }

    pTxBuffer[0] = pMessage->slaveAddr;
    pTxBuffer[1] = pMessage->funcCode;
    pTxBuffer[2] = (uint8_t)(pMessage->startAddr >> 8U);
    pTxBuffer[3] = (uint8_t)(pMessage->startAddr & 0x00FFU);
    pTxBuffer[4] = (uint8_t)(pMessage->quantity  >> 8U);
    pTxBuffer[5] = (uint8_t)(pMessage->quantity  & 0x00FFU);

    *pTxLength = ModbusRegister_AppendCrc(pTxBuffer, 6U);

    return MODBUS_ERR_NONE;
}

/*******************************************************************************
//  Function   : ModbusRegister_BuildWriteRequest
//  Description: 构建写多个保持寄存器请求帧
//  Param      : pMessage    - Modbus消息指针
//               pTxBuffer   - 发送缓冲区
//               txBufferSize- 发送缓冲区大小
//               pTxLength   - 返回请求帧长度
//  Return     : Modbus RTU处理状态
//  Others     : 无
//  Author     : Leo Chen 2026-08-19
*******************************************************************************/
static modbus_err_t ModbusRegister_BuildWriteRequest(const modbus_item_t *pMessage,
                                                            uint8_t      *pTxBuffer,
                                                            uint16_t     *pTxLength)
{
    uint16_t byteCount;
    uint16_t frameLength;
	uint16_t idx;
    byteCount   = (uint16_t)(pMessage->quantity * 2U);
    frameLength = (uint16_t)(9U + byteCount);

    if (MODBUS_RTU_TX_BUFFER_SIZE < frameLength)
    {
        return MODBUS_ERR_PARAM;
    }

    pTxBuffer[0] = pMessage->slaveAddr;
    pTxBuffer[1] = pMessage->funcCode;
    pTxBuffer[2] = (uint8_t)(pMessage->startAddr >> 8U);
    pTxBuffer[3] = (uint8_t)(pMessage->startAddr & 0x00FFU);
    pTxBuffer[4] = (uint8_t)(pMessage->quantity  >> 8U);
    pTxBuffer[5] = (uint8_t)(pMessage->quantity  & 0x00FFU);
    pTxBuffer[6] = (uint8_t)byteCount;
	
	for (idx = 0U; idx < pMessage->quantity; idx++)
	{
		uint16_t val;
		pMessage->fill.func(pMessage->startAddr + idx,&val);
		pTxBuffer[7U + (idx * 2U)] = (uint8_t)(val >> 8U);
        pTxBuffer[8U + (idx * 2U)] = (uint8_t)(val & 0x00FFU);
	}
	
    *pTxLength = ModbusRegister_AppendCrc(pTxBuffer,(uint16_t)(7U + byteCount));

    return MODBUS_ERR_NONE;
}

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
                                         uint16_t            *pTxLength)
{
    if (pTxLength != 0)
    {
        *pTxLength = 0U;
    }

    if ((pTxBuffer           == 0)
    ||  (pTxLength           == 0)
    ||  (pMessage            == 0)
    ||  (pMessage            == 0)
    ||  (pMessage->fill.func == 0)
    ||  (pMessage->slaveAddr == 0U)
    ||  (pMessage->quantity  == 0U)
    ||  (pMessage->quantity > MODBUS_RTU_MAX_REGISTERS))
    {
        return MODBUS_ERR_PARAM;
    }

    switch (pMessage->funcCode)
    {
        //读取保持寄存器数据帧构建
        case MODBUS_FUNC_READ_HOLDING_REGISTERS:
        {
            return ModbusRegister_BuildReadRequest(pMessage,pTxBuffer,pTxLength);
        }
        //写入保持寄存器数据帧构建
        case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
        {
            return ModbusRegister_BuildWriteRequest(pMessage,pTxBuffer,pTxLength);
        }
    }
    return MODBUS_ERR_FUNC;

}
//----------------------------------------------------------------------------------------------接收解析
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
                                          uint16_t       rxLength)
{
	uint16_t idx;
    if ((pRxBuffer           == 0)
    ||  (pMessage            == 0)
    ||  (pMessage->fill.func == 0))
    {
        return MODBUS_ERR_PARAM;
    }

    //检测 CRC
    if(!ModbusRegister_CheckCrc(pRxBuffer, rxLength)) return MODBUS_ERR_CRC;


    if ((rxLength >= MODBUS_REGISTER_EXCEPTION_RSP_LEN) 
    &&  (pRxBuffer[0] == pMessage->slaveAddr)
    &&  (pRxBuffer[1] == (uint8_t)(pMessage->funcCode | MODBUS_REGISTER_EXCEPTION_MASK)))
    {
        return MODBUS_ERR_EXCEPTION;
    }

    //检测 地址和功能码
    if ((pRxBuffer[0] != pMessage->slaveAddr) || (pRxBuffer[1] != pMessage->funcCode))
    {
        return MODBUS_ERR_FRAME;
    }
 
    switch (pMessage->funcCode)
    {
        case MODBUS_FUNC_READ_HOLDING_REGISTERS:
        {
            if (pRxBuffer[2] != (uint8_t)(pMessage->quantity * 2U))
            {
                return MODBUS_ERR_FRAME;
            }
				
			for (idx = 0U; idx < pMessage->quantity; idx++)
            {
                uint16_t val = ((uint16_t)pRxBuffer[3U + (idx * 2U)] << 8U) |
                                (uint16_t)pRxBuffer[4U + (idx * 2U)];
				
				pMessage->fill.func(pMessage->startAddr + idx,&val);
            }
           
        }
        break;

        case MODBUS_FUNC_WRITE_MULTIPLE_REGISTERS:
        {
            if ((pRxBuffer[2] != (uint8_t)(pMessage->startAddr >> 8U)) 
            ||  (pRxBuffer[3] != (uint8_t)(pMessage->startAddr & 0x00FFU)) 
            ||  (pRxBuffer[4] != (uint8_t)(pMessage->quantity >> 8U)) 
            ||  (pRxBuffer[5] != (uint8_t)(pMessage->quantity & 0x00FFU)))
            {
                return MODBUS_ERR_FRAME;
            }
        }
        break;
        
        default:
        return MODBUS_ERR_FUNC;
    }
    return MODBUS_ERR_NONE;
}
