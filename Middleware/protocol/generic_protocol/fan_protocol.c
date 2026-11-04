
/**
 ***********************************************************************************************************************
 * @file    fan_protocol.c
 * @brief   风扇通信协议管理，实现 UART1 数据收发、命令解析、CRC16 校验及 OTA 升级控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-30
 ***********************************************************************************************************************
 * @note
 *          1. 支持 Application 和 Bootloader 两种工程模式。
 *          2. APP_PROJECT 用于设备信息查询、系统参数及功能控制。
 *          3. BOOT_PROJECT 用于 OTA 数据接收及 Flash 固件烧录。
 *          4. 通信协议使用 Cursor 宏完成数据解析及应答组帧。
 *          5. 协议数据帧使用 CRC16 进行完整性校验。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "crc.h"
#include "cursor.h"
#include "param.h"
#include "uart1_port.h"
#include "fan_protocol.h"

#ifdef APP_PROJECT
#include "app_bms.h"
#include "app_system.h"
#include "bsp_rcc.h"
#include "button.h"
#endif

#ifdef BOOT_PROJECT
#include "bootloader.h"
#endif


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** 风扇通信协议实例，保存 UART 收发缓冲区及当前请求、应答命令 */
fan_protocol_t protocol = {0};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_protocol_request
//  Description: 解析接收到的协议请求命令及数据区
//  Param      : data - 接收协议帧的数据区首地址
//  Return     : None
//  Others     : 根据协议命令选择 Application 或 Bootloader 处理逻辑；
//               请求命令清除最高位后生成对应应答命令；
//               不支持的命令统一设置为 PROTO_ERR；
//               当前使用 Cursor 读取宏，调用方需保证数据区长度有效
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
static void fan_protocol_request(uint8_t *data)
{
    uint16_t len = 0U;

    /* 启动协议请求数据区读取游标。 */
    _START_CURSOR_READ(data, len)

        /* 清除请求命令最高位，生成默认应答命令。 */
        protocol.response = (protocol_cmd_t)((uint8_t)protocol.request & 0x7FU);

        /* 根据请求命令执行对应的业务处理。 */
        switch ((uint8_t)protocol.request)
        {
#ifdef APP_PROJECT

            case PROTO_INFO_REQUEST:
            case PROTO_SYSTEM_REQUEST:
            case PROTO_BATTERY_INFO_REQUEST:
            {
                /* 查询命令不修改设备状态，直接生成对应应答。 */
                break;
            }

            case PROTO_FACTORY_RESET_REQUEST:
            {
                uint8_t value;

                /* 读取参数恢复控制值。 */
                _CURSOR_READ_BYTE(value);

                /* 根据恢复控制值执行参数恢复。 */
                param_Restore(value);
                break;
            }

            case PROTO_COOL_REQUEST:
            {
                /* 读取制冷设备目标温度。 */
                _CURSOR_READ_VAR(System.coolDev.temp);

                /* 读取制冷设备运行档位。 */
                _CURSOR_READ_VAR(System.coolDev.level);

                /* 读取制冷设备故障信息。 */
                _CURSOR_READ_VAR(System.coolDev.err);

                /* 读取制冷设备软件版本。 */
                _CURSOR_READ_VAR(System.coolDev.version);
                break;
            }

#endif /* APP_PROJECT */

            case PROTO_ENTER_OTA_REQUEST:
            {
#ifdef BOOT_PROJECT
                /* 初始化 Bootloader 固件升级状态及烧录地址。 */
                bootloader_Init();
#endif
#ifdef APP_PROJECT
                /* 保存 OTA 升级开始状态。 */
                param_ota_save(OTC_STATUS_START);
                /* Application 工程通过系统复位进入升级流程。 */
                bsp_rcc_SystemReset();
#endif
                break;
            }

#ifdef BOOT_PROJECT

            case PROTO_OTA_UPDATE_REQUEST:
            {
                uint16_t pack_size;
                uint16_t seq;
                
                /* 读取本次 OTA 固件数据包长度。 */
                _CURSOR_READ_VAR(seq);

                /* 读取本次 OTA 固件数据包长度。 */
                _CURSOR_READ_VAR(pack_size);
                
                /* 烧录当前固件数据包。 */
                if (!bootloader_burn(&data[4], pack_size))
                {
                    /* Flash 烧录或校验失败，返回错误应答。 */
                    protocol.response = PROTO_ERR;
                }
                break;
            }

            case PROTO_OTA_COMPLETION_REQUEST:
            {
                /* TODO: 校验完整固件并保存 OTA 升级完成状态。 */
				param_ota_save(OTC_STATUS_SUCCESS);
                break;
            }

#endif /* BOOT_PROJECT */

            default:
            {
                /* 不支持的请求命令，设置错误应答。 */
                protocol.response = PROTO_ERR;
                break;
            }
        }

    /* 结束协议请求数据区读取游标。 */
    _STOP_CURSOR_READ();
}


