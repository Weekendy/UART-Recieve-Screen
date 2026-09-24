/**
  ******************************************************************************
  * @file   ring.c
  * @brief  这个文件实现了环形缓冲区的初始化、数据接收和解析功能，主要用于处理通过UART接收到的数据。
  *         这个文件依赖YFframe.c或其他解析数据帧库，用于解析数据帧，因此也需要添加到工程中。
  *          
  *         简要逻辑链路为：主机发送数据帧，本机接收到的数据，由空闲中断进入环形缓冲区的写入（生产者），
  *         由主函数while循环调用proc函数判断是否有数据帧解析后，并调用外部函数解析数据帧（消费者）。
  ******************************************************************************
  * @attention
  * 该文件依赖于HAL库中的UART和DMA和中断功能，因此需要在工程中启用UART和DMA，并且在CubeMX
  * 中配置好相关的UART和DMA参数。
  * 
  * @note
  * 1. 将文件 ring.c 和 ring.h 添加到工程中。
  * 2. 在 main.c 中调用 ring_init() 初始化环形缓冲区。
  * 3. 在 ring_proc 中添加外部的解析数据帧函数。
  * 4. 在 main.c 中调用 ring_proc() 函数。
  ******************************************************************************
  */
 
#include "ring.h"

volatile uint8_t rx_pending;
volatile uint8_t rx_restart_pending;
volatile uint32_t ring_overflow_count;
volatile Ring_TypeDef ring;

/***
 * @brief 获取当前缓冲区当前下标的下一个下标（环形缓冲区）
 * @param index 当前下标
 * @retval 下一个下标
 */
/***
 * @brief 初始化环形缓冲区
 * @retval ring状态
*/
void ring_init(void)
{
    rx_pending = 0U;
    rx_restart_pending = 0U;
    ring.read_pos = 0;
    ring.read_total = 0U;
    ring.write_total = 0U;
    ring.dma_write_pos = 0U;
    ring.overflow_flag = 0;
    ring_overflow_count = 0U;



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
    uint8_t received = 0U;

    // 如果重启挂起标志为1，则重启环形缓冲区
    if (rx_restart_pending != 0U)
    {
        rx_restart_pending = 0U;
        YFframe_reset();
        frame_ready = 0U;
        ring.read_pos = 0U;
        ring.read_total = 0U;
        ring.write_total = 0U;
        ring.dma_write_pos = 0U;
        ring.overflow_flag = 0U;
        // 重启DMA，若未成功，则继续重启挂起，等待下一次重启
        if (ring_start() != RING_OK)
        {
            rx_restart_pending = 1U;
            return RING_ERROR;
        }
    }

    // 处理数据循环
    while(1)
    {
        uint32_t write_total = ring.write_total;

        // 如果写总数多读总数一个缓冲区（即写位置超了读位置一轮），则将溢出标志置1，并抛弃该帧
        if ((write_total - ring.read_total) >= RING_BUFFER_SIZE)
        {
            ring_overflow_count++;
            ring.overflow_flag = 1U;
            ring.read_total = write_total;
            ring.read_pos = (uint16_t)(write_total % RING_BUFFER_SIZE);
            YFframe_reset();
            frame_ready = 0U;
            return RING_ERROR;
        }

        // 当前读总数等于写总数（即写位置追上读位置），则退出循环
        if (ring.read_total == write_total)
        {
            break;
        }

        // 既未溢出又未写完，则将接收数据为置1，表示有数据可读
        received = 1U;
        // 当读总数小于写总数，则开始解析数据
        while (ring.read_total < write_total)
        {
            ring.read_pos = (uint16_t)(ring.read_total % RING_BUFFER_SIZE);
            (void)YFframe_parse_byte(ring.buffer[ring.read_pos]);
            ring.read_total++;
        }
    }

    // 接收挂起位置0
    rx_pending = 0;
    // 返回接收状态
    return received ? RING_OK : RING_UNREADY;
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
        uint16_t write_pos = (Size >= RING_BUFFER_SIZE) ? 0U : Size;
        uint16_t delta = (uint16_t)((write_pos + RING_BUFFER_SIZE - ring.dma_write_pos) % RING_BUFFER_SIZE);

        ring.write_total += delta;
        ring.dma_write_pos = write_pos;
        rx_pending = 1;
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        rx_restart_pending = 1U;
    }
}
