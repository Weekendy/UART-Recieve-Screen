#include "Screen_Data.h"

#include "ring.h"
#include "st7735.h"
void Screen_Data_init(void)
{
    ST7735_init();
    ST7735_fillScreen(ST7735_BLACK);
}

void Screen_Data_display_data(uint8_t cmd, uint8_t *data, uint16_t length)
{
    if (data == NULL || length == 0)
    {
        return;
    }

    ST7735_fillScreen(ST7735_BLACK);
    ST7735_drawImage(0, 0, 128, 160, (uint16_t *)data);
}
