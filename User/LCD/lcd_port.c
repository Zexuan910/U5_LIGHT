#include "lcd_port.h"

#include "main.h"

#include <stddef.h>

#define LCD_SPI_TIMEOUT_MS 1000U
#define LCD_TX_ROWS        8U
#define LCD_TX_PIXELS      (LCD_PORT_WIDTH * LCD_TX_ROWS)
#define LCD_TX_BYTES       (LCD_TX_PIXELS * 2U)
#define LCD_USE_SOFT_SPI   0U
#define LCD_USE_P169H002_INIT 1U
#define LCD_GRAM_WIDTH     240U
#define LCD_GRAM_HEIGHT    320U

extern SPI_HandleTypeDef hspi1;

static uint8_t lcd_tx_buffer[LCD_TX_BYTES];

#if LCD_USE_SOFT_SPI
static void lcd_bus_delay(void)
{
  for (volatile uint32_t i = 0U; i < 12U; i++)
  {
    __NOP();
  }
}
#endif

static void lcd_bus_init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

#if LCD_USE_SOFT_SPI
  GPIO_InitStruct.Pin = LCD_CLK_Pin;
  HAL_GPIO_Init(LCD_CLK_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_MOSI_Pin;
  HAL_GPIO_Init(LCD_MOSI_GPIO_Port, &GPIO_InitStruct);
#endif

  GPIO_InitStruct.Pin = LCD_RST_Pin | LCD_CS_Pin | LCD_DC_Pin;
  HAL_GPIO_Init(LCD_RST_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_BLK_Pin;
  HAL_GPIO_Init(LCD_BLK_GPIO_Port, &GPIO_InitStruct);

#if LCD_USE_SOFT_SPI
  HAL_GPIO_WritePin(LCD_CLK_GPIO_Port, LCD_CLK_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_MOSI_GPIO_Port, LCD_MOSI_Pin, GPIO_PIN_RESET);
#endif
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
}

#if LCD_USE_SOFT_SPI
static void lcd_write_bytes_soft(const uint8_t* data, uint16_t size)
{
  uint16_t index;

  for (index = 0U; index < size; index++)
  {
    uint8_t value = data[index];
    uint8_t bit;

    for (bit = 0U; bit < 8U; bit++)
    {
      HAL_GPIO_WritePin(LCD_MOSI_GPIO_Port,
                        LCD_MOSI_Pin,
                        ((value & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
      lcd_bus_delay();
      HAL_GPIO_WritePin(LCD_CLK_GPIO_Port, LCD_CLK_Pin, GPIO_PIN_SET);
      lcd_bus_delay();
      HAL_GPIO_WritePin(LCD_CLK_GPIO_Port, LCD_CLK_Pin, GPIO_PIN_RESET);
      lcd_bus_delay();
      value <<= 1;
    }
  }
}
#endif

static void lcd_select(void)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
}

static void lcd_deselect(void)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void lcd_cmd_mode(void)
{
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);
}

static void lcd_data_mode(void)
{
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
}

static void lcd_write_bytes(const uint8_t* data, uint16_t size)
{
  if ((data == NULL) || (size == 0U))
  {
    return;
  }

#if LCD_USE_SOFT_SPI
  lcd_write_bytes_soft(data, size);
#else
  if (HAL_SPI_Transmit(&hspi1, (uint8_t*)data, size, LCD_SPI_TIMEOUT_MS) != HAL_OK)
  {
    Error_Handler();
  }
#endif
}

static void lcd_write_command(uint8_t command)
{
  lcd_cmd_mode();
  lcd_write_bytes(&command, 1U);
}

static void lcd_write_data8(uint8_t data)
{
  lcd_data_mode();
  lcd_write_bytes(&data, 1U);
}

static void lcd_write_data16(uint16_t data)
{
  uint8_t bytes[2];

  bytes[0] = (uint8_t)(data >> 8);
  bytes[1] = (uint8_t)(data & 0xFFU);
  lcd_data_mode();
  lcd_write_bytes(bytes, sizeof(bytes));
}

static void lcd_reset(void)
{
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(100U);
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(100U);
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(100U);
}

static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  x0 = (uint16_t)(x0 + LCD_PORT_X_OFFSET);
  x1 = (uint16_t)(x1 + LCD_PORT_X_OFFSET);
  y0 = (uint16_t)(y0 + LCD_PORT_Y_OFFSET);
  y1 = (uint16_t)(y1 + LCD_PORT_Y_OFFSET);

  lcd_write_command(0x2AU);
  lcd_write_data16(x0);
  lcd_write_data16(x1);

  lcd_write_command(0x2BU);
  lcd_write_data16(y0);
  lcd_write_data16(y1);

  lcd_write_command(0x2CU);
}

#if 0
static void lcd_set_physical_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
  lcd_write_command(0x2AU);
  lcd_write_data16(x0);
  lcd_write_data16(x1);

  lcd_write_command(0x2BU);
  lcd_write_data16(y0);
  lcd_write_data16(y1);

  lcd_write_command(0x2CU);
}
#endif

