/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "DEV_Config.h"
#include "LCD_1in69.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LCD_USE_HAL_SPI 1

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

SPI_HandleTypeDef hspi1;

/* USER CODE BEGIN PV */
static UBYTE ui_page = 0U;
static UBYTE touch_pressed_prev = 0U;
static uint32_t ui_last_switch_ms = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
#if LCD_USE_HAL_SPI
static void MX_SPI1_Init(void);
#endif
/* USER CODE BEGIN PFP */
static void LCD_ShowHelloBuaa(void);
static void UI_ShowCarouselScreen(UBYTE page);
static void LCD_FillRectByRows(UWORD x0, UWORD y0, UWORD x1, UWORD y1, UWORD color);
static void TouchPins_Init(void);
static UBYTE Touch_IsPressed(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static UWORD LCD_RGB565(UBYTE r, UBYTE g, UBYTE b)
{
  return (UWORD)((((UWORD)r & 0xF8U) << 8) | (((UWORD)g & 0xFCU) << 3) | ((UWORD)b >> 3));
}

static UWORD LCD_TextWidth(const char* text, UWORD scale)
{
  UWORD len = 0U;

  while (text[len] != '\0')
  {
    len++;
  }

  return (len == 0U) ? 0U : (UWORD)(((len * 6U) - 1U) * scale);
}

static const UBYTE* LCD_GlyphFor(char ch)
{
  static const UBYTE glyph_space[7] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};
  static const UBYTE glyph_colon[7] = {0x00U, 0x04U, 0x04U, 0x00U, 0x04U, 0x04U, 0x00U};
  static const UBYTE glyph_dash[7] = {0x00U, 0x00U, 0x00U, 0x1FU, 0x00U, 0x00U, 0x00U};
  static const UBYTE glyph_dot[7] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x0CU, 0x0CU};
  static const UBYTE glyph_percent[7] = {0x19U, 0x1AU, 0x04U, 0x08U, 0x13U, 0x13U, 0x00U};
  static const UBYTE glyph_0[7] = {0x0EU, 0x11U, 0x13U, 0x15U, 0x19U, 0x11U, 0x0EU};
  static const UBYTE glyph_1[7] = {0x04U, 0x0CU, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU};
  static const UBYTE glyph_2[7] = {0x0EU, 0x11U, 0x01U, 0x02U, 0x04U, 0x08U, 0x1FU};
  static const UBYTE glyph_3[7] = {0x1EU, 0x01U, 0x01U, 0x0EU, 0x01U, 0x01U, 0x1EU};
  static const UBYTE glyph_4[7] = {0x02U, 0x06U, 0x0AU, 0x12U, 0x1FU, 0x02U, 0x02U};
  static const UBYTE glyph_5[7] = {0x1FU, 0x10U, 0x10U, 0x1EU, 0x01U, 0x01U, 0x1EU};
  static const UBYTE glyph_6[7] = {0x0EU, 0x10U, 0x10U, 0x1EU, 0x11U, 0x11U, 0x0EU};
  static const UBYTE glyph_7[7] = {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x08U, 0x08U};
  static const UBYTE glyph_8[7] = {0x0EU, 0x11U, 0x11U, 0x0EU, 0x11U, 0x11U, 0x0EU};
  static const UBYTE glyph_9[7] = {0x0EU, 0x11U, 0x11U, 0x0FU, 0x01U, 0x01U, 0x0EU};
  static const UBYTE glyph_A[7] = {0x0EU, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U};
  static const UBYTE glyph_B[7] = {0x1EU, 0x11U, 0x11U, 0x1EU, 0x11U, 0x11U, 0x1EU};
  static const UBYTE glyph_C[7] = {0x0EU, 0x11U, 0x10U, 0x10U, 0x10U, 0x11U, 0x0EU};
  static const UBYTE glyph_D[7] = {0x1EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x1EU};
  static const UBYTE glyph_E[7] = {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x1FU};
  static const UBYTE glyph_F[7] = {0x1FU, 0x10U, 0x10U, 0x1EU, 0x10U, 0x10U, 0x10U};
  static const UBYTE glyph_G[7] = {0x0EU, 0x11U, 0x10U, 0x17U, 0x11U, 0x11U, 0x0FU};
  static const UBYTE glyph_H[7] = {0x11U, 0x11U, 0x11U, 0x1FU, 0x11U, 0x11U, 0x11U};
  static const UBYTE glyph_I[7] = {0x0EU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x0EU};
  static const UBYTE glyph_J[7] = {0x07U, 0x02U, 0x02U, 0x02U, 0x12U, 0x12U, 0x0CU};
  static const UBYTE glyph_K[7] = {0x11U, 0x12U, 0x14U, 0x18U, 0x14U, 0x12U, 0x11U};
  static const UBYTE glyph_L[7] = {0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x10U, 0x1FU};
  static const UBYTE glyph_M[7] = {0x11U, 0x1BU, 0x15U, 0x15U, 0x11U, 0x11U, 0x11U};
  static const UBYTE glyph_N[7] = {0x11U, 0x19U, 0x15U, 0x13U, 0x11U, 0x11U, 0x11U};
  static const UBYTE glyph_O[7] = {0x0EU, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU};
  static const UBYTE glyph_P[7] = {0x1EU, 0x11U, 0x11U, 0x1EU, 0x10U, 0x10U, 0x10U};
  static const UBYTE glyph_Q[7] = {0x0EU, 0x11U, 0x11U, 0x11U, 0x15U, 0x12U, 0x0DU};
  static const UBYTE glyph_R[7] = {0x1EU, 0x11U, 0x11U, 0x1EU, 0x14U, 0x12U, 0x11U};
  static const UBYTE glyph_S[7] = {0x0FU, 0x10U, 0x10U, 0x0EU, 0x01U, 0x01U, 0x1EU};
  static const UBYTE glyph_T[7] = {0x1FU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U};
  static const UBYTE glyph_U[7] = {0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x11U, 0x0EU};
  static const UBYTE glyph_V[7] = {0x11U, 0x11U, 0x11U, 0x11U, 0x0AU, 0x0AU, 0x04U};
  static const UBYTE glyph_W[7] = {0x11U, 0x11U, 0x11U, 0x15U, 0x15U, 0x15U, 0x0AU};
  static const UBYTE glyph_X[7] = {0x11U, 0x0AU, 0x04U, 0x04U, 0x04U, 0x0AU, 0x11U};
  static const UBYTE glyph_Y[7] = {0x11U, 0x0AU, 0x04U, 0x04U, 0x04U, 0x04U, 0x04U};
  static const UBYTE glyph_Z[7] = {0x1FU, 0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x1FU};

  if ((ch >= 'a') && (ch <= 'z'))
  {
    ch = (char)(ch - ('a' - 'A'));
  }

  switch (ch)
  {
  case ':':
    return glyph_colon;
  case '-':
    return glyph_dash;
  case '.':
    return glyph_dot;
  case '%':
    return glyph_percent;
  case '0':
    return glyph_0;
  case '1':
    return glyph_1;
  case '2':
    return glyph_2;
  case '3':
    return glyph_3;
  case '4':
    return glyph_4;
  case '5':
    return glyph_5;
  case '6':
    return glyph_6;
  case '7':
    return glyph_7;
  case '8':
    return glyph_8;
  case '9':
    return glyph_9;
  case 'A':
    return glyph_A;
  case 'B':
    return glyph_B;
  case 'C':
    return glyph_C;
  case 'D':
    return glyph_D;
  case 'E':
    return glyph_E;
  case 'F':
    return glyph_F;
  case 'G':
    return glyph_G;
  case 'H':
    return glyph_H;
  case 'I':
    return glyph_I;
  case 'J':
    return glyph_J;
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
  case 'Q':
    return glyph_Q;
  case 'R':
    return glyph_R;
  case 'S':
    return glyph_S;
  case 'T':
    return glyph_T;
  case 'U':
    return glyph_U;
  case 'V':
    return glyph_V;
  case 'W':
    return glyph_W;
  case 'X':
    return glyph_X;
  case 'Y':
    return glyph_Y;
  case 'Z':
    return glyph_Z;
  case ' ':
  default:
    return glyph_space;
  }
}

