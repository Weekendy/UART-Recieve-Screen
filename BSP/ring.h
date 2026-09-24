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
#include "YFframe.h"

#define RING_BUFFER_SIZE 512

// ring文件的函数返回状态
typedef enum {
    RING_OK = 0,
    RING_ERROR,
    RING_UNCOMPLETED,
    RING_UNREADY,
} RING_STATUS_TypeDef;

// 环形缓冲区结构体
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    uint16_t length;
    uint16_t read_pos;
    uint8_t overflow_flag;
} Ring_TypeDef;

extern volatile uint8_t rx_pending;

void ring_init(void);
RING_STATUS_TypeDef ring_start(void);
RING_STATUS_TypeDef ring_proc(void);
#endif
