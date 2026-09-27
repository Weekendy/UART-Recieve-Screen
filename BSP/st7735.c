/**
  ******************************************************************************
  * @file   st7735.c
  * @brief  本文件实现 ST7735 TFT 液晶屏驱动。
  *         通过 SPI 向控制器发送命令和 RGB565 像素数据，提供像素、矩形、
  *         图像及文本绘制接口；较大的屏幕区域或图像数据可使用 SPI DMA 传输。
  *         背光亮度通过配置的定时器 PWM 通道控制。
  *
  *         简要逻辑链路为：
  *         绘图接口 -> 设置显示窗口并发送 RAMWR -> SPI/DMA 分块传输 RGB565 数据
  *         -> DMA 完成回调启动后续数据块，全部传输完成后释放 CS。
  ******************************************************************************
  * @attention
  * SPI、GPIO、DMA、定时器句柄及引脚由 st7735.h 中的配置宏指定，
  * 必须与 CubeMX 生成的工程配置保持一致。
  * 
  * @note
  * 1. 将文件 ring.c 和 ring.h 添加到工程中。
  * 2. 外设初始化完成后调用 ST7735_init() 初始化显示控制器。
  * 3. 调用 st7735.h 中声明的绘图接口更新屏幕内容。
  * 4. 使用 DMA 绘图时，可通过 ST7735_is_busy() 或 ST7735_wait_for_DMA() 查询或等待传输完成。
  ******************************************************************************
  */
#include "st7735.h"

/**
 * @brief ST7735 控制器命令码。
 * @note 以下命令用于复位、设置显示窗口、像素格式、供电和 Gamma 曲线。
 */
#define ST7735_SWRESET 0x01U
#define ST7735_SLPOUT  0x11U
#define ST7735_NORON   0x13U
#define ST7735_INVOFF  0x20U
#define ST7735_INVON   0x21U
#define ST7735_DISPON  0x29U
#define ST7735_CASET   0x2AU
#define ST7735_RASET   0x2BU
#define ST7735_RAMWR   0x2CU
#define ST7735_MADCTL  0x36U
#define ST7735_COLMOD  0x3AU
#define ST7735_GAMSET  0x26U
#define ST7735_FRMCTR1 0xB1U
#define ST7735_FRMCTR2 0xB2U
#define ST7735_FRMCTR3 0xB3U
#define ST7735_INVCTR  0xB4U
#define ST7735_DISSET5 0xB6U
#define ST7735_PWCTR1  0xC0U
#define ST7735_PWCTR2  0xC1U
#define ST7735_PWCTR3  0xC2U
#define ST7735_PWCTR4  0xC3U
#define ST7735_PWCTR5  0xC4U
#define ST7735_VMCTR1  0xC5U
#define ST7735_GMCTRP1 0xE0U
#define ST7735_GMCTRN1 0xE1U

#define ST7735_DMA_BLOCK_BYTES 512U
#define ST7735_CHAR_1_WIDTH 6U
#define ST7735_CHAR_1_HEIGHT 8U
#define ST7735_LINE_BUF_SIZE 2048

#define ST7735_SOFT_PWM_ENABLE

/** 屏幕片选控制：低电平选中 ST7735，高电平取消选中。 */
#define ST7735_CS_LOW()  HAL_GPIO_WritePin(ST7735_CS_GPIO_PORT, ST7735_CS_PIN, GPIO_PIN_RESET)
#define ST7735_CS_HIGH() HAL_GPIO_WritePin(ST7735_CS_GPIO_PORT, ST7735_CS_PIN, GPIO_PIN_SET)
/** 数据/命令选择：低电平表示命令，高电平表示显示数据。 */
#define ST7735_DC_COMMAND() HAL_GPIO_WritePin(ST7735_DC_GPIO_PORT, ST7735_DC_PIN, GPIO_PIN_RESET)
#define ST7735_DC_DATA()    HAL_GPIO_WritePin(ST7735_DC_GPIO_PORT, ST7735_DC_PIN, GPIO_PIN_SET)

static uint8_t st7735_dma_line_buf[ST7735_LINE_BUF_SIZE];
static volatile uint8_t st7735_dma_busy;

static void st7735_wait_until_ready(void);

#ifdef ST7735_SOFT_PWM_ENABLE
void ST7735_BL_Start(void);
#endif

/**
 * @brief 结束当前 ST7735 DMA 传输。
 * @retval None
 * @note 释放片选并清除 DMA 传输状态；传输完成或发生错误时调用。
 */
static void st7735_finish_dma(void)
{
    ST7735_CS_HIGH();
    st7735_dma_busy = 0U;
}

/**
 * @brief 等待 ST7735 DMA 传输完成。
 * @retval None
 * @note 该函数会阻塞当前流程，直到 DMA 忙标志被清除。
 */
static void st7735_wait_until_ready(void)
{
    while (st7735_dma_busy != 0U) {
    }
}

/**
 * @brief 发送一个 ST7735 命令字节。
 * @param command 要发送的控制器命令码。
 * @retval None
 */
static void st7735_command(uint8_t command)
{
    st7735_wait_until_ready();
    ST7735_DC_COMMAND();
    (void)HAL_SPI_Transmit(&ST7735_SPI_HANDLE, &command, 1U, HAL_MAX_DELAY);
}

/**
 * @brief 通过 SPI 发送显示数据。
 * @param data 数据缓冲区首地址。
 * @param length 要发送的字节数。
 * @retval None
 */
static void st7735_data(const uint8_t *data, uint16_t length)
{
    st7735_wait_until_ready();
    ST7735_DC_DATA();
    (void)HAL_SPI_Transmit(&ST7735_SPI_HANDLE, (uint8_t *)data, length, HAL_MAX_DELAY);
}

/**
 * @brief 发送一个命令及其可选的数据参数。
 * @param command 要发送的控制器命令码。
 * @param data 命令参数缓冲区；当 length 为 0 时可为空。
 * @param length 命令参数的字节数。
 * @retval None
 */