static void lcd_write_init_sequence(void)
{
#if LCD_USE_P169H002_INIT
  lcd_write_command(0x11U);
  HAL_Delay(120U);

  lcd_write_command(0x36U);
  lcd_write_data8(0x00U);

  lcd_write_command(0x3AU);
  lcd_write_data8(0x55U);

  lcd_write_command(0xB2U);
  lcd_write_data8(0x0CU);
  lcd_write_data8(0x0CU);
  lcd_write_data8(0x00U);
  lcd_write_data8(0x33U);
  lcd_write_data8(0x33U);

  lcd_write_command(0xB7U);
  lcd_write_data8(0x35U);

  lcd_write_command(0xBBU);
  lcd_write_data8(0x32U);

  lcd_write_command(0xC2U);
  lcd_write_data8(0x01U);

  lcd_write_command(0xC3U);
  lcd_write_data8(0x15U);

  lcd_write_command(0xC4U);
  lcd_write_data8(0x20U);

  lcd_write_command(0xC6U);
  lcd_write_data8(0x0FU);

  lcd_write_command(0xD0U);
  lcd_write_data8(0xA4U);
  lcd_write_data8(0xA1U);

  lcd_write_command(0xE0U);
  lcd_write_data8(0xD0U);
  lcd_write_data8(0x08U);
  lcd_write_data8(0x0EU);
  lcd_write_data8(0x09U);
  lcd_write_data8(0x09U);
  lcd_write_data8(0x05U);
  lcd_write_data8(0x31U);
  lcd_write_data8(0x33U);
  lcd_write_data8(0x48U);
  lcd_write_data8(0x17U);
  lcd_write_data8(0x14U);
  lcd_write_data8(0x15U);
  lcd_write_data8(0x31U);
  lcd_write_data8(0x34U);

  lcd_write_command(0xE1U);
  lcd_write_data8(0xD0U);
  lcd_write_data8(0x08U);
  lcd_write_data8(0x0EU);
  lcd_write_data8(0x09U);
  lcd_write_data8(0x09U);
  lcd_write_data8(0x15U);
  lcd_write_data8(0x31U);
  lcd_write_data8(0x33U);
  lcd_write_data8(0x48U);
  lcd_write_data8(0x17U);
  lcd_write_data8(0x14U);
  lcd_write_data8(0x15U);
  lcd_write_data8(0x31U);
  lcd_write_data8(0x34U);

  lcd_write_command(0x21U);
  lcd_write_command(0x13U);
  lcd_write_command(0x29U);
  HAL_Delay(20U);
#else
  lcd_write_command(0x36U);
  lcd_write_data8(0x00U);

  lcd_write_command(0x3AU);
  lcd_write_data8(0x05U);

  lcd_write_command(0xB2U);
  lcd_write_data8(0x0BU);
  lcd_write_data8(0x0BU);
  lcd_write_data8(0x00U);
  lcd_write_data8(0x33U);
  lcd_write_data8(0x35U);

  lcd_write_command(0xB7U);
  lcd_write_data8(0x11U);

  lcd_write_command(0xBBU);
  lcd_write_data8(0x35U);

  lcd_write_command(0xC0U);
  lcd_write_data8(0x2CU);

  lcd_write_command(0xC2U);
  lcd_write_data8(0x01U);

  lcd_write_command(0xC3U);
  lcd_write_data8(0x0DU);

  lcd_write_command(0xC4U);
  lcd_write_data8(0x20U);

  lcd_write_command(0xC6U);
  lcd_write_data8(0x13U);

  lcd_write_command(0xD0U);
  lcd_write_data8(0xA4U);
  lcd_write_data8(0xA1U);

  lcd_write_command(0xD6U);
  lcd_write_data8(0xA1U);

  lcd_write_command(0xE0U);
  lcd_write_data8(0xF0U);
  lcd_write_data8(0x06U);
  lcd_write_data8(0x0BU);
  lcd_write_data8(0x0AU);
  lcd_write_data8(0x09U);
  lcd_write_data8(0x26U);
  lcd_write_data8(0x29U);
  lcd_write_data8(0x33U);
  lcd_write_data8(0x41U);
  lcd_write_data8(0x18U);
  lcd_write_data8(0x16U);
  lcd_write_data8(0x15U);
  lcd_write_data8(0x29U);
  lcd_write_data8(0x2DU);

  lcd_write_command(0xE1U);
  lcd_write_data8(0xF0U);
  lcd_write_data8(0x04U);
  lcd_write_data8(0x08U);
  lcd_write_data8(0x08U);
  lcd_write_data8(0x07U);
  lcd_write_data8(0x03U);
  lcd_write_data8(0x28U);
  lcd_write_data8(0x32U);
  lcd_write_data8(0x40U);
  lcd_write_data8(0x3BU);
  lcd_write_data8(0x19U);
  lcd_write_data8(0x18U);
  lcd_write_data8(0x2AU);
  lcd_write_data8(0x2EU);

  lcd_write_command(0xE4U);
  lcd_write_data8(0x25U);
  lcd_write_data8(0x00U);
  lcd_write_data8(0x00U);

  lcd_write_command(0x21U);

  lcd_write_command(0x11U);
  HAL_Delay(120U);

  lcd_write_command(0x29U);
  HAL_Delay(20U);
#endif
}

