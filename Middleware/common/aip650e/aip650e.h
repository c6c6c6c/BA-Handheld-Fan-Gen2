
/**
 ***********************************************************************************************************************
 * @file    aip650e.h
 * @brief   AiP650E 软件两线通信、数码管显示及矩阵按键驱动接口
 * @author  Leo Chen
 * @version 1.1.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 */

#ifndef _AIP650E_H_
#define _AIP650E_H_

#include <stdbool.h>
#include <stdint.h>


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** AiP650E 显示 RAM 寄存器数量 */
#define AIP650E_COM_COUNT              4U

/** 当前硬件使用的七段数码管数量 */
#define AIP650E_DIGIT_COUNT            2U

/** COM1 显示数据缓存索引 */
#define AIP650E_COM1_INDEX             0U

/** COM2 显示数据缓存索引 */
#define AIP650E_COM2_INDEX             1U

/** COM3 显示数据缓存索引，用于指示灯及 RGB 控制 */
#define AIP650E_COM3_INDEX             2U

/** COM4 显示数据缓存索引，用于独立指示灯控制 */
#define AIP650E_COM4_INDEX             3U

/** 软件两线通信延时循环次数，需根据 MCU 主频及实测波形调整 */
#define AIP650E_DELAY_CYCLES           30U

/** 显示亮度最大等级，软件接口范围为 1~8 */
#define AIP650E_BRIGHTNESS_MAX         8U

/** 无有效按键时返回的键值 */
#define AIP650E_KEY_NONE               0U

/** 七段数码管小数点 DP 对应的段码位 */
#define AIP650E_SEGMENT_DP             0x80U


/* COM3 Indicator Control --------------------------------------------------------------------------------------------*/

/** COM3 bit0：指示灯 LAMP1 */
#define AIP650E_COM3_LAMP1             (1U << 0)

/** COM3 bit1：指示灯 LAMP2 */
#define AIP650E_COM3_LAMP2             (1U << 1)

/** COM3 bit2：指示灯 LAMP3 */
#define AIP650E_COM3_LAMP3             (1U << 2)

/** COM3 bit3：RGB D 通道 */
#define AIP650E_COM3_RGB_D             (1U << 3)

/** COM3 bit4：RGB E 通道 */
#define AIP650E_COM3_RGB_E             (1U << 4)

/** COM3 bit5：RGB F 通道 */
#define AIP650E_COM3_RGB_F             (1U << 5)


/* COM4 Indicator Control --------------------------------------------------------------------------------------------*/

/** COM4 指示灯关闭时的控制值 */
#define AIP650E_COM4_OFF               0U

/** COM4 指示灯开启时的控制值 */
#define AIP650E_COM4_ON                1U

/** AiP650E 系统控制命令地址 */
#define AIP650_CMD_SYSTEM             0x48U

/** AiP650E 按键读取命令地址 */
#define AIP650_CMD_READ_KEY           0x4FU

/** 相邻显示 RAM 地址的偏移量 */
#define AIP650_ADDR_STEP              0x02U

/** 芯片未检测到按键时返回的原始编码 */
#define AIP650_NO_KEY_RAW             0x2EU

/** 系统控制字：睡眠使能位 */
#define AIP650_CONTROL_SLEEP          0x04U

/** 系统控制字：显示使能位 */
#define AIP650_CONTROL_DISPLAY        0x01U

/** 系统控制字：七段显示模式选择位 */
#define AIP650_CONTROL_7SEG           0x08U


/* 软件两线 GPIO 操作，使用开漏输出，高电平表示释放总线 */
#define AIP650_CLK_HIGH()             LL_GPIO_SetOutputPin(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN)
#define AIP650_CLK_LOW()              LL_GPIO_ResetOutputPin(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN)
#define AIP650_DIO_HIGH()             LL_GPIO_SetOutputPin(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN)
#define AIP650_DIO_LOW()              LL_GPIO_ResetOutputPin(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN)
#define AIP650_DIO_READ()             LL_GPIO_IsInputPinSet(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN)
#define AIP650_CLK_READ()             LL_GPIO_IsInputPinSet(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN)


/* Typedef -----------------------------------------------------------------------------------------------------------*/




/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief AiP650E 数码管显示模式
 */
typedef enum
{
    AIP650_7_SEGMENT = 0x09U,       /**< 七段显示模式 */
    AIP650_8_SEGMENT = 0x01U        /**< 八段显示模式，支持 DP 段 */
} E_Segment;


/**
 * @brief AiP650E 显示亮度寄存器编码
 * @note  亮度编码并非完全按数值递增，最高亮度等级编码为 0x00。
 */
typedef enum
{
    BRIGHTNESS_1_LEVEL = 0x01U,     /**< 亮度等级 1 */
    BRIGHTNESS_2_LEVEL = 0x02U,     /**< 亮度等级 2 */
    BRIGHTNESS_3_LEVEL = 0x03U,     /**< 亮度等级 3 */
    BRIGHTNESS_4_LEVEL = 0x04U,     /**< 亮度等级 4 */
    BRIGHTNESS_5_LEVEL = 0x05U,     /**< 亮度等级 5 */
    BRIGHTNESS_6_LEVEL = 0x06U,     /**< 亮度等级 6 */
    BRIGHTNESS_7_LEVEL = 0x07U,     /**< 亮度等级 7 */
    BRIGHTNESS_8_LEVEL = 0x00U      /**< 亮度等级 8，最高亮度 */
} E_Bright_level;


