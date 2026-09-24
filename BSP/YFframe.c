/**
  ******************************************************************************
  * @file   YFframe.c
  * @brief  本文件实现了状态机解析数据帧的功能。数据帧的格式为：
  *         [0xAA][0x55][length][cmd]{ [data_type][data]... }[checksum]
  *         其中，0xAA和0x55是帧头，length是数据的个数，cmd是命令字节，
  *         data_type是数据类型，data是数据内容，checksum是校验和。
  * 
  *         @data_tpye 有u8, i8, u16, i18, u32, i32, float类型，
  *         详见枚举类型 @Frame_Type_TypeDef。
  *         @checksum 为从length到最后一个data的最后一位的校验和。
  *          
  *         简要逻辑链路为：
  *         由 YFframe_parse_byte() 利用状态机单个字节，需要外部缓冲区文件循环调用（消费者）。
  *         再 YFframe_get_cmdAdata() 获取数据帧的命令字节和数据内容的字符串。
  *         外部暴露的ready_frame也作为保存数据的载体，在frame_ready为1时调用。
  ******************************************************************************
  * @attention
  * 无
  * 
  * @note
  * 1. 将文件 ring.c 和 ring.h 添加到工程中。
  * 2. 在 main.c 中调用 YFframe_init() 初始化环形缓冲区。
  * 3. 在 ring.c 中的ring_proc()或者其他缓冲区的循环函数中调用YFframe_parse_byte(b)。
  * 4. 在 main.c 中直接调用 YFframe_get_cmdAdata() 获取数据帧的命令字节和数据内容的字符串。 或者
  * 4. 在 main.c 中在if判断frame_ready为1的情况下，直接使用ready_frame中的数据。
  ******************************************************************************
  */

#include "YFframe.h"

volatile uint8_t frame_ready;

static volatile YFFrame_TypeDef frame;
volatile YFFrame_TypeDef ready_frame;

static void YFframe_write_u32(char *text, uint8_t capacity, uint32_t value);
static void YFframe_write_i32(char *text, uint8_t capacity, int32_t value);
static void YFframe_write_float(char *text, uint8_t capacity, float value);

/***
 * @brief 将字节搬到数据帧中，并返回是否完成数据帧
 * @param b 字节
 * @param times 数据长度，搬运次数
 * @retval YFframe状态
 */
static YFFrame_Status_TypeDef YFframe_put_load_times(uint8_t b, uint8_t times)
{
    frame.data[frame.data_index].load.raw[frame.data[frame.data_index].load_index] = b;
    frame.data[frame.data_index].load_index++;
    frame.checksum += b;

    // 数据长度未达到数据包设定值时，继续接收数据
    if(frame.data[frame.data_index].load_index < times)
    {
        frame.state = YFFRAME_DATA;
        return YFFRAME_UNCOMPLETED;
    }
    else
    {
        frame.data_index++;
        // 数据长度达到数据包设定值时，转到校验和状态
        if(frame.data_index >= frame.length)
        {
            frame.state = YFFRAME_CHECKSUM;
            return YFFRAME_UNCOMPLETED;
        }
        else
        {
            frame.state = YFFRAME_DATA_TYPE;
            return YFFRAME_UNCOMPLETED;
        }
    }
}

/***
 * @brief 初始化帧结构体
 * @retval YFframe状态
 */
YFFrame_Status_TypeDef YFframe_init(void)
{
    YFframe_reset();
    return YFFRAME_OK;
}

/***
 * @brief 解析一个字节
 * @param b 字节
 * @retval YFframe状态
 */
