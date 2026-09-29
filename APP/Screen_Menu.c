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

static uint8_t menu_level = SCREEN_MENU_LEVEL_ROOT;
static uint8_t current_menu_index;
static uint8_t current_top_index;
static uint8_t current_list_start_index;

static uint8_t get_list_count(void)
{
    if (menu_level == SCREEN_MENU_LEVEL_BAUDRATE)
    {
        return (uint8_t)(sizeof(baudrate_text) / sizeof(baudrate_text[0]));
        YFframe_reset
    }
    if (menu_level == SCREEN_MENU_LEVEL_DATAPACK)
    {
        return (uint8_t)(sizeof(datapack_text) / sizeof(datapack_text[0]));
    }
    return 2U;
}

static const char *get_item_text(uint8_t index)
{

}

static void setUartBaudrate(uint16_t index)
{
   
}

static void setDatapackEnable(uint16_t index)
{
    data_pack_enbale = (index == 0U) ? 1U : 0U;
}

void Screen_Menu_update(void)
{
    
}

void Screen_Menu_next(void)
{
    if()
}

void Screen_Menu_previous(void)
{
    
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
