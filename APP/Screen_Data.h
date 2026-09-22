#ifndef SCREEN_DATA_H
#define SCREEN_DATA_H

#include "main.h"

void Screen_Data_clear(void);
void Screen_Data_display_package(void);
void Screen_Data_display_raw_byte(uint8_t byte);
void Screen_Data_reset_raw_display(void);
void Screen_Data_display_fps(uint32_t fps);
void Screen_Data_display_status(const char *status);

#endif /* SCREEN_DATA_H */