YFFrame_Status_TypeDef YFframe_parse_byte(uint8_t b)
{
    switch (frame.state)
    {
    // 帧头1
    case YFFRAME_WAITING_HEAD_1:
        if (b == RING_FRAME_HEAD_1)
        {
            frame.state = YFFRAME_WAITING_HEAD_2;
            return YFFRAME_UNCOMPLETED;
        }
        return YFFRAME_UNCOMPLETED;
    // 帧头2
    case YFFRAME_WAITING_HEAD_2:
        if (b == RING_FRAME_HEAD_2) { frame.state = YFFRAME_LENGTH; return YFFRAME_UNCOMPLETED; }
        if (b == RING_FRAME_HEAD_1) { return YFFRAME_UNCOMPLETED; }
        frame.state = YFFRAME_WAITING_HEAD_1;
        return YFFRAME_UNCOMPLETED;
    // 数据长度
    case YFFRAME_LENGTH:
        if (b == 0 || b > YFFRAME_DATA_MAX) {
            frame.state = YFFRAME_WAITING_HEAD_1;
            return YFFRAME_ERROR;
        }
        frame.length = b;
        frame.checksum = b;
        frame.state = YFFRAME_CMD;
        return YFFRAME_UNCOMPLETED;
    // 帧命令
    case YFFRAME_CMD:
        frame.cmd = b;
        frame.state = YFFRAME_DATA_TYPE;
        frame.data_index = 0;
        frame.checksum += b;
        return YFFRAME_UNCOMPLETED;
    // 数据类型
    case YFFRAME_DATA_TYPE:
        // 数据长度超出最大值时，返回错误
        if (frame.data_index >= YFFRAME_DATA_MAX)
        {
            frame.state = YFFRAME_WAITING_HEAD_1;
            return YFFRAME_ERROR;
        }
        frame.data[frame.data_index].type = (Frame_Type_TypeDef)b;
        frame.checksum += b;
        frame.state = YFFRAME_DATA;
        frame.data[frame.data_index].load_index = 0;
        return YFFRAME_UNCOMPLETED;
    // 数据
    case YFFRAME_DATA:
        switch (frame.data[frame.data_index].type)
        {
        case YFFRAME_U8_TYPE: 
            return YFframe_put_load_times(b, 1);
        case YFFRAME_I8_TYPE:
            return YFframe_put_load_times(b, 1);
        case YFFRAME_U16_TYPE:
            return YFframe_put_load_times(b, 2);
        case YFFRAME_I16_TYPE:
            return YFframe_put_load_times(b, 2);
        case YFFRAME_U32_TYPE:
            return YFframe_put_load_times(b, 4);
        case YFFRAME_I32_TYPE:
            return YFframe_put_load_times(b, 4);
        case YFFRAME_FLOAT_TYPE:
            return YFframe_put_load_times(b, 4);
        default:
            YFframe_reset();
            return YFFRAME_ERROR;
        }
    case YFFRAME_CHECKSUM:
        if (b == frame.checksum)
        {
            memcpy((void *)&ready_frame, (const void *)&frame, sizeof(frame));
            YFframe_reset();
            frame_ready = 1;
            return YFFRAME_OK;
        }
        YFframe_reset();
    default:
        frame.state = YFFRAME_WAITING_HEAD_1;
        return YFFRAME_ERROR;
    }
}

/***
 * @brief 重置帧结构体
 * @retval YFframe状态
 */
YFFrame_Status_TypeDef YFframe_reset()
{
    frame.state = YFFRAME_WAITING_HEAD_1;
    frame.length = 0;
    frame.cmd = 0;
    frame.data_index = 0;
    frame.checksum = 0;
    return YFFRAME_OK;
}

/***
 * @brief 获取当前帧的命令字节
 * @param cmd 命令字节指针
 * @retval YFframe状态
 */
YFFrame_Status_TypeDef YFframe_get_cmd(uint8_t *cmd)
{
    if (cmd == NULL)
    {
        return YFFRAME_ERROR;
    }
    *cmd = ready_frame.cmd;
    return YFFRAME_OK;
}

/***
 * @brief 获取当前帧的数据
 * @param data 数据结构体指针
 * @retval YFframe状态
 */
YFFrame_Status_TypeDef YFframe_get_data_text(YFFrame_Data_Text_TypeDef *text)
{
    if (text == NULL)
    {
        return YFFRAME_ERROR;
    }

    // 逐数据转化为字符串存在data->data_text中
    for (uint8_t i = 0; i < ready_frame.length; i++)
    {
        switch (ready_frame.data[i].type)
        {
        case YFFRAME_U8_TYPE: {
            uint8_t u8_data = ready_frame.data[i].load.u8_data;
            YFframe_write_u32(text->data_text[i], sizeof(text->data_text[i]), u8_data);
            break;  }
        case YFFRAME_I8_TYPE: {
            int8_t i8_data = ready_frame.data[i].load.i8_data;
            YFframe_write_i32(text->data_text[i], sizeof(text->data_text[i]), i8_data);
            break;  }
        case YFFRAME_U16_TYPE: {
            uint16_t u16_data = ready_frame.data[i].load.u16_data;
            YFframe_write_u32(text->data_text[i], sizeof(text->data_text[i]), u16_data);
            break;  }
        case YFFRAME_I16_TYPE: {
            int16_t i16_data = ready_frame.data[i].load.i16_data;
            YFframe_write_i32(text->data_text[i], sizeof(text->data_text[i]), i16_data);
            break;  }
        case YFFRAME_U32_TYPE: {
            uint32_t u32_data = ready_frame.data[i].load.u32_data;
            YFframe_write_u32(text->data_text[i], sizeof(text->data_text[i]), u32_data);
            break;  }
        case YFFRAME_I32_TYPE: {
            int32_t i32_data = ready_frame.data[i].load.i32_data;
            YFframe_write_i32(text->data_text[i], sizeof(text->data_text[i]), i32_data);
            break;  }
        case YFFRAME_FLOAT_TYPE: {
            float float_data = ready_frame.data[i].load.float_data;   
            YFframe_write_float(text->data_text[i], sizeof(text->data_text[i]), float_data);
            break;  }
        default:
            return YFFRAME_ERROR;
        }
    }

    text->data_length = (uint8_t)ready_frame.length;
    return YFFRAME_OK;
}