/**
 * @brief AiP650E 显示 RAM 地址
 * @note  相邻 DIG 寄存器地址间隔为 2。
 */
typedef enum
{
    DIG1 = 0x68U,                  /**< DIG1 显示数据寄存器 */
    DIG2 = 0x6AU,                  /**< DIG2 显示数据寄存器 */
    DIG3 = 0x6CU,                  /**< DIG3 显示数据寄存器 */
    DIG4 = 0x6EU                   /**< DIG4 显示数据寄存器 */
} E_Digit_Bit;


/**
 * @brief 七段数码管数字及符号段码
 * @note  bit0~bit6 分别对应 A~G 段，不包含 DP 段。
 */
typedef enum
{
    NUMBER_0 = 0x3FU,              /**< 数字 0 */
    NUMBER_1 = 0x06U,              /**< 数字 1 */
    NUMBER_2 = 0x5BU,              /**< 数字 2 */
    NUMBER_3 = 0x4FU,              /**< 数字 3 */
    NUMBER_4 = 0x66U,              /**< 数字 4 */
    NUMBER_5 = 0x6DU,              /**< 数字 5 */
    NUMBER_6 = 0x7DU,              /**< 数字 6 */
    NUMBER_7 = 0x07U,              /**< 数字 7 */
    NUMBER_8 = 0x7FU,              /**< 数字 8 */
    NUMBER_9 = 0x6FU,              /**< 数字 9 */
    MINUS    = 0x40U               /**< 负号，仅点亮 G 段 */
} E_Number;

/**
 * @brief AiP650E 按键原始编码映射结构
 */
typedef struct
{
    uint8_t raw;                    /**< 芯片返回的原始按键编码 */
    uint8_t matrix;                 /**< 对应的矩阵行列编码 */
} aip650_key_map_t;


/**
 * @brief AiP650E 系统控制状态
 */
typedef struct
{
    E_Segment      segment;         /**< 七段或八段显示模式 */
    E_Bright_level brightness;      /**< 当前显示亮度等级 */
    bool           displayEnabled;  /**< 显示使能状态 */
    bool           sleeping;        /**< 芯片睡眠状态 */
} aip650e_ctrl_t;


/**
 * @brief AiP650E 驱动运行数据
 */
typedef struct
{
    aip650e_ctrl_t ctrl;                         /**< 系统控制寄存器软件镜像 */
    uint8_t        show[AIP650E_COM_COUNT];      /**< DIG1~DIG4 显示 RAM 软件镜像 */
} aip650e_t;
/* Main API ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : aip650_init
//  Description: 初始化 AiP650E 软件两线接口及显示芯片
//  Param      : None
//  Return     : None
//  Others     : 配置开漏 GPIO，检查总线状态，清除显示 RAM 并开启显示；
//               GPIO 时钟应由 BSP 层提前初始化
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void aip650_init(void);


/*******************************************************************************
//  Function   : aip650_write_digit
//  Description: 向指定 DIG 显示寄存器写入段码数据
//  Param      : digit    - DIG 索引，范围 0~AIP650E_COM_COUNT-1
//               segments - 待写入的原始段码
//  Return     : true：写入成功；false：索引无效或通信失败
//  Others     : 写入成功后同步更新内部显示缓存
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_write_digit(uint8_t digit, uint8_t segments);


/*******************************************************************************
//  Function   : aip650_write_segments
//  Description: 批量更新 DIG1~DIG4 显示数据
//  Param      : data - 包含 AIP650E_COM_COUNT 个段码的数组
//  Return     : true：全部写入成功；false：参数无效或通信失败
//  Others     : 仅写入与内部缓存不同的数据，减少总线通信次数
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_write_segments(const uint8_t data[AIP650E_COM_COUNT]);


/*******************************************************************************
//  Function   : aip650_write_number
//  Description: 使用原始段码数组更新 DIG1~DIG4 显示内容
//  Param      : data - 待写入的显示段码数组
//  Return     : None
//  Others     : 兼容接口，内部调用 aip650_write_segments()，不返回通信状态
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void aip650_write_number(uint8_t *data);


/*******************************************************************************
//  Function   : aip650_clear
//  Description: 清除全部 DIG 显示数据
//  Param      : None
//  Return     : true：清屏成功；false：通信失败
//  Others     : 将 DIG1~DIG4 段码设置为 0，不改变显示模式及亮度配置
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_clear(void);


/*******************************************************************************
//  Function   : aip650_get_key
//  Description: 读取 AiP650E 矩阵按键状态
//  Param      : None
//  Return     : 有效按键的矩阵行列编码；无按键时返回 AIP650E_KEY_NONE
//  Others     : 通过软件两线通信读取原始按键编码，并使用内部映射表转换；
//               芯片处于软件睡眠状态时不执行按键读取
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
char aip650_get_key(void);

/*******************************************************************************
//  Function   : aip650_WriteControl
//  Description: 将控制状态转换为系统控制字并写入 AiP650E
//  Param      : ctrl - 系统控制状态结构体
//  Return     : true：控制配置写入成功；false：参数无效或通信失败
//  Others     : 将显示模式、亮度、显示使能及睡眠状态组合为控制寄存器数据
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_WriteControl(const aip650e_ctrl_t *ctrl);
#endif /* _AIP650E_H_ */
