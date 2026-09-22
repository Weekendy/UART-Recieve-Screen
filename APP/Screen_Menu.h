#ifndef SCREEN_MENU_H
#define SCREEN_MENU_H

#include "main.h"

#define SCREEN_MENU_TYPE_MENU 0
#define SCREEN_MENU_TYPE_OPERATION 1
#define SCREEN_MENU_UNSELECTED 0
#define SCREEN_MENU_SELECTED 1

#define NULL_ID 0xFFFF

typedef struct 
{
    uint16_t id;
    uint16_t parent_id;
    uint16_t first_child_id;
    uint16_t next_sibling_id;
    uint8_t type; // 0: menu, 1: operation
    uint8_t is_selected;
    void (*to_operation_func)(uint16_t);
    const char *text;
} Screen_Menu_t;

void Screen_Menu_update(void);
void Screen_Menu_next(void);
void Screen_Menu_previous(void);
void Screen_Menu_confirm(void);
uint8_t Screen_Menu_back(void);

#endif // SCREEN_MENU_H