static void st7735_command_data(uint8_t command, const uint8_t *data, uint16_t length)
{
    st7735_command(command);
    if (length != 0U) {
        st7735_data(data, length);
    }
}

/**
 * @brief 设置 ST7735 的内存写入窗口。
 * @param x0 窗口左边界逻辑坐标，包含此坐标。
 * @param y0 窗口上边界逻辑坐标，包含此坐标。
 * @param x1 窗口右边界逻辑坐标，包含此坐标。
 * @param y1 窗口下边界逻辑坐标，包含此坐标。
 * @retval None
 * @note 函数会自动加上 XSTART/YSTART 偏移，并发送 RAMWR 命令。
 */
static void st7735_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    data[0] = (uint8_t)((x0 + ST7735_XSTART) >> 8);
    data[1] = (uint8_t)(x0 + ST7735_XSTART);
    data[2] = (uint8_t)((x1 + ST7735_XSTART) >> 8);
    data[3] = (uint8_t)(x1 + ST7735_XSTART);
    st7735_command_data(ST7735_CASET, data, sizeof(data));

    data[0] = (uint8_t)((y0 + ST7735_YSTART) >> 8);
    data[1] = (uint8_t)(y0 + ST7735_YSTART);
    data[2] = (uint8_t)((y1 + ST7735_YSTART) >> 8);
    data[3] = (uint8_t)(y1 + ST7735_YSTART);
    st7735_command_data(ST7735_RASET, data, sizeof(data));
    st7735_command(ST7735_RAMWR);
}

/**
 * @brief 复位 ST7735 控制器。
 * @retval None
 * @note 启用硬件复位时先操作 RST 引脚，否则仅发送软件复位命令；两种方式均包含必要延时。
 */
static void st7735_reset(void)
{
#if ST7735_USE_RESET_PIN
    HAL_GPIO_WritePin(ST7735_RST_GPIO_PORT, ST7735_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(5U);
    HAL_GPIO_WritePin(ST7735_RST_GPIO_PORT, ST7735_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(5U);
#endif
    st7735_command(ST7735_SWRESET);
    HAL_Delay(150U);
}

/**
 * @brief 初始化 ST7735 显示控制器。
 * @retval None
 * @note 配置帧率、供电、扫描方向、RGB565 像素格式和 Gamma 曲线，然后开启显示。
 */
void ST7735_init(void)
{
#ifdef ST7735_SOFT_PWM_ENABLE
    /* PC13 has no TIM1 alternate-function output, so drive it from TIM1 IRQs. */
    ST7735_BL_Start();
#else
    (void)HAL_TIM_PWM_Start(&ST7735_BACKLIGHT_TIMER_HANDLE,
                            ST7735_BACKLIGHT_TIMER_CHANNEL);
#endif

    /* 设置背光 PWM 占空比为 800 */
    ST7735_set_BL(ST7735_BACKLIGHT_DEFAULT_COMPARE);

    /** 完整显示模式下的帧率控制参数。 */
    static const uint8_t frame_rate[] = {0x01U, 0x2CU, 0x2DU};
    /** 部分显示与空闲显示模式下的帧率控制参数。 */
    static const uint8_t frame_rate_partial[] = {0x01U, 0x2CU, 0x2DU,
                                                   0x01U, 0x2CU, 0x2DU};
    /** 正向 Gamma 曲线参数。 */
    static const uint8_t gamma_positive[] = {
        0x02U, 0x1CU, 0x07U, 0x12U, 0x37U, 0x32U, 0x29U, 0x2DU,
        0x29U, 0x25U, 0x2BU, 0x39U, 0x00U, 0x01U, 0x03U, 0x10U};
    /** 反向 Gamma 曲线参数。 */
    static const uint8_t gamma_negative[] = {
        0x03U, 0x1DU, 0x07U, 0x06U, 0x2EU, 0x2CU, 0x29U, 0x2DU,
        0x2EU, 0x2EU, 0x37U, 0x3FU, 0x00U, 0x00U, 0x02U, 0x10U};
    uint8_t data[3];

    st7735_wait_until_ready();
    ST7735_CS_LOW();
    st7735_reset();
    st7735_command(ST7735_SLPOUT);
    HAL_Delay(150U);
    st7735_command_data(ST7735_FRMCTR1, frame_rate, sizeof(frame_rate));
    st7735_command_data(ST7735_FRMCTR2, frame_rate, sizeof(frame_rate));
    st7735_command_data(ST7735_FRMCTR3, frame_rate_partial, sizeof(frame_rate_partial));
    data[0] = 0x07U;
    st7735_command_data(ST7735_INVCTR, data, 1U);
    data[0] = 0xA2U; data[1] = 0x02U; data[2] = 0x84U;
    st7735_command_data(ST7735_PWCTR1, data, 3U);
    data[0] = 0xC5U; st7735_command_data(ST7735_PWCTR2, data, 1U);
    data[0] = 0x0AU; data[1] = 0x00U; st7735_command_data(ST7735_PWCTR3, data, 2U);
    data[0] = 0x8AU; data[1] = 0x2AU; st7735_command_data(ST7735_PWCTR4, data, 2U);
    data[0] = 0x8AU; data[1] = 0xEEU; st7735_command_data(ST7735_PWCTR5, data, 2U);
    data[0] = 0x0EU; st7735_command_data(ST7735_VMCTR1, data, 1U);
    st7735_command(ST7735_INVOFF);
    data[0] = 0xC0U; /* MX | MY | RGB：匹配下方定义的 RGB565 颜色格式。 */
    st7735_command_data(ST7735_MADCTL, data, 1U);
    data[0] = 0x05U;
    st7735_command_data(ST7735_COLMOD, data, 1U);
    st7735_command(ST7735_GMCTRP1);
    st7735_data(gamma_positive, sizeof(gamma_positive));
    st7735_command_data(ST7735_GMCTRN1, gamma_negative, sizeof(gamma_negative));
    st7735_command(ST7735_NORON);
    HAL_Delay(10U);
    st7735_command(ST7735_DISPON);
    HAL_Delay(100U);
    ST7735_CS_HIGH();
    ST7735_fill_rectangle(0, 0, ST7735_WIDTH, ST7735_HEIGHT, ST7735_BLACK);
}

/**
 * @brief 设置背光 PWM 占空比。
 * @param compare TIM3 通道 3 的比较值；大于自动重装载值时会被限制为该值。
 * @retval None
 */
void ST7735_set_BL(uint16_t compare)
{
    if (compare > __HAL_TIM_GET_AUTORELOAD(&ST7735_BACKLIGHT_TIMER_HANDLE)) {
        compare = __HAL_TIM_GET_AUTORELOAD(&ST7735_BACKLIGHT_TIMER_HANDLE);
    }
    __HAL_TIM_SET_COMPARE(&ST7735_BACKLIGHT_TIMER_HANDLE,
                          ST7735_BACKLIGHT_TIMER_CHANNEL, compare);
}

/**
 * @brief 在屏幕指定位置绘制一个像素。
 * @param x 横坐标，范围为 0~ST7735_WIDTH-1。
 * @param y 纵坐标，范围为 0~ST7735_HEIGHT-1。
 * @param color RGB565 颜色值。
 * @retval None
 * @note 坐标越界时不执行绘制。
 */
void ST7735_draw_pixel(uint16_t x, uint16_t y, uint16_t color)
{
    uint8_t data[2];
    if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT)) return;
    data[0] = (uint8_t)(color >> 8); data[1] = (uint8_t)color;
    st7735_wait_until_ready();
    ST7735_CS_LOW();
    st7735_window(x, y, x, y);
    st7735_data(data, sizeof(data));
    ST7735_CS_HIGH();
}

