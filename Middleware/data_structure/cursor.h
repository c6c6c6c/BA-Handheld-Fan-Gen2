
/**
 ***********************************************************************************************************************
 * @file    cursor.h
 * @brief   数据缓冲区游标管理接口，实现数据写入、读取、跳过及指定位置访问
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-30
 ***********************************************************************************************************************
 * @note
 *          1. 支持数据缓冲区连续写入和读取。
 *          2. 支持单字节、数组及变量类型的数据操作。
 *          3. 支持跳过字节及指定位置访问。
 *          4. 多字节变量按照 MCU 内存字节序进行读写。
 *          5. 调用方必须保证缓冲区容量及数据长度合法。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _CURSOR_H_
#define _CURSOR_H_

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>


/* Cursor Write Macro ------------------------------------------------------------------------------------------------*/

/**
 * @brief 启动数据缓冲区写入游标
 * @param p_dat 数据缓冲区首地址
 * @param p_len 当前写入长度变量，单位 Byte
 * @note  创建局部写入指针及偏移量指针。
 *        必须与 _STOP_CURSOR_WRITE() 配对使用。
 *        p_len 必须为 uint16_t 类型的可修改变量。
 */
#define _START_CURSOR_WRITE(p_dat, p_len)                      \
    do {                                                       \
        uint8_t  *cursor = (uint8_t *)(p_dat);                 \
        uint16_t *offset = &(p_len);

/**
 * @brief 向缓冲区追加一个字节
 * @param byte 待写入的数据
 * @note  写入完成后偏移量自动增加 1。
 */
#define _CURSOR_WRITE_BYTE(byte)                               \
    do {                                                       \
        cursor[(*offset)++] = (uint8_t)(byte);                 \
    } while (0)

/**
 * @brief 写入一个占位字节
 * @note  在当前游标位置写入 0x00，
 *        并将偏移量增加 1。
 *        适用于预留协议长度、CRC 等字段。
 */
#define _CURSOR_WRITE_BYTE_NULL()                              \
    do {                                                       \
        cursor[(*offset)++] = 0U;                              \
    } while (0)

/**
 * @brief 修改已写入区域中指定位置的字节
 * @param pin  目标字节索引
 * @param byte 待写入的数据
 * @note  仅允许修改当前偏移量之前的字节。
 *        不修改当前写入偏移量。
 */
#define _CURSOR_WRITE_BYTE_SPECIFY(pin, byte)                  \
    do {                                                       \
        uint16_t _cursor_pin = (uint16_t)(pin);                \
        if (_cursor_pin < (*offset))                           \
        {                                                      \
            cursor[_cursor_pin] = (uint8_t)(byte);             \
        }                                                      \
    } while (0)

/**
 * @brief 向缓冲区追加指定长度的数据
 * @param dat 待写入数据缓冲区首地址
 * @param len 待写入的数据长度，单位 Byte
 * @note  使用 memcpy() 批量复制数据，
 *        写入完成后偏移量增加 len。
 */
#define _CURSOR_WRITE_DATA(dat, len)                           \
    do {                                                       \
        uint16_t _cursor_len = (uint16_t)(len);                \
        memcpy(&cursor[*offset], (dat), _cursor_len);          \
        (*offset) += _cursor_len;                              \
    } while (0)

/**
 * @brief 将变量的内存数据追加至缓冲区
 * @param var 待写入变量
 * @note  自动根据 sizeof(var) 确定写入长度。
 *        数据按照变量在 MCU 内存中的字节顺序写入。
 *        var 必须为可取地址的对象。
 */
#define _CURSOR_WRITE_VAR(var)                                 \
    do {                                                       \
        uint16_t _cursor_len = (uint16_t)sizeof(var);          \
        memcpy(&cursor[*offset], &(var), _cursor_len);         \
        (*offset) += _cursor_len;                              \
    } while (0)

/**
 * @brief 结束数据缓冲区写入操作
 * @note  必须与 _START_CURSOR_WRITE() 配对使用。
 */
#define _STOP_CURSOR_WRITE()                                   \
    } while (0)


/* Cursor Read Macro -------------------------------------------------------------------------------------------------*/

/**
 * @brief 启动数据缓冲区读取游标
 * @param p_dat 数据缓冲区首地址
 * @param p_len 当前读取偏移量变量，单位 Byte
 * @note  创建只读数据指针及偏移量指针。
 *        必须与 _STOP_CURSOR_READ() 配对使用。
 *        p_len 必须为 uint16_t 类型的可修改变量。
 */
#define _START_CURSOR_READ(p_dat, p_len)                       \
    do {                                                       \
        const uint8_t *cursor = (const uint8_t *)(p_dat);      \
        uint16_t *offset = &(p_len);                           \
        (void)cursor;                                          \
        (void)offset;

/**
 * @brief 从当前游标位置读取一个字节
 * @param byte 接收读取结果的变量
 * @note  读取成功后偏移量自动增加 1。
 */
#define _CURSOR_READ_BYTE(byte)                                \
    do {                                                       \
        (byte) = cursor[(*offset)++];                          \
    } while (0)

/**
 * @brief 跳过一个字节
 * @note  不读取缓冲区数据，仅将读取偏移量增加 1。
 */
#define _CURSOR_READ_BYTE_NULL()                               \
    do {                                                       \
        (*offset)++;                                           \
    } while (0)

/**
 * @brief 读取指定位置的一个字节
 * @param pin  目标字节索引
 * @param byte 接收读取结果的变量
 * @note  仅允许读取当前偏移量之前的数据。
 *        不改变当前读取偏移量。
 */
#define _CURSOR_READ_BYTE_SPECIFY(pin, byte)                   \
    do {                                                       \
        uint16_t _cursor_pin = (uint16_t)(pin);                \
        if (_cursor_pin < (*offset))                           \
        {                                                      \
            (byte) = cursor[_cursor_pin];                      \
        }                                                      \
    } while (0)

/**
 * @brief 从缓冲区读取指定长度的数据
 * @param dat 接收数据的缓冲区首地址
 * @param len 需要读取的数据长度，单位 Byte
 * @note  使用 memcpy() 批量复制数据，
 *        读取完成后偏移量增加 len。
 */
#define _CURSOR_READ_DATA(dat, len)                            \
    do {                                                       \
        uint16_t _cursor_len = (uint16_t)(len);                \
        memcpy((dat), &cursor[*offset], _cursor_len);           \
        (*offset) += _cursor_len;                              \
    } while (0)

/**
 * @brief 从缓冲区读取变量的内存数据
 * @param var 接收数据的变量
 * @note  自动根据 sizeof(var) 确定读取长度。
 *        按 MCU 内存字节序恢复变量原始数据。
 *        var 必须为可取地址且可修改的对象。
 */
#define _CURSOR_READ_VAR(var)                                  \
    do {                                                       \
        uint16_t _cursor_len = (uint16_t)sizeof(var);          \
        memcpy(&(var), &cursor[*offset], _cursor_len);          \
        (*offset) += _cursor_len;                              \
    } while (0)

/**
 * @brief 结束数据缓冲区读取操作
 * @note  必须与 _START_CURSOR_READ() 配对使用。
 */
#define _STOP_CURSOR_READ()                                    \
    } while (0)


#endif /* _CURSOR_H_ */
