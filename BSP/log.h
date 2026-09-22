/**
 * @file log.h
 * @brief 调试串口日志及简单数据帧发送接口。
 *
 * 日志发送为阻塞式；格式化仅支持 d/i/u/x/X/b/c/s/%%。
 */
#ifndef LOG_H
#define LOG_H

#include "usart.h"

#include <stdint.h>

#define LOG_UART_HANDLE       huart1
#define LOG_TX_TIMEOUT_MS     100U
#define LOG_TEXT_BUFFER_SIZE  192U

#ifndef PACK_HEAD
#define PACK_HEAD LOG_PACK_HEAD
#endif

#ifndef PACK_TAIL
#define PACK_TAIL LOG_PACK_TAIL
#endif

/** @brief 允许日志 UART 发送；需在 CubeMX 完成串口初始化后调用。 */
void log_init(void);
/** @brief 使用受限格式化器阻塞发送日志，不自动换行。 */
void log_printf(const char *format, ...);
/** @brief 使用受限格式化器阻塞发送日志并追加 CRLF。 */
void log_println(const char *format, ...);
/** @brief 发送原始字节流。 */
void my_uart_send(const uint8_t *data, uint16_t length);
/** @brief 发送以 NUL 结尾的字符串。 */
void my_uart_send_str(const char *text);

#endif
