
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
#include "crc.h"
#include "cursor.h"
#include "button.h"
#include "param.h"
#include "uart1_port.h"
#include "fan_protocol.h"

#include "app_bms.h"
#include "app_system.h"

/* Variables ---------------------------------------------------------------------------------------------------------*/

/** 风扇通信协议实例，保存 UART 收发缓冲区、接口及当前请求和应答命令 */
fan_protocol_t protocol = {0};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_protocol_request
//  Description: 解析接收到的风扇协议请求命令及数据区
//  Param      : data - 接收协议帧的数据区首地址
//  Return     : None
//  Others     : 使用 Cursor 读取宏解析请求数据；
//               首先清除请求命令最高位，生成对应应答命令；
//               根据请求类型执行对应业务处理；
//               未支持的命令设置为 PROTO_ERR；
//               当前控制命令仅预留处理入口，尚未实现具体功能
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
static void fan_protocol_request(uint8_t *data)
{
    uint16_t len = 0U;

    /* 启动请求数据区读取游标。 */
    _START_CURSOR_READ(data, len)

        /* 清除请求命令最高位，生成默认应答命令。 */
        protocol.response =(proto_cmd_t)((uint8_t)protocol.request & 0x7FU);

        /* 根据请求命令执行对应的业务处理。 */
        switch ((uint8_t)protocol.request)
        {

            case PROTO_INFO_REQUEST:
            case PROTO_SYSTEM_REQUEST:
            case PROTO_BATTERY_INFO_REQUEST:
            {
                /* 查询命令无须修改设备状态，直接使用对应应答。 */
                break;
            }

            case PROTO_FACTORY_RESET_REQUEST:
            {
                uint8_t value;

                _CURSOR_READ_BYTE(value);
                param_Restore(value);

                break;
            }
			
            case PROTO_COOL_REQUEST:
            {
                _CURSOR_READ_VAR(System.coolDev.temp);
                _CURSOR_READ_VAR(System.coolDev.level);
                _CURSOR_READ_VAR(System.coolDev.err);
                _CURSOR_READ_VAR(System.coolDev.version);
                break;
            }

            case PROTO_ENTER_OTA_REQUEST:
            {
                param_ota_save(OTC_STATUS_START);
                break;
            }

            case PROTO_OTA_UPDATE_REQUEST:
            {
                /* TODO: 处理固件更新请求。 */
                break;
            }

            case PROTO_OTA_COMPLETION_REQUEST:
            {
                /* TODO: 处理固件更新完成请求。 */
                break;
            }

            default:
            {
                /* 未支持的请求命令，设置错误应答。 */
                protocol.response = PROTO_ERR;
                break;
            }
        }

    /* 结束请求数据区读取操作。 */
    _STOP_CURSOR_READ();
}


