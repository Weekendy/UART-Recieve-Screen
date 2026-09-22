/**
 * @file log.c
 * @brief 有界日志格式化和 UART 阻塞发送实现。
 */
#include "log.h"

#include <stdarg.h>
#include <stddef.h>

typedef struct {
    char data[LOG_TEXT_BUFFER_SIZE];
    uint16_t length;
} log_buffer_t;

static uint8_t g_log_ready;

static void append_char(log_buffer_t *buffer, char character)
{
    if (buffer->length < (LOG_TEXT_BUFFER_SIZE - 1U)) {
        buffer->data[buffer->length++] = character;
    }
}

static void append_text(log_buffer_t *buffer, const char *text)
{
    if (text == NULL) {
        text = "(null)";
    }
    while (*text != '\0') {
        append_char(buffer, *text++);
    }
}

static void append_unsigned(log_buffer_t *buffer, uint32_t value, uint8_t base, uint8_t upper_case)
{
    char digits[10];
    const char *alphabet = upper_case ? "0123456789ABCDEF" : "0123456789abcdef";
    uint8_t count = 0U;

    do {
        digits[count++] = alphabet[value % base];
        value /= base;
    } while ((value != 0U) && (count < sizeof(digits)));

    while (count != 0U) {
        append_char(buffer, digits[--count]);
    }
}

static void append_signed(log_buffer_t *buffer, int32_t value)
{
    uint32_t magnitude;
    if (value < 0) {
        append_char(buffer, '-');
        magnitude = (uint32_t)(-(value + 1)) + 1U;
    } else {
        magnitude = (uint32_t)value;
    }
    append_unsigned(buffer, magnitude, 10U, 0U);
}

/** @brief 将受支持的格式化子集追加至固定日志缓冲区，超长内容静默截断。 */
static void format_message(log_buffer_t *buffer, const char *format, va_list arguments)
{
    while ((format != NULL) && (*format != '\0')) {
        if (*format != '%') {
            append_char(buffer, *format++);
            continue;
        }

        format++;
        switch (*format) {
        case 'd':
        case 'i':
            append_signed(buffer, va_arg(arguments, int));
            break;
        case 'u':
            append_unsigned(buffer, va_arg(arguments, unsigned int), 10U, 0U);
            break;
        case 'x':
            append_unsigned(buffer, va_arg(arguments, unsigned int), 16U, 0U);
            break;
        case 'X':
            append_unsigned(buffer, va_arg(arguments, unsigned int), 16U, 1U);
            break;
        case 'b':
            append_unsigned(buffer, va_arg(arguments, unsigned int), 2U, 0U);
            break;
        case 'c':
            append_char(buffer, (char)va_arg(arguments, int));
            break;
        case 's':
            append_text(buffer, va_arg(arguments, const char *));
            break;
        case '%':
            append_char(buffer, '%');
            break;
        case '\0':
            append_char(buffer, '%');
            return;
        default:
            append_char(buffer, '%');
            append_char(buffer, *format);
            break;
        }
        format++;
    }
}

static void transmit_buffer(const log_buffer_t *buffer)
{
    if ((g_log_ready != 0U) && (buffer->length != 0U)) {
        (void)HAL_UART_Transmit(&LOG_UART_HANDLE, (uint8_t *)buffer->data, buffer->length, LOG_TX_TIMEOUT_MS);
    }
}

static void log_vprint(const char *format, va_list arguments, uint8_t add_newline)
{
    log_buffer_t buffer = {{0}, 0U};
    format_message(&buffer, format, arguments);
    if (add_newline != 0U) {
        append_char(&buffer, '\r');
        append_char(&buffer, '\n');
    }
    transmit_buffer(&buffer);
}

void log_init(void)
{
    g_log_ready = 1U;
}

void log_printf(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    log_vprint(format, arguments, 0U);
    va_end(arguments);
}

void log_println(const char *format, ...)
{
    va_list arguments;
    va_start(arguments, format);
    log_vprint(format, arguments, 1U);
    va_end(arguments);
}

void my_uart_send(const uint8_t *data, uint16_t length)
{
    if ((g_log_ready != 0U) && (data != NULL) && (length != 0U)) {
        (void)HAL_UART_Transmit(&LOG_UART_HANDLE, (uint8_t *)data, length, LOG_TX_TIMEOUT_MS);
    }
}

void my_uart_send_str(const char *text)
{
    uint16_t length = 0U;
    if (text == NULL) return;
    while ((text[length] != '\0') && (length < UINT16_MAX)) {
        length++;
    }
    my_uart_send((const uint8_t *)text, length);
}
