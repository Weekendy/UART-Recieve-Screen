#include "Receive_Data.h"
#include "usart.h"
#include <string.h>

#define REC_DAT_DMA_BUFFER_SIZE 128U
#define REC_DAT_RX_QUEUE_SIZE 256U

static uint8_t RecDat_buffer[REC_DAT_DMA_BUFFER_SIZE];
static uint8_t RecDat_rx_queue[REC_DAT_RX_QUEUE_SIZE];
static volatile uint16_t RecDat_rx_head;
static volatile uint16_t RecDat_rx_tail;
static uint8_t RecDat_frame_buffer[RECEIVE_DATA_BUFFER_SIZE];
static uint16_t RecDat_frame_size;
static uint16_t RecDat_expected_size;
static uint32_t RecDat_error[RECEIVE_DATA_ERROR_NUM] = { 0 };
static uint32_t RecDat_retry_tick;
static uint8_t RecDat_retry_pending;
static Receive_Data_Mode_TypeDef RecDat_mode = RECEIVE_DATA_MODE_PACKAGE;
static volatile uint8_t RecDat_is_receiving;
static volatile uint8_t RecDat_paused;

uint8_t RecDat_accomplish_flag = 0;
uint16_t RecDat_frame_count = 0;
uint8_t g_RecDat_data_size[RECEIVE_DATA_TYPE_NUM] = { 0 };
uint8_t u8_data[RECEIVE_DATA_EVERY_LENGTH] = { 0 };
uint16_t u16_data[RECEIVE_DATA_EVERY_LENGTH] = { 0 };
int16_t i16_data[RECEIVE_DATA_EVERY_LENGTH] = { 0 };
int32_t int_data[RECEIVE_DATA_EVERY_LENGTH] = { 0 };
float float_data[RECEIVE_DATA_EVERY_LENGTH] = { 0 };

uint8_t display_data_size[RECEIVE_DATA_TYPE_NUM];
uint8_t display_u8[RECEIVE_DATA_EVERY_LENGTH];
uint16_t display_u16[RECEIVE_DATA_EVERY_LENGTH];
int16_t display_i16[RECEIVE_DATA_EVERY_LENGTH];
int32_t display_int[RECEIVE_DATA_EVERY_LENGTH];
float display_float[RECEIVE_DATA_EVERY_LENGTH];

static uint16_t RecDat_read_u16_be(uint16_t index)
{
    return (uint16_t)(((uint16_t)RecDat_frame_buffer[index] << 8U) |
                      (uint16_t)RecDat_frame_buffer[index + 1U]);
}

static uint32_t RecDat_read_u32_be(uint16_t index)
{
    return ((uint32_t)RecDat_frame_buffer[index] << 24U) |
           ((uint32_t)RecDat_frame_buffer[index + 1U] << 16U) |
           ((uint32_t)RecDat_frame_buffer[index + 2U] << 8U) |
           (uint32_t)RecDat_frame_buffer[index + 3U];
}

static void RecDat_clear_values(void)
{
    uint8_t i;

    for (i = 0U; i < RECEIVE_DATA_TYPE_NUM; i++)
    {
        g_RecDat_data_size[i] = 0U;
    }
    for (i = 0U; i < RECEIVE_DATA_EVERY_LENGTH; i++)
    {
        u8_data[i] = 0U;
        u16_data[i] = 0U;
        i16_data[i] = 0;
        int_data[i] = 0;
        float_data[i] = 0.0F;
    }
}

static void RecDat_save_last_frame(void)
{
    memcpy(display_data_size, g_RecDat_data_size,
           sizeof(display_data_size));
    memcpy(display_u8, u8_data, sizeof(display_u8));
    memcpy(display_u16, u16_data, sizeof(display_u16));
    memcpy(display_i16, i16_data, sizeof(display_i16));
    memcpy(display_int, int_data, sizeof(display_int));
    memcpy(display_float, float_data, sizeof(display_float));
}

