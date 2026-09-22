#include "Screen_Data.h"

#include "ring.h"
#include "st7735.h"

void Screen_Data_init(void)
{
    ST7735_init();
    ST7735_fillScreen(ST7735_BLACK);
}

Screen_Data_t