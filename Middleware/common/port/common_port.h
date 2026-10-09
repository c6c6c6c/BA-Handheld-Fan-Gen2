/**
 ***********************************************************************************************************************
 * @file    common_port.h
 * @brief   通用异步通信物理端口接口，提供不定长接收及环形缓冲发送的数据结构定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-09-02
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef __COMMON_PORT_H
#define __COMMON_PORT_H

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

/* Typedef -----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
 * @brief 通用端口接收状态
 *******************************************************************************/
typedef enum
{
    COMMON_PORT_RX_IDLE  = 0U,   // 接收空闲，当前没有完整数据帧
    COMMON_PORT_RX_DONE  = 1U,   // 一帧数据接收完成，等待上层处理
    COMMON_PORT_RX_ERROR = 2U,   // 接收发生错误，如缓冲区溢出或串口硬件错误
} common_port_rx_state_t;

/*******************************************************************************
 * @brief 通用端口发送状态
 *******************************************************************************/
typedef enum
{
    COMMON_PORT_TX_IDLE  = 0U,   // 发送空闲，发送环形缓冲区中没有待发送数据
    COMMON_PORT_TX_BUSY  = 1U,   // 正在发送，或发送缓冲区中仍有待发送数据
    COMMON_PORT_TX_ERROR = 2U,   // 发送发生错误
} common_port_tx_state_t;

/*******************************************************************************
 * @brief 通用物理端口操作接口
 *
 * 上层通信协议通过该接口访问具体物理端口，不直接依赖串口、SPI等
 * 底层驱动，从而实现通信协议层与硬件驱动层解耦。
 *******************************************************************************/
typedef struct
{
    bool (*Init)  (uint8_t *rx_buf, uint16_t rx_size,uint8_t *tx_buf, uint16_t tx_size); //初始化物理端port
    void (*Deinit)(void);                                                                //反初始化物理端port
    //接收接口
    common_port_rx_state_t (*receive_state) (void);                       //获取当前接收状态
    void *                 (*receive_data)  (void);                       //获取接收数据缓冲区地址
    uint16_t               (*receive_length)(void);                       //获取当前完整接收帧的数据长度
	void                   (*receive_rst)(void);                          //获取当前完整接收帧的数据长度
    //发送接口
    void*                  (*transfer_data)();                            //获取发送缓冲区地址
    bool                   (*start_transfer)(uint16_t length);            //启动发送指定长度数据
    common_port_tx_state_t (*transfer_state)(void);                       //获取当前发送状态

} common_port_api_t;

/*******************************************************************************
 * @brief 通用发送环形缓冲区控制结构
 *
 * 采用Head、Tail和Count管理发送数据：
 *
 * - Head：下一次写入数据的位置。
 * - Tail：下一次读取并发送数据的位置。
 * - Count：当前缓冲区中的有效数据数量。
 *******************************************************************************/
typedef struct
{
    uint8_t *Buffer;                    // 发送环形缓冲区首地址
    uint16_t BufferSize;                // 发送环形缓冲区总容量，单位为字节

    volatile uint16_t Count;              // 写入索引，指向下一次数据写入位置
    volatile uint16_t Length;           // 读取索引，指向下一次数据发送位置

    volatile common_port_tx_state_t State; // 当前发送状态
} common_port_tx_t;

/*******************************************************************************
 * @brief 通用不定长接收控制结构
 *
 * 接收过程中持续将数据写入Buffer，并在每次收到数据时清零TimerMs。
 * 当接收字节间隔达到设定的帧超时时间后，将当前数据判定为完整帧。
 *******************************************************************************/
typedef struct
{
    uint8_t *Buffer;                    // 接收缓冲区首地址
    uint16_t BufferSize;                // 接收缓冲区总容量，单位为字节

    volatile uint16_t Length;           // 当前已经接收的数据长度
    volatile uint16_t TimerMs;          // 接收字节间隔计时，单位为毫秒
    volatile uint8_t  Receiving;        // 接收过程标志：0-未接收，1-正在接收

    volatile common_port_rx_state_t State; // 当前接收状态
} common_port_rx_t;

/*******************************************************************************
 * @brief 通用不定长接收控制结构
 *
 * 接收过程中持续将数据写入Buffer，并在每次收到数据时清零TimerMs。
 * 当接收字节间隔达到设定的帧超时时间后，将当前数据判定为完整帧。
 *******************************************************************************/
typedef struct 
{
    uint8_t                  init;
    common_port_tx_t         tx;
    common_port_rx_t         rx;
    const common_port_api_t *api;
}common_port_t;