static void RecDat_restore_last_frame(void)
{
    memcpy(g_RecDat_data_size, display_data_size,
           sizeof(g_RecDat_data_size));
    memcpy(u8_data, display_u8, sizeof(u8_data));
    memcpy(u16_data, display_u16, sizeof(u16_data));
    memcpy(i16_data, display_i16, sizeof(i16_data));
    memcpy(int_data, display_int, sizeof(int_data));
    memcpy(float_data, display_float, sizeof(float_data));
}

static uint8_t RecDat_check_frame(uint16_t frame_size)
{
    uint16_t i;
    uint16_t check_index = frame_size - 2U;
    uint8_t check_sum = 0U;

    for (i = 1U; i < check_index; i++)
    {
        check_sum = (uint8_t)(check_sum + RecDat_frame_buffer[i]);
    }
    if (check_sum != RecDat_frame_buffer[check_index])
    {
        RecDat_error[RecDat_CHECK_ERROR]++;
        return 0U;
    }

    return 1U;
}

static void RecDat_reset_receive_state(void)
{
    RecDat_rx_head = 0U;
    RecDat_rx_tail = 0U;
    RecDat_frame_size = 0U;
    RecDat_expected_size = 0U;
    RecDat_retry_pending = 0U;
    RecDat_accomplish_flag = 0U;
    RecDat_frame_count = 0U;
}

static void RecDat_clear_uart_receive_state(void)
{
    /* Data arriving while DMA is paused can leave RXNE/ORE pending. */
    SET_BIT(huart1.Instance->RQR, UART_RXDATA_FLUSH_REQUEST);
    __HAL_UART_CLEAR_FLAG(&huart1, UART_CLEAR_OREF | UART_CLEAR_NEF |
                          UART_CLEAR_PEF | UART_CLEAR_FEF |
                          UART_CLEAR_IDLEF);
}

static void RecDat_schedule_restart(void)
{
    RecDat_is_receiving = 0U;
    RecDat_rx_tail = RecDat_rx_head;
    RecDat_frame_size = 0U;
    RecDat_expected_size = 0U;
    RecDat_accomplish_flag = 0U;
    RecDat_frame_count = 0U;
    RecDat_retry_tick = HAL_GetTick() + RECEIVE_DATA_RETRY_DELAY_MS;
    RecDat_retry_pending = 1U;
}

static uint8_t RecDat_parse_frame(uint16_t frame_size)
{
    uint16_t i;
    uint16_t payload_end = frame_size - 2U;
    uint32_t raw_float;

    i = 2U;
    while (i < payload_end)
    {
        switch (RecDat_frame_buffer[i++])
        {
        case RecDat_TYPE_U8:
            if ((payload_end - i < 1U) ||
                (g_RecDat_data_size[RecDat_TYPE_U8] >= RECEIVE_DATA_EVERY_LENGTH))
            {
                RecDat_error[RecDat_BROKEN_ERROR]++;
                return 0U;
            }
            u8_data[g_RecDat_data_size[RecDat_TYPE_U8]++] =
                RecDat_frame_buffer[i++];
            break;

        case RecDat_TYPE_U16:
            if ((payload_end - i < 2U) ||
                (g_RecDat_data_size[RecDat_TYPE_U16] >= RECEIVE_DATA_EVERY_LENGTH))
            {
                RecDat_error[RecDat_BROKEN_ERROR]++;
                return 0U;
            }
            u16_data[g_RecDat_data_size[RecDat_TYPE_U16]++] = RecDat_read_u16_be(i);
            i += 2U;
            break;

        case RecDat_TYPE_I16:
            if ((payload_end - i < 2U) ||
                (g_RecDat_data_size[RecDat_TYPE_I16] >= RECEIVE_DATA_EVERY_LENGTH))
            {
                RecDat_error[RecDat_BROKEN_ERROR]++;
                return 0U;
            }
            i16_data[g_RecDat_data_size[RecDat_TYPE_I16]++] =
                (int16_t)RecDat_read_u16_be(i);
            i += 2U;
            break;

        case RecDat_TYPE_INT:
            if ((payload_end - i < 4U) ||
                (g_RecDat_data_size[RecDat_TYPE_INT] >= RECEIVE_DATA_EVERY_LENGTH))
            {
                RecDat_error[RecDat_BROKEN_ERROR]++;
                return 0U;
            }
            int_data[g_RecDat_data_size[RecDat_TYPE_INT]++] =
                (int32_t)RecDat_read_u32_be(i);
            i += 4U;
            break;

        case RecDat_TYPE_FLOAT:
            if ((payload_end - i < 4U) ||
                (g_RecDat_data_size[RecDat_TYPE_FLOAT] >= RECEIVE_DATA_EVERY_LENGTH))
            {
                RecDat_error[RecDat_BROKEN_ERROR]++;
                return 0U;
            }
            raw_float = RecDat_read_u32_be(i);
            memcpy(&float_data[g_RecDat_data_size[RecDat_TYPE_FLOAT]++],
                   &raw_float, sizeof(raw_float));
            i += 4U;
            break;

        default:
            RecDat_error[RecDat_BROKEN_ERROR]++;
            return 0U;
        }
    }



    return 1U;
}

