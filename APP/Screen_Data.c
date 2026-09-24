#include "Screen_Data.h"

#include "ring.h"
#include "st7735.h"
#include <stdarg.h>
void Screen_Data_init(void)
{
    ST7735_init();
    ST7735_fill_screen(ST7735_BLACK);
}

static size_t safe_cat(char *dst, size_t dstsz, size_t used, const char *s)
{
    if (used + 1 >= dstsz) return used;
    while (*s && used + 1 < dstsz)
        dst[used++] = *s++;
    dst[used] = '\0';
    return used;
}
size_t str_joinv(char *dst, size_t dstsz, const char *sep, int n, ...)
{
    va_list ap;
    size_t used = 0;
    dst[0] = '\0';

    va_start(ap, n);
    for (int i = 0; i < n; i++) {
        const char *s = va_arg(ap, const char *);
        if (used > 0) used = safe_cat(dst, dstsz, used, sep);
        used = safe_cat(dst, dstsz, used, s);
    }
    va_end(ap);
    return used;
}

// @mark: show_data函数有待优化，封装好main函数里比较复杂的逻辑。
void Screen_Data_show_data(const uint8_t *cmd, YFframe_Data_Text_TypeDef *text)
{
    static uint8_t data_x = 0, data_y = 0;

    ST7735_show_string(data_x, data_y, "cmd:", 1, ST7735_WHITE, ST7735_BLACK);
    data_x += CHAR_WIDTH * 4;
    ST7735_show_integer(data_x, data_y, (int32_t)*cmd, 1, 1, ST7735_WHITE, ST7735_BLACK);
    data_y += CHAR_HEIGHT;
    data_x = 0;
    for(uint8_t i = 0; i < text->data_length; i++)
    {
        // @mark: 这里实现了简易的换行实现，可以考虑封装函数。
        ST7735_show_string(data_x,data_y, text->data_type_text[i], 1, ST7735_WHITE, ST7735_BLACK);
        data_x = CHAR_WIDTH * 5;
        ST7735_show_string(data_x, data_y, text->data_text[i], 1, ST7735_WHITE, ST7735_BLACK);
        data_x = 0;
        data_y += CHAR_HEIGHT;
        // @mark: 未实现清屏操作，旧数据会滞留在屏幕上，需更改。
    }

    data_x = 0;
    data_y = 0;
}