#include "Control.h"

#include "ring.h"
#include "YFframe.h"
#include "log.h"
#include "Screen_Data.h"
#include "key.h"

#define CONTROL_DISPLAY_INTERVAL_MS 5U

uint8_t cmd;
static YFframe_Data_Text_TypeDef text;
static uint32_t reported_ring_overflow_count;
static Ring_Raw_Data_TypeDef raw_data;

Key_Handle_Typedef key1;

static Control_Mode_TypeDef control_display_mode = Control_DISPLAY_PACKAGE_MODE;

void Control_init(void)
{
    log_init();
    ring_init();
    YFframe_init();
    Screen_Data_init();

    key_init(&key1, Key1_GPIO_Port, Key1_Pin, GPIO_PIN_RESET);
}


void Control_display_proc()
{
    if (ring_overflow_count != reported_ring_overflow_count)
    {
      log_printf("ring overflow=%u\r\n", (unsigned int)ring_overflow_count);
      reported_ring_overflow_count = ring_overflow_count;
    }
    
    if(control_display_mode == Control_DISPLAY_PACKAGE_MODE)
    {
        ring_proc();
        if (YFframe_get_cmdAdata(&cmd, &text) == YFFRAME_OK)
        {
            Screen_Data_show_data(&cmd, &text);
        }
    }
        
    if(control_display_mode == Control_DISPLAY_RAW_DATA_MODE)
    {
        if(ring_proc_raw(36) == RING_OK)
        {
        ring_get_raw_text(&raw_data);
        
        Screen_Data_show_data_raw(&raw_data);
        }
    }
    Screen_Data_show_FPS();
}

void Control_key_proc(void)
{
    if(key_scan(&key1) == KEY_EVENT_LONG_PRESS)
    {
        switch (control_display_mode)
        {
        case Control_DISPLAY_PACKAGE_MODE:
            control_display_mode = Control_DISPLAY_RAW_DATA_MODE;
            break;
        case Control_DISPLAY_RAW_DATA_MODE:
            control_display_mode = Control_DISPLAY_PACKAGE_MODE;
            break;
        default:
            control_display_mode = Control_DISPLAY_PACKAGE_MODE;
            break;
        }
    } 
}