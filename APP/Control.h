#ifndef CONTROL_H
#define CONTROL_H

#define Control_MODE_NUM 3
#define Control_DISPLAY_UNPAUSE 0
#define Control_DISPLAY_PAUSE 1

typedef enum
{
    Control_DISPLAY_PACKAGE_MODE = 0,
    Control_DISPLAY_RAW_DATA_MODE = 1,
    Control_DISPLAY_MENU_MODE = 2,
} Control_Mode_TypeDef;

void Control_init(void);
void Control_display_proc(void); 
void Control_key_proc(void);


#endif // !CONTROL_H
