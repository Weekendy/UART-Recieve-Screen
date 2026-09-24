#include "Control.h"

#include "gpio.h"
#include "key.h"
#include "Receive_Data.h"
#include "Screen_Data.h"
#include "st7735.h"

#define CONTROL_DISPLAY_INTERVAL_MS 5U

static uint32_t fps_window_start;
static uint32_t displayed_fps;
static uint32_t frame_count;
static uint32_t display_next_tick;
static uint8_t display_pending;
static uint8_t receive_status_pending;
static Control_Mode_TypeDef mode = Control_DISPLAY_PACKAGE;
static uint8_t is_paused;

static Key_Handle_Typedef key1;

static void Control_Set_Pause(uint8_t pause);
static void Control_Toggle_Display_Mode(void);
static void Control_Process_Received_Data(void);
static void Control_Update_Fps(void);
static void Control_Update_Receive_Status(void);

void Control_init(void)
{
    ST7735_init();
    ST7735_fill_screen(ST7735_BLACK);
    Receive_Data_init();
    Screen_Data_reset_raw_display();

    fps_window_start = HAL_GetTick();
    display_next_tick = fps_window_start;
    display_pending = 0U;
    displayed_fps = 0U;
    frame_count = 0U;
    is_paused = Control_DISPLAY_UNPAUSE;
    receive_status_pending = 1U;
    Screen_Data_display_fps(displayed_fps);

    key_init(&key1, Key1_GPIO_Port, Key1_Pin, GPIO_PIN_RESET);
    Receive_Data_start();
    Control_Update_Receive_Status();
}

void Control_Key_Proc(void)
{
    Key_Event_Typedef key1_event = key_scan(&key1);
    if (key1_event == KEY_EVENT_SHORT_PRESS)
    {
        Control_Set_Pause((is_paused == Control_DISPLAY_UNPAUSE) ?
                          Control_DISPLAY_PAUSE : Control_DISPLAY_UNPAUSE);
    }
    else if (key1_event == KEY_EVENT_LONG_PRESS)
    {
        Control_Toggle_Display_Mode();
    }
}

void Control_display_data_Proc(void)
{
    uint8_t raw_byte;

    if (is_paused == Control_DISPLAY_PAUSE)
    {
        return;
    }

    Receive_Data_process();
    Control_Update_Receive_Status();
    if (mode == Control_DISPLAY_RAW_DATA)
    {
        while (Receive_Data_read_raw_byte(&raw_byte) != 0U)
        {
            Screen_Data_display_raw_byte(raw_byte);
        }
    }

    Control_Process_Received_Data();

    if ((mode == Control_DISPLAY_PACKAGE) &&
        (display_pending != 0U) &&
        ((int32_t)(HAL_GetTick() - display_next_tick) >= 0))
    {
        Screen_Data_display_package();
        display_pending = 0U;
        display_next_tick = HAL_GetTick() + CONTROL_DISPLAY_INTERVAL_MS;
    }

    Control_Update_Fps();
}

static void Control_Set_Pause(uint8_t pause)
{
    is_paused = pause;
    display_pending = 0U;

    if (is_paused == Control_DISPLAY_PAUSE)
    {
        Receive_Data_pause();
        receive_status_pending = 0U;
        Screen_Data_display_status("PAUSE");
    }
    else
    {
        receive_status_pending = 1U;
        Receive_Data_resume();
        Control_Update_Receive_Status();
        frame_count = 0U;
        fps_window_start = HAL_GetTick();
    }
}

static void Control_Toggle_Display_Mode(void)
{
    display_pending = 0U;
    if (mode == Control_DISPLAY_PACKAGE)
    {
        mode = Control_DISPLAY_RAW_DATA;
        Receive_Data_set_mode(RECEIVE_DATA_MODE_RAW);
        Screen_Data_reset_raw_display();
    }
    else
    {
        mode = Control_DISPLAY_PACKAGE;
        Receive_Data_set_mode(RECEIVE_DATA_MODE_PACKAGE);
        /* The package renderer only redraws changed rows.  Remove raw text
           before its first package frame is available. */
        Screen_Data_clear();
    }

    receive_status_pending = 1U;
    Control_Update_Receive_Status();
}

static void Control_Process_Received_Data(void)
{
    if (RecDat_accomplish_flag == 1U)
    {
        RecDat_accomplish_flag = 0U;
        frame_count += RecDat_frame_count;
        if (RecDat_frame_count != 0U)
        {
            display_pending = 1U;
        }
        Receive_Data_reset();
    }
}

static void Control_Update_Fps(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - fps_window_start) >= 1000U)
    {
        displayed_fps = frame_count;
        frame_count = 0U;
        fps_window_start = now;
        Screen_Data_display_fps(displayed_fps);
    }
}

static void Control_Update_Receive_Status(void)
{
    if ((receive_status_pending == 0U) ||
        (Receive_Data_is_receiving() == 0U))
    {
        return;
    }

    if (mode == Control_DISPLAY_PACKAGE)
    {
        Screen_Data_display_status("PACKAGE");
    }
    else
    {
        Screen_Data_display_status("RAW_DATA");
    }
    receive_status_pending = 0U;
}
