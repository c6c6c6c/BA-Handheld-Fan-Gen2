
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
 *          6. 通过有效请求刷新连接超时计数，超时后执行连接退出回调。
 *          7. APP 应答支持系统状态、电池参数及雾化追频调试数据。
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
#include "app_mist.h"
#include "app_system.h"
#include "app_sampling.h"
#include "app_system.h"
#include "bsp_rcc.h"
#include "button.h"
#include "timer_base.h"
#include "config.h"
#endif

#ifdef BOOT_PROJECT
#include "bootloader.h"
#endif


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** 风扇通信协议实例，保存 UART 收发缓冲区及当前请求、应答命令 */
fan_protocol_t   protocol      = {0};
/** 协议业务数据，保存上位机控制参数及制冷设备状态。 */
protocol_data_t  protocol_data = {0};



/*******************************************************************************
//  Function   : fan_protocol_ConnectExitCallback
//  Description: 通信连接超时退出回调
//  Param      : None
//  Return     : None
//  Others     : Weak 函数，允许应用层覆盖默认实现；
//               默认清除禁止充电控制标志，不直接操作充电硬件
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
__weak void fan_protocol_ConnectExitCallback()
{
    /* 连接超时后撤销上位机禁止充电请求。 */
    protocol_data.ctrl.prohibit_charging = false;
}

/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : fan_protocol_app_request
//  Description: 处理 Application 工程支持的协议请求
//  Param      : data - 接收协议帧的数据区首地址
//  Return     : None
//  Others     : 仅在 APP_PROJECT 工程中编译
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
#ifdef APP_PROJECT
static void fan_protocol_app_request(uint8_t *data)
{
    uint16_t len = 0U;

    _START_CURSOR_READ(data, len)
        switch ((uint8_t)protocol.request)
        {
            case PROTO_INFO_REQUEST:
            case PROTO_BATTERY_INFO_REQUEST:
			case PROTO_CHAT_REQUEST:
			case PROTO_DEBUG_MIST_REQUEST:
            {
                /* 查询命令不修改设备状态，直接生成对应应答。 */
                break;
            }

            case PROTO_SYSTEM_REQUEST:
            {
                /* 读取上位机禁止充电控制字节，并保存至协议控制状态。 */
                _CURSOR_READ_BYTE(protocol_data.ctrl.prohibit_charging);
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
                _CURSOR_READ_VAR(protocol_data.cool_model.temp);

                /* 读取制冷设备故障信息。 */
                _CURSOR_READ_VAR(protocol_data.cool_model.err);

                /* 读取制冷设备运行状态。 */
                _CURSOR_READ_VAR(protocol_data.cool_model.state);

                /* 读取制冷设备软件版本。 */
                _CURSOR_READ_VAR(protocol_data.cool_model.version);
                break;
            }

            case PROTO_ENTER_OTA_REQUEST:
            {
                /* 保存 OTA 升级开始状态。 */
                param_ota_save(OTC_STATUS_START);

                /* 保存 BMS 数据，供系统复位后恢复使用。 */
                app_bms_Save();

                /* Application 工程通过系统复位进入升级流程。 */
                bsp_rcc_SystemReset();
                break;
            }

            default:
            {
                protocol.response = PROTO_ERR;
                break;
            }
        }
    _STOP_CURSOR_READ();
}
#endif /* APP_PROJECT */


/*******************************************************************************
//  Function   : fan_protocol_boot_request
//  Description: 处理 Bootloader 工程支持的协议请求
//  Param      : data - 接收协议帧的数据区首地址
//  Return     : None
//  Others     : 仅在 BOOT_PROJECT 工程中编译
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
#ifdef BOOT_PROJECT
static void fan_protocol_boot_request(uint8_t *data)
{
    uint16_t len = 0U;

    _START_CURSOR_READ(data, len)
        switch ((uint8_t)protocol.request)
        {
            case PROTO_ENTER_OTA_REQUEST:
            {
                /* 初始化 Bootloader 升级控制状态。 */
                bootloader_Init();
                break;
            }

            case PROTO_OTA_UPDATE_REQUEST:
            {
                uint16_t pack_size;
                uint16_t seq;

                /*
                 * OTA 数据区格式：seq（2 Byte）+ pack_size（2 Byte）+ 固件数据。
                 * 按 Cursor 接口读取包序号及固件数据长度。
                 */
                _CURSOR_READ_VAR(seq);
                _CURSOR_READ_VAR(pack_size);

                /* 当前仅解析包序号，未进行顺序或重复包校验。 */
                (void)seq;

                /* 跳过 4 Byte 包头，将固件数据交给 Bootloader 烧录。 */
                if (!bootloader_burn(&data[4], pack_size))
                {
                    protocol.response = PROTO_ERR;
                }
                break;
            }

            case PROTO_OTA_COMPLETION_REQUEST:
            {
                /* 执行升级完成处理，失败时返回协议错误应答。 */
                if (!bootloader_finish())
                {
                    protocol.response = PROTO_ERR;
                }
                break;
            }

            default:
            {
                protocol.response = PROTO_ERR;
                break;
            }
        }
    _STOP_CURSOR_READ();
}
#endif /* BOOT_PROJECT */