static void LCD_DrawChar(UWORD x, UWORD y, char ch, UWORD color, UWORD bg_color, UWORD scale)
{
  const UBYTE* glyph = LCD_GlyphFor(ch);
  UWORD row;
  UWORD col;

  for (row = 0U; row < 7U; row++)
  {
    for (col = 0U; col < 5U; col++)
    {
      UWORD pixel_color = ((glyph[row] & (1U << (4U - col))) != 0U) ? color : bg_color;
      UWORD x0 = (UWORD)(x + (col * scale));
      UWORD y0 = (UWORD)(y + (row * scale));

      LCD_1IN69_FillRect_FastStatic(x0, y0, (UWORD)(x0 + scale - 1U), (UWORD)(y0 + scale - 1U), pixel_color);
    }
  }
}

static void LCD_DrawText(UWORD x, UWORD y, const char* text, UWORD color, UWORD bg_color, UWORD scale)
{
  UWORD i = 0U;

  while (text[i] != '\0')
  {
    LCD_DrawChar((UWORD)(x + (i * 6U * scale)), y, text[i], color, bg_color, scale);
    i++;
  }
}

static void LCD_DrawCenteredText(const char* text, UWORD y, UWORD color, UWORD bg_color, UWORD scale)
{
  UWORD text_width;
  UWORD x;

  text_width = LCD_TextWidth(text, scale);
  x = (text_width >= LCD_1IN69.WIDTH) ? 0U : (UWORD)((LCD_1IN69.WIDTH - text_width) / 2U);
  LCD_DrawText(x, y, text, color, bg_color, scale);
}

