/**
 * @brief 通信协议配置
 */
#define PROTO_SOF1            0x55U
#define PROTO_SOF2            0xAAU
#define PROTO_VERSION         0x01U

#define PROTO_MAX_DATA_LEN    256U
#define PROTO_FRAME_OVERHEAD  10U
#define PROTO_MAX_FRAME_LEN   (PROTO_MAX_DATA_LEN + PROTO_FRAME_OVERHEAD)


#define PROTO_REQUEST(x)     (0x80U | (x))
#define PROTO_RESPONSE(x)    (0x80U | (x))



/**
 * @brief 协议命令
 */
typedef enum
{
    PROTO_IDEL                                 = 0x00U,
     
    PROTO_INFO_REQUEST                         = 0x81,
    PROTO_SYSTEM_REQUEST                       = 0x82,
    PROTO_FACTORY_RESET_REQUEST                = 0x83,
     
    PROTO_COOL_REQUEST                         = 0x84,
     
    PROTO_ENTER_OTA_REQUEST                    = 0x85,
    PROTO_ENTER_UPDATE_REQUEST                 = 0x86,
    PROTO_ENTER_COMPLETION_REQUEST             = 0x87,


} proto_cmd_t;




/**
 * @brief 协议帧数据
 * @note 逻辑结构，不可直接强转为线上的字节流。
 */
typedef struct
{
    uint8_t  version;                      /**< 协议版本 */
    uint8_t  type;                         /**< 帧类型 */
    uint8_t  cmd;                          /**< 命令码 */
    uint16_t length;                       /**< 数据长度 */
    uint8_t  data[PROTO_MAX_DATA_LEN];     /**< 数据缓冲区 */
} proto_frame_t;