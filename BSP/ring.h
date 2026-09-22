/**
  ******************************************************************************
  * @file    ring.h
  * @brief   这个文件包含了所有ring.c的结构体和enum定义，以及函数声明。
  ******************************************************************************
  * @attention
  * 该文件依赖于HAL库中的UART和DMA和中断功能，因此需要在工程中启用UART和DMA，并且在CubeMX
  * 中配置好相关的UART和DMA参数。
  ******************************************************************************
  */
#ifndef RING_H
#define RING_H

#include "main.h"
#include "usart.h"
#include <string.h>

#define RING_FRAME_HEAD_1 0xAA
#define RING_FRAME_HEAD_2 0x55
#define RING_BUFFER_SIZE 512
#define FRAME_DATA_MAX 512

// 解析帧状态
typedef enum {
    RING_WAITING_HEAD_1,
    RING_WAITING_HEAD_2,
    RING_LENGTH,
    RING_CMD,
    RING_DATA,
    RING_CHECKSUM,
    RING_FRAME_COMPLETE
} Ring_Frame_State_TypeDef;

typedef enum {
    RING_OK = 0,
    RING_ERROR,
    RING_UNCOMPLETED,
    RING_UNREADY,
} RING_STATUS_TypeDef;

typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    uint16_t length;
    uint16_t read_pos;
    uint8_t overflow_flag;
} Ring_Buffer_TypeDef;

typedef struct {
    uint8_t state;
    uint16_t length;
    uint8_t cmd;
    uint8_t data[FRAME_DATA_MAX];
    uint8_t data_index;
    uint8_t checksum;
} Ring_Frame_TypeDef;

extern uint8_t rx_pending;
extern volatile Ring_Buffer_TypeDef ring;
extern volatile Ring_Frame_TypeDef frame;
extern volatile uint8_t frame_ready;

void ring_init(void);
RING_STATUS_TypeDef ring_start(void);
RING_STATUS_TypeDef ring_parse_byte(uint8_t b);
RING_STATUS_TypeDef ring_frame_get_data(uint8_t *data, uint16_t *length);
RING_STATUS_TypeDef ring_frame_get_cmd(uint8_t *cmd);
RING_STATUS_TypeDef ring_proc(void);
RING_STATUS_TypeDef ring_frame_reset(void);
RING_STATUS_TypeDef ring_frame_get_cmdAdata(uint8_t *cmd, uint8_t *data, uint16_t *length);
#endif

/*
*/