static void RecDat_restart_from_next_head(void)
{
    uint16_t index;

    for (index = 1U; index < RecDat_frame_size; index++)
    {
        if (RecDat_frame_buffer[index] == RECEIVE_DATA_HEAD)
        {
            memmove(RecDat_frame_buffer, &RecDat_frame_buffer[index],
                    (size_t)(RecDat_frame_size - index));
            RecDat_frame_size = (uint16_t)(RecDat_frame_size - index);
            RecDat_expected_size = (RecDat_frame_size >= 2U) ?
                                   (uint16_t)RecDat_frame_buffer[1] + 4U : 0U;
            return;
        }
    }

    RecDat_frame_size = 0U;
    RecDat_expected_size = 0U;
}

static void RecDat_feed_byte(uint8_t byte)
{
    if (RecDat_frame_size == 0U)
    {
        if (byte == RECEIVE_DATA_HEAD)
        {
            RecDat_frame_buffer[0] = byte;
            RecDat_frame_size = 1U;
        }
        return;
    }

    if (RecDat_frame_size >= RECEIVE_DATA_BUFFER_SIZE)
    {
        RecDat_error[RecDat_SIZE_ERROR]++;
        RecDat_restart_from_next_head();
        return;
    }

    RecDat_frame_buffer[RecDat_frame_size++] = byte;
    if (RecDat_frame_size == 2U)
    {
        RecDat_expected_size = (uint16_t)byte + 4U;
        if (RecDat_expected_size > RECEIVE_DATA_BUFFER_SIZE)
        {
            RecDat_error[RecDat_SIZE_ERROR]++;
            RecDat_restart_from_next_head();
            return;
        }
    }

    if ((RecDat_expected_size == 0U) ||
        (RecDat_frame_size < RecDat_expected_size))
    {
        return;
    }

    if (RecDat_frame_buffer[RecDat_expected_size - 1U] != RECEIVE_DATA_TAIL)
    {
        RecDat_error[RecDat_TAIL_ERROR]++;
        RecDat_restart_from_next_head();
        return;
    }

    if (RecDat_check_frame(RecDat_expected_size) == 0U)
    {
        RecDat_restart_from_next_head();
        return;
    }

    RecDat_clear_values();
    if (RecDat_parse_frame(RecDat_expected_size) != 0U)
    {
        RecDat_save_last_frame();
        RecDat_frame_count++;
        RecDat_frame_size = 0U;
        RecDat_expected_size = 0U;
    }
    else
    {
        RecDat_restart_from_next_head();
    }
}

void Receive_Data_init(void)
{
    RecDat_reset_receive_state();
    RecDat_mode = RECEIVE_DATA_MODE_PACKAGE;
    RecDat_is_receiving = 0U;
    RecDat_paused = 0U;
    RecDat_clear_values();
}

void Receive_Data_start(void)
{
    HAL_StatusTypeDef status;

    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, RecDat_buffer,
                                          REC_DAT_DMA_BUFFER_SIZE);
    if (status == HAL_OK)
    {
        RecDat_retry_pending = 0U;
        RecDat_is_receiving = 1U;
    }
    else
    {
        RecDat_is_receiving = 0U;
        RecDat_error[RecDat_UART_ERROR]++;
        RecDat_retry_tick = HAL_GetTick() + RECEIVE_DATA_RETRY_DELAY_MS;
        RecDat_retry_pending = 1U;
    }
}

