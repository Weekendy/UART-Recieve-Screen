#ifndef RECEIVE_DATA_H
#define RECEIVE_DATA_H

#include "main.h"

#define RECEIVE_DATA_BUFFER_SIZE (0xFF+1)
#define RECEIVE_DATA_HEAD 0x5A
#define RECEIVE_DATA_TAIL 0xA5
#define RECEIVE_DATA_TYPE_NUM 5
#define RECEIVE_DATA_EVERY_LENGTH 10
#define RECEIVE_DATA_RETRY_DELAY_MS 10U
#define RECEIVE_DATA_ERROR_NUM 6

extern uint8_t RecDat_accomplish_flag;
extern uint16_t RecDat_frame_count;
extern uint8_t g_RecDat_data_size[RECEIVE_DATA_TYPE_NUM];
extern uint8_t u8_data[RECEIVE_DATA_EVERY_LENGTH];
extern uint16_t u16_data[RECEIVE_DATA_EVERY_LENGTH];
extern int16_t i16_data[RECEIVE_DATA_EVERY_LENGTH];
extern int32_t int_data[RECEIVE_DATA_EVERY_LENGTH];
extern float float_data[RECEIVE_DATA_EVERY_LENGTH];
extern uint8_t display_data_size[RECEIVE_DATA_TYPE_NUM];
extern uint8_t display_u8[RECEIVE_DATA_EVERY_LENGTH];
extern uint16_t display_u16[RECEIVE_DATA_EVERY_LENGTH];
extern int16_t display_i16[RECEIVE_DATA_EVERY_LENGTH];
extern int32_t display_int[RECEIVE_DATA_EVERY_LENGTH];
extern float display_float[RECEIVE_DATA_EVERY_LENGTH];

typedef struct
{
    volatile uint16_t receive_size;
    uint8_t head_index;
    uint8_t tail_index;
    volatile uint16_t data_size;
} Receive_Data_Package_TypeDef;

typedef enum
{
    RecDat_HEAD_ERROR = 0,
    RecDat_TAIL_ERROR = 1,
    RecDat_SIZE_ERROR = 2,
    RecDat_BROKEN_ERROR = 3,
    RecDat_CHECK_ERROR = 4,
    RecDat_UART_ERROR = 5,
} Receive_Data_Error_TypeDef;

typedef enum
{
    RecDat_TYPE_U8 = 0,
    RecDat_TYPE_U16 = 1,
    RecDat_TYPE_I16 = 2,
    RecDat_TYPE_INT = 3,
    RecDat_TYPE_FLOAT = 4,
} Receive_Data__Data_Type_TypeDef;

typedef enum
{
    RecDat_IDLE_STATE = 0,
    RecDat_RECEIVE_STATE = 1,
    RecDat_PROC_STATE = 2,
    RecDat_ACCOMPLISH_STATE = 3
} Receive_Data_State_TypeDef;

typedef enum
{
    RecDat_PARSE_HEAD = 0,
    RecDat_PARSE_SIZE = 1,
    RecDat_PARSE_DATA_TYPE = 2,
    RecDat_PARSE_DATA = 3,
    RecDat_PARSE_CHECK = 4,
    RecDat_PARSE_TAIL = 5
} Receive_Data_Parse_TypeDef;

typedef enum
{
    RECEIVE_DATA_MODE_PACKAGE = 0,
    RECEIVE_DATA_MODE_RAW = 1,
} Receive_Data_Mode_TypeDef;

void Receive_Data_init(void);
void Receive_Data_start(void);
uint8_t Receive_Data_is_receiving(void);
void Receive_Data_process(void);
void Receive_Data_set_mode(Receive_Data_Mode_TypeDef mode);
uint8_t Receive_Data_read_raw_byte(uint8_t *byte);
void Receive_Data_pause(void);
void Receive_Data_resume(void);

void Receive_Data_reset(void);

#endif // !RECEIVE_DATA_H