static void LCD_DrawCenteredTextInRect(UWORD x, UWORD y, UWORD w, const char* text, UWORD color, UWORD bg_color, UWORD scale)
{
  UWORD text_width = LCD_TextWidth(text, scale);
  UWORD tx = (text_width >= w) ? x : (UWORD)(x + ((w - text_width) / 2U));

  LCD_DrawText(tx, y, text, color, bg_color, scale);
}

static void LCD_ShowHelloBuaa(void)
{
  UWORD bg_color = LCD_COLOR_WHITE;

  LCD_1IN69_FillRect_FastStatic(0U, 0U, (UWORD)(LCD_1IN69.WIDTH - 1U), (UWORD)(LCD_1IN69.HEIGHT - 1U), bg_color);
  LCD_1IN69_FillRect_FastStatic(0U, 0U, (UWORD)(LCD_1IN69.WIDTH - 1U), 7U, LCD_COLOR_BLACK);
  LCD_1IN69_FillRect_FastStatic(0U, 272U, (UWORD)(LCD_1IN69.WIDTH - 1U), (UWORD)(LCD_1IN69.HEIGHT - 1U), LCD_COLOR_BLACK);
  LCD_DrawCenteredText("Hello BUAA", 126U, LCD_COLOR_BLACK, bg_color, 4U);
}

static void LCD_FillRectByRows(UWORD x0, UWORD y0, UWORD x1, UWORD y1, UWORD color)
{
  UWORD y;

  if (x0 >= LCD_1IN69.WIDTH)
  {
    x0 = (UWORD)(LCD_1IN69.WIDTH - 1U);
  }
  if (x1 >= LCD_1IN69.WIDTH)
  {
    x1 = (UWORD)(LCD_1IN69.WIDTH - 1U);
  }
  if (y0 >= LCD_1IN69.HEIGHT)
  {
    y0 = (UWORD)(LCD_1IN69.HEIGHT - 1U);
  }
  if (y1 >= LCD_1IN69.HEIGHT)
  {
    y1 = (UWORD)(LCD_1IN69.HEIGHT - 1U);
  }
  if ((x1 < x0) || (y1 < y0))
  {
    return;
  }

  for (y = y0; y <= y1; y++)
  {
    LCD_1IN69_FillRect_FastStatic(x0, y, x1, y, color);
  }
}

static void LCD_FillScreenByRows(UWORD color)
{
  LCD_FillRectByRows(0U, 0U, (UWORD)(LCD_1IN69.WIDTH - 1U), (UWORD)(LCD_1IN69.HEIGHT - 1U), color);
}