/**
 * @brief 填充屏幕上的矩形区域。
 * @param x 矩形左上角横坐标。
 * @param y 矩形左上角纵坐标。
 * @param w 矩形宽度，单位：像素。
 * @param h 矩形高度，单位：像素。
 * @param color RGB565 填充颜色。
 * @retval None
 * @note 超出屏幕右侧或下侧的区域会被裁剪，零宽或零高请求会被忽略。
 */
void ST7735_fill_rectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    uint8_t line[ST7735_WIDTH * 2U];
    uint16_t i;
    if ((x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT) || (w == 0U) || (h == 0U)) return;
    if (w > ST7735_WIDTH - x) w = ST7735_WIDTH - x;
    if (h > ST7735_HEIGHT - y) h = ST7735_HEIGHT - y;
    for (i = 0U; i < w; i++) { line[2U * i] = (uint8_t)(color >> 8); line[2U * i + 1U] = (uint8_t)color; }
    st7735_wait_until_ready();
    ST7735_CS_LOW();
    st7735_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    for (i = 0U; i < h; i++) st7735_data(line, (uint16_t)(2U * w));
    ST7735_CS_HIGH();
}

/**
 * 
 * @brief 使用指定颜色填充整个屏幕。
 * @param color RGB565 填充颜色。
 * @retval None
 */
void ST7735_fill_screen(uint16_t color)
{

}

/**
 * @brief 查询 ST7735 DMA 是否正在传输。
 * @retval 非零值表示 DMA 忙，0 表示 DMA 空闲。
 */
uint8_t ST7735_is_busy(void)
{
    return st7735_dma_busy;
}

/**
 * @brief 等待 ST7735 DMA 传输完成。
 * @retval None
 * @note 该函数会阻塞当前流程，直到当前 DMA 操作完成或结束。
 */
void ST7735_wait_for_DMA(void)
{
    st7735_wait_until_ready();
}

/**
 * @brief 使用快速接口填充矩形。
 * @param x 矩形左上角横坐标。
 * @param y 矩形左上角纵坐标。
 * @param w 矩形宽度，单位：像素。
 * @param h 矩形高度，单位：像素。
 * @param color RGB565 填充颜色。
 * @retval None
 */
void ST7735_fill_rectangle_fast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    ST7735_fill_rectangle(x, y, w, h, color);
}

/**
 * @brief 将一幅 RGB565 图像写入屏幕。
 * @param x 图像左上角横坐标。
 * @param y 图像左上角纵坐标。
 * @param w 图像宽度，单位：像素。
 * @param h 图像高度，单位：像素。
 * @param data 图像数据首地址，按行优先排列。
 * @retval None
 * @note 数据为空、尺寸为零或图像未完全位于屏幕内时不执行绘制。
 */
void ST7735_draw_image(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *data)
{
    uint8_t line[ST7735_WIDTH * 2U];
    uint16_t row, column;
    if ((data == NULL) || (x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT) ||
        (w == 0U) || (h == 0U) || (w > ST7735_WIDTH - x) || (h > ST7735_HEIGHT - y)) return;
    st7735_wait_until_ready();
    ST7735_CS_LOW();
    st7735_window(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));
    for (row = 0U; row < h; row++) {
        for (column = 0U; column < w; column++) {
            uint16_t pixel = data[(uint32_t)row * w + column];
            line[2U * column] = (uint8_t)(pixel >> 8);
            line[2U * column + 1U] = (uint8_t)pixel;
        }
        st7735_data(line, (uint16_t)(2U * w));
    }
    ST7735_CS_HIGH();
}

/** 字体尺寸：每个字形实际绘制区域为 5x7，外加 1 列和 1 行间距。 */
#define ST7735_FONT_WIDTH  5U
#define ST7735_FONT_HEIGHT 7U

/**
 * @brief 5x7 大写字母点阵表，索引对应 ASCII 'A'~'Z'。
 * @note 每个元素表示一行，低 5 位对应从左到右的像素。
 */
