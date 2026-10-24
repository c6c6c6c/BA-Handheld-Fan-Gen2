
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

/** 当前硬件使用的数码管数量 */
#define AIP650E_DIGIT_COUNT            2U

/** 软件两线通信延时循环次数，需根据 MCU 主频及实测波形调整 */
#define AIP650E_DELAY_CYCLES           30U

/** 无有效按键时返回的键值 */
#define AIP650E_KEY_NONE               0U


/* Command -----------------------------------------------------------------------------------------------------------*/

/** AiP650E 系统控制命令地址 */
#define AIP650_CMD_SYSTEM              0x48U

/** AiP650E 矩阵按键读取命令地址 */
#define AIP650_CMD_READ_KEY            0x4FU

/** 相邻显示 RAM 寄存器的地址间隔 */
#define AIP650_ADDR_STEP               0x02U

/** 芯片未检测到按键时返回的原始编码 */
#define AIP650_NO_KEY_RAW              0x2EU


/* GPIO Control ------------------------------------------------------------------------------------------------------*/

/** 软件两线使用开漏输出，写入高电平表示释放总线 */
#define AIP650_CLK_HIGH()              LL_GPIO_SetOutputPin(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN)
#define AIP650_CLK_LOW()               LL_GPIO_ResetOutputPin(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN)
#define AIP650_DIO_HIGH()              LL_GPIO_SetOutputPin(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN)
#define AIP650_DIO_LOW()               LL_GPIO_ResetOutputPin(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN)

/** 读取两线总线当前实际电平 */
#define AIP650_DIO_READ()              LL_GPIO_IsInputPinSet(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN)
#define AIP650_CLK_READ()              LL_GPIO_IsInputPinSet(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN)


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief AiP650E 数码管显示模式
 * @note  对应系统控制字中的 S 位。
 */
typedef enum
{
    AIP650_7_SEGMENT = 0x01U,       /**< 七段显示模式，S = 1 */
    AIP650_8_SEGMENT = 0x00U        /**< 八段显示模式，S = 0 */
} E_Segment;


/**
 * @brief AiP650E 显示亮度等级编码
 * @note  对应系统控制字 B4~B6；第 8 级亮度的寄存器编码为 0。
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
 * @brief AiP650E 显示 RAM 寄存器地址
 * @note  DIG1~DIG4 分别对应四组显示数据，相邻寄存器地址间隔为 2。
 */
typedef enum
{
    DIG1 = 0x68U,                  /**< DIG1 显示 RAM 地址 */
    DIG2 = 0x6AU,                  /**< DIG2 显示 RAM 地址 */
    DIG3 = 0x6CU,                  /**< DIG3 显示 RAM 地址 */
    DIG4 = 0x6EU                   /**< DIG4 显示 RAM 地址 */
} E_Digit_Bit;


/**
 * @brief AiP650E 按键编码映射结构
 */
typedef struct
{
    uint8_t raw;                   /**< 芯片返回的原始按键编码 */
    uint8_t matrix;                /**< 对应的矩阵行列编码 */
} aip650_key_map_t;


/**
 * @brief AiP650E 系统显示控制字
 * @note  通过位域管理显示使能、工作模式、段显示模式及亮度。
 *        位域布局按照当前编译器的约定映射，需确认 data 与 bit 的对应关系。
 */
typedef union
{
    uint8_t data;                   /**< 完整的 8 位系统控制字 */

    struct
    {
        uint8_t displayEnabled : 1; /**< B0：显示使能，1=开启，0=关闭 */
        uint8_t reserved0      : 1; /**< B1：保留位 */
        uint8_t sleeping       : 1; /**< B2：工作模式，1=睡眠，0=正常工作 */
        uint8_t segment        : 1; /**< B3：显示模式，1=七段，0=八段 */
        uint8_t brightness     : 3; /**< B4~B6：显示亮度等级编码 */
        uint8_t reserved1      : 1; /**< B7：保留位 */
    } bit;

} aip650e_ctrl_t;


/**
 * @brief AiP650E 驱动运行数据
 * @note  保存目标系统控制字及 DIG1~DIG4 显示数据，
 *        可用于应用层显示状态转换与刷新管理。
 */
typedef struct
{
    aip650e_ctrl_t ctrl;                    /**< 系统控制寄存器软件镜像 */
    uint8_t        show[AIP650E_COM_COUNT]; /**< DIG1~DIG4 显示数据缓存 */
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
//  Others     : 硬件写入成功后同步更新内部显示缓存
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_write_digit(uint8_t digit, uint8_t segments);


/*******************************************************************************
//  Function   : aip650_write_segments
//  Description: 批量更新 DIG1~DIG4 显示数据
//  Param      : data - 包含 AIP650E_COM_COUNT 个段码的数组
//  Return     : true：全部写入成功；false：参数无效或通信失败
//  Others     : 仅写入与内部缓存不同的显示数据，减少两线通信次数；
//               部分写入失败时，已成功更新的数据仍保留在内部缓存中
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_write_segments(const uint8_t data[AIP650E_COM_COUNT]);


/*******************************************************************************
//  Function   : aip650_write_number
//  Description: 使用原始段码数组更新 DIG1~DIG4 显示内容
//  Param      : data - 待写入的显示段码数组
//  Return     : None
//  Others     : 兼容接口，内部调用 aip650_write_segments()；
//               不向调用方返回通信结果
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void aip650_write_number(uint8_t *data);


/*******************************************************************************
//  Function   : aip650_clear
//  Description: 清除 DIG1~DIG4 的全部显示内容
//  Param      : None
//  Return     : true：清屏成功；false：通信失败
//  Others     : 将全部显示数据置零，不修改当前显示模式及亮度配置
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_clear(void);


/*******************************************************************************
//  Function   : aip650_get_key
//  Description: 读取 AiP650E 矩阵按键状态
//  Param      : None
//  Return     : 有效按键的矩阵行列编码；无有效按键时返回 AIP650E_KEY_NONE
//  Others     : 通过两线接口读取原始按键编码，并使用按键映射表进行转换；
//               软件睡眠状态下不执行按键读取
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
char aip650_get_key(void);


/*******************************************************************************
//  Function   : aip650_WriteControl
//  Description: 向 AiP650E 写入系统显示控制字
//  Param      : ctrl - 目标显示控制配置
//  Return     : true：写入成功；false：参数无效或通信失败
//  Others     : 根据控制字配置显示使能、睡眠模式、七段/八段模式及亮度；
//               用于应用层初始化及显示控制状态同步
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_WriteControl(const aip650e_ctrl_t *ctrl);


/*******************************************************************************
//  Function   : aip650_ctrl_data
//  Description: 获取驱动当前保存的显示控制字
//  Param      : None
//  Return     : 当前显示控制寄存器的软件镜像值
//  Others     : 仅读取驱动内部保存的控制数据，不直接访问芯片；
//               可用于比较应用层目标控制字与驱动控制缓存是否一致
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
uint8_t aip650_ctrl_data(void);


#endif /* _AIP650E_H_ */