static const uint8_t* lcd_glyph_for(char ch)
{
  static const uint8_t glyph_space[7] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
  static const uint8_t glyph_c[7] = {0x00U, 0x00U, 0x0EU, 0x10U, 0x10U, 0x0EU, 0x00U};
  static const uint8_t glyph_e[7] = {0x00U, 0x0EU, 0x11U, 0x1FU, 0x10U, 0x0EU, 0x00U};
  static const uint8_t glyph_h[7] = {0x10U, 0x10U, 0x16U, 0x19U, 0x11U, 0x11U, 0x00U};
  static const uint8_t glyph_l[7] = {0x0CU, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU, 0x00U};
  static const uint8_t glyph_n[7] = {0x00U, 0x16U, 0x19U, 0x11U, 0x11U, 0x11U, 0x00U};
  static const uint8_t glyph_o[7] = {0x00U, 0x0EU, 0x11U, 0x11U, 0x11U, 0x0EU, 0x00U};
  static const uint8_t glyph_p[7] = {0x00U, 0x1EU, 0x11U, 0x1EU, 0x10U, 0x10U, 0x00U};
  static const uint8_t glyph_r[7] = {0x00U, 0x16U, 0x19U, 0x10U, 0x10U, 0x10U, 0x00U};
  static const uint8_t glyph_u[7] = {0x00U, 0x11U, 0x11U, 0x11U, 0x13U, 0x0DU, 0x00U};
  static const uint8_t glyph_A[7] = {0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x00U};
  static const uint8_t glyph_B[7] = {0x1EU, 0x11U, 0x11U, 0x1EU, 0x11U, 0x1EU, 0x00U};
  static const uint8_t glyph_E[7] = {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x1FU, 0x00U};
  static const uint8_t glyph_F[7] = {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x00U};
  static const uint8_t glyph_G[7] = {0x0EU, 0x11U, 0x10U, 0x17U, 0x11U, 0x0EU, 0x00U};
  static const uint8_t glyph_H[7] = {0x11U, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x00U};
  static const uint8_t glyph_K[7] = {0x11U, 0x12U, 0x14U, 0x18U, 0x14U, 0x12U, 0x00U};
  static const uint8_t glyph_L[7] = {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x1FU, 0x00U};
  static const uint8_t glyph_M[7] = {0x11U, 0x1BU, 0x15U, 0x15U, 0x11U, 0x11U, 0x00U};
  static const uint8_t glyph_N[7] = {0x11U, 0x19U, 0x15U, 0x13U, 0x11U, 0x11U, 0x00U};
  static const uint8_t glyph_O[7] = {0x0EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU, 0x00U};
  static const uint8_t glyph_P[7] = {0x1EU, 0x11U, 0x11U, 0x1EU, 0x10U, 0x10U, 0x00U};
  static const uint8_t glyph_R[7] = {0x1EU, 0x11U, 0x11U, 0x1EU, 0x14U, 0x12U, 0x00U};
  static const uint8_t glyph_T[7] = {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x00U};
  static const uint8_t glyph_U[7] = {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU, 0x00U};
  static const uint8_t glyph_W[7] = {0x11U, 0x11U, 0x11U, 0x15U, 0x15U, 0x0AU, 0x00U};
  static const uint8_t glyph_X[7] = {0x11U, 0x0AU, 0x04U, 0x04U, 0x0AU, 0x11U, 0x00U};

  switch (ch)
  {
  case 'c':
    return glyph_c;
  case 'e':
    return glyph_e;
  case 'h':
    return glyph_h;
  case 'l':
    return glyph_l;
  case 'n':
    return glyph_n;
  case 'o':
    return glyph_o;
  case 'p':
    return glyph_p;
  case 'r':
    return glyph_r;
  case 'u':
    return glyph_u;
  case 'A':
    return glyph_A;
  case 'B':
    return glyph_B;
  case 'E':
    return glyph_E;
  case 'F':
    return glyph_F;
  case 'G':
    return glyph_G;
  case 'H':
    return glyph_H;
  case 'K':
    return glyph_K;
  case 'L':
    return glyph_L;
  case 'M':
    return glyph_M;
  case 'N':
    return glyph_N;
  case 'O':
    return glyph_O;
  case 'P':
    return glyph_P;
  case 'R':
    return glyph_R;
  case 'T':
    return glyph_T;
  case 'U':
    return glyph_U;
  case 'W':
    return glyph_W;
  case 'X':
    return glyph_X;
  case ' ':
  default:
    return glyph_space;
  }
}