/*******************************************************************************
//  Function   : fan_protocol_response
//  Description: 根据应答命令生成并发送协议数据帧
//  Param      : None
//  Return     : None
//  Others     : 使用 Cursor 写入宏生成帧头、命令及数据区；
//               根据当前工程模式选择支持的应答数据；
//               组帧完成后回填数据长度并追加 CRC16；
//               最后启动 UART1 发送并清除应答命令
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
static void fan_protocol_response(void)
{
    uint16_t send_len = 0U;
    uint16_t crc;

    /* 检查通信接口及当前应答状态。 */
    if ((protocol.api == 0)
    ||  (protocol.response == PROTO_IDLE) 
    ||  (protocol.api->transfer_data() == 0))
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

        /* 根据应答命令填写协议数据区。 */
        switch ((uint8_t)protocol.response)
        {

#ifdef APP_PROJECT

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
				
				/* 固件编译时间。 */
                _CURSOR_WRITE_VAR(System.compile_param.hour);
                _CURSOR_WRITE_VAR(System.compile_param.minute);
                _CURSOR_WRITE_VAR(System.compile_param.second);

                break;
            }

            case PROTO_SYSTEM_RESPONSE:
            {
                /* 电池充放电状态。 */
                _CURSOR_WRITE_BYTE(bms_battery.state);

                /* 电池及 USB 实时采样数据。 */
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

                /* 系统电源及运行模式。 */
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
                /* 检查电池配置参数是否有效。 */
                if (bms_battery.configInfo != 0)
                {
                    /* 电池设计容量。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->design_capacity_mah);

                    /* 满充及截止电压。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->full_voltage_v);
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->cutoff_voltage_v);

                    /* 静置判断条件。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->rest_current_ma);
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->rest_dvdt_v_per_s);

                    /* 库仑积分效率参数。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->charge_efficiency);
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->discharge_efficiency);

                    /* OCV 静置修正时间。 */
                    _CURSOR_WRITE_VAR(bms_battery.configInfo->rest_time_ms);
                }

                break;
            }

#endif /* APP_PROJECT */

            default:
            {
                /* 其他应答命令不附加数据区。 */
                break;
            }
        }

        /* 回填协议数据区长度，不包含 CRC16。 */
        _CURSOR_WRITE_BYTE_SPECIFY(PROTO_LEN_INDEX,send_len - PROTO_DATA_INDEX);

        /* 计算帧头至数据区末尾的 CRC16。 */
        crc = CRC_Calculate(protocol.api->transfer_data(), send_len);

        /* 在数据帧末尾追加 CRC16。 */
        _CURSOR_WRITE_VAR(crc);

    /* 结束协议应答帧组包。 */
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
//  Others     : 绑定 UART1 通信接口并初始化收发缓冲区；
//               清除当前协议请求和应答状态
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void fan_protocol_Init(void)
{
    /* 绑定 UART1 通信接口。 */
    protocol.api = &s_uart1PortApi;

    /* 初始化 UART1 接收及发送缓冲区。 */
    protocol.api->Init(protocol.RxDat,PROTO_MAX_FRAME_LEN,
                       protocol.TxDat,PROTO_MAX_FRAME_LEN);

    /* 初始化请求和应答命令。 */
    protocol.request  = PROTO_IDLE;
    protocol.response = PROTO_IDLE;
}


/*******************************************************************************
//  Function   : fan_protocol_Deinit
//  Description: 反初始化风扇通信协议模块
//  Param      : None
//  Return     : None
//  Others     : 关闭 UART1 通信接口并解除绑定；
//               清除当前请求及应答命令
//  Author     : Chen Liang 2026-10-30
*******************************************************************************/
void fan_protocol_Deinit(void)
{
    /* 检查通信接口是否有效。 */
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
//  Others     : 周期推进 UART1 通信状态机；
//               接收完整协议帧后检查长度、帧头及 CRC16；
//               校验成功时执行协议请求解析；
//               校验失败时生成错误应答；
//               完成应答处理后复位 UART 接收状态
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
void fan_protocol_1ms_task(void)
{
    uint8_t *rx_data;
    uint16_t rx_len;

    /* 检查通信接口是否已经初始化。 */
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
         * 1. 接收数据指针有效；
         * 2. 数据帧长度在允许范围内；
         * 3. 帧头与协议定义一致；
         * 4. CRC16 整帧校验结果为 0。
         */
        if ((rx_data != 0) 
        &&  (rx_len >= FAN_PROTOCOL_MIN_FRAME_LEN)    && (rx_len <= PROTO_MAX_FRAME_LEN) 
        &&  (rx_data[PROTO_SOF1_INDEX] == PROTO_SOF1) && (rx_data[PROTO_SOF2_INDEX] == PROTO_SOF2) 
        &&  (CRC_Calculate(rx_data, rx_len) == 0U))
        {
            /* 获取当前协议请求命令。 */
            protocol.request = (protocol_cmd_t)rx_data[PROTO_CMD_INDEX];

            /* 解析协议请求及其数据区。 */
            fan_protocol_request(&rx_data[PROTO_DATA_INDEX]);
        }
        else
        {
            /* 接收帧非法，设置错误应答。 */
            protocol.response = PROTO_ERR;
        }

        /* 复位 UART1 接收状态，准备接收下一帧。 */
        protocol.api->receive_rst();
    }
	
	/* 根据处理结果组帧并发送应答。 */
	fan_protocol_response();
}