/*******************************************************************************
//  Function   : fan_protocol_response
//  Description: 根据当前应答命令生成并发送协议数据帧
//  Param      : None
//  Return     : None
//  Others     : 依次写入帧头、应答命令、数据长度及数据区；
//               根据不同应答命令填充设备信息、系统状态或电池配置；
//               完成组帧后计算并追加 CRC16；
//               最后启动 UART1 发送并清除当前应答命令
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
static void fan_protocol_response(void)
{
    uint16_t send_len = 0U;
    uint16_t crc;

    /* 检查通信接口、发送缓冲区及待应答命令。 */
    if ((protocol.api == 0) 
	||  (protocol.response == PROTO_IDLE)
	||  (protocol.api->transfer_data() == 0) )
    {
        return;
    }

    /* 启动协议应答帧写入游标。 */
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
                /* 当前通信协议版本。 */
                _CURSOR_WRITE_BYTE(PROTO_VERSION);

                /* MCU 唯一设备标识。 */
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

                /* 电池及 USB 输入实时采样数据。 */
                _CURSOR_WRITE_VAR(bms_battery.usb_in_v);
                _CURSOR_WRITE_VAR(bms_battery.voltage_v);
                _CURSOR_WRITE_VAR(bms_battery.current_ma);

                /* 电池 SOC/SOH 及历史统计数据。 */
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

                /* 设备电源及运行模式。 */
                _CURSOR_WRITE_BYTE(System.logic.power);
                _CURSOR_WRITE_BYTE(System.logic.fan_mode);
                _CURSOR_WRITE_BYTE(System.logic.mist_mode);

                /* 雾化片 PWM 工作参数。 */
                _CURSOR_WRITE_VAR(System.logic.mist_freq);
                _CURSOR_WRITE_VAR(System.logic.mist_duty);

                /* 设备运行时间统计。 */
                _CURSOR_WRITE_VAR(System.logic.onTimes);
                _CURSOR_WRITE_VAR(System.logic.offTimes);
                _CURSOR_WRITE_VAR(System.logic.mistTimes);

                /* 当前按键状态。 */
                _CURSOR_WRITE_BYTE(botton_getValue());
                break;
            }

            case PROTO_BATTERY_INFO_RESPONSE:
            {
                /* 检查电池配置参数是否有效。 */
                if (bms_battery.configInfo != 0)
                {
                    /* 电池设计容量。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->design_capacity_mah);

                    /* 满充电压与放电截止电压。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->full_voltage_v);
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->cutoff_voltage_v);

                    /* 电池静置判断条件。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->rest_current_ma);
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->rest_dvdt_v_per_s);

                    /* 充放电库仑效率参数。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->charge_efficiency);
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->discharge_efficiency);

                    /* OCV 静置修正时间阈值。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->rest_time_ms);
                }
                break;
            }

            default:
            {
                /* 其他应答命令暂不附加数据区。 */
                break;
            }
        }

        /* 回填协议数据区长度，不包含 CRC16。 */
        _CURSOR_WRITE_BYTE_SPECIFY(PROTO_LEN_INDEX,send_len - PROTO_DATA_INDEX);

        /* 计算帧头至数据区末尾的 CRC16。 */
        crc = CRC_Calculate(protocol.api->transfer_data(), send_len);

        /* 追加 CRC16 校验字段。 */
        _CURSOR_WRITE_VAR(crc);

    /* 结束协议数据帧组包。 */
    _STOP_CURSOR_WRITE();

    /* 启动 UART1 数据发送。 */
    protocol.api->start_transfer(send_len);

    /* 清除本次应答命令。 */
    protocol.response = PROTO_IDLE;
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_protocol_Init
//  Description: 初始化风扇通信协议模块
//  Param      : None
//  Return     : None
//  Others     : 绑定 UART1 底层通信接口；
//               初始化协议收发缓冲区；
//               将请求及应答命令设置为 PROTO_IDLE
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void fan_protocol_Init(void)
{
    /* 绑定 UART1 通信接口。 */
    protocol.api = &s_uart1PortApi;

    /* 初始化 UART1 接收及发送缓冲区。 */
    protocol.api->Init(protocol.RxDat,PROTO_MAX_FRAME_LEN,
                       protocol.TxDat,PROTO_MAX_FRAME_LEN);

    /* 清除协议请求及应答状态。 */
    protocol.request  = PROTO_IDLE;
    protocol.response = PROTO_IDLE;
}


/*******************************************************************************
//  Function   : fan_protocol_Deinit
//  Description: 反初始化风扇通信协议模块
//  Param      : None
//  Return     : None
//  Others     : 关闭 UART1 通信接口并解除绑定；
//               清除当前请求及应答状态
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
//  Others     : 推进 UART1 收发状态机；
//               收到完整帧后验证长度、帧头及 CRC16；
//               校验成功后解析请求命令及数据区；
//               校验失败时生成错误应答；
//               应答处理完成后复位 UART 接收状态
//  Author     : Chen Liang 2026-11-02
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
        /* 获取接收缓冲区及实际数据长度。 */
        rx_data = protocol.api->receive_data();
        rx_len  = protocol.api->receive_length();

        /*
         * 执行协议帧有效性检查：
         * 1. 接收缓冲区不能为空；
         * 2. 帧长度必须处于合法范围；
         * 3. 帧头必须符合协议定义；
         * 4. 整帧 CRC16 校验结果必须为 0。
         */
        if ((rx_data != 0) &&
            (rx_len >= FAN_PROTOCOL_MIN_FRAME_LEN) &&
            (rx_len <= PROTO_MAX_FRAME_LEN) &&
            (rx_data[PROTO_SOF1_INDEX] == PROTO_SOF1) &&
            (rx_data[PROTO_SOF2_INDEX] == PROTO_SOF2) &&
            (CRC_Calculate(rx_data, rx_len) == 0U))
        {
            /* 提取当前协议请求命令。 */
            protocol.request = (proto_cmd_t)rx_data[PROTO_CMD_INDEX];

            /* 解析请求命令及数据区。 */
            fan_protocol_request(&rx_data[PROTO_DATA_INDEX]);
        }
        else
        {
            /* 接收帧无效，设置错误应答。 */
            protocol.response = PROTO_ERR;
        }

        /* 根据应答命令组帧并发送。 */
        fan_protocol_response();

        /* 复位 UART1 接收状态，准备下一帧。 */
        protocol.api->receive_rst();
    }
}
