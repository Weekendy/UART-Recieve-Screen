#include "YFframe.h"

volatile uint8_t frame_ready;
char YFframe_data_text[YFFRAME_DATA_MAX][24];
uint8_t YFframe_data_text_length;

static volatile YFFrame_TypeDef frame;
static volatile YFFrame_TypeDef ready_frame;

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
YFFrame_Status_TypeDef YFframe_init()
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
 * @param data 数据指针
 * @param length 数据长度指针
 * @retval YFframe状态
 */
YFFrame_Status_TypeDef YFframe_get_data_text(uint8_t *data_length)
{
    if (data_length == NULL)
    {
        return YFFRAME_ERROR;
    }

    for (uint8_t i = 0; i < ready_frame.length; i++)
    {
        switch (ready_frame.data[i].type)
        {
        case YFFRAME_U8_TYPE: {
            uint8_t u8_data = ready_frame.data[i].load.u8_data;
            snprintf(YFframe_data_text[i], sizeof(YFframe_data_text[i]), "%d", u8_data);   
            break;  }
        case YFFRAME_I8_TYPE: {
            int8_t i8_data = ready_frame.data[i].load.i8_data;
            snprintf(YFframe_data_text[i], sizeof(YFframe_data_text[i]), "%d", i8_data);
            break;  }
        case YFFRAME_U16_TYPE: {
            uint16_t u16_data = ready_frame.data[i].load.u16_data;
            snprintf(YFframe_data_text[i], sizeof(YFframe_data_text[i]), "%d", u16_data);
            break;  }
        case YFFRAME_I16_TYPE: {
            int16_t i16_data = ready_frame.data[i].load.i16_data;
            snprintf(YFframe_data_text[i], sizeof(YFframe_data_text[i]), "%d", i16_data);
            break;  }
        case YFFRAME_U32_TYPE: {
            uint32_t u32_data = ready_frame.data[i].load.u32_data;
            snprintf(YFframe_data_text[i], sizeof(YFframe_data_text[i]), "%lu",
                     (unsigned long)u32_data);
            break;  }
        case YFFRAME_I32_TYPE: {
            int32_t i32_data = ready_frame.data[i].load.i32_data;
            snprintf(YFframe_data_text[i], sizeof(YFframe_data_text[i]), "%d", i32_data);
            break;  }
        case YFFRAME_FLOAT_TYPE: {
            float float_data = ready_frame.data[i].load.float_data;   
            snprintf(YFframe_data_text[i], sizeof(YFframe_data_text[i]), "%.3f", float_data);
            break;  }
        default:
            return YFFRAME_ERROR;
        }
    }

    *data_length = (uint8_t)ready_frame.length;
    return YFFRAME_OK;
}

/***
 * @brief 获取当前帧的命令字节和数据
 * @param cmd 命令字节指针
 * @param data 数据指针
 * @param length 数据长度指针
 * @retval YFframe状态
 */
YFFrame_Status_TypeDef YFframe_get_cmdAdata(uint8_t *cmd, uint8_t *data_length)
{
    if(cmd == NULL ||  data_length == NULL)
    {
        return YFFRAME_ERROR;
    }

    if (frame_ready)
    {
        frame_ready = 0;
        if (YFframe_get_data_text(&YFframe_data_text_length) == YFFRAME_OK)
        {
            for (uint8_t i = 0; i < YFframe_data_text_length; i++)
            {
                log_printf("data[%d]=%s\r\n", i, YFframe_data_text[i]);
            }
        }
    }
}
