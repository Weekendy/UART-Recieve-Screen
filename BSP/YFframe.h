/**
  ******************************************************************************
  * @file    YFframe.h
  * @brief   这个文件包含了所有YFframe.c的结构体和enum定义，以及函数声明。
  ******************************************************************************
  * @attention
  * 无
  ******************************************************************************
  */

#ifndef YFFRAME_H
#define YFFRAME_H 

#include "q_format.h"
#include <string.h>

#define RING_FRAME_HEAD_1 0xAA
#define RING_FRAME_HEAD_2 0x55
#define YFFRAME_DATA_MAX 0x40
#define YFFRAME_DATA_TEXT_MAX 24

// YFframe文件的函数返回状态
typedef enum {
    YFFRAME_OK = 0,
    YFFRAME_ERROR,
    YFFRAME_UNCOMPLETED,
    YFFRAME_UNREADY,
} YFframe_Status_TypeDef;

// 解析帧状态
typedef enum {
    YFFRAME_WAITING_HEAD_1,
    YFFRAME_WAITING_HEAD_2,
    YFFRAME_LENGTH,
    YFFRAME_CMD,
    YFFRAME_DATA_TYPE,
    YFFRAME_DATA,
    YFFRAME_CHECKSUM,
    YFFRAME_FRAME_COMPLETE
} YFframe_Parse_State_TypeDef;

// 帧数据类型枚举
typedef enum
{
    YFFRAME_U8_TYPE,
    YFFRAME_I8_TYPE,
    YFFRAME_U16_TYPE,
    YFFRAME_I16_TYPE,
    YFFRAME_U32_TYPE,
    YFFRAME_I32_TYPE,
    YFFRAME_FLOAT_TYPE,
} YFframe_Type_TypeDef;

// 帧数据联合体
typedef union
{
    uint8_t u8_data;
    int8_t i8_data;
    uint16_t u16_data;
    int16_t i16_data;
    uint32_t u32_data;
    int32_t i32_data;
    float float_data;
    uint8_t raw[4];
} YFframe_Data_Type_TypeDef;

// 帧数据存储结构体
typedef struct
{
    YFframe_Type_TypeDef type;
    YFframe_Data_Type_TypeDef load;
    uint8_t load_index;
} YFframe_Data_TypeDef;

// 帧结构体
typedef struct {
    uint8_t state;
    uint16_t length;
    uint8_t cmd;
    YFframe_Data_TypeDef data[YFFRAME_DATA_MAX];
    uint8_t data_index;
    uint8_t checksum;
} YFframe_TypeDef;

// 帧数据文本结构体
typedef struct {
    char data_text[YFFRAME_DATA_MAX][YFFRAME_DATA_TEXT_MAX];
    char data_type_text[YFFRAME_DATA_MAX][YFFRAME_DATA_TEXT_MAX];
    uint8_t data_length;
} YFframe_Data_Text_TypeDef;

extern volatile uint8_t frame_ready;
extern volatile YFframe_TypeDef ready_frame;
extern const char YYFRAME_DATA_TYPE_TEXT[7][10];

YFframe_Status_TypeDef YFframe_init(void);
YFframe_Status_TypeDef YFframe_parse_byte(uint8_t b);
YFframe_Status_TypeDef YFframe_get_data_text(YFframe_Data_Text_TypeDef *text);
YFframe_Status_TypeDef YFframe_get_cmd(uint8_t *cmd);
YFframe_Status_TypeDef YFframe_reset(void);
YFframe_Status_TypeDef YFframe_get_cmdAdata(uint8_t *cmd, YFframe_Data_Text_TypeDef *text);
#endif
