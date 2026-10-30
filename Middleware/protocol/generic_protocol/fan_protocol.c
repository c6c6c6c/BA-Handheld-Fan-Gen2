
/**
 ***********************************************************************************************************************
 * @file    fan_protocol.c
 * @brief   风扇通信协议管理，实现 UART1 数据收发、请求解析、CRC16 校验及协议应答
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-30
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_system.h"
#include "app_bms.h"

#include "fan_protocol.h"
#include "uart1_port.h"
#include "button.h"
#include "crc.h"
#include "cursor.h"


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** 风扇通信协议实例，保存收发缓冲区、通信接口及当前请求和应答命令 */
fan_protocol_t protocol = {0};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_protocol_request
//  Description: 解析当前协议请求并生成对应的应答命令
//  Param      : data - 接收到的协议数据区首地址
//  Return     : None
//  Others     : 根据 protocol.request 判断请求命令是否合法；
//               对支持的查询命令清除最高位，生成对应的应答命令；
//               不支持的命令统一设置为 PROTO_ERR；
//               当前查询命令暂未使用 data 参数
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
static void fan_protocol_request(uint8_t *data)
{
    /* 当前查询命令暂不需要解析数据区。 */
    (void)data;

    /* 根据请求命令确定应答类型。 */
    switch ((uint8_t)protocol.request)
    {
        case PROTO_INFO_REQUEST:
        case PROTO_SYSTEM_REQUEST:
        case PROTO_BATTERY_INFO_REQUEST:
        {
            /* 清除请求命令最高位，转换为应答命令。 */
            protocol.response = (proto_cmd_t)
                                ((uint8_t)protocol.request & 0x7FU);
            break;
        }

        default:
        {
            /* 未支持的请求命令，生成错误应答。 */
            protocol.response = PROTO_ERR;
            break;
        }
    }
}


/*******************************************************************************
//  Function   : fan_protocol_response
//  Description: 根据应答命令生成并发送协议数据帧
//  Param      : None
//  Return     : None
//  Others     : 使用 Cursor 宏依次写入帧头、应答命令及数据区；
//               组帧完成后回填数据区长度，计算并追加 CRC16；
//               最后启动 UART1 发送，并复位应答命令；
//               发送缓冲区容量和发送状态需由底层接口保证
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
static void fan_protocol_response(void)
{
    uint16_t send_len = 0U;
    uint16_t crc;

    /* 检查通信接口、发送缓冲区及应答命令。 */
    if ((protocol.api == 0) ||
        (protocol.api->transfer_data() == 0) ||
        (protocol.response == PROTO_IDLE))
    {
        return;
    }

    /* 启动发送缓冲区游标。 */
    _START_CURSOR_WRITE(protocol.api->transfer_data(), send_len)

        /* 写入协议帧头。 */
        _CURSOR_WRITE_BYTE(PROTO_SOF1);
        _CURSOR_WRITE_BYTE(PROTO_SOF2);

        /* 写入当前应答命令。 */
        _CURSOR_WRITE_BYTE(protocol.response);

        /* 预留协议数据区长度字段。 */
        _CURSOR_WRITE_BYTE_NULL();

        /* 根据应答命令生成对应的数据区。 */
        switch ((uint8_t)protocol.response)
        {
            case PROTO_INFO_RESPONSE:
            {
                /* 通信协议版本。 */
                _CURSOR_WRITE_BYTE(PROTO_VERSION);

                /* MCU 唯一设备标识的第一个元素。 */
                _CURSOR_WRITE_VAR(System.unique_id);

                /* 固件编译日期。 */
                _CURSOR_WRITE_VAR(System.compile_param.year);
                _CURSOR_WRITE_VAR(System.compile_param.month);
                _CURSOR_WRITE_VAR(System.compile_param.day);
                break;
            }

            case PROTO_SYSTEM_RESPONSE:
            {
                /* 电池充放电状态。 */
                _CURSOR_WRITE_BYTE(bms_battery.state);

                /* 电池实时采样数据。 */
                _CURSOR_WRITE_VAR(bms_battery.usb_in_v);
                _CURSOR_WRITE_VAR(bms_battery.voltage_v);
                _CURSOR_WRITE_VAR(bms_battery.current_ma);

                /* 电池 SOC、SOH 及累计电量统计。 */
                _CURSOR_WRITE_VAR(bms_battery.soc_info.cycle_count);
                _CURSOR_WRITE_VAR(bms_battery.soc_info.soc);
                _CURSOR_WRITE_VAR(bms_battery.soc_info.static_soc);
                _CURSOR_WRITE_VAR(bms_battery.soc_info.soh);
                _CURSOR_WRITE_VAR(bms_battery.soc_info.remaining_capacity_mah);
                _CURSOR_WRITE_VAR(bms_battery.soc_info.charge_mah_total);
                _CURSOR_WRITE_VAR(bms_battery.soc_info.discharge_mah_total);

                /* 电池实时故障及锁存故障。 */
                _CURSOR_WRITE_VAR(bms_battery.fault_real.data);
                _CURSOR_WRITE_VAR(bms_battery.fault_hold.data);

                /* 系统电源及设备运行模式。 */
                _CURSOR_WRITE_BYTE(System.logic.power);
                _CURSOR_WRITE_BYTE(System.logic.fan_mode);
                _CURSOR_WRITE_BYTE(System.logic.mist_mode);

                /* 雾化片 PWM 工作参数。 */
                _CURSOR_WRITE_VAR(System.logic.mist_freq);
                _CURSOR_WRITE_VAR(System.logic.mist_duty);

                /* 系统运行时间统计。 */
                _CURSOR_WRITE_VAR(System.logic.onTimes);
                _CURSOR_WRITE_VAR(System.logic.offTimes);
                _CURSOR_WRITE_VAR(System.logic.mistTimes);

                /* 当前按键状态。 */
                _CURSOR_WRITE_BYTE(botton_getValue());
                break;
            }

            case PROTO_BATTERY_INFO_RESPONSE:
            {
                /* 检查电池参数配置是否有效。 */
                if (bms_battery.configInfo != 0)
                {
                    /* 电池设计容量。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->design_capacity_mah);

                    /* 电池充满及放电截止电压。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->full_voltage_v);
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->cutoff_voltage_v);

                    /* 电池静置判断参数。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->rest_current_ma);
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->rest_dvdt_v_per_s);

                    /* 电池充放电效率系数。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->charge_efficiency);
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->discharge_efficiency);

                    /* 电池静置时间阈值。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->rest_time_ms);
                }
                break;
            }

            default:
            {
                /* 未定义应答命令不附加数据区。 */
                break;
            }
        }

        /* 回填数据区长度，不包含 CRC16。 */
        _CURSOR_WRITE_BYTE_SPECIFY(PROTO_LEN_INDEX,send_len - PROTO_DATA_INDEX);

        /* 计算协议帧 CRC16。 */
        crc = CRC_Calculate(protocol.api->transfer_data(), send_len);

        /* 将 CRC16 追加至数据帧末尾。 */
        _CURSOR_WRITE_VAR(crc);

    /* 结束游标组帧。 */
    _STOP_CURSOR();

    /* 启动 UART1 数据发送。 */
    protocol.api->start_transfer(send_len);

    /* 清除当前待应答命令。 */
    protocol.response = PROTO_IDLE;
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_protocol_Init
//  Description: 初始化风扇通信协议管理模块
//  Param      : None
//  Return     : None
//  Others     : 绑定 UART1 通信接口；
//               初始化协议接收和发送缓冲区；
//               将请求及应答命令初始化为 PROTO_IDLE
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void fan_protocol_Init(void)
{
    /* 绑定 UART1 通信接口。 */
    protocol.api = &s_uart1PortApi;

    /* 初始化 UART1 接收及发送缓冲区。 */
    protocol.api->Init(protocol.RxDat,PROTO_MAX_FRAME_LEN,
                       protocol.TxDat,PROTO_MAX_FRAME_LEN);

    /* 初始化协议命令状态。 */
    protocol.request  = PROTO_IDLE;
    protocol.response = PROTO_IDLE;
}


/*******************************************************************************
//  Function   : fan_protocol_Deinit
//  Description: 反初始化风扇通信协议管理模块
//  Param      : None
//  Return     : None
//  Others     : 关闭 UART1 通信接口并解除绑定；
//               不清除原有协议收发缓冲区数据
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void fan_protocol_Deinit(void)
{
    /* 检查通信接口是否已经绑定。 */
    if (protocol.api == 0)
    {
        return;
    }

    /* 反初始化 UART1 通信接口。 */
    protocol.api->Deinit();

    /* 解除通信接口绑定。 */
    protocol.api = 0;

    /* 清除协议命令状态。 */
    protocol.request  = PROTO_IDLE;
    protocol.response = PROTO_IDLE;
}


