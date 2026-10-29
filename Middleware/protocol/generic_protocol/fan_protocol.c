#include "fan_protocol.h"
#include "protocol_config.h"
#include "uart1_port.h"
#include "crc.h"

typedef struct
{
    uint8_t  RxDat[PROTO_MAX_FRAME_LEN];                         /**< 帧头1 */
    uint8_t  TxDat[PROTO_MAX_FRAME_LEN];                         /**< 帧头2 */

    const common_port_api_t *api;                                 /**< 接收帧指针 */

    proto_cmd_t             CurrentCmd;                          /**< 当前命令 */
} fan_protocol_t;

fan_protocol_t protocol = {0};



void FanProtocol_Init()
{
    protocol.api = &s_uart1PortApi;
    protocol.api->Init(protocol.RxDat, PROTO_MAX_FRAME_LEN, protocol.TxDat, PROTO_MAX_FRAME_LEN);
}


void FanProtocol_Deinit()
{
    protocol.api->Deinit();
}


void FanProtocol_1msTask()
{
    if (protocol.api == 0)
    {
        return;
    }

    common_port_1msTask(protocol.api);

    if(protocol.api->receive_state() == COMMON_PORT_RX_DONE)
    {
        uint8_t *rx_data = protocol.api->receive_data();
        uint16_t rx_len  = protocol.api->receive_length();

        if(rx_data[0] == PROTO_SOF1 
        && rx_data[1] == PROTO_SOF2
        && CRC16_Calc(rx_data, rx_len) == 0)
        {
            protocol.CurrentCmd = (proto_cmd_t)rx_data[3];

            switch(protocol.CurrentCmd)
            {
                case PROTO_CMD_READ_INFO:
                    break;

                case PROTO_CMD_READ_SYSTEM:
                    break;

                case PROTO_CMD_READ_DEV:
                    break;

                case PROTO_CMD_FACTORY_RESET:
                    break;

                default:
                    break;
            }
        }
        else
        {
           
        }
         protocol.api->receive_rst();
    }
}