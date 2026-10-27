/**
  ******************************************************************************
  * @file   key.c
  * @brief  本文件实现了按键状态的检测功能，能够识别按键的短按、长按和双击事件，并提供去抖动处理。
  * 
  *          
  *         简要逻辑链路为：
  *         由 key_scan() 利用状态机解析按键状态，需要外部循环调用（消费者）。
  *         或者通过 key_get_state() 获取按键的电平状态。
  ******************************************************************************
  * @attention
  * 无
  * 
  * @note
  * 1. 将文件 key.c 和 key.h 添加到工程中。
  * 2. 在 main.c 中调用 key_init() 初始化按键。
  * 3. 在 main.c 中循环调用 key_scan() 来检测按键状态。
  * 4. 在 main.c 中通过 key_get_state() 获取按键的电平状态。
  ******************************************************************************
  */
#include "key.h"

typedef enum {
    KEY_IDLE,
    KEY_CONFIRM_PRESS,
    KEY_HELD,
    KEY_CONFIRM_RELEASE,
    KEY_AWAIT_SECOND_PRESS,
} key_state_t;

static uint8_t input_is_active(const Key_Handle_Typedef *key)
{
    return (HAL_GPIO_ReadPin(key->gpio_port, key->gpio_pin) == key->active_state) ? 1U : 0U;
}

/** @brief 用无符号减法比较时间间隔，安全处理 HAL tick 回绕。 */
static uint8_t interval_elapsed(uint32_t now, uint32_t start, uint32_t interval)
{
    return ((uint32_t)(now - start) >= interval) ? 1U : 0U;
}

void key_init(Key_Handle_Typedef *key, GPIO_TypeDef *gpio_port, uint16_t gpio_pin, GPIO_PinState active_state)
{
    if (key == NULL) return;

    key->gpio_port = gpio_port;
    key->gpio_pin = gpio_pin;
    key->active_state = active_state;
    key->state = (uint8_t)KEY_IDLE;
    key->long_reported = 0U;
    key->waiting_second_click = 0U;
    key->debounce_start_ms = 0U;
    key->press_start_ms = 0U;
    key->click_wait_start_ms = 0U;
}

Key_Event_Typedef key_scan(Key_Handle_Typedef *key)
{
    uint32_t now;
    uint8_t active;

    if ((key == NULL) || (key->gpio_port == NULL)) return KEY_EVENT_NONE;

    now = HAL_GetTick();
    active = input_is_active(key);

    switch ((key_state_t)key->state) {
    case KEY_IDLE:
        if (active != 0U) {
            key->debounce_start_ms = now;
            key->state = (uint8_t)KEY_CONFIRM_PRESS;
        }
        break;

    case KEY_CONFIRM_PRESS:
        if (active == 0U) {
            key->state = (key->waiting_second_click != 0U) ? (uint8_t)KEY_AWAIT_SECOND_PRESS : (uint8_t)KEY_IDLE;
        } else if (interval_elapsed(now, key->debounce_start_ms, KEY_DEBOUNCE_MS) != 0U) {
            key->press_start_ms = now;
            key->long_reported = 0U;
            key->state = (uint8_t)KEY_HELD;
        }
        break;

    case KEY_HELD:
        if (active == 0U) {
            key->debounce_start_ms = now;
            key->state = (uint8_t)KEY_CONFIRM_RELEASE;
        } else if ((key->long_reported == 0U) &&
                   (interval_elapsed(now, key->press_start_ms, KEY_LONG_PRESS_MS) != 0U)) {
            key->long_reported = 1U;
            key->waiting_second_click = 0U;
            return KEY_EVENT_LONG_PRESS;
        }
        break;

    case KEY_CONFIRM_RELEASE:
        if (active != 0U) {
            key->state = (uint8_t)KEY_HELD;
        } else if (interval_elapsed(now, key->debounce_start_ms, KEY_DEBOUNCE_MS) != 0U) {
            if (key->long_reported != 0U) {
                key->long_reported = 0U;
                key->waiting_second_click = 0U;
                key->state = (uint8_t)KEY_IDLE;
            } else if (key->waiting_second_click != 0U) {
                key->waiting_second_click = 0U;
                key->state = (uint8_t)KEY_IDLE;
                return KEY_EVENT_DOUBLE_CLICK;
            } else {
                key->waiting_second_click = 1U;
                key->click_wait_start_ms = now;
                key->state = (uint8_t)KEY_AWAIT_SECOND_PRESS;
            }
        }
        break;

    case KEY_AWAIT_SECOND_PRESS:
        if (interval_elapsed(now, key->click_wait_start_ms, KEY_DOUBLE_CLICK_MS) != 0U) {
            key->waiting_second_click = 0U;
            key->state = (uint8_t)KEY_IDLE;
            return KEY_EVENT_SHORT_PRESS;
        }
        if (active != 0U) {
            key->debounce_start_ms = now;
            key->state = (uint8_t)KEY_CONFIRM_PRESS;
        }
        break;

    default:
        key->state = (uint8_t)KEY_IDLE;
        key->long_reported = 0U;
        key->waiting_second_click = 0U;
        break;
    }

    return KEY_EVENT_NONE;
}

uint8_t key_get_state(Key_Handle_Typedef *key)
{
    return (key_scan(key) == KEY_EVENT_SHORT_PRESS) ? 1U : 0U;
}

uint8_t key_getstate(Key_TypeDef *key)
{
    return key_get_state(key);
}
