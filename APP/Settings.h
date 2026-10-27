#ifndef SETTINGS_H
#define SETTINGS_H

#include "main.h"
#include "YFframe.h"

extern const uint32_t BAUDRATE_LSIT[];
extern const uint8_t BAUDRATE_LSIT_SIZE;
extern uint8_t baudrate_index;
extern uint8_t data_frame_enable;

typedef struct
{
    YFframe_Type_TypeDef type;
    uint8_t *u8_data;
    uint16_t *u16_data;
    uint32_t *u32_data;
} Frame_Data_Setting_TypeDef;

#endif // !SETTINGS_H
