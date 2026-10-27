#include "Screen_Menu.h"

#include "Settings.h"
#include "YFframe.h"
#include "ring.h"
#include "st7735.h"
#include "usart.h"

#define SCREEN_MENU_LEVEL_ROOT       0U
#define SCREEN_MENU_LEVEL_BAUDRATE   1U
#define SCREEN_MENU_ITEM_HEIGHT      16U
#define SCREEN_MENU_VISIBLE_ROWS     ((ST7735_HEIGHT / SCREEN_MENU_ITEM_HEIGHT) - 1U)

static const char * const menu1_text[] = {
    "Baudrate"
};

static const char * const menu2_text[][10] = {
    { "9600", "19200", "38400", "57600", "74880", "115200", "230400", "460800", "921600", "1000000" }
};

static uint8_t menu_level;
static uint8_t menu1_index;
static uint8_t menu2_index;
static uint8_t menu_top_index;
static uint8_t menu_dirty;

static uint8_t Screen_Menu_get_menu1_count(void)
{
    return (uint8_t)(sizeof(menu1_text) / sizeof(menu1_text[0]));
}

static uint8_t Screen_Menu_get_menu2_count(uint8_t parent_index)
{
    if (parent_index >= Screen_Menu_get_menu1_count())
    {
        return 0U;
    }

    return BAUDRATE_LSIT_SIZE;
}

static void Screen_Menu_adjust_scroll(uint8_t selected_index, uint8_t item_count)
{
    uint8_t max_top_index;

    if (item_count <= SCREEN_MENU_VISIBLE_ROWS)
    {
        menu_top_index = 0U;
        return;
    }

    if (selected_index < menu_top_index)
    {
        menu_top_index = selected_index;
    }
    else if (selected_index >= (uint8_t)(menu_top_index + SCREEN_MENU_VISIBLE_ROWS))
    {
        menu_top_index = (uint8_t)(selected_index - SCREEN_MENU_VISIBLE_ROWS + 1U);
    }

    max_top_index = (uint8_t)(item_count - SCREEN_MENU_VISIBLE_ROWS);
    if (menu_top_index > max_top_index)
    {
        menu_top_index = max_top_index;
    }
}

static uint8_t Screen_Menu_set_uart_baudrate(uint8_t index)
{
    uint32_t baudrate;
    uint32_t brr;

    if (index >= BAUDRATE_LSIT_SIZE)
    {
        return 0U;
    }

    baudrate = BAUDRATE_LSIT[index];
    brr = UART_BRR_SAMPLING16(HAL_RCC_GetPCLK2Freq(), baudrate);

    /* Stop DMA first, then release and recreate the USART through HAL. */
    (void)HAL_UART_DMAStop(&huart1);
    __HAL_UART_DISABLE(&huart1);
    if (HAL_UART_DeInit(&huart1) != HAL_OK)
    {
        return 0U;
    }

    huart1.Init.BaudRate = baudrate;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        return 0U;
    }

    /* HAL_UART_Init uses the same dynamic PCLK2 calculation; write it explicitly
       so the configured BRR remains tied to the selected baudrate. */
    huart1.Instance->BRR = brr;
    baudrate_index = index;

    YFframe_reset();
    (void)ring_reset();
    (void)ring_start();
    return 1U;
}

void Screen_Menu_init(void)
{
    menu_level = SCREEN_MENU_LEVEL_ROOT;
    menu1_index = 0U;
    menu2_index = 0U;
    menu_top_index = 0U;
    menu_dirty = 1U;
}