static const uint8_t st7735_uppercase[26][7] = {
    {0x0EU,0x11U,0x11U,0x1FU,0x11U,0x11U,0x11U},{0x1EU,0x11U,0x11U,0x1EU,0x11U,0x11U,0x1EU},
    {0x0EU,0x11U,0x10U,0x10U,0x10U,0x11U,0x0EU},{0x1EU,0x11U,0x11U,0x11U,0x11U,0x11U,0x1EU},
    {0x1FU,0x10U,0x10U,0x1EU,0x10U,0x10U,0x1FU},{0x1FU,0x10U,0x10U,0x1EU,0x10U,0x10U,0x10U},
    {0x0EU,0x11U,0x10U,0x17U,0x11U,0x11U,0x0EU},{0x11U,0x11U,0x11U,0x1FU,0x11U,0x11U,0x11U},
    {0x0EU,0x04U,0x04U,0x04U,0x04U,0x04U,0x0EU},{0x01U,0x01U,0x01U,0x01U,0x11U,0x11U,0x0EU},
    {0x11U,0x12U,0x14U,0x18U,0x14U,0x12U,0x11U},{0x10U,0x10U,0x10U,0x10U,0x10U,0x10U,0x1FU},
    {0x11U,0x1BU,0x15U,0x15U,0x11U,0x11U,0x11U},{0x11U,0x19U,0x15U,0x13U,0x11U,0x11U,0x11U},
    {0x0EU,0x11U,0x11U,0x11U,0x11U,0x11U,0x0EU},{0x1EU,0x11U,0x11U,0x1EU,0x10U,0x10U,0x10U},
    {0x0EU,0x11U,0x11U,0x11U,0x15U,0x12U,0x0DU},{0x1EU,0x11U,0x11U,0x1EU,0x14U,0x12U,0x11U},
    {0x0FU,0x10U,0x10U,0x0EU,0x01U,0x01U,0x1EU},{0x1FU,0x04U,0x04U,0x04U,0x04U,0x04U,0x04U},
    {0x11U,0x11U,0x11U,0x11U,0x11U,0x11U,0x0EU},{0x11U,0x11U,0x11U,0x0AU,0x0AU,0x04U,0x04U},
    {0x11U,0x11U,0x11U,0x15U,0x15U,0x15U,0x0AU},{0x11U,0x11U,0x0AU,0x04U,0x0AU,0x11U,0x11U},
    {0x11U,0x11U,0x0AU,0x04U,0x04U,0x04U,0x04U},{0x1FU,0x01U,0x02U,0x04U,0x08U,0x10U,0x1FU}
};

/**
 * @brief 5x7 数字点阵表，索引对应 ASCII '0'~'9'。
 * @note 每个元素表示一行，低 5 位对应从左到右的像素。
 */
static const uint8_t st7735_digits[10][7] = {
    {0x0EU,0x11U,0x13U,0x15U,0x19U,0x11U,0x0EU},{0x04U,0x0CU,0x04U,0x04U,0x04U,0x04U,0x0EU},
    {0x0EU,0x11U,0x01U,0x02U,0x04U,0x08U,0x1FU},{0x1EU,0x01U,0x01U,0x0EU,0x01U,0x01U,0x1EU},
    {0x02U,0x06U,0x0AU,0x12U,0x1FU,0x02U,0x02U},{0x1FU,0x10U,0x1EU,0x01U,0x01U,0x11U,0x0EU},
    {0x06U,0x08U,0x10U,0x1EU,0x11U,0x11U,0x0EU},{0x1FU,0x01U,0x02U,0x04U,0x08U,0x08U,0x08U},
    {0x0EU,0x11U,0x11U,0x0EU,0x11U,0x11U,0x0EU},{0x0EU,0x11U,0x11U,0x0FU,0x01U,0x02U,0x0CU}
};

/**
 * @brief 5x7 小写字母点阵表，索引对应 ASCII 'a'~'z'。
 * @note 每个元素表示一行，低 5 位对应从左到右的像素。
 */
static const uint8_t st7735_lowercase[26][7] = {
    {0,0,0x0EU,0x01U,0x0FU,0x11U,0x0FU},{0x10U,0x10U,0x1EU,0x11U,0x11U,0x11U,0x1EU},
    {0,0,0x0EU,0x11U,0x10U,0x11U,0x0EU},{0x01U,0x01U,0x0FU,0x11U,0x11U,0x11U,0x0FU},
    {0,0,0x0EU,0x11U,0x1FU,0x10U,0x0EU},{0x06U,0x09U,0x08U,0x1EU,0x08U,0x08U,0x08U},
    {0,0,0x0FU,0x11U,0x0FU,0x01U,0x0EU},{0x10U,0x10U,0x1EU,0x11U,0x11U,0x11U,0x11U},
    {0x04U,0,0x0CU,0x04U,0x04U,0x04U,0x0EU},{0x02U,0,0x06U,0x02U,0x02U,0x12U,0x0CU},
    {0x10U,0x10U,0x12U,0x14U,0x18U,0x14U,0x12U},{0x0CU,0x04U,0x04U,0x04U,0x04U,0x04U,0x0EU},
    {0,0,0x1AU,0x15U,0x15U,0x15U,0x15U},{0,0,0x1EU,0x11U,0x11U,0x11U,0x11U},
    {0,0,0x0EU,0x11U,0x11U,0x11U,0x0EU},{0,0,0x1EU,0x11U,0x1EU,0x10U,0x10U},
    {0,0,0x0FU,0x11U,0x0FU,0x01U,0x01U},{0,0,0x16U,0x19U,0x10U,0x10U,0x10U},
    {0,0,0x0FU,0x10U,0x0EU,0x01U,0x1EU},{0x08U,0x08U,0x1EU,0x08U,0x08U,0x09U,0x06U},
    {0,0,0x11U,0x11U,0x11U,0x13U,0x0DU},{0,0,0x11U,0x11U,0x11U,0x0AU,0x04U},
    {0,0,0x11U,0x15U,0x15U,0x15U,0x0AU},{0,0,0x11U,0x0AU,0x04U,0x0AU,0x11U},
    {0,0,0x11U,0x11U,0x0FU,0x01U,0x0EU},{0,0,0x1FU,0x02U,0x04U,0x08U,0x1FU}
};

/**
 * @brief 获取指定字符的一个点阵行。
 * @param ch 要查询的字符。
 * @param row 点阵行号，范围为 0~ST7735_FONT_HEIGHT-1。
 * @retval 指定字符和行对应的 5 位点阵数据；不支持的字符返回空白。
 */
