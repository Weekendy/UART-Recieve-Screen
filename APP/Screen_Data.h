#ifndef SCREEN_DATA_H
#define SCREEN_DATA_H

#include "main.h"
#include "YFframe.h"

#define CHAR_WIDTH 6
#define CHAR_HEIGHT 8

void Screen_Data_init(void);
void Screen_Data_show_data(const uint8_t *cmd, YFframe_Data_Text_TypeDef *text);

#endif /* SCREEN_DATA_H */
