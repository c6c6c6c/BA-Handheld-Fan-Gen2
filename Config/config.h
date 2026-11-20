/**
 ***********************************************************************************************************************
 * @file    config.h
 * @brief   固件版本、编译日期及设备型号配置
 ***********************************************************************************************************************
 */

#ifndef CONFIG_H
#define CONFIG_H

/** 固件版本字符串，发布新固件时手动更新。 */
#define FIRMWARE_VERSION       "V1.0.0"

/** 编译日期的数字分量，由编译器自动生成。 */
#define FIRMWARE_BUILD_YEAR    ((__DATE__[7] - '0') * 1000U + \
                                (__DATE__[8] - '0') * 100U  + \
                                (__DATE__[9] - '0') * 10U   + (__DATE__[10] - '0'))

#define FIRMWARE_BUILD_MONTH   ((__DATE__[0] == 'J' && __DATE__[1] == 'a') ? 1U : \
                                (__DATE__[0] == 'F') ? 2U : \
                                (__DATE__[0] == 'M' && __DATE__[2] == 'r') ? 3U : \
                                (__DATE__[0] == 'A' && __DATE__[1] == 'p') ? 4U : \
                                (__DATE__[0] == 'M') ? 5U : \
                                (__DATE__[0] == 'J' && __DATE__[2] == 'n') ? 6U : \
                                (__DATE__[0] == 'J') ? 7U : \
                                (__DATE__[0] == 'A') ? 8U : \
                                (__DATE__[0] == 'S') ? 9U : \
                                (__DATE__[0] == 'O') ? 10U : \
                                (__DATE__[0] == 'N') ? 11U : 12U)

#define FIRMWARE_BUILD_DAY     (((__DATE__[4] == ' ') ? 0U : (__DATE__[4] - '0') * 10U) + \
                               (__DATE__[5] - '0'))

/** 数字编译日期，格式 YYYYMMDD，可使用 uint32_t 保存。 */
#define FIRMWARE_BUILD_TIME    (FIRMWARE_BUILD_YEAR * 10000U + \
                                FIRMWARE_BUILD_MONTH * 100U + FIRMWARE_BUILD_DAY)

/** 设备型号字符串。 */
#define DEVICE_MODEL          "BA Fan Gen2 - MIST 30% - WDG"

#endif /* CONFIG_H */
