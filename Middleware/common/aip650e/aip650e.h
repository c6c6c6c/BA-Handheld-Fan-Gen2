/**
 ***********************************************************************************************************************
 * @file    aip650e.h
 * @brief   AiP650E 显示板驱动接口定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _AIP650E_H_
#define _AIP650E_H_

#include <stdbool.h>
#include <stdint.h>


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** 显示板共 4 个 COM，其中 COM1/COM2 为两位数码管。 */
#define AIP650E_COM_COUNT             4U
#define AIP650E_DIGIT_COUNT           2U
#define AIP650E_COM1_INDEX            0U
#define AIP650E_COM2_INDEX            1U
#define AIP650E_COM3_INDEX            2U
#define AIP650E_COM4_INDEX            3U

/** 八段显示段码定义，bit0~bit7 分别对应 A~G、DP */
#define AIP650E_SEG_A                 (1U << 0)
#define AIP650E_SEG_B                 (1U << 1)
#define AIP650E_SEG_C                 (1U << 2)
#define AIP650E_SEG_D                 (1U << 3)
#define AIP650E_SEG_E                 (1U << 4)
#define AIP650E_SEG_F                 (1U << 5)
#define AIP650E_SEG_G                 (1U << 6)
#define AIP650E_SEG_DP                (1U << 7)

/** COM3：A/B/C 为 3 颗独立灯；D/E/F 为三色灯的 3 个通道，颜色顺序未标注。 */
#define AIP650E_COM3_LAMP1            AIP650E_SEG_A
#define AIP650E_COM3_LAMP2            AIP650E_SEG_B
#define AIP650E_COM3_LAMP3            AIP650E_SEG_C
#define AIP650E_COM3_RGB_D            AIP650E_SEG_D
#define AIP650E_COM3_RGB_E            AIP650E_SEG_E
#define AIP650E_COM3_RGB_F            AIP650E_SEG_F
#define AIP650E_COM3_VALID_MASK       0x3FU

/** COM4 上的两个发光支路 A/B 组成同一个指示灯，data[3] 使用 0/1 控制。 */
#define AIP650E_COM4_OFF              0U
#define AIP650E_COM4_ON               1U

/** 显示亮度范围 */
#define AIP650E_BRIGHTNESS_MIN        1U
#define AIP650E_BRIGHTNESS_MAX        8U
#define AIP650E_DEFAULT_BRIGHTNESS    4U

/** 软件总线半周期延时 NOP 次数 */
#ifndef AIP650E_DELAY_CYCLES
#define AIP650E_DELAY_CYCLES          24U
#endif


/* Configuration check -----------------------------------------------------------------------------------------------*/

#if (AIP650E_DEFAULT_BRIGHTNESS < AIP650E_BRIGHTNESS_MIN) || \
    (AIP650E_DEFAULT_BRIGHTNESS > AIP650E_BRIGHTNESS_MAX) || \
    (AIP650E_DELAY_CYCLES == 0U)
#error "Invalid AiP650E default brightness or bus delay"
#endif


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : aip650e_Init
//  Description: 初始化 AiP650E 驱动及显示
//  Param      : None
//  Return     : true：初始化成功；false：总线异常或芯片未应答
//  Others     : 配置 CLK/DIO 为开漏输出，清空显示 RAM 后按默认亮度开启显示
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_Init(void);


/*******************************************************************************
//  Function   : aip650e_SetBrightness
//  Description: 设置 AiP650E 全局显示亮度
//  Param      : level - 亮度等级，范围 1~8，8 为最高亮度
//  Return     : true：设置成功；false：参数无效、未初始化或通信失败
//  Others     : 不改变当前显示开关状态及显示 RAM 内容
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_SetBrightness(uint8_t level);


/*******************************************************************************
//  Function   : aip650e_SetDisplay
//  Description: 设置 AiP650E 显示开关
//  Param      : enabled - true：开启显示；false：关闭显示
//  Return     : true：设置成功；false：未初始化或通信失败
//  Others     : 仅修改显示开关，不清除显示 RAM 和当前亮度
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_SetDisplay(bool enabled);


/*******************************************************************************
//  Function   : aip650e_ShowSegments
//  Description: 向 COM1 或 COM2 数码管写入原始段码
//  Param      : digit    - 数码管位号，范围 1~2
//               segments - 七段显示数据，bit0~bit6 对应 A~G
//  Return     : true：写入成功；false：参数无效、未初始化或通信失败
//  Others     : DP/KP 未连接；该接口不会改动 COM3/COM4 灯光状态
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_ShowSegments(uint8_t digit, uint8_t segments);


/*******************************************************************************
//  Function   : aip650e_ShowDigit
//  Description: 在指定数码管位置显示单个十进制数字
//  Param      : digit - 数码管位号，范围 1~2
//               value - 显示数字，范围 0~9
//  Return     : true：显示成功；false：参数无效、未初始化或通信失败
//  Others     : 使用内部数字段码表转换后写入显示 RAM
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_ShowDigit(uint8_t digit, uint8_t value);


/*******************************************************************************
//  Function   : aip650e_ShowNumber
//  Description: 在 COM1/COM2 两位数码管显示十进制整数
//  Param      : value        - 显示数值，范围 0~99
//               leadingZeros - true：显示前导零；false：隐藏前导零
//  Return     : true：显示成功；false：参数无效、未初始化或通信失败
//  Others     : COM1 为十位，COM2 为个位；不会改动 COM3/COM4 灯光状态
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_ShowNumber(uint16_t value, bool leadingZeros);


/*******************************************************************************
//  Function   : aip650e_ShowData
//  Description: 一次写入显示板上四个 COM 的内容
//  Param      : data - 指向 4 字节数组：
//                     [0] COM1 十位数码管 A~G 段码；[1] COM2 个位数码管 A~G 段码；
//                     [2] COM3 位 A/B/C 为三颗独立灯，D/E/F 为三色灯三个通道；
//                     [3] COM4 独立指示灯，0 关闭、1 点亮
//  Return     : true：四个 COM 均写入成功；false：参数无效、未初始化或通信失败
//  Others     : 不猜测三色灯 D/E/F 对应的颜色；中途失败时已写入的 COM 不回滚
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_ShowData(const uint8_t *data);


/*******************************************************************************
//  Function   : aip650e_Clear
//  Description: 清空 COM1~COM4 的显示内容
//  Param      : None
//  Return     : true：清除成功；false：未初始化或通信失败
//  Others     : 将四个 COM 显示 RAM 写 0，不改变亮度和显示开关状态
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_Clear(void);


#endif /* _AIP650E_H_ */
