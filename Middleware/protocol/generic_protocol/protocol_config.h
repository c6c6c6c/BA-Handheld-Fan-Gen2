/**
 * @brief 通信协议配置
 */
#define PROTO_SOF1            0x55U
#define PROTO_SOF2            0xAAU
#define PROTO_VERSION         0x01U

#define PROTO_MAX_DATA_LEN    256U
#define PROTO_FRAME_OVERHEAD  10U
#define PROTO_MAX_FRAME_LEN   (PROTO_MAX_DATA_LEN + PROTO_FRAME_OVERHEAD)

/**
 * @brief 协议帧类型
 */
typedef enum
{
    PROTO_TYPE_REQUEST  = 0x00U,  /**< 请求 */
    PROTO_TYPE_RESPONSE = 0x01U,  /**< 应答 */
    PROTO_TYPE_EVENT    = 0x02U   /**< 主动上报 */
} proto_type_t;

/**
 * @brief 协议命令
 */
typedef enum
{
    PROTO_CMD_READ_INFO     = 0x01U,
    PROTO_CMD_READ_BMS      = 0x02U,
    PROTO_CMD_READ_SYSTEM   = 0x03U,
    PROTO_CMD_READ_DEV      = 0x04U,
    PROTO_CMD_FACTORY_RESET = 0x05U,
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