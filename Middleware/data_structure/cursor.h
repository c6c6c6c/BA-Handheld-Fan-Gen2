/**
 ***********************************************************************************************************************
 * @file    cursor.h
 * @brief   数据缓冲区游标管理接口，实现数据追加写入、长度查询及游标复位
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-30
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _CURSOR_H_
#define _CURSOR_H_

#include <stdint.h>


/* Cursor Macro ------------------------------------------------------------------------------------------------------*/

/**
 * @brief 启动数据缓冲区游标
 * @param p_dat 缓冲区首地址
 * @param p_len 当前写入长度变量，单位 Byte
 * @note  创建局部游标指针和偏移量指针。
 *        必须与 _STOP_CURSOR() 配对使用。
 */
#define _START_CURSOR_WRITE(p_dat, p_len)                      \
    do {                                                       \
        uint8_t  *cursor = (p_dat);                            \
        uint16_t *offset = &(p_len);

/**
 * @brief 向缓冲区追加一个字节
 * @param byte 待写入的数据
 * @note  写入完成后偏移量自动加 1。
 */
#define _CURSOR_WRITE_BYTE(byte)                               \
    do {                                                       \
        cursor[(*offset)++] = (uint8_t)(byte);                 \
    } while (0)

/**
 * @brief 跳过一个字节位置
 * @note  仅增加偏移量，不修改缓冲区原有数据。
 *        适用于预留 CRC、长度字段等位置。
 */
#define _CURSOR_WRITE_BYTE_NULL()                              \
    do {                                                       \
        (*offset)++;                                           \
    } while (0)

/**
 * @brief 修改已写入区域中的指定字节
 * @param pin  需要修改的缓冲区索引
 * @param byte 待写入的数据
 * @note  仅允许修改当前偏移量之前的数据。
 */
#define _CURSOR_WRITE_BYTE_SPECIFY(pin, byte)                  \
    do {                                                       \
        if ((pin) < (*offset))                                 \
        {                                                      \
            cursor[(pin)] = (uint8_t)(byte);                   \
        }                                                      \
    } while (0)

/**
 * @brief 向缓冲区追加指定长度的数据
 * @param dat 待写入数据数组或指针
 * @param len 待写入的数据长度，单位 Byte
 * @note  按顺序复制数据，完成后更新偏移量。
 */
#define _CURSOR_WRITE_DATA(dat, len)                           \
    do {                                                       \
        uint16_t _cursor_len = (uint16_t)(len);                \
        for (uint16_t i = 0U; i < _cursor_len; i++)            \
        {                                                      \
            cursor[(*offset)++] = (dat)[i];                    \
        }                                                      \
    } while (0)

/**
 * @brief 将变量的内存数据追加到缓冲区
 * @param var 待写入的变量
 * @note  按变量在内存中的字节顺序写入。
 *        多字节整数受 MCU 字节序影响。
 */
#define _CURSOR_WRITE_VAR(var)                                 \
    do {                                                       \
        const uint8_t *_cursor_ptr =(const uint8_t *)&(var);   \
        for (uint16_t i = 0U; i < sizeof(var); i++)            \
        {                                                      \
            cursor[(*offset)++] = _cursor_ptr[i];              \
        }                                                      \
    } while (0)

/**
 * @brief 结束游标写入操作
 * @note  与 _START_CURSOR() 配对，结束局部作用域。
 */
#define _STOP_CURSOR()                                         \
    } while (0)



#endif