uint8_t Receive_Data_is_receiving(void)
{
    return RecDat_is_receiving;
}

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    uint16_t index;
    uint16_t next_head;

    if ((huart->Instance == USART1) && (RecDat_is_receiving != 0U))
    {
        if (HAL_UARTEx_GetRxEventType(huart) == HAL_UART_RXEVENT_HT)
        {
            return;
        }
        if ((Size == 0U) || (Size > REC_DAT_DMA_BUFFER_SIZE))
        {
            return;
        }

        for (index = 0U; index < Size; index++)
        {
            next_head = (uint16_t)(RecDat_rx_head + 1U);
            if (next_head >= REC_DAT_RX_QUEUE_SIZE)
            {
                next_head = 0U;
            }
            if (next_head == RecDat_rx_tail)
            {
                RecDat_error[RecDat_UART_ERROR]++;
                break;
            }
            RecDat_rx_queue[RecDat_rx_head] = RecDat_buffer[index];
            RecDat_rx_head = next_head;
        }

        Receive_Data_start();
    }
}

void Receive_Data_set_mode(Receive_Data_Mode_TypeDef mode)
{
    if (RecDat_mode != mode)
    {
        RecDat_rx_tail = RecDat_rx_head;
        RecDat_frame_size = 0U;
        RecDat_expected_size = 0U;
        RecDat_accomplish_flag = 0U;
        RecDat_frame_count = 0U;
    }
    RecDat_mode = mode;
}

uint8_t Receive_Data_read_raw_byte(uint8_t *byte)
{
    uint16_t next_tail;

    if ((RecDat_mode != RECEIVE_DATA_MODE_RAW) ||
        (RecDat_rx_tail == RecDat_rx_head) || (byte == NULL))
    {
        return 0U;
    }

    *byte = RecDat_rx_queue[RecDat_rx_tail];
    next_tail = (uint16_t)(RecDat_rx_tail + 1U);
    if (next_tail >= REC_DAT_RX_QUEUE_SIZE)
    {
        next_tail = 0U;
    }
    RecDat_rx_tail = next_tail;

    return 1U;
}

void Receive_Data_pause(void)
{
    RecDat_paused = 1U;
    RecDat_is_receiving = 0U;
    (void)HAL_UART_DMAStop(&huart1);
    RecDat_clear_uart_receive_state();
    RecDat_reset_receive_state();
}

void Receive_Data_resume(void)
{
    RecDat_reset_receive_state();
    RecDat_paused = 0U;
    RecDat_clear_uart_receive_state();
    Receive_Data_start();
}

void Receive_Data_process(void)
{
    uint8_t byte;
    uint16_t next_tail;

    if (RecDat_retry_pending != 0U)
    {
        if ((int32_t)(HAL_GetTick() - RecDat_retry_tick) >= 0)
        {
            RecDat_clear_uart_receive_state();
            Receive_Data_start();
        }
        return;
    }

    if (RecDat_mode == RECEIVE_DATA_MODE_RAW)
    {
        return;
    }

    while (RecDat_rx_tail != RecDat_rx_head)
    {
        byte = RecDat_rx_queue[RecDat_rx_tail];
        next_tail = (uint16_t)(RecDat_rx_tail + 1U);
        if (next_tail >= REC_DAT_RX_QUEUE_SIZE)
        {
            next_tail = 0U;
        }
        RecDat_rx_tail = next_tail;
        RecDat_feed_byte(byte);
    }

    if (RecDat_frame_count == 0U)
    {
        return;
    }

    if (RecDat_mode == RECEIVE_DATA_MODE_PACKAGE)
    {
        RecDat_restore_last_frame();
    }

    RecDat_accomplish_flag = 1;
}

void Receive_Data_reset(void)
{
    RecDat_clear_values();
    RecDat_frame_count = 0U;
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if ((huart->Instance == USART1) && (RecDat_paused == 0U))
    {
        RecDat_clear_uart_receive_state();
        RecDat_schedule_restart();
    }
}
