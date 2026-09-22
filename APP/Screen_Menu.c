#include "Screen_Menu.h"

#include "Settings.h"
#include "st7735.h"
#include "usart.h"

#define SCREEN_MENU_DATA_AREA_HEIGHT 152U
#define SCREEN_MENU_HEADER_HEIGHT 16U
#define SCREEN_MENU_ITEM_HEIGHT 8U
#define SCREEN_MENU_VISIBLE_ITEM_COUNT 17U

#define SCREEN_MENU_LEVEL_ROOT 0U
#define SCREEN_MENU_LEVEL_BAUDRATE 1U
#define SCREEN_MENU_LEVEL_DATAPACK 2U

static const char *const baudrate_text[] = {
    "9600", "19200", "38400", "57600", "74880",
    "115200", "230400", "460800", "921600", "1000000"
};
static const char *const datapack_text[] = { "Enable", "Disable" };

static uint8_t menu_level = SCREEN_MENU_LEVEL_ROOT;
static uint8_t current_menu_index;
static uint8_t current_top_index;
static uint8_t current_list_start_index;

static uint8_t get_list_count(void)
{
    if (menu_level == SCREEN_MENU_LEVEL_BAUDRATE)
    {
        return (uint8_t)(sizeof(baudrate_text) / sizeof(baudrate_text[0]));
    }
    if (menu_level == SCREEN_MENU_LEVEL_DATAPACK)
    {
        return (uint8_t)(sizeof(datapack_text) / sizeof(datapack_text[0]));
    }
    return 2U;
}

static const char *get_item_text(uint8_t index)
{
    if (menu_level == SCREEN_MENU_LEVEL_BAUDRATE)
    {
        return baudrate_text[index];
    }
    if (menu_level == SCREEN_MENU_LEVEL_DATAPACK)
    {
        return datapack_text[index];
    }
    return (index == 0U) ? "Baudrate" : "Datapack";
}

static const char *get_title(void)
{
    if (menu_level == SCREEN_MENU_LEVEL_BAUDRATE)
    {
        return "Baudrate";
    }
    if (menu_level == SCREEN_MENU_LEVEL_DATAPACK)
    {
        return "Datapack";
    }
    return "Menu";
}

static const uint16_t baudrate_brr[] = {
    7500U, 3750U, 1875U, 1250U, 962U,
    625U, 313U, 156U, 78U, 72U
};

static void setUartBaudrate(uint16_t index)
{
    uint32_t baudrate;

    if (index >= (uint16_t)(sizeof(baudrate_text) / sizeof(baudrate_text[0])))
    {
        return;
    }

    baudrate = BAUDRATE_LSIT[index];
    __HAL_UART_DISABLE(&huart1);
    huart1.Init.BaudRate = baudrate;
    huart1.Instance->BRR = baudrate_brr[index];
    __HAL_UART_ENABLE(&huart1);
    baudrate_index = (uint8_t)index;
}

static void setDatapackEnable(uint16_t index)
{
    data_pack_enbale = (index == 0U) ? 1U : 0U;
}

void Screen_Menu_update(void)
{
    uint8_t list_count = get_list_count();
    uint8_t selected_index = (menu_level == SCREEN_MENU_LEVEL_ROOT) ?
                             current_top_index : current_menu_index;
    uint8_t index;
    uint8_t id;
    uint16_t y;

    if (selected_index < current_list_start_index)
    {
        current_list_start_index = selected_index;
    }
    else if (selected_index >= (uint8_t)(current_list_start_index +
                                         SCREEN_MENU_VISIBLE_ITEM_COUNT))
    {
        current_list_start_index = (uint8_t)(selected_index -
                                              SCREEN_MENU_VISIBLE_ITEM_COUNT + 1U);
    }

    ST7735_fillRectangle(0U, 0U, ST7735_WIDTH, SCREEN_MENU_DATA_AREA_HEIGHT,
                         ST7735_BLACK);
    ST7735_fillRectangle(0U, 0U, ST7735_WIDTH, SCREEN_MENU_HEADER_HEIGHT,
                         ST7735_BLUE);
    ST7735_writeString(2U, 4U, get_title(), 1U, ST7735_WHITE, ST7735_BLUE);

    for (index = current_list_start_index;
         (index < list_count) &&
         (index < (uint8_t)(current_list_start_index + SCREEN_MENU_VISIBLE_ITEM_COUNT));
         index++)
    {
        id = (uint8_t)(index - current_list_start_index);
        y = (uint16_t)(SCREEN_MENU_HEADER_HEIGHT + id * SCREEN_MENU_ITEM_HEIGHT);
        if (index == selected_index)
        {
            ST7735_drawChar(0U, y, '>', 1U, ST7735_WHITE, ST7735_BLUE);
            ST7735_writeString(8U, y, get_item_text(index), 1U,
                               ST7735_WHITE, ST7735_BLUE);
        }
        else
        {
            ST7735_writeString(8U, y, get_item_text(index), 1U,
                               ST7735_WHITE, ST7735_BLACK);
        }
    }
}

void Screen_Menu_next(void)
{
    uint8_t list_count = get_list_count();

    if (menu_level == SCREEN_MENU_LEVEL_ROOT)
    {
        current_top_index++;
        if (current_top_index >= list_count)
        {
            current_top_index = 0U;
        }
    }
    else
    {
        current_menu_index++;
        if (current_menu_index >= list_count)
        {
            current_menu_index = 0U;
        }
    }
}

void Screen_Menu_previous(void)
{
    uint8_t *index = (menu_level == SCREEN_MENU_LEVEL_ROOT) ?
                     &current_top_index : &current_menu_index;
    uint8_t list_count = get_list_count();

    if (*index == 0U)
    {
        *index = (uint8_t)(list_count - 1U);
    }
    else
    {
        (*index)--;
    }
}

void Screen_Menu_confirm(void)
{
    if (menu_level == SCREEN_MENU_LEVEL_ROOT)
    {
        menu_level = (current_top_index == 0U) ?
                     SCREEN_MENU_LEVEL_BAUDRATE : SCREEN_MENU_LEVEL_DATAPACK;
        current_menu_index = 0U;
    }
    else if (menu_level == SCREEN_MENU_LEVEL_BAUDRATE)
    {
        setUartBaudrate(current_menu_index);
    }
    else
    {
        setDatapackEnable(current_menu_index);
    }
}

uint8_t Screen_Menu_back(void)
{
    if (menu_level == SCREEN_MENU_LEVEL_ROOT)
    {
        return 1U;
    }

    menu_level = SCREEN_MENU_LEVEL_ROOT;
    return 1U;
}
