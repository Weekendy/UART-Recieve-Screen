#ifndef SCREEN_MENU_H
#define SCREEN_MENU_H

#include "main.h"

/* Compatibility type retained for callers that build menu descriptors. */
typedef struct
{
    uint8_t id;
    uint8_t parent_id;
    uint8_t first_child_id;
    uint8_t next_sibling_id;
    uint8_t type;
    void (*to_operation_func)(uint16_t);
    const char *text;
} Screen_Menu_t;

void Screen_Menu_init(void);
void Screen_Menu_proc(void);
void Screen_Menu_update(void);
void Screen_Menu_next(void);
void Screen_Menu_previous(void);
void Screen_Menu_confirm(void);

/* Returns 1 when already at the top menu, otherwise returns to the top menu. */
uint8_t Screen_Menu_back(void);

#endif /* SCREEN_MENU_H */