static void LCD_FillBox(UWORD x, UWORD y, UWORD w, UWORD h, UWORD color)
{
  if ((w == 0U) || (h == 0U))
  {
    return;
  }

  LCD_FillRectByRows(x, y, (UWORD)(x + w - 1U), (UWORD)(y + h - 1U), color);
}

static void UI_DrawDots(UBYTE active, UWORD bg_color)
{
  UBYTE i;
  UWORD inactive = LCD_RGB565(104U, 126U, 146U);
  UWORD active_color = LCD_RGB565(248U, 252U, 255U);

  for (i = 0U; i < 4U; i++)
  {
    UWORD x = (UWORD)(96U + (i * 14U));
    LCD_FillBox(x, 264U, 8U, 8U, (i == active) ? active_color : inactive);
    LCD_FillBox((UWORD)(x + 2U), 266U, 4U, 4U, (i == active) ? active_color : inactive);
  }
  (void)bg_color;
}

static void UI_ShowHome(void)
{
  UWORD sky_top = LCD_RGB565(72U, 171U, 214U);
  UWORD sky_mid = LCD_RGB565(43U, 144U, 196U);
  UWORD sky_low = LCD_RGB565(23U, 96U, 145U);
  UWORD footer = LCD_RGB565(16U, 55U, 84U);
  UWORD sun = LCD_RGB565(255U, 206U, 72U);
  UWORD cloud_shadow = LCD_RGB565(176U, 214U, 228U);
  UWORD cloud = LCD_RGB565(231U, 244U, 249U);
  UWORD dark_text = LCD_RGB565(13U, 44U, 65U);
  UWORD pale_text = LCD_RGB565(220U, 238U, 247U);

  LCD_FillBox(0U, 0U, 240U, 64U, sky_top);
  LCD_FillBox(0U, 64U, 240U, 64U, sky_mid);
  LCD_FillBox(0U, 128U, 240U, 80U, sky_low);
  LCD_FillBox(0U, 208U, 240U, 72U, footer);

  LCD_FillBox(178U, 32U, 48U, 16U, sun);
  LCD_FillBox(194U, 16U, 16U, 48U, sun);
  LCD_FillBox(186U, 24U, 32U, 32U, sun);

  LCD_FillBox(132U, 144U, 66U, 20U, cloud_shadow);
  LCD_FillBox(122U, 138U, 84U, 22U, cloud);
  LCD_FillBox(140U, 122U, 28U, 24U, cloud);
  LCD_FillBox(168U, 126U, 24U, 22U, cloud);

  LCD_DrawText(18U, 18U, "BUAA TEAM", dark_text, sky_top, 2U);
  LCD_DrawText(28U, 72U, "12:30", LCD_COLOR_WHITE, sky_mid, 4U);
  LCD_FillBox(28U, 128U, 34U, 4U, LCD_COLOR_WHITE);
  LCD_DrawText(34U, 178U, "2026 07 07", pale_text, sky_low, 2U);
  LCD_DrawCenteredTextInRect(0U, 222U, 240U, "TUESDAY", pale_text, footer, 2U);
  UI_DrawDots(0U, footer);
}

static void UI_ShowSportMenu(void)
{
  UWORD bg = LCD_RGB565(8U, 18U, 28U);
  UWORD accent = LCD_RGB565(63U, 212U, 122U);
  UWORD title = LCD_RGB565(245U, 248U, 255U);
  UWORD hint = LCD_RGB565(151U, 169U, 190U);
  UWORD card0 = LCD_RGB565(24U, 78U, 66U);
  UWORD card1 = LCD_RGB565(106U, 51U, 38U);
  UWORD card2 = LCD_RGB565(48U, 58U, 125U);
  UWORD card_text = LCD_RGB565(190U, 206U, 222U);

  LCD_FillScreenByRows(bg);
  LCD_FillBox(0U, 0U, 240U, 5U, accent);
  LCD_DrawText(18U, 18U, "SPORT", title, bg, 2U);
  LCD_DrawText(18U, 44U, "TAP MODE", hint, bg, 1U);

  LCD_FillBox(18U, 70U, 204U, 44U, card0);
  LCD_DrawText(30U, 78U, "WALK", title, card0, 2U);
  LCD_DrawText(142U, 88U, "START", card_text, card0, 1U);

  LCD_FillBox(18U, 128U, 204U, 44U, card1);
  LCD_DrawText(30U, 136U, "RUN", title, card1, 2U);
  LCD_DrawText(142U, 146U, "START", card_text, card1, 1U);

  LCD_FillBox(18U, 186U, 204U, 44U, card2);
  LCD_DrawText(30U, 194U, "ROPE", title, card2, 2U);
  LCD_DrawText(142U, 204U, "START", card_text, card2, 1U);

  UI_DrawDots(1U, bg);
}