static void lcd_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg_color, uint8_t scale)
{
  const uint8_t* glyph = lcd_glyph_for(ch);
  uint8_t row;
  uint8_t col;

  if (scale == 0U)
  {
    scale = 1U;
  }

  for (row = 0U; row < 7U; row++)
  {
    for (col = 0U; col < 5U; col++)
    {
      uint16_t pixel_color = (glyph[row] & (uint8_t)(1U << (4U - col))) ? color : bg_color;
      LCD_FillRectRGB565((uint16_t)(x + (uint16_t)col * scale),
                         (uint16_t)(y + (uint16_t)row * scale),
                         scale,
                         scale,
                         pixel_color);
    }
  }
}

#if 0
static void lcd_fill_physical_rgb565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
  uint32_t remaining;
  uint16_t fillPixels;
  uint16_t i;

  if ((x >= LCD_GRAM_WIDTH) || (y >= LCD_GRAM_HEIGHT) || (width == 0U) || (height == 0U))
  {
    return;
  }

  if ((uint32_t)x + width > LCD_GRAM_WIDTH)
  {
    width = (uint16_t)(LCD_GRAM_WIDTH - x);
  }
  if ((uint32_t)y + height > LCD_GRAM_HEIGHT)
  {
    height = (uint16_t)(LCD_GRAM_HEIGHT - y);
  }

  remaining = (uint32_t)width * height;
  fillPixels = (remaining > LCD_TX_PIXELS) ? LCD_TX_PIXELS : (uint16_t)remaining;
  for (i = 0U; i < fillPixels; i++)
  {
    lcd_tx_buffer[(uint16_t)i * 2U] = (uint8_t)(color >> 8);
    lcd_tx_buffer[(uint16_t)i * 2U + 1U] = (uint8_t)(color & 0xFFU);
  }

  lcd_select();
  lcd_set_physical_window(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U));
  lcd_data_mode();

  while (remaining > 0U)
  {
    uint16_t chunk = (remaining > LCD_TX_PIXELS) ? LCD_TX_PIXELS : (uint16_t)remaining;
    lcd_write_bytes(lcd_tx_buffer, (uint16_t)(chunk * 2U));
    remaining -= chunk;
  }

  lcd_deselect();
}
#endif

