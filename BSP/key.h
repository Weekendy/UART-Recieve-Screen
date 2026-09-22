/**
 * @file key.h
 * @brief GPIO 按键去抖、短按、长按和双击事件接口。
 *
 * 应在主循环中持续调用 key_scan()；事件仅在状态转换时返回一次。
 */
#ifndef KEY_H
#define KEY_H

#include "main.h"

#include <stdint.h>

#ifndef KEY_DEBOUNCE_MS
#define KEY_DEBOUNCE_MS      10U
#endif

#ifndef KEY_LONG_PRESS_MS
#define KEY_LONG_PRESS_MS    500U
#endif

#ifndef KEY_DOUBLE_CLICK_MS
#define KEY_DOUBLE_CLICK_MS  150U
#endif

typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_SHORT_PRESS,
    KEY_EVENT_LONG_PRESS,
    KEY_EVENT_DOUBLE_CLICK,
} Key_Event_Typedef;

typedef struct {
    GPIO_TypeDef *gpio_port;
    uint16_t gpio_pin;
    GPIO_PinState active_state;
    uint8_t state;
    uint8_t long_reported;
    uint8_t waiting_second_click;
    uint32_t debounce_start_ms;
    uint32_t press_start_ms;
    uint32_t click_wait_start_ms;
} Key_Handle_Typedef;

/** @brief 初始化按键句柄；active_state 指定按下时的 GPIO 电平。 */
void key_init(Key_Handle_Typedef *key, GPIO_TypeDef *gpio_port, uint16_t gpio_pin, GPIO_PinState active_state);
/** @brief 扫描按键状态机。 @return 当前时刻产生的单次事件，未触发时返回 KEY_EVENT_NONE。 */
Key_Event_Typedef key_scan(Key_Handle_Typedef *key);
/** @brief 兼容的短按查询接口；短按事件仅返回一次。 */
uint8_t key_get_state(Key_Handle_Typedef *key);

/* Compatibility names used by the pre-migration control module. */
typedef Key_Handle_Typedef Key_TypeDef;
#define KEY_PRESS   1U
#define KEY_RELEASE 0U
uint8_t key_getstate(Key_TypeDef *key);

#endif
