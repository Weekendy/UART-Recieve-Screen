/**
  ******************************************************************************
  * @file   ring.c
  * @brief  这个文件实现了环形缓冲区的初始化、数据接收和解析功能，主要用于处理通过UART接收到的数据。
  *         并且还实现了从环形缓冲区中解析数据帧的功能。数据帧的格式为：
  *         [0xAA][0x55][length][cmd][data...][checksum]
  *         其中，0xAA和0x55是帧头，length是数据长度，cmd是命令字节，data是数据内容，checksum是校验和。
  *          
  *         简要逻辑链路为：主机发送数据帧，本机接收到的数据，由空闲中断进入环形缓冲区的写入（生产者），
  *         由主函数while循环调用proc函数判断是否有数据帧解析后，利用状态机逐字节解析数据帧，
  *         由主函数while循环调用get_cmdAdata函数获取数据帧的命令字节和数据内容（消费者）。
  ******************************************************************************
  * @attention
  * 该文件依赖于HAL库中的UART和DMA和中断功能，因此需要在工程中启用UART和DMA，并且在CubeMX
  * 中配置好相关的UART和DMA参数。
  * 
  * @note
  * 1. 将文件 ring.c 和 ring.h 添加到工程中。
  * 2. 在 main.c 中调用 ring_init() 初始化环形缓冲区。
  * 3. 在 main.c 中调用 ring_proc() 函数，在循环中调用该函数，该函数会解析数据帧。
  * 4. 在 main.c 中调用 ring_frame_get_cmdAdata() 获取数据帧的命令字节和数据内容。
  ******************************************************************************
  */
#include "ring.h"

volatile uint8_t frame_ready;
static volatile Ring_Frame_TypeDef ready_frame;

/***
 * @brief 获取当前缓冲区当前下标的下一个下标（环形缓冲区）
 * @param index 当前下标
 * @retval 下一个下标
 */
static uint16_t next_index(uint16_t *index)
{
    return (*index + 1) % RING_BUFFER_SIZE;
}

/***
 * @brief 初始化环形缓冲区
 * @retval ring状态
*/
void ring_init(void)
{
    ring.read_pos = 0;
    ring.overflow_flag = 0;

    frame.state = RING_WAITING_HEAD_1;
    frame.length = 0;
    frame.cmd = 0;
    frame.data_index = 0;
    frame.checksum = 0;
    frame.data[0] = 0;

    ring_start();
}

/***
 * @brief 启动环形缓冲区接收
 * @retval ring状态
 */
RING_STATUS_TypeDef ring_start(void)
{
    return HAL_UARTEx_ReceiveToIdle_DMA(&huart1, ring.buffer, RING_BUFFER_SIZE) ? RING_ERROR : RING_OK;
}

/***
 * @brief 读取环形缓冲区解析一至多个字节
 * @retval ring状态
 */
RING_STATUS_TypeDef ring_proc(void)
{
    if(rx_pending)
    {
        rx_pending = 0;

        uint16_t write_pos = RING_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(huart1.hdmarx);

        while (ring.read_pos != write_pos)
        {
            uint8_t b = ring.buffer[ring.read_pos];
            uint8_t result = ring_parse_byte(b);
            ring.read_pos = next_index(&ring.read_pos);
        }

        rx_pending = 0;
        return RING_OK;
    }
}

/***
 * @brief 解析一个字节
 * @param b 字节
 * @retval ring状态
 */
RING_STATUS_TypeDef ring_parse_byte(uint8_t b)
{
    switch (frame.state)
    {
    case RING_WAITING_HEAD_1:
        if (b == RING_FRAME_HEAD_1)
        {
            frame.state = RING_WAITING_HEAD_2;
            return RING_UNCOMPLETED;
        }
        return RING_UNCOMPLETED;
    case RING_WAITING_HEAD_2:
        if (b == RING_FRAME_HEAD_2) { frame.state = RING_LENGTH; return RING_UNCOMPLETED; }
        if (b == RING_FRAME_HEAD_1) { return RING_UNCOMPLETED; }
        frame.state = RING_WAITING_HEAD_1;
        return RING_UNCOMPLETED;
    case RING_LENGTH:
        if (b == 0 || b > FRAME_DATA_MAX) {
            frame.state = RING_WAITING_HEAD_1;
            return RING_ERROR;
        }
        frame.length = b;
        frame.checksum = b;
        frame.state = RING_CMD;
        return RING_UNCOMPLETED;
    case RING_CMD:
        frame.cmd = b;
        frame.state = RING_DATA;
        frame.data_index = 0;
        frame.checksum += b;
        return RING_UNCOMPLETED;
    case RING_DATA:
        if (frame.data_index >= FRAME_DATA_MAX) {
            frame.state = RING_WAITING_HEAD_1;
            return RING_ERROR;
        }
        frame.data[frame.data_index++] = b;
        frame.checksum += b;
        if (frame.data_index == frame.length) frame.state = RING_CHECKSUM;
        return RING_UNCOMPLETED;
    case RING_CHECKSUM:
        if (b == frame.checksum)
        {
            memcpy((void *)&ready_frame, (const void *)&frame, sizeof(frame));
            ring_frame_reset();
            frame_ready = 1;
            return RING_OK;
        }
        ring_frame_reset();
    default:
        frame.state = RING_WAITING_HEAD_1;
        return RING_ERROR;
    }
}

/***
 * @brief 重置帧结构体
 * @retval ring状态
 */
RING_STATUS_TypeDef ring_frame_reset()
{
    frame.state = RING_WAITING_HEAD_1;
    frame.length = 0;
    frame.cmd = 0;
    frame.data_index = 0;
    frame.checksum = 0;
    return RING_OK;
}

/***
 * @brief USART接收完成中断回调
 * @param huart USART句柄
 * @param Size 接收数据长度
 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{ 
    if (huart->Instance == USART1)
    {
        rx_pending = 1;
    }
}

/***
 * @brief 获取当前帧的命令字节
 * @param cmd 命令字节指针
 * @retval ring状态
 */
RING_STATUS_TypeDef ring_frame_get_cmd(uint8_t *cmd)
{
    if (cmd == NULL)
    {
        return RING_ERROR;
    }
    *cmd = ready_frame.cmd;
    return RING_OK;
}

/***
 * @brief 获取当前帧的数据
 * @param data 数据指针
 * @param length 数据长度指针
 * @retval ring状态
 */
RING_STATUS_TypeDef ring_frame_get_data(uint8_t *data, uint16_t *length)
{
    
        if (data == NULL || length == NULL)
        {
            return RING_ERROR;
        }
        for (uint8_t i = 0; i < ready_frame.length; i++)
        {
            data[i] = ready_frame.data[i];
        }
        *length = ready_frame.length;
        return RING_OK;
}

/***
 * @brief 获取当前帧的命令字节和数据
 * @param cmd 命令字节指针
 * @param data 数据指针
 * @param length 数据长度指针
 * @retval ring状态
 */
RING_STATUS_TypeDef ring_frame_get_cmdAdata(uint8_t *cmd, uint8_t *data, uint16_t *length)
{
    if(cmd == NULL || data == NULL || length == NULL)
    {
        return RING_ERROR;
    }

    if(frame_ready)
    {
        frame_ready = 0;
        ring_frame_get_cmd(cmd);
        ring_frame_get_data(data, length);
        return RING_OK;
    }
    return RING_UNREADY;
}