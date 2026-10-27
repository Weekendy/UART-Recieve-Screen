#include "Control.h"

#include "ring.h"
#include "YFframe.h"
#include "log.h"
#include "Screen_Data.h"
#include "Screen_Menu.h"
#include "key.h"
#include "st7735.h"

uint8_t cmd;
static uint32_t reported_ring_overflow_count;
static Control_Mode_TypeDef control_display_mode = Control_DISPLAY_PACKAGE_MODE;
static Control_Mode_TypeDef control_display_mode_before_menu = Control_DISPLAY_PACKAGE_MODE;
static uint8_t is_paused;

Key_Handle_Typedef key1;
Key_Handle_Typedef key2;
static YFframe_Data_Text_TypeDef text;
static Ring_Raw_Data_TypeDef raw_data;

static void Control_display_pause(void);
static void Control_display_reset(void);
static void Control_show_display_status(void);
static void Control_enter_menu(void);
static void Control_exit_menu(void);

void Control_init(void)
{
    log_init();
    ring_init();
    YFframe_init();
    Screen_Data_init();
    Screen_Menu_init();

    /* Preserve the project's existing physical-key binding. */
    key_init(&key1, Key2_GPIO_Port, Key2_Pin, GPIO_PIN_RESET);
    key_init(&key2, Key1_GPIO_Port, Key1_Pin, GPIO_PIN_RESET);
    Control_show_display_status();
}

void Control_display_proc(void)
{
    if (control_display_mode == Control_DISPLAY_MENU_MODE)
    {
        Screen_Menu_proc();
        return;
    }

    if (ring_overflow_count != reported_ring_overflow_count)
    {
        log_printf("ring overflow=%u\r\n", (unsigned int)ring_overflow_count);
        reported_ring_overflow_count = ring_overflow_count;
    }

    Screen_Data_show_FPS();
    if (is_paused != 0U)
    {
        return;
    }

    if (control_display_mode == Control_DISPLAY_PACKAGE_MODE)
    {
        (void)ring_proc();
        if (YFframe_get_cmdAdata(&cmd, &text) == YFFRAME_OK)
        {
            Screen_Data_show_data(&cmd, &text);
        }
    }
    else if (control_display_mode == Control_DISPLAY_RAW_DATA_MODE)
    {
        if (ring_proc_raw(36U) == RING_OK)
        {
            ring_get_raw_text(&raw_data);
            Screen_Data_show_data_raw(&raw_data);
        }
    }
}

void Control_key_proc(void)
{
    Key_Event_Typedef key1_event = key_scan(&key1);
    Key_Event_Typedef key2_event = key_scan(&key2);

    if (control_display_mode == Control_DISPLAY_MENU_MODE)
    {
        if (key1_event == KEY_EVENT_SHORT_PRESS)
        {
            Screen_Menu_previous();
        }
        else if (key1_event == KEY_EVENT_LONG_PRESS)
        {
            Screen_Menu_confirm();
        }
        else if (key2_event == KEY_EVENT_SHORT_PRESS)
        {
            Screen_Menu_next();
        }
        else if (key2_event == KEY_EVENT_LONG_PRESS)
        {
            Control_exit_menu();
        }
        return;
    }

    if (key1_event == KEY_EVENT_SHORT_PRESS)
    {
        if (is_paused == 0U)
        {
            Control_display_pause();
            is_paused = 1U;
            ST7735_show_string(0U, ST7735_HEIGHT - CHAR_HEIGHT, "PAUSED ", 1U,
                                ST7735_RED, ST7735_BLACK);
        }
        else
        {
            Control_display_reset();
            is_paused = 0U;
            Control_show_display_status();
        }
    }
    else if (key1_event == KEY_EVENT_LONG_PRESS)
    {
        if (control_display_mode == Control_DISPLAY_PACKAGE_MODE)
        {
            control_display_mode = Control_DISPLAY_RAW_DATA_MODE;
        }
        else
        {
            control_display_mode = Control_DISPLAY_PACKAGE_MODE;
        }
        Control_display_reset();
        Control_show_display_status();
    }
    else if ((key2_event == KEY_EVENT_LONG_PRESS) && (is_paused == 0U))
    {
        Control_enter_menu();
    }
}

static void Control_display_pause(void)
{
    (void)ring_stop();
}

static void Control_display_reset(void)
{
    (void)ring_stop();
    ST7735_fill_screen(ST7735_BLACK);
    YFframe_reset();
    (void)ring_reset();
    (void)ring_start();
}

static void Control_show_display_status(void)
{
    if (control_display_mode == Control_DISPLAY_PACKAGE_MODE)
    {
        ST7735_show_string(0U, ST7735_HEIGHT - CHAR_HEIGHT, "PACKAGE", 1U,
                            ST7735_RED, ST7735_BLACK);
    }
    else if (control_display_mode == Control_DISPLAY_RAW_DATA_MODE)
    {
        ST7735_show_string(0U, ST7735_HEIGHT - CHAR_HEIGHT, "RAW    ", 1U,
                            ST7735_RED, ST7735_BLACK);
    }
}

static void Control_enter_menu(void)
{
    control_display_mode_before_menu = control_display_mode;
    control_display_mode = Control_DISPLAY_MENU_MODE;
    Control_display_pause();
    Screen_Menu_init();
    Screen_Menu_proc();
}

static void Control_exit_menu(void)
{
    control_display_mode = control_display_mode_before_menu;
    Control_display_reset();
    Control_show_display_status();
}