static uint8_t st7735_glyph_row(char ch, uint8_t row)
{
    uint8_t code = (uint8_t)ch;
    if ((code >= (uint8_t)'a') && (code <= (uint8_t)'z')) return st7735_lowercase[code - (uint8_t)'a'][row];
    if ((code >= (uint8_t)'A') && (code <= (uint8_t)'Z')) return st7735_uppercase[code - (uint8_t)'A'][row];
    if ((code >= (uint8_t)'0') && (code <= (uint8_t)'9')) return st7735_digits[code - (uint8_t)'0'][row];
    switch (code) {
    case ' ': return 0x00U; case '!': return (row == ST7735_CHAR_1_WIDTH) ? 0x04U : ((row < 5U) ? 0x04U : 0x00U);
    case '"': return (row < 2U) ? 0x0AU : 0x00U; case '#': return (row == 1U || row == 4U) ? 0x1FU : ((row == 0U || row == 2U || row == 3U || row == 5U) ? 0x0AU : 0x00U);
    case '$': { static const uint8_t g[7] = {0x0EU,0x15U,0x14U,0x0EU,0x05U,0x15U,0x0EU}; return g[row]; }
    case '%': { static const uint8_t g[7] = {0x19U,0x19U,0x02U,0x04U,0x08U,0x13U,0x13U}; return g[row]; }
    case '&': { static const uint8_t g[7] = {0x0CU,0x12U,0x14U,0x08U,0x15U,0x12U,0x0DU}; return g[row]; }
    case '\'': return (row < 2U) ? 0x04U : 0x00U; case '(': return (row == 0U || row == ST7735_CHAR_1_WIDTH) ? 0x02U : ((row == 1U || row == 5U) ? 0x04U : 0x08U);
    case ')': return (row == 0U || row == ST7735_CHAR_1_WIDTH) ? 0x08U : ((row == 1U || row == 5U) ? 0x04U : 0x02U); case '*': return (row == 2U) ? 0x1FU : ((row == 1U || row == 3U) ? 0x0AU : 0x00U);
    case '+': return (row == 3U) ? 0x1FU : ((row == 2U || row == 4U) ? 0x04U : 0x00U); case ',': return (row >= 5U) ? 0x04U : 0x00U;
    case '-': return (row == 3U) ? 0x1FU : 0x00U; case '.': return (row == ST7735_CHAR_1_WIDTH) ? 0x04U : 0x00U;
    case '/': { static const uint8_t g[7] = {0x01U,0x02U,0x02U,0x04U,0x08U,0x08U,0x10U}; return g[row]; }
    case ':': return (row == 2U || row == 5U) ? 0x04U : 0x00U; case ';': return (row == 2U || row >= 5U) ? 0x04U : 0x00U;
    case '<': return (row == 1U || row == 5U) ? 0x04U : ((row == 2U || row == 4U) ? 0x08U : ((row == 3U) ? 0x10U : 0x00U));
    case '=': return (row == 2U || row == 4U) ? 0x1FU : 0x00U;
    case '>': return (row == 1U || row == 5U) ? 0x04U : ((row == 2U || row == 4U) ? 0x02U : ((row == 3U) ? 0x01U : 0x00U));
    case '?': { static const uint8_t g[7] = {0x0EU,0x11U,0x01U,0x02U,0x04U,0x00U,0x04U}; return g[row]; }
    case '@': { static const uint8_t g[7] = {0x0EU,0x11U,0x17U,0x15U,0x17U,0x10U,0x0EU}; return g[row]; }
    case '[': return (row == 0U || row == ST7735_CHAR_1_WIDTH) ? 0x0EU : 0x08U;
    case '\\': { static const uint8_t g[7] = {0x10U,0x08U,0x08U,0x04U,0x02U,0x02U,0x01U}; return g[row]; }
    case ']': return (row == 0U || row == ST7735_CHAR_1_WIDTH) ? 0x0EU : 0x02U; case '^': return (row == 0U) ? 0x04U : ((row == 1U) ? 0x0AU : ((row == 2U) ? 0x11U : 0x00U));
    case '_': return (row == ST7735_CHAR_1_WIDTH) ? 0x1FU : 0x00U; case '`': return (row == 0U) ? 0x08U : ((row == 1U) ? 0x04U : 0x00U);
    case '{': return (row == 0U || row == ST7735_CHAR_1_WIDTH) ? 0x06U : ((row == 3U) ? 0x04U : ((row == 1U || row == 2U) ? 0x08U : 0x08U));
    case '|': return 0x04U; case '}': return (row == 0U || row == ST7735_CHAR_1_WIDTH) ? 0x0CU : ((row == 3U) ? 0x04U : 0x02U);
    case '~': return (row == 3U) ? 0x09U : ((row == 2U) ? 0x12U : 0x00U); default: return 0x00U;
    }
}

/**
 * @brief 绘制一个指定缩放倍数的字符。
 * @param x 字符左上角横坐标。
 * @param y 字符左上角纵坐标。
 * @param ch 要绘制的 ASCII 字符。
 * @param size 整数缩放倍数；1 倍字符占用 6x8 像素。
 * @param color 字符前景色，RGB565 格式。
 * @param bgcolor 字符背景色，RGB565 格式。
 * @retval None
 * @note 字符完整区域越界或 size 为 0 时直接返回。
 */
