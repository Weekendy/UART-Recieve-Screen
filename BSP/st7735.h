#ifndef BSP_ST7735_H
#define BSP_ST7735_H

#include "main.h"
#include "spi.h"
#include "tim.h"
#include <stdint.h>

/*
 * 板级硬件配置。
 * 可在包含本头文件之前重新定义以下宏，或直接修改默认值，使其与
 * CubeMX 生成的 SPI、定时器、GPIO 和 DMA 句柄保持一致。
 * ST7735_DMA_IRQ_HANDLER 必须与 CubeMX 中启用的 DMA 中断向量一致。
 */
/** ST7735 所使用的 SPI 句柄。 */
#ifndef ST7735_SPI_HANDLE
#define ST7735_SPI_HANDLE hspi1
#endif
/** ST7735 所使用的 SPI 外设实例，用于判断 DMA 回调是否属于本屏幕。 */
#ifndef ST7735_SPI_INSTANCE
#define ST7735_SPI_INSTANCE SPI1
#endif
/** 屏幕片选 CS 引脚所在的 GPIO 端口。 */
#ifndef ST7735_CS_GPIO_PORT
#define ST7735_CS_GPIO_PORT CS_GPIO_Port
#endif
/** 屏幕片选 CS 引脚。CS 为低电平时选中屏幕。 */
#ifndef ST7735_CS_PIN
#define ST7735_CS_PIN CS_Pin
#endif
/** 数据/命令选择 DC 引脚所在的 GPIO 端口。 */
#ifndef ST7735_DC_GPIO_PORT
#define ST7735_DC_GPIO_PORT DC_GPIO_Port
#endif
/** 数据/命令选择 DC 引脚；低电平发送命令，高电平发送数据。 */
#ifndef ST7735_DC_PIN
#define ST7735_DC_PIN DC_Pin
#endif
/** 控制屏幕背光的定时器句柄。 */
#ifndef ST7735_BACKLIGHT_TIMER_HANDLE
#define ST7735_BACKLIGHT_TIMER_HANDLE htim1
#endif
/** 控制屏幕背光的 PWM 通道。 */
#ifndef ST7735_BACKLIGHT_TIMER_CHANNEL
#define ST7735_BACKLIGHT_TIMER_CHANNEL TIM_CHANNEL_3
#endif
/** 控制屏幕背光的 GPIO 端口。 */
#ifndef ST7735_BL_GPIO_PORT
#define ST7735_BL_GPIO_PORT BL_GPIO_Port
#endif
/** 控制屏幕背光的 GPIO 引脚。 */
#ifndef ST7735_BL_PIN
#define ST7735_BL_PIN BL_Pin
#endif
/** 数据/命令选择 DC 引脚；低电平发送命令，高电平发送数据。 */
#ifndef ST7735_DC_PIN
#define ST7735_DC_PIN DC_Pin
#endif
/* Default PWM compare value. Set to 0 for an active-low backlight. */
#ifndef ST7735_BACKLIGHT_DEFAULT_COMPARE
#define ST7735_BACKLIGHT_DEFAULT_COMPARE 0xFFFFU
#endif
/** SPI 发送 DMA 通道句柄。 */
#ifndef ST7735_DMA_HANDLE
#define ST7735_DMA_HANDLE hdma_spi1_tx
#endif
/** SPI 发送 DMA 完成中断处理函数名。 */
#ifndef ST7735_DMA_IRQ_HANDLER
#define ST7735_DMA_IRQ_HANDLER DMA1_Channel3_IRQHandler
#endif

/**
 * @file    st7735.h
 * @brief   ST7735 TFT 液晶屏驱动接口。
 * @note    坐标以屏幕左上角为原点，颜色采用 RGB565 格式。
 */

/** 屏幕的逻辑宽度，单位：像素。 */
#define ST7735_WIDTH       128U
/** 屏幕的逻辑高度，单位：像素。 */
#define ST7735_HEIGHT      160U
/** 控制器列地址相对于逻辑坐标的偏移量。 */
#define ST7735_XSTART      2U
/** 控制器行地址相对于逻辑坐标的偏移量。 */
#define ST7735_YSTART      1U

/**
 * @brief 是否使用独立的硬件复位引脚。
 * @note 当前工程未连接 RST GPIO，值为 0 时使用软件复位命令。
 *       若改为 1，还必须定义 ST7735_RST_GPIO_Port 和 ST7735_RST_Pin。
 */
#ifndef ST7735_USE_RESET_PIN
#define ST7735_USE_RESET_PIN 0
#endif
/* 启用硬件复位时，还必须定义 ST7735_RST_GPIO_PORT 和 ST7735_RST_PIN。 */

/** RGB565 黑色。 */
#define ST7735_BLACK   0x0000U
/** RGB565 蓝色。 */
#define ST7735_BLUE    0x001FU
/** RGB565 红色。 */
#define ST7735_RED     0xF800U
/** RGB565 绿色。 */
#define ST7735_GREEN   0x07E0U
/** RGB565 青色。 */
#define ST7735_CYAN    0x07FFU
/** RGB565 品红色。 */
#define ST7735_MAGENTA 0xF81FU
/** RGB565 黄色。 */
#define ST7735_YELLOW  0xFFE0U
/** RGB565 白色。 */
#define ST7735_WHITE   0xFFFFU

#define ST7735_COLOR565(r, g, b) \
    (uint16_t)((((r) & 0xF8U) << 8) | (((g) & 0xFCU) << 3) | (((b) & 0xF8U) >> 3))

void ST7735_init(void);

void ST7735_setBacklight(uint16_t compare);
void ST7735_drawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7735_fillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_fillRectangleFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_fillScreen(uint16_t color);
HAL_StatusTypeDef ST7735_fillScreenDMA(uint16_t color);
HAL_StatusTypeDef ST7735_drawImageDMA(uint16_t x, uint16_t y, uint16_t w,
                                      uint16_t h, const uint8_t *rgb565_be);
uint8_t ST7735_isBusy(void);
void ST7735_waitForDMA(void);
void ST7735_drawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data);
void ST7735_drawChar(uint16_t x, uint16_t y, char ch, uint8_t size,
                     uint16_t color, uint16_t bgcolor);
void ST7735_writeString(uint16_t x, uint16_t y, const char *text, uint8_t size,
                        uint16_t color, uint16_t bgcolor);
void ST7735_showInteger(uint16_t x, uint16_t y, int32_t value, uint8_t min_digits,
                        uint8_t size, uint16_t color, uint16_t bgcolor);
void ST7735_showDecimal(uint16_t x, uint16_t y, float value,
                        uint8_t min_integer_digits, uint8_t decimal_digits,
                        uint8_t size, uint16_t color, uint16_t bgcolor);
void ST7735_invertColors(uint8_t invert);
void ST7735_setGamma(uint8_t gamma);

#endif
