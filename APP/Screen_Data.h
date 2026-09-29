#ifndef SCREEN_DATA_H
#define SCREEN_DATA_H

#include "main.h"
#include "YFframe.h"
#include "ring.h"

#define CHAR_WIDTH 6
#define CHAR_HEIGHT 8
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 160

void Screen_Data_init(void);
void Screen_Data_show_data(const uint8_t *cmd, YFframe_Data_Text_TypeDef *text);
void Screen_Data_show_data_raw(Ring_Raw_Data_TypeDef *raw);
void Screen_Data_show_FPS();

#endif /* SCREEN_DATA_H */