static void UI_DrawSportDetail(const char* title_text, UWORD accent, const char* main_value, const char* main_label,
                               const char* v0, const char* v1, const char* v2, const char* v3, const char* v4,
                               const char* l0, const char* l1, const char* l2, const char* l3, const char* l4,
                               UBYTE dot)
{
  UWORD bg = LCD_RGB565(5U, 13U, 26U);
  UWORD title = LCD_RGB565(245U, 248U, 255U);
  UWORD muted = LCD_RGB565(144U, 160U, 180U);
  UWORD heart = LCD_RGB565(255U, 112U, 112U);
  UWORD stat_text = LCD_RGB565(238U, 244U, 255U);
  UWORD stat_label = LCD_RGB565(142U, 158U, 178U);
  UWORD box0 = LCD_RGB565(18U, 44U, 82U);
  UWORD box1 = LCD_RGB565(16U, 91U, 118U);
  UWORD box2 = LCD_RGB565(22U, 104U, 63U);
  UWORD box3 = LCD_RGB565(128U, 72U, 29U);
  UWORD box4 = LCD_RGB565(70U, 70U, 143U);

  LCD_FillScreenByRows(bg);
  LCD_FillBox(0U, 0U, 240U, 5U, accent);
  LCD_DrawText(12U, 14U, title_text, title, bg, 2U);
  LCD_DrawText(157U, 18U, "HR 146", heart, bg, 1U);
  LCD_DrawCenteredTextInRect(0U, 48U, 240U, main_value, LCD_COLOR_WHITE, bg, 3U);
  LCD_DrawCenteredTextInRect(0U, 84U, 240U, main_label, muted, bg, 1U);

  LCD_FillBox(12U, 110U, 104U, 58U, box0);
  LCD_DrawText(20U, 119U, v0, stat_text, box0, 2U);
  LCD_DrawText(20U, 149U, l0, stat_label, box0, 1U);

  LCD_FillBox(124U, 110U, 104U, 58U, box1);
  LCD_DrawText(132U, 119U, v1, stat_text, box1, 2U);
  LCD_DrawText(132U, 149U, l1, stat_label, box1, 1U);

  LCD_FillBox(12U, 186U, 62U, 58U, box2);
  LCD_DrawText(20U, 195U, v2, stat_text, box2, 1U);
  LCD_DrawText(20U, 225U, l2, stat_label, box2, 1U);

  LCD_FillBox(85U, 186U, 70U, 58U, box3);
  LCD_DrawText(93U, 195U, v3, stat_text, box3, 1U);
  LCD_DrawText(93U, 225U, l3, stat_label, box3, 1U);

  LCD_FillBox(158U, 186U, 70U, 58U, box4);
  LCD_DrawText(166U, 195U, v4, stat_text, box4, 1U);
  LCD_DrawText(166U, 225U, l4, stat_label, box4, 1U);

  LCD_FillBox(52U, 248U, 136U, 28U, accent);
  LCD_DrawCenteredTextInRect(52U, 254U, 136U, "START", bg, accent, 2U);
  UI_DrawDots(dot, bg);
}

static void UI_ShowCarouselScreen(UBYTE page)
{
  switch (page % 4U)
  {
  case 0U:
    UI_ShowHome();
    break;
  case 1U:
    UI_ShowSportMenu();
    break;
  case 2U:
    UI_DrawSportDetail("RUN", LCD_RGB565(255U, 106U, 61U), "4.32", "KM",
                       "00:26", "97%", "3.6", "2.9", "84",
                       "TIME", "SPO2", "NOW", "AVG", "CAD", 2U);
    break;
  case 3U:
  default:
    UI_DrawSportDetail("ROPE", LCD_RGB565(108U, 140U, 255U), "128", "TIMES",
                       "01:06", "91%", "42", "6.4", "128",
                       "TIME", "SPO2", "PACE", "CAL", "CNT", 3U);
    break;
  }
}