/***
 * @brief 获取当前帧的命令字节和数据
 * @param cmd 命令字节指针
 * @param data 数据指针
 * @param length 数据长度指针
 * @retval YFframe状态
 */
YFFrame_Status_TypeDef YFframe_get_cmdAdata(uint8_t *cmd, YFFrame_Data_Text_TypeDef *text)
{
    if (!frame_ready)
    {
        return YFFRAME_UNREADY;
    }

    if (cmd == NULL || text == NULL)
    {
        return YFFRAME_ERROR;
    }

    *cmd = ready_frame.cmd;
    if (YFframe_get_data_text(text) != YFFRAME_OK)
    {
        return YFFRAME_ERROR;
    }

    frame_ready = 0;
    return YFFRAME_OK;
}


/*-------------------------以下为各类型转化为字符串的函数-----------------------------------*/

/***
 * @brief 将u32转化为字符串
 * @param text 字符串指针
 * @param capacity 字符串容量
 * @param value u32值
 */
static void YFframe_write_u32(char *text, uint8_t capacity, uint32_t value)
{
    char digits[10];
    uint8_t count = 0U;
    uint8_t index = 0U;

    if (capacity == 0U)
    {
        return;
    }

    do
    {
        digits[count++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while ((value != 0U) && (count < sizeof(digits)));

    while ((count != 0U) && (index + 1U < capacity))
    {
        text[index++] = digits[--count];
    }
    text[index] = '\0';
}

/***
 * @brief 将i32转化为字符串
 * @param text 字符串指针
 * @param capacity 字符串容量
 * @param value i32值
 */
static void YFframe_write_i32(char *text, uint8_t capacity, int32_t value)
{
    uint32_t magnitude;

    if (capacity == 0U)
    {
        return;
    }

    if (value < 0)
    {
        text[0] = '-';
        if (capacity == 1U)
        {
            return;
        }
        magnitude = (uint32_t)(-(value + 1)) + 1U;
        YFframe_write_u32(&text[1], (uint8_t)(capacity - 1U), magnitude);
    }
    else
    {
        YFframe_write_u32(text, capacity, (uint32_t)value);
    }
}


/***
 * @brief 将float转化为字符串
 * @param text 字符串指针
 * @param capacity 字符串容量
 * @param value float值
 */
static void YFframe_write_float(char *text, uint8_t capacity, float value)
{
    char whole_text[11];
    uint32_t whole;
    uint32_t fraction;
    uint8_t index = 0U;
    uint8_t whole_index = 0U;
    uint8_t negative = 0U;

    if (capacity == 0U)
    {
        return;
    }

    if (value != value)
    {
        if (capacity > 3U)
        {
            text[0] = 'n';
            text[1] = 'a';
            text[2] = 'n';
            text[3] = '\0';
        }
        else
        {
            text[0] = '\0';
        }
        return;
    }

    if (value < 0.0f)
    {
        negative = 1U;
        value = -value;
    }

    whole = (uint32_t)value;
    fraction = (uint32_t)((value - (float)whole) * 1000.0f + 0.5f);
    if (fraction >= 1000U)
    {
        whole++;
        fraction -= 1000U;
    }

    YFframe_write_u32(whole_text, sizeof(whole_text), whole);
    if (negative && (index + 1U < capacity))
    {
        text[index++] = '-';
    }
    while ((whole_text[whole_index] != '\0') && (index + 1U < capacity))
    {
        text[index++] = whole_text[whole_index++];
    }
    if (index + 1U < capacity)
    {
        text[index++] = '.';
    }
    if (index + 1U < capacity)
    {
        text[index++] = (char)('0' + (fraction / 100U));
    }
    if (index + 1U < capacity)
    {
        text[index++] = (char)('0' + ((fraction / 10U) % 10U));
    }
    if (index + 1U < capacity)
    {
        text[index++] = (char)('0' + (fraction % 10U));
    }
    text[index] = '\0';
}