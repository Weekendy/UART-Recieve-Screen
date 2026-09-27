#include "Screen_Data.h"

#include "ring.h"
#include "st7735.h"
#include "string.h"
#include <stdarg.h>
#include "log.h"

const uint16_t SCREEN_ROWS = SCREEN_WIDTH / CHAR_WIDTH;
const uint16_t SCREEN_COLUMNS = SCREEN_HEIGHT / CHAR_HEIGHT;
static uint16_t data_x = 0, data_y = 0;

char cmd_buf[4];
static char data_text_buf[256];
static uint8_t last_data_length = 0;
static uint8_t last_data_text_str_length[ST7735_HEIGHT / CHAR_HEIGHT];
static size_t data_text_used = 0U;

static uint32_t FPS_update_ms = 0;
static uint32_t FPS_count = 0;
static char FPS_buf[10];

/***
 * @brief 屏幕数据显示初始化
 */
void Screen_Data_init(void)
{
    ST7735_init();
    ST7735_fill_screen(ST7735_BLACK);
}

/***
 * @brief 安全的字符串拼接函数
 */

static size_t safe_cat(char *dst, size_t dstsz, size_t used, const char *s)
{
    if (used + 1 >= dstsz) return used;
    while (*s && used + 1 < dstsz)
        dst[used++] = *s++;
    dst[used] = '\0';
    return used;
}

/***
 * @brief 重复连接字符串函数
 * @param dst 目标字符串
 * @param dstsz 目标字符串最大长度
 * @param sep 分割符
 * @param n 参数个数
 * @param ... 参数
 * @retval 返回拼接后的字符串长度
 */
size_t str_joinv(char *dst, size_t dstsz, size_t used, const char *sep, int n, ...)
{
    va_list ap;

    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const char *s = va_arg(ap, const char *);
        if (used > 0) used = safe_cat(dst, dstsz, used, sep);
        used = safe_cat(dst, dstsz, used, s);
    }
    va_end(ap);
    return used;
}

/***
 * @brief 清空字符串缓冲区
 */
static void text_buf_clear(void)
{
    data_text_buf[0] = '\0';
    data_text_used = 0;
}
/***
 * @brief 追加字符串到缓冲区
 * @param s 字符串
 */
static size_t text_buf_append(const char *s)
{
    return safe_cat(data_text_buf, sizeof(data_text_buf), data_text_used, s);
}

/***
 * @brief 将u32转化为字符串
 * @param text 字符串指针
 * @param capacity 字符串容量
 * @param value u32值
 */
static void Screen_Data_write_u32(char *text, uint8_t capacity, uint32_t value)
{
    char digits[10];
    uint8_t count = 0U;
    uint8_t index = 0U;

    if (capacity == 0U)
    {
        return;
    }

    do
    {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(digits)));

    while ((count != 0U) && (index + 1U < capacity))
    {
        text[index++] = digits[--count];
    }
    text[index] = '\0';
}

void Screen_Data_show_data(const uint8_t *cmd, YFframe_Data_Text_TypeDef *text)
{
    static uint32_t show_ms = 0;
    data_x = 0;
    data_y = 0;
    text_buf_clear();

    // 为文本缓冲区添加命令部分
    Screen_Data_write_u32(cmd_buf, sizeof(cmd_buf), *cmd);
    data_text_used = str_joinv(data_text_buf, sizeof(data_text_buf), data_text_used, "", 3, "cmd = ", cmd_buf, "\n");

    // 为文本缓冲区添加数据部分
    for(uint8_t i = 0; i < text->data_length; i++)
    {
        data_text_used = str_joinv(data_text_buf, sizeof(data_text_buf), data_text_used, "", 2, text->data_type_text[i], text->data_text[i]);
        // 清除上一个数据包相比的多余列
        int8_t times = last_data_text_str_length[i] - strlen(text->data_text[i]);
        for(int8_t i = 0; i < times; i++)
        {
            data_text_used = text_buf_append("\x20");
        }
        last_data_text_str_length[i] = strlen(text->data_text[i]);
        data_text_used = text_buf_append("\n");
    }

    //　清除与上一个数据包相比的多余行
    if(text->data_length < last_data_length)
    {
        uint16_t clear_rows = last_data_length - text->data_length;
        ST7735_fill_rectangle(0, (text->data_length + 1) * CHAR_HEIGHT, SCREEN_WIDTH, clear_rows * CHAR_HEIGHT, ST7735_BLACK);
    }

    // 显示数据
    ST7735_show_string_DMA(data_x, data_y, data_text_buf, 1, ST7735_WHITE, ST7735_BLACK);
    last_data_length = text->data_length;
    FPS_count++;


    uint32_t ms = show_ms;
    // log_println("%d\r\n", HAL_GetTick() - ms);
    show_ms = HAL_GetTick();
}

void Screen_Data_show_FPS()
{
    if(HAL_GetTick() > FPS_update_ms + 1000)
    {
        FPS_update_ms = HAL_GetTick();

        // 显示FPS
        static char FPS_count_buf[5];
        Screen_Data_write_u32(FPS_count_buf, sizeof(FPS_count_buf), FPS_count);
        str_joinv(FPS_buf, sizeof(FPS_buf), 0, "", 2, "FPS:", FPS_count_buf);
        ST7735_show_string(SCREEN_WIDTH - strlen(FPS_buf) * CHAR_WIDTH, SCREEN_HEIGHT - CHAR_HEIGHT, FPS_buf, 1, ST7735_WHITE, ST7735_BLACK); 
        
        FPS_count = 0;
    }
}

//@mark 帧率显示仍有残留，等待去除。
//实现原始数据显示
//ST7735在去除和引入DMA后注释仍需完善
//感觉速度还能往上提，现在是多7个数据包一起扔过去46帧，还算可观。