void ST7735_show_char(uint16_t x, uint16_t y, char ch, uint8_t size,
                     uint16_t color, uint16_t bgcolor)
{
    uint8_t line[ST7735_WIDTH * 2U];
    uint16_t char_width = ST7735_CHAR_1_WIDTH * (uint16_t)size;
    uint16_t char_height = ST7735_CHAR_1_HEIGHT * (uint16_t)size;
    uint16_t row, column, sx, sy, index;
    if ((size == 0U) || (char_width > ST7735_WIDTH) || (char_height > ST7735_HEIGHT) ||
        (x > ST7735_WIDTH - char_width) || (y > ST7735_HEIGHT - char_height)) return;
    st7735_wait_until_ready();
    ST7735_CS_LOW();
    st7735_window(x, y, (uint16_t)(x + char_width - 1U), (uint16_t)(y + char_height - 1U));
    for (row = 0U; row < ST7735_CHAR_1_HEIGHT; row++) {
        uint8_t glyph_row = (row < ST7735_FONT_HEIGHT) ? st7735_glyph_row(ch, (uint8_t)row) : 0U;
        index = 0U;
        for (column = 0U; column < ST7735_CHAR_1_WIDTH; column++) {
            uint16_t pixel = (column < ST7735_FONT_WIDTH && (glyph_row & (1U << (4U - column))) != 0U) ? color : bgcolor;
            for (sx = 0U; sx < size; sx++) { line[index++] = (uint8_t)(pixel >> 8); line[index++] = (uint8_t)pixel; }
        }
        for (sy = 0U; sy < size; sy++) st7735_data(line, index);
    }
    ST7735_CS_HIGH();
}

/**
 * @brief 绘制一行的字符串，利用DMA减少CPU负担。
 * @param x 字符串起始横坐标。
 * @param y 字符串起始纵坐标。
 * @param text 以 '\0' 结尾的字符串；'\n' 会使光标移至下一行行首。
 * @param size 字符整数缩放倍数。
 * @param length 字符串的长度。
 * @param color 字符前景色，RGB565 格式。
 * @param bgcolor 字符背景色，RGB565 格式。
 * @retval None
 * @note 文字到达右边界自动换行，到达下边界停止绘制；空指针或 size 为 0 时直接返回。
 */
void ST7735_show_line_string(uint16_t x, uint16_t y, const char *text, uint8_t size,
                             uint8_t length, uint16_t color, uint16_t bgcolor)
{
    uint16_t char_width;
    uint16_t char_height;
    uint16_t draw_chars;
    uint16_t char_index;
    uint16_t pixel_y;
    uint16_t pixel_x;
    uint32_t bytes_per_char;
    uint32_t max_chars_by_buffer;
    uint32_t buffer_index = 0U;

    /* 参数无效或起点位于屏幕外时，不启动绘制。 */
    if ((text == NULL) || (length == 0U) || (size == 0U) ||
        (x >= ST7735_WIDTH) || (y >= ST7735_HEIGHT))
    {
        return;
    }

    char_width = ST7735_CHAR_1_WIDTH * (uint16_t)size;
    char_height = ST7735_CHAR_1_HEIGHT * (uint16_t)size;
    if ((char_width > ST7735_WIDTH - x) || (char_height > ST7735_HEIGHT - y))
    {
        return;
    }

    /* 按屏幕宽度和静态 DMA 缓冲区容量限制本次可绘制字符数。 */
    bytes_per_char = (uint32_t)char_width * char_height * 2U;
    max_chars_by_buffer = ST7735_LINE_BUF_SIZE / bytes_per_char;
    draw_chars = length;
    if (draw_chars > (ST7735_WIDTH - x) / char_width)
    {
        draw_chars = (ST7735_WIDTH - x) / char_width;
    }
    if (draw_chars > max_chars_by_buffer)
    {
        draw_chars = (uint16_t)max_chars_by_buffer;
    }
    /* length 是最大字符数；遇到字符串结束符时提前停止。 */
    for (char_index = 0U; char_index < draw_chars; char_index++)
    {
        if (text[char_index] == '\0')
        {
            draw_chars = char_index;
            break;
        }
    }
    if (draw_chars == 0U)
    {
        return;
    }

    /* DMA 使用共享缓冲区，必须等上一次传输结束后才能改写它。 */
    st7735_wait_until_ready();
    /* 将字模按 size 放大，并按 RGB565 高字节在前的顺序展开到缓冲区。 */
    for (pixel_y = 0U; pixel_y < char_height; pixel_y++)
    {
        uint16_t glyph_y = pixel_y / size;
        for (char_index = 0U; char_index < draw_chars; char_index++)
        {
            uint8_t glyph_row = (glyph_y < ST7735_FONT_HEIGHT) ?
                                st7735_glyph_row(text[char_index], (uint8_t)glyph_y) : 0U;
            for (pixel_x = 0U; pixel_x < char_width; pixel_x++)
            {
                uint16_t glyph_x = pixel_x / size;
                uint16_t pixel = ((glyph_x < ST7735_FONT_WIDTH) &&
                                  ((glyph_row & (1U << (ST7735_FONT_WIDTH - 1U - glyph_x))) != 0U)) ?
                                 color : bgcolor;
                st7735_dma_line_buf[buffer_index++] = (uint8_t)(pixel >> 8);
                st7735_dma_line_buf[buffer_index++] = (uint8_t)pixel;
            }
        }
    }

    /* 设置与缓冲区像素数一致的窗口，再异步发送；完成回调负责释放 CS。 */
    ST7735_CS_LOW();
    st7735_window(x, y,
                  (uint16_t)(x + draw_chars * char_width - 1U),
                  (uint16_t)(y + char_height - 1U));
    ST7735_DC_DATA();
    st7735_dma_busy = 1U;
    if (HAL_SPI_Transmit_DMA(&ST7735_SPI_HANDLE, st7735_dma_line_buf,
                             (uint16_t)buffer_index) != HAL_OK)
    {
        st7735_finish_dma();
    }
}

/**
 * @brief 绘制字符串，并支持换行和自动折行，利用DMA减少CPU负担。
 * @param x 字符串起始横坐标。
 * @param y 字符串起始纵坐标。
 * @param text 以 '\0' 结尾的字符串；'\n' 会使光标移至下一行行首。
 * @param size 字符整数缩放倍数。
 * @param color 字符前景色，RGB565 格式。
 * @param bgcolor 字符背景色，RGB565 格式。
 * @retval None
 * @note 文字到达右边界自动换行，到达下边界停止绘制；空指针或 size 为 0 时直接返回。
 */
void ST7735_show_string_DMA(uint16_t x, uint16_t y, const char *text, uint8_t size,
                        uint16_t color, uint16_t bgcolor)
{
    if(text == NULL || size == 0U)
        return;

    uint16_t char_width = ST7735_CHAR_1_WIDTH * (uint16_t)size;
    uint16_t char_height = ST7735_CHAR_1_HEIGHT * (uint16_t)size;
    char line_text[ST7735_WIDTH / char_width + 1];
    uint16_t line_text_index = 0U;
    if ((text == NULL) || (size == 0U) || (char_width > ST7735_WIDTH) || (char_height > ST7735_HEIGHT)) return;
    while (*text != '\0') {
        if (*text == '\n')
        {
            x = 0U;
            line_text[line_text_index] = '\0';
            ST7735_show_line_string(x, y, line_text, size, line_text_index, color, bgcolor);
            y = y + char_height;
            line_text_index = 0U;
            text++;
            continue;
        }
        if (x > ST7735_WIDTH - char_width)
        {
            x = 0U;
            line_text[line_text_index] = '\0';
            ST7735_show_line_string(x, y, line_text, size, line_text_index, color, bgcolor);
            y = (uint16_t)(y + char_height);
            line_text_index = 0U;
        }
        if (y > ST7735_HEIGHT - char_height)
            break;
        line_text[line_text_index++] = *text;
        x += char_width;
        text++;
    }
    ST7735_show_line_string(x, y, line_text, size, line_text_index, color, bgcolor);
}

/**
 * @brief 绘制字符串，并支持换行和自动折行。
 * @param x 字符串起始横坐标。
 * @param y 字符串起始纵坐标。
 * @param text 以 '\0' 结尾的字符串；'\n' 会使光标移至下一行行首。
 * @param size 字符整数缩放倍数。
 * @param color 字符前景色，RGB565 格式。
 * @param bgcolor 字符背景色，RGB565 格式。
 * @retval None
 * @note 文字到达右边界自动换行，到达下边界停止绘制；空指针或 size 为 0 时直接返回。
 */
void ST7735_show_string(uint16_t x, uint16_t y, const char *text, uint8_t size,
                        uint16_t color, uint16_t bgcolor)
{
    uint16_t char_width = 6U * (uint16_t)size;
    uint16_t char_height = 8U * (uint16_t)size;
    if ((text == NULL) || (size == 0U) || (char_width > ST7735_WIDTH) || (char_height > ST7735_HEIGHT)) return;
    while (*text != '\0') {
        if (*text == '\n') { x = 0U; y = (uint16_t)(y + char_height); text++; continue; }
        if (x > ST7735_WIDTH - char_width) { x = 0U; y = (uint16_t)(y + char_height); }
        if (y > ST7735_HEIGHT - char_height) break;
        ST7735_show_char(x, y, *text++, size, color, bgcolor);
        x = (uint16_t)(x + char_width);
    }
}

/**
 * @brief 将无符号整数转换为十进制文本。
 * @param text 输出文本缓冲区。
 * @param value 要转换的无符号整数。
 * @param min_digits 最小输出位数，不足时在左侧补零。
 * @retval 转换后文本的字符数，不包含字符串结束符。
 */
static uint8_t st7735_write_unsigned(char *text, uint32_t value, uint8_t min_digits)
{
    char reversed[10];
    uint8_t digits = 0U;
    uint8_t index;

    if (min_digits > 10U) {
        min_digits = 10U;
    }
    do {
        reversed[digits++] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);
    while (digits < min_digits) {
        reversed[digits++] = '0';
    }
    for (index = 0U; index < digits; index++) {
        text[index] = reversed[digits - index - 1U];
    }
    return digits;
}

/**
 * @brief 在屏幕上显示一个整数。
 * @param x 整数起始横坐标。
 * @param y 整数起始纵坐标。
 * @param value 要显示的有符号整数。
 * @param min_digits 整数部分的最小显示位数，不足时在左侧补零。
 * @param size 字符整数缩放倍数。
 * @param color 字符前景色，RGB565 格式。
 * @param bgcolor 字符背景色，RGB565 格式。
 * @retval None
 * @note 负数会显示负号；最终显示范围和换行规则由 ST7735_show_string_DMA() 处理。
 */
void ST7735_show_integer(uint16_t x, uint16_t y, int32_t value, uint8_t min_digits,
                        uint8_t size, uint16_t color, uint16_t bgcolor)
{
    char text[12];
    uint8_t length = 0U;
    uint32_t magnitude;

    if (value < 0) {
        text[length++] = '-';
        magnitude = (uint32_t)(-(value + 1)) + 1U;
    } else {
        magnitude = (uint32_t)value;
    }
    length += st7735_write_unsigned(&text[length], magnitude, min_digits);
    text[length] = '\0';
    ST7735_show_string_DMA(x, y, text, size, color, bgcolor);
}

/**
 * @brief 根据 IEEE 754 位模式将浮点数转换为十进制文本。
 * @param bits 浮点数的 IEEE 754 单精度位模式。
 * @param text 输出文本缓冲区。
 * @param min_integer_digits 整数部分的最小输出位数，不足时在左侧补零。
 * @param decimal_digits 小数部分输出位数，最多支持 6 位。
 * @retval 转换后文本的字符数；不支持的特殊值返回 0。
 */