void Screen_Menu_proc(void)
{
    uint8_t item_count;
    uint8_t selected_index;
    uint8_t row;
    uint8_t item_index;
    const char * const *menu_text;

    if (menu_dirty == 0U)
    {
        return;
    }

    if (menu_level == SCREEN_MENU_LEVEL_ROOT)
    {
        item_count = Screen_Menu_get_menu1_count();
        selected_index = menu1_index;
        menu_text = menu1_text;
        ST7735_fill_screen(ST7735_BLACK);
        ST7735_show_string(0U, 0U, "MENU", 1U, ST7735_YELLOW, ST7735_BLACK);
    }
    else
    {
        item_count = Screen_Menu_get_menu2_count(menu1_index);
        selected_index = menu2_index;
        menu_text = menu2_text[menu1_index];
        ST7735_fill_screen(ST7735_BLACK);
        ST7735_show_string(0U, 0U, menu1_text[menu1_index], 1U,
                            ST7735_YELLOW, ST7735_BLACK);
    }

    Screen_Menu_adjust_scroll(selected_index, item_count);
    for (row = 0U; row < SCREEN_MENU_VISIBLE_ROWS; row++)
    {
        item_index = (uint8_t)(menu_top_index + row);
        if (item_index >= item_count)
        {
            break;
        }

        if (item_index == selected_index)
        {
            ST7735_fill_rectangle(0U, (uint16_t)(row + 1U) * SCREEN_MENU_ITEM_HEIGHT,
                                  ST7735_WIDTH, SCREEN_MENU_ITEM_HEIGHT, ST7735_WHITE);
            ST7735_show_string(0U, (uint16_t)(row + 1U) * SCREEN_MENU_ITEM_HEIGHT,
                                menu_text[item_index], 2U, ST7735_BLACK, ST7735_WHITE);
        }
        else
        {
            ST7735_show_string(0U, (uint16_t)(row + 1U) * SCREEN_MENU_ITEM_HEIGHT,
                                menu_text[item_index], 2U, ST7735_WHITE, ST7735_BLACK);
        }
    }

    menu_dirty = 0U;
}

void Screen_Menu_update(void)
{
    Screen_Menu_proc();
}

void Screen_Menu_next(void)
{
    uint8_t item_count;
    uint8_t *selected_index;

    if (menu_level == SCREEN_MENU_LEVEL_ROOT)
    {
        item_count = Screen_Menu_get_menu1_count();
        selected_index = &menu1_index;
    }
    else
    {
        item_count = Screen_Menu_get_menu2_count(menu1_index);
        selected_index = &menu2_index;
    }

    if (item_count == 0U)
    {
        return;
    }

    *selected_index = (uint8_t)((*selected_index + 1U) % item_count);
    Screen_Menu_adjust_scroll(*selected_index, item_count);
    menu_dirty = 1U;
}

void Screen_Menu_previous(void)
{
    uint8_t item_count;
    uint8_t *selected_index;

    if (menu_level == SCREEN_MENU_LEVEL_ROOT)
    {
        item_count = Screen_Menu_get_menu1_count();
        selected_index = &menu1_index;
    }
    else
    {
        item_count = Screen_Menu_get_menu2_count(menu1_index);
        selected_index = &menu2_index;
    }

    if (item_count == 0U)
    {
        return;
    }

    if (*selected_index == 0U)
    {
        *selected_index = (uint8_t)(item_count - 1U);
    }
    else
    {
        (*selected_index)--;
    }

    Screen_Menu_adjust_scroll(*selected_index, item_count);
    menu_dirty = 1U;
}

void Screen_Menu_confirm(void)
{
    if (menu_level == SCREEN_MENU_LEVEL_ROOT)
    {
        menu_level = SCREEN_MENU_LEVEL_BAUDRATE;
        menu2_index = (baudrate_index < BAUDRATE_LSIT_SIZE) ? baudrate_index : 0U;
        menu_top_index = 0U;
    }
    else if (Screen_Menu_set_uart_baudrate(menu2_index) != 0U)
    {
        /* Applying a setting returns to the setting-entry menu. */
        menu_level = SCREEN_MENU_LEVEL_ROOT;
        menu_top_index = 0U;
    }

    menu_dirty = 1U;
}

uint8_t Screen_Menu_back(void)
{
    if (menu_level == SCREEN_MENU_LEVEL_ROOT)
    {
        return 1U;
    }

    menu_level = SCREEN_MENU_LEVEL_ROOT;
    menu_top_index = 0U;
    menu_dirty = 1U;
    return 0U;
}