void LCD_Port_Init(void)
{
  lcd_bus_init();
  LCD_Port_SetBacklight(1U);

  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);

  lcd_reset();
  lcd_select();
  lcd_write_init_sequence();
  lcd_deselect();
}

void LCD_Port_SetBacklight(uint8_t on)
{
  HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void LCD_ClearRGB565(uint16_t color)
{
  LCD_FillRectRGB565(0U, 0U, LCD_PORT_WIDTH, LCD_PORT_HEIGHT, color);
}

void LCD_FillRectRGB565(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
  uint32_t remaining;
  uint16_t fillPixels;
  uint16_t i;

  if ((x >= LCD_PORT_WIDTH) || (y >= LCD_PORT_HEIGHT) || (width == 0U) || (height == 0U))
  {
    return;
  }

  if ((uint32_t)x + width > LCD_PORT_WIDTH)
  {
    width = (uint16_t)(LCD_PORT_WIDTH - x);
  }
  if ((uint32_t)y + height > LCD_PORT_HEIGHT)
  {
    height = (uint16_t)(LCD_PORT_HEIGHT - y);
  }

  remaining = (uint32_t)width * height;
  fillPixels = (remaining > LCD_TX_PIXELS) ? LCD_TX_PIXELS : (uint16_t)remaining;
  for (i = 0U; i < fillPixels; i++)
  {
    lcd_tx_buffer[(uint16_t)i * 2U] = (uint8_t)(color >> 8);
    lcd_tx_buffer[(uint16_t)i * 2U + 1U] = (uint8_t)(color & 0xFFU);
  }

  lcd_select();
  lcd_set_window(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U));
  lcd_data_mode();

  while (remaining > 0U)
  {
    uint16_t chunk = (remaining > LCD_TX_PIXELS) ? LCD_TX_PIXELS : (uint16_t)remaining;
    lcd_write_bytes(lcd_tx_buffer, (uint16_t)(chunk * 2U));
    remaining -= chunk;
  }

  lcd_deselect();
}

void LCD_DrawPixelRGB565(uint16_t x, uint16_t y, uint16_t color)
{
  LCD_FillRectRGB565(x, y, 1U, 1U, color);
}

void LCD_DrawTextRGB565(uint16_t x, uint16_t y, const char* text, uint16_t color, uint16_t bg_color, uint8_t scale)
{
  size_t i = 0U;

  if (text == NULL)
  {
    return;
  }

  if (scale == 0U)
  {
    scale = 1U;
  }

  while (text[i] != '\0')
  {
    lcd_draw_char((uint16_t)(x + (uint16_t)i * 6U * scale), y, text[i], color, bg_color, scale);
    i++;
  }
}

void LCD_DrawCenteredText(const char* text, uint16_t color, uint16_t bg_color, uint8_t scale)
{
  size_t len = 0U;
  uint16_t text_width;
  uint16_t text_height;
  uint16_t x;
  uint16_t y;
  size_t i;

  if (text == NULL)
  {
    return;
  }

  if (scale == 0U)
  {
    scale = 1U;
  }

  while (text[len] != '\0')
  {
    len++;
  }

  if (len == 0U)
  {
    return;
  }

  text_width = (uint16_t)(((uint32_t)len * 6U - 1U) * scale);
  text_height = (uint16_t)(7U * scale);
  x = (text_width >= LCD_PORT_WIDTH) ? 0U : (uint16_t)((LCD_PORT_WIDTH - text_width) / 2U);
  y = (uint16_t)((LCD_PORT_HEIGHT - text_height) / 2U);

  for (i = 0U; i < len; i++)
  {
    lcd_draw_char((uint16_t)(x + (uint16_t)i * 6U * scale), y, text[i], color, bg_color, scale);
  }
}