/*******************************************************************************
//  Function   : fan_protocol_request
//  Description: 根据当前工程类型分发协议请求
//  Param      : data - 接收协议帧的数据区首地址
//  Return     : None
//  Others     : 公共入口负责生成默认应答命令
//  Author     : Chen Liang 2026-11-13
*******************************************************************************/
static void fan_protocol_request(uint8_t *data)
{
    /* 清除请求命令最高位，生成默认应答命令。 */
    protocol.response = (protocol_cmd_t)((uint8_t)protocol.request & 0x7FU);

#ifdef APP_PROJECT
    fan_protocol_app_request(data);
#elif defined(BOOT_PROJECT)
    fan_protocol_boot_request(data);
#else
    (void)data;
    protocol.response = PROTO_ERR;
#endif
}


/*******************************************************************************
//  Function   : fan_protocol_app_response
//  Description: 填写 Application 工程应答数据区
//  Param      : data - 应答数据区首地址
//  Return     : 应答数据区长度，单位 Byte
//  Others     : 仅在 APP_PROJECT 工程中编译
//  Author     : Chen Liang 2026-11-13
*******************************************************************************/
#ifdef APP_PROJECT
static uint16_t fan_protocol_app_response(uint8_t *data)
{
    uint16_t data_len = 0U;

    _START_CURSOR_WRITE(data, data_len)
        switch ((uint8_t)protocol.response)
        {
            case PROTO_INFO_RESPONSE:
            {
                /* 当前通信协议版本。 */
                _CURSOR_WRITE_BYTE(PROTO_VERSION);

                /* MCU 唯一设备标识。 */
                _CURSOR_WRITE_VAR(System.unique_id);

                /* 固件校验码。 */
                _CURSOR_WRITE_VAR(System.app_flash_crc);
                _CURSOR_WRITE_VAR(System.boot_flash_crc);
                
                /* 编译日期。 */
                _CURSOR_WRITE_WORD(FIRMWARE_BUILD_TIME);
                
                /* 版本 */
                _CURSOR_WRITE_BYTE(sizeof(FIRMWARE_VERSION));
                _CURSOR_WRITE_VAR(FIRMWARE_VERSION);
                
                /* 设备型号 */
                _CURSOR_WRITE_BYTE(sizeof(DEVICE_MODEL));
                _CURSOR_WRITE_VAR(DEVICE_MODEL);
                
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

                /* 系统实时故障及经防抖、恢复延时处理后的保持故障。 */
                _CURSOR_WRITE_VAR(System.fault_real.data);
                _CURSOR_WRITE_VAR(System.fault_hold.data);

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
                _CURSOR_WRITE_VAR(System.logic.fanTimes);

                /* 当前按键状态。 */
                _CURSOR_WRITE_BYTE(botton_getValue());

                /* 控制 IO 状态。 */
                _CURSOR_WRITE_VAR(System.io_real.data);

                /* ntc温度 */
                _CURSOR_WRITE_VAR(System.ntc_temp);

                /* 系统故障 */
                _CURSOR_WRITE_VAR(System.err);
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

            case PROTO_CHAT_RESPONSE:
            {
                /* 写入系统毫秒计数，作为调试数据时间戳。 */
                _CURSOR_WRITE_WORD(getSystemMs());

                /* 写入雾化及电机采样值，具体单位由采样接口定义。 */
                _CURSOR_WRITE_WORD(get_mist_val());
                _CURSOR_WRITE_WORD(get_motor_val());

                /* 写入电压、电流及 SOC 调试数据。 */
                _CURSOR_WRITE_VAR(bms_battery.usb_in_v);
                _CURSOR_WRITE_VAR(bms_battery.voltage_v);
                _CURSOR_WRITE_VAR(bms_battery.current_ma);
                _CURSOR_WRITE_VAR(bms_battery.soc_info.soc);
                _CURSOR_WRITE_VAR(bms_battery.soc_info.static_soc);
                
                _CURSOR_WRITE_VAR(System.ntc_temp);
                
                _CURSOR_WRITE_VAR(System.fan_curr_diff);
                break;
            }

            case PROTO_DEBUG_MIST_RESPONSE:
            {
                _CURSOR_WRITE_WORD(getSystemMs());
                _CURSOR_WRITE_WORD(get_mist_val());
                /* 追频状态及当前目标 PWM 参数。 */
                _CURSOR_WRITE_BYTE((uint8_t)mist_ctrl.stat.state);
                _CURSOR_WRITE_VAR(System.logic.mist_freq);
                _CURSOR_WRITE_VAR(System.logic.mist_duty);
                /* 扫描峰值频率、锁定频率及对应 ADC 参数。 */
                _CURSOR_WRITE_VAR(mist_ctrl.stat.peakFreq);
                _CURSOR_WRITE_VAR(mist_ctrl.stat.lockFreq);
                _CURSOR_WRITE_VAR(mist_ctrl.stat.peakAd);
                _CURSOR_WRITE_VAR(mist_ctrl.stat.targetAd);
                _CURSOR_WRITE_VAR(mist_ctrl.stat.refAd);
                /* 锁定标志、重新扫描次数及故障统计。 */
                _CURSOR_WRITE_VAR(mist_ctrl.stat.locked);
                _CURSOR_WRITE_VAR(mist_ctrl.stat.rescanCnt);
                _CURSOR_WRITE_VAR(mist_ctrl.stat.fault);
                _CURSOR_WRITE_VAR(mist_ctrl.stat.errCnt);

                break;
            }


            default:
            {
                break;
            }
        }
    _STOP_CURSOR_WRITE();

    return data_len;
}
#endif /* APP_PROJECT */


/*******************************************************************************
//  Function   : fan_protocol_boot_response
//  Description: 填写 Bootloader 工程应答数据区
//  Param      : data - 应答数据区首地址
//  Return     : 应答数据区长度，单位 Byte
//  Others     : 当前 Bootloader 应答均不携带数据区
//  Author     : Chen Liang 2026-11-13
*******************************************************************************/
#ifdef BOOT_PROJECT
static uint16_t fan_protocol_boot_response(uint8_t *data)
{
    (void)data;
    return 0U;
}
#endif /* BOOT_PROJECT */


/*******************************************************************************
//  Function   : fan_protocol_response
//  Description: 组装并发送公共协议应答帧
//  Param      : None
//  Return     : None
//  Others     : 工程专属函数只负责填写数据区；
//               公共入口填写帧头、命令、长度及 CRC16；
//               CRC 按当前 MCU 内存字节顺序复制至帧尾；
//               发送启动后清除应答命令
//  Author     : Chen Liang 2026-11-13
*******************************************************************************/
static void fan_protocol_response(void)
{
    uint8_t *tx_data;
    uint16_t data_len;
    uint16_t send_len;
    uint16_t crc;

    if ((protocol.api == 0)
    ||  (protocol.response == PROTO_IDLE)
    ||  (protocol.api->transfer_data() == 0))
    {
        return;
    }

    /* 获取发送缓冲区并填写公共帧头及应答命令。 */
    tx_data = protocol.api->transfer_data();
    tx_data[PROTO_SOF1_INDEX] = PROTO_SOF1;
    tx_data[PROTO_SOF2_INDEX] = PROTO_SOF2;
    tx_data[PROTO_CMD_INDEX]  = (uint8_t)protocol.response;

#ifdef APP_PROJECT
    data_len = fan_protocol_app_response(&tx_data[PROTO_DATA_INDEX]);
#elif defined(BOOT_PROJECT)
    data_len = fan_protocol_boot_response(&tx_data[PROTO_DATA_INDEX]);
#else
    data_len = 0U;
#endif

    /* 填写单字节数据区长度，并计算不含 CRC 的帧长度。 */
    tx_data[PROTO_LEN_INDEX] = (uint8_t)data_len;
    send_len = PROTO_DATA_INDEX + data_len;
    /* 计算帧头至数据区的 CRC16，并追加至帧尾。 */
    crc = CRC_Calculate(tx_data, send_len);
    memcpy(&tx_data[send_len], &crc, sizeof(crc));
    send_len += sizeof(crc);

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
//               解析完成后复位 UART 接收状态，再统一处理应答；
//               无错误应答的请求刷新连接超时计数；
//               超时计数归零时执行连接退出回调
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
void fan_protocol_1ms_task(void)
{
    uint8_t *rx_data;
    uint16_t rx_len;

    /* 检查通信接口是否已经初始化。 */
    if (protocol.api == 0)
    {
        protocol.IsConnect = false;
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

            /* 请求未产生错误应答时刷新协议连接状态及超时计数。 */
            if(protocol.response != PROTO_ERR)
            {
                protocol.IsConnect        = true;
                protocol.ConnectExitTimer = PROTO_CONNECT_EXIT_TIME;
            }
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

    /*
     * 连接超时管理：每次周期任务递减连接计数。
     * 计数归零时清除连接标志，并执行一次退出回调。
     * 实际超时时间取决于任务调用周期；刷新当次也会递减。
     */
    if(protocol.IsConnect && protocol.ConnectExitTimer)
    {
        protocol.ConnectExitTimer --;
        protocol.IsConnect = (protocol.ConnectExitTimer != 0);

        if(!protocol.IsConnect)
        {
            fan_protocol_ConnectExitCallback();
        }
    }
}
