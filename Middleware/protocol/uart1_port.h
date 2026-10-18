/**
 ***********************************************************************************************************************
 * @file    uart1_port.h
 * @brief   UART1 通用物理端口接口
 ***********************************************************************************************************************
 */

#ifndef _UART1_PORT_H_
#define _UART1_PORT_H_

#include "common_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/** 每 1 ms 调用一次，用于接收帧间隔计时。 */
void uart1_port_1msTask(void);

/** UART1 物理端口接口，供上层协议调用。 */
extern const common_port_api_t s_uart1PortApi;

#ifdef __cplusplus
}
#endif

#endif /* _UART1_PORT_H_ */
