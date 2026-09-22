#ifndef CONTROL_H
#define CONTROL_H

#define Control_MODE_NUM 2
#define Control_DISPLAY_UNPAUSE 0
#define Control_DISPLAY_PAUSE 1

typedef enum
{
    Control_DISPLAY_PACKAGE = 0,
    Control_DISPLAY_RAW_DATA = 1,
} Control_Mode_TypeDef;

void Control_init(void);
void Control_display_data_Proc(void); 
void Control_Key_Proc(void);


#endif // !CONTROL_H