static uint8_t st7735_format_float_bits(uint32_t bits, char *text,
                                       uint8_t min_integer_digits,
                                       uint8_t decimal_digits)
{
    static const uint32_t pow10[] = {
        1U, 10U, 100U, 1000U, 10000U, 100000U, 1000000U
    };
    uint8_t length = 0U;
    uint32_t sign = bits >> 31U;
    uint32_t exponent = (bits >> 23U) & 0xFFU;
    uint32_t fraction = bits & 0x7FFFFFU;
    uint32_t mantissa;
    uint32_t integer_part;
    uint32_t fractional_part;
    uint32_t scale;
    int32_t shift;
    uint64_t scaled_value;

    if (decimal_digits > ST7735_CHAR_1_WIDTH) {
        decimal_digits = ST7735_CHAR_1_WIDTH;
    }
    if (min_integer_digits > 10U) {
        min_integer_digits = 10U;
    }
    if (exponent == 0xFFU) {
        return 0U;
    }
    scale = pow10[decimal_digits];

    if (exponent == 0U) {
        mantissa = fraction;
        shift = -149;
    } else {
        mantissa = fraction | 0x800000U;
        shift = (int32_t)exponent - 150;
    }

    if (shift >= 0) {
        if ((uint32_t)shift >= 32U) {
            return 0U;
        }
        if (mantissa > (0xFFFFFFFFU >> (uint32_t)shift)) {
            return 0U;
        }
        scaled_value = ((uint64_t)mantissa * scale) << (uint32_t)shift;
    } else {
        uint32_t shift_amount = (uint32_t)(-shift);
        scaled_value = (uint64_t)mantissa * scale;
        if (shift_amount >= 64U) {
            scaled_value = 0U;
        } else if (shift_amount != 0U) {
            scaled_value = (scaled_value + (1ULL << (shift_amount - 1U))) >>
                           shift_amount;
        }
    }

    integer_part = (uint32_t)(scaled_value / scale);
    fractional_part = (uint32_t)(scaled_value % scale);

    if (sign != 0U) {
        text[length++] = '-';
    }
    length += st7735_write_unsigned(&text[length], integer_part, min_integer_digits);
    if (decimal_digits != 0U) {
        text[length++] = '.';
        length += st7735_write_unsigned(&text[length], fractional_part,
                                        decimal_digits);
    }
    text[length] = '\0';
    return length;
}

/**
 * @brief 在屏幕上显示一个小数。
 * @param x 小数起始横坐标。
 * @param y 小数起始纵坐标。
 * @param value 要显示的浮点数。
 * @param min_integer_digits 整数部分的最小显示位数，不足时在左侧补零。
 * @param decimal_digits 小数部分显示位数，最多支持 6 位。
 * @param size 字符整数缩放倍数。
 * @param color 字符前景色，RGB565 格式。
 * @param bgcolor 字符背景色，RGB565 格式。
 * @retval None
 * @note 浮点数先转换为文本，再通过 ST7735_show_string_DMA() 绘制。
 */
void ST7735_show_decimal(uint16_t x, uint16_t y, float value,
                        uint8_t min_integer_digits, uint8_t decimal_digits,
                        uint8_t size, uint16_t color, uint16_t bgcolor)
{
    union {
        float float_value;
        uint32_t bits;
    } converter;
    char text[20];
    uint8_t length;

    converter.float_value = value;
    length = st7735_format_float_bits(converter.bits, text,
                                      min_integer_digits, decimal_digits);
    if (length != 0U) {
        ST7735_show_string_DMA(x, y, text, size, color, bgcolor);
    }
}

/**
 * @brief 开启或关闭显示颜色反相。
 * @param invert 非零值开启颜色反相，0 关闭颜色反相。
 * @retval None
 */
void ST7735_invert_colors(uint8_t invert)
{
    st7735_wait_until_ready();
    ST7735_CS_LOW();
    st7735_command(invert ? ST7735_INVON : ST7735_INVOFF);
    ST7735_CS_HIGH();
}

/**
 * @brief 设置 ST7735 的 Gamma 曲线。
 * @param gamma 控制器支持的 Gamma 编号，通常为 0~4。
 * @retval None
 */
void ST7735_set_gamma(uint8_t gamma)
{
    st7735_wait_until_ready();
    ST7735_CS_LOW();
    st7735_command_data(ST7735_GAMSET, &gamma, 1U);
    ST7735_CS_HIGH();
}

/**
 * @brief SPI DMA 发送完成回调。
 * @param hspi 产生回调的 SPI 外设句柄。
 * @retval None
 * @note 当前 SPI 属于 ST7735 且行文本 DMA 正在运行时，释放片选并清除忙状态。
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if ((hspi->Instance == ST7735_SPI_INSTANCE) && (st7735_dma_busy != 0U)) {
        st7735_finish_dma();
    }
}

/**
 * @brief SPI 错误回调。
 * @param hspi 产生错误的 SPI 外设句柄。
 * @retval None
 * @note 当前 SPI 属于 ST7735 时结束 DMA 状态并释放片选。
 */
void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
    if ((hspi->Instance == ST7735_SPI_INSTANCE) && (st7735_dma_busy != 0U)) {
        st7735_finish_dma();
    }
}


#ifdef ST7735_SOFT_PWM_ENABLE
/**
 * @brief 启动 ST7735 背光的软件 PWM。
 * @retval None
 * @note 使用定时器更新中断和比较中断分别控制背光开启与关闭。
 */
void ST7735_BL_Start(void)
{
    HAL_NVIC_SetPriority(TIM1_UP_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(TIM1_UP_IRQn);
    HAL_NVIC_SetPriority(TIM1_CC_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(TIM1_CC_IRQn);
    __HAL_TIM_ENABLE_IT(&ST7735_BACKLIGHT_TIMER_HANDLE, TIM_IT_UPDATE);   // 溢出中断：开灯
    HAL_TIM_OC_Start_IT(&ST7735_BACKLIGHT_TIMER_HANDLE, ST7735_BACKLIGHT_TIMER_CHANNEL);   // 比较中断：关灯
}

/**
 * @brief 定时器更新事件回调，开启背光输出。
 * @param htim 产生更新事件的定时器句柄。
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == ST7735_BACKLIGHT_TIMER_HANDLE.Instance)
        HAL_GPIO_WritePin(ST7735_BL_GPIO_PORT, ST7735_BL_PIN, GPIO_PIN_SET);
}

/**
 * @brief 定时器输出比较事件回调，关闭背光输出。
 * @param htim 产生比较事件的定时器句柄。
 * @retval None
 */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef *htim)
{
    if ((htim->Instance == ST7735_BACKLIGHT_TIMER_HANDLE.Instance) &&
        (htim->Channel == ST7735_BACKLIGHT_TIMER_CHANNEL))
        HAL_GPIO_WritePin(ST7735_BL_GPIO_PORT, ST7735_BL_PIN, GPIO_PIN_RESET);
}

#endif