static void TouchPins_Init(void)
{
  HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_RESET);
  HAL_Delay(10U);
  HAL_GPIO_WritePin(TP_RST_GPIO_Port, TP_RST_Pin, GPIO_PIN_SET);
  HAL_Delay(80U);
}

static UBYTE Touch_IsPressed(void)
{
  return (HAL_GPIO_ReadPin(TP_INT_GPIO_Port, TP_INT_Pin) == GPIO_PIN_RESET) ? 1U : 0U;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
#if LCD_USE_HAL_SPI
  MX_SPI1_Init();
#endif
  /* USER CODE BEGIN 2 */
  if (DEV_Module_Init() != 0)
  {
    Error_Handler();
  }

  LCD_1IN69_SetBackLight(1000U);
  LCD_1IN69_Init(VERTICAL);
  TouchPins_Init();
  LCD_ShowHelloBuaa();
  HAL_Delay(5000U);
  ui_page = 0U;
  touch_pressed_prev = Touch_IsPressed();
  ui_last_switch_ms = HAL_GetTick();
  UI_ShowCarouselScreen(ui_page);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now_ms = HAL_GetTick();
    UBYTE touch_pressed = Touch_IsPressed();
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    if (((now_ms - ui_last_switch_ms) >= 1600U) ||
        ((touch_pressed != 0U) && (touch_pressed_prev == 0U)))
    {
      ui_page = (UBYTE)((ui_page + 1U) % 4U);
      UI_ShowCarouselScreen(ui_page);
      ui_last_switch_ms = HAL_GetTick();
    }
    touch_pressed_prev = touch_pressed;
    HAL_Delay(40U);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 20;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLLVCIRANGE_1;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_PCLK3;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
#if LCD_USE_HAL_SPI
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  SPI_AutonomousModeConfTypeDef HAL_SPI_AutonomousMode_Cfg_Struct = {0};

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES_TXONLY;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 0x7;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  hspi1.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi1.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi1.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi1.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi1.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi1.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi1.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  hspi1.Init.ReadyMasterManagement = SPI_RDY_MASTER_MANAGEMENT_INTERNALLY;
  hspi1.Init.ReadyPolarity = SPI_RDY_POLARITY_HIGH;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerState = SPI_AUTO_MODE_DISABLE;
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerSelection = SPI_GRP1_GPDMA_CH0_TCF_TRG;
  HAL_SPI_AutonomousMode_Cfg_Struct.TriggerPolarity = SPI_TRIG_POLARITY_RISING;
  if (HAL_SPIEx_SetConfigAutonomousMode(&hspi1, &HAL_SPI_AutonomousMode_Cfg_Struct) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}
#endif

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, LCD_RST_Pin|LCD_CS_Pin|LCD_DC_Pin|TP_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : LCD_RST_Pin LCD_CS_Pin LCD_DC_Pin TP_RST_Pin */
  GPIO_InitStruct.Pin = LCD_RST_Pin|LCD_CS_Pin|LCD_DC_Pin|TP_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : TP_INT_Pin */
  GPIO_InitStruct.Pin = TP_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TP_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BLK_Pin */
  GPIO_InitStruct.Pin = LCD_BLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_BLK_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @param None
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitStruct.Pin = LCD_RST_Pin|LCD_CS_Pin|LCD_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LCD_BLK_Pin;
  HAL_GPIO_Init(LCD_BLK_GPIO_Port, &GPIO_InitStruct);

  __disable_irq();
  while (1)
  {
    HAL_GPIO_WritePin(GPIOE, LCD_RST_Pin|LCD_CS_Pin|LCD_DC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_SET);
    for (volatile uint32_t i = 0U; i < 250000U; i++)
    {
      __NOP();
    }

    HAL_GPIO_WritePin(GPIOE, LCD_RST_Pin|LCD_CS_Pin|LCD_DC_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_RESET);
    for (volatile uint32_t i = 0U; i < 250000U; i++)
    {
      __NOP();
    }
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