/*******************************************************************************
//  Function   : fan_protocol_1ms_task
//  Description: 执行风扇通信协议 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 推进 UART1 通信状态机；
//               检测完整接收帧并检查长度、帧头和 CRC16；
//               校验成功后解析请求并生成对应应答；
//               校验失败时生成错误应答；
//               完成应答处理后复位 UART1 接收状态
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void fan_protocol_1ms_task(void)
{
    uint8_t *rx_data;
    uint16_t rx_len;

    /* 检查通信接口是否已初始化。 */
    if (protocol.api == 0)
    {
        return;
    }

    /* 推进 UART1 通信状态机。 */
    uart1_port_1msTask();

    /* 检查是否接收到完整协议帧。 */
    if (protocol.api->receive_state() == COMMON_PORT_RX_DONE)
    {
        /* 获取接收缓冲区及接收长度。 */
        rx_data = protocol.api->receive_data();
        rx_len  = protocol.api->receive_length();

        /*
         * 执行协议帧有效性检查：
         * 1. 接收缓冲区有效；
         * 2. 接收长度满足协议要求；
         * 3. 帧头符合协议格式；
         * 4. 整帧 CRC16 余数为 0。
         */
        if ((rx_data != 0) &&
            (rx_len >= FAN_PROTOCOL_MIN_FRAME_LEN) &&
            (rx_len <= PROTO_MAX_FRAME_LEN) &&
            (rx_data[PROTO_SOF1_INDEX] == PROTO_SOF1) &&
            (rx_data[PROTO_SOF2_INDEX] == PROTO_SOF2) &&
            (CRC_Calculate(rx_data, rx_len) == 0U))
        {
            /* 获取本次接收的协议请求命令。 */
            protocol.request = (proto_cmd_t)rx_data[PROTO_CMD_INDEX];

            /* 解析请求并生成应答命令。 */
            fan_protocol_request(&rx_data[PROTO_DATA_INDEX]);
        }
        else
        {
            /* 接收帧校验失败，设置错误应答。 */
            protocol.response = PROTO_ERR;
        }

        /* 根据应答命令生成并发送协议数据帧。 */
        fan_protocol_response();

        /* 复位 UART1 接收状态。 */
        protocol.api->receive_rst();
    }
}