/* Exported functions ------------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Function   : common_port_Init
//  Description: 初始化通用端口对象、收发缓冲区及底层物理端口
//  Param      : port    - 通用端口对象
//               api     - 底层物理端口操作接口
//               rx_buf  - 接收缓冲区
//               rx_size - 接收缓冲区大小，单位 Byte
//               tx_buf  - 发送环形缓冲区
//               tx_size - 发送环形缓冲区大小，单位 Byte
//  Return     : None
//  Others     : 参数有效时会调用底层 Init 接口，并将收发状态置为空闲
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
bool common_port_Init(common_port_t *port,
                      uint8_t       *rx_buf,uint16_t rx_size,
                      uint8_t       *tx_buf,uint16_t tx_size);
/*******************************************************************************
//  Function   : common_port_Deinit
//  Description: 反初始化底层物理端口并清空通用端口运行状态
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 不释放用户传入的收发缓冲区
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void common_port_Deinit(common_port_t *port);

/*******************************************************************************
//  Function   : common_port_ReceiveState
//  Description: 获取通用端口当前接收状态
//  Param      : port - 通用端口对象
//  Return     : 当前接收状态
//  Others     : COMMON_PORT_RX_DONE 表示一帧不定长数据接收完成
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
common_port_rx_state_t common_port_ReceiveState(common_port_t *port);

/*******************************************************************************
//  Function   : common_port_ReceiveData
//  Description: 获取通用端口接收数据缓冲区地址
//  Param      : port - 通用端口对象
//  Return     : 接收缓冲区首地址
//  Others     : 建议仅在接收状态为 COMMON_PORT_RX_DONE 时读取完整帧
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void *common_port_ReceiveData(common_port_t *port);

/*******************************************************************************
//  Function   : common_port_ReceiveLength
//  Description: 获取通用端口当前接收数据长度
//  Param      : port - 通用端口对象
//  Return     : 接收数据长度，单位 Byte
//  Others     : 接收未完成时返回当前累计长度
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
uint16_t common_port_ReceiveLength(common_port_t *port);

/*******************************************************************************
//  Function   : common_port_ReceiveReset
//  Description: 清空通用端口接收状态，准备接收下一帧数据
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 上层处理完 COMMON_PORT_RX_DONE 或 COMMON_PORT_RX_ERROR 后调用
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void common_port_ReceiveReset(common_port_t *port);

/*******************************************************************************
//  Function   : common_port_TransferState
//  Description: 获取通用端口当前发送状态
//  Param      : port - 通用端口对象
//  Return     : 当前发送状态
//  Others     : 环形缓冲区发送完成后返回 COMMON_PORT_TX_IDLE
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
common_port_tx_state_t common_port_TransferState(common_port_t *port);

/*******************************************************************************
//  Function   : common_port_TransferState
//  Description: 获取通用端口当前发送状态
//  Param      : port - 通用端口对象
//  Return     : 当前发送状态
//  Others     : 环形缓冲区发送完成后返回 COMMON_PORT_TX_IDLE
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
uint8_t *common_port_TransferData(common_port_t *port);


/*******************************************************************************
//  Function   : common_port_Transfer
//  Description: 将待发送数据写入通用端口发送环形缓冲区
//  Param      : port   - 通用端口对象
//               pData  - 待发送数据指针
//               length - 待发送数据长度，单位 Byte
//  Return     : 1 - 写入成功
//               0 - 参数错误、未初始化或缓冲区空间不足
//  Others     : 发送空闲时会立即取出首字节并调用底层 Transfer 接口启动异步发送
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
bool common_port_Transfer(common_port_t *port,uint16_t length);


/*******************************************************************************
//  Function   : common_port_RxEventCallback
//  Description: 处理底层物理端口接收字节事件
//  Param      : port - 通用端口对象
//               data - 接收到的字节
//  Return     : None
//  Others     : 接收缓冲区满时进入 COMMON_PORT_RX_ERROR 状态
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void common_port_RxEventCallback(common_port_t *port, uint8_t data);

/*******************************************************************************
//  Function   : common_port_TxEventCallback
//  Description: 处理底层物理端口发送完成或发送寄存器空事件
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 环形缓冲区有数据时继续发送下一字节，否则切换为空闲状态
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
bool common_port_TxEventCallback(common_port_t *port , uint8_t *data);

/*******************************************************************************
//  Function   : common_port_1msTask
//  Description: 执行通用端口不定长接收帧间隔计时
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 应在 1ms 周期任务中调用；超时后将当前接收数据标记为完整帧
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
void common_port_1msTask(common_port_t *port);

/*******************************************************************************
//  Function   : common_port_api_check
//  Description: 检查通用端口底层物理接口是否完整
//  Param      : port - 通用端口对象
//  Return     : None
//  Others     : 应在 1ms 周期任务中调用；超时后将当前接收数据标记为完整帧
//  Author     : Leo Chen 2026-09-02
*******************************************************************************/
bool common_port_api_check(const common_port_api_t *api);
#ifdef __cplusplus
}
#endif

#endif /* __COMMON_PORT_H */
