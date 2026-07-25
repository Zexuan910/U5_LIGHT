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
#include <stdio.h>
#include <string.h>

#include "battery_monitor.h"
#include "DEV_Config.h"
#include "gh3018_comm.h"
#include "gh3018_goodix_hrspo2.h"
#include "LCD_1in69.h"
#include "cst816t.h"
#include "imu_sensor.h"
#include "touch_controller.h"
#include "ui_asset_programmer.h"
#include "ui_assets.h"
#include "ui_pc_time.h"
#include "walk_metrics.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LCD_USE_HAL_SPI 1
#define UI_SPORT_COUNT 3U
#define UI_EXERCISE_TIME_SCALE_NUM 2UL
#define UI_EXERCISE_TIME_SCALE_DEN 1UL
#define UI_BATTERY_DISPLAY_TOGGLE_MS 2000UL
#define UI_WALK_FIELD_INVALID 0xFFFFFFFFUL
#define GH3018_HRSPO2_POLL_INTERVAL_MS 50UL
#define GH3018_HRSPO2_UI_REFRESH_MS 500UL
#define GH3018_DIAGNOSTIC_SCREEN_ENABLE 0U
#define HR_DISPLAY_MIN_BPM 40U
#define HR_DISPLAY_MAX_BPM 150U
#define HR_SPORT_HEALTH_REFRESH_MS 500UL

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

ADC_HandleTypeDef hadc1;
SPI_HandleTypeDef hspi1;
I2C_HandleTypeDef hi2c2;
I2C_HandleTypeDef hi2c3;

/* USER CODE BEGIN PV */
typedef enum {
  UI_PAGE_HOME = 0,
  UI_PAGE_NAV,
  UI_PAGE_WALK,
  UI_PAGE_RUN,
  UI_PAGE_ROPE,
  UI_PAGE_LOCK
} UIPage;

typedef struct {
  UBYTE pressed;
  UBYTE has_xy;
  UWORD x;
  UWORD y;
} TouchSample;

typedef struct {
  uint16_t year;
  UBYTE month;
  UBYTE day;
  UBYTE hour;
  UBYTE minute;
  UBYTE second;
  UBYTE wday;
} UIClock;

typedef enum {
  HR_UI_IDLE = 0,
  HR_UI_RUNNING
} HrUiState;

static UIPage ui_page = UI_PAGE_HOME;
static UBYTE selected_sport = 0U;
static UBYTE touch_pressed_prev = 0U;
static uint32_t touch_down_ms = 0U;
static UBYTE touch_down_has_xy = 0U;
static UWORD touch_down_x = 0U;
static UWORD touch_down_y = 0U;
static UIPage touch_down_page = UI_PAGE_HOME;
static UBYTE touch_last_has_xy = 0U;
static UWORD touch_last_x = 0U;
static UWORD touch_last_y = 0U;
static UBYTE touch_swipe_consumed = 0U;
static UBYTE touch_suppress_until_release = 1U;
static uint32_t last_touch_ms = 0U;
static uint32_t touch_last_active_ms = 0U;
static uint32_t lock_enter_ms = 0U;
static uint32_t lock_touch_candidate_ms = 0U;
static uint32_t exercise_start_ms[UI_SPORT_COUNT] = {0U, 0U, 0U};
static uint32_t exercise_elapsed_seconds[UI_SPORT_COUNT] = {0U, 0U, 0U};
static uint32_t ui_sport_time_key[UI_SPORT_COUNT] = {0xFFFFFFFFUL, 0xFFFFFFFFUL, 0xFFFFFFFFUL};
static UBYTE exercise_running[UI_SPORT_COUNT] = {0U, 0U, 0U};
static HrUiState hr_ui_state[UI_SPORT_COUNT] = {
  HR_UI_IDLE,
  HR_UI_IDLE,
  HR_UI_IDLE
};
static WalkMetricsState walk_metrics_state;
static WalkMetricsOutput walk_metrics_output;
static uint32_t ui_walk_distance_key = UI_WALK_FIELD_INVALID;
static uint32_t ui_walk_now_speed_key = UI_WALK_FIELD_INVALID;
static uint32_t ui_walk_avg_speed_key = UI_WALK_FIELD_INVALID;
static uint32_t ui_walk_step_key = UI_WALK_FIELD_INVALID;
static UBYTE ui_exit_confirm_visible = 0U;
static UIPage ui_exit_confirm_target = UI_PAGE_NAV;
static UIClock ui_clock = {
  (uint16_t)UI_PC_TIME_YEAR,
  (UBYTE)UI_PC_TIME_MONTH,
  (UBYTE)UI_PC_TIME_DAY,
  (UBYTE)UI_PC_TIME_HOUR,
  (UBYTE)UI_PC_TIME_MINUTE,
  (UBYTE)UI_PC_TIME_SECOND,
  (UBYTE)UI_PC_TIME_WEEKDAY
};
static uint32_t ui_clock_tick_ms = 0U;
static uint32_t ui_home_clock_key = 0xFFFFFFFFUL;
static uint32_t ui_home_battery_key = 0xFFFFFFFFUL;
static UBYTE screen_locked = 0U;
static uint32_t gh3018_last_poll_tick = 0U;
static uint32_t ui_hrspo2_last_draw_tick = 0U;
static uint32_t ui_sport_health_last_draw_tick = 0U;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_ADC1_Init(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_I2C3_Init(void);
#if LCD_USE_HAL_SPI
static void MX_SPI1_Init(void);
#endif
static void MX_USART1_Debug_Init(void);
/* USER CODE BEGIN PFP */
static void LCD_ShowHelloBuaa(void);
static void UI_ShowPage(UIPage page);
static void UI_ShowHome(void);
static void UI_GotoPage(UIPage page);
static UBYTE UI_PageIsDetail(UIPage page);
static void UI_ClockInit(uint32_t now_ms);
static void UI_ClockUpdate(uint32_t now_ms);
static void UI_UpdateHomeClock(UBYTE force);
static void UI_UpdateHomeBattery(UBYTE force);
static void UI_HandleTouchPressed(uint32_t now_ms, const TouchSample* sample);
static void UI_HandleTouchReleased(uint32_t now_ms, uint32_t press_ms);
static UBYTE UI_TryRealtimeSwipe(uint32_t now_ms);
static void UI_DrawSportActionButton(UWORD accent);
static void UI_UpdateSportTimer(uint32_t now_ms, UBYTE force);
static void UI_UpdateWalkMetrics(uint32_t now_ms);
static void UI_UpdateWalkDataDisplay(UBYTE force);
static void UI_StartExercise(uint32_t now_ms);
static void UI_StopExercise(uint32_t now_ms);
static void UI_HandleSportAction(
    uint32_t now_ms,
    const Gh3018GoodixHrSpo2Snapshot *snapshot);
static void UI_UpdateSportHealthDisplay(
    uint32_t now_ms,
    const Gh3018GoodixHrSpo2Snapshot *snapshot,
    UBYTE force);
static void UI_ShowExitConfirm(UIPage target);
static void UI_HideExitConfirm(void);
static void UI_ConfirmExerciseExit(uint32_t now_ms);
static void LCD_FillRectByRows(UWORD x0, UWORD y0, UWORD x1, UWORD y1, UWORD color);
static TouchSample Touch_ReadSample(void);
static const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2Service_Update(
    uint32_t now_ms,
    const Gh3018GoodixHrSpo2Snapshot *snapshot);
static void UI_ShowHrSpo2Display(const Gh3018GoodixHrSpo2Snapshot *snapshot);
static void UI_UpdateHrSpo2Display(
    uint32_t now_ms,
    const Gh3018GoodixHrSpo2Snapshot *snapshot,
    UBYTE force);
static void PrintHrSpo2Snapshot(
    const char *tag,
    const Gh3018GoodixHrSpo2Snapshot *snapshot);
static void PrintBatterySnapshot(
    const char *tag,
    const BatteryMonitorSnapshot *snapshot);

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

static void LCD_DrawCharTransparent(UWORD x, UWORD y, char ch, UWORD color, UWORD scale)
{
  const UBYTE* glyph = LCD_GlyphFor(ch);
  UWORD row;
  UWORD col;

  for (row = 0U; row < 7U; row++)
  {
    for (col = 0U; col < 5U; col++)
    {
      if ((glyph[row] & (1U << (4U - col))) != 0U)
      {
        UWORD x0 = (UWORD)(x + (col * scale));
        UWORD y0 = (UWORD)(y + (row * scale));

        LCD_1IN69_FillRect_FastStatic(x0, y0, (UWORD)(x0 + scale - 1U), (UWORD)(y0 + scale - 1U), color);
      }
    }
  }
}

static void LCD_DrawTextTransparent(UWORD x, UWORD y, const char* text, UWORD color, UWORD scale)
{
  UWORD i = 0U;

  while (text[i] != '\0')
  {
    LCD_DrawCharTransparent((UWORD)(x + (i * 6U * scale)), y, text[i], color, scale);
    i++;
  }
}

static void LCD_DrawCenteredTextInRectTransparent(UWORD x, UWORD y, UWORD w, const char* text, UWORD color, UWORD scale)
{
  UWORD text_width = LCD_TextWidth(text, scale);
  UWORD tx = (text_width >= w) ? x : (UWORD)(x + ((w - text_width) / 2U));

  LCD_DrawTextTransparent(tx, y, text, color, scale);
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

static UBYTE UI_ClockIsLeapYear(uint16_t year)
{
  if ((year % 400U) == 0U)
  {
    return 1U;
  }
  if ((year % 100U) == 0U)
  {
    return 0U;
  }
  return ((year % 4U) == 0U) ? 1U : 0U;
}

static UBYTE UI_ClockDaysInMonth(uint16_t year, UBYTE month)
{
  static const UBYTE days[12] = {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

  if ((month == 2U) && (UI_ClockIsLeapYear(year) != 0U))
  {
    return 29U;
  }
  if ((month == 0U) || (month > 12U))
  {
    return 31U;
  }
  return days[month - 1U];
}

static void UI_ClockAddOneSecond(void)
{
  ui_clock.second++;
  if (ui_clock.second < 60U)
  {
    return;
  }

  ui_clock.second = 0U;
  ui_clock.minute++;
  if (ui_clock.minute < 60U)
  {
    return;
  }

  ui_clock.minute = 0U;
  ui_clock.hour++;
  if (ui_clock.hour < 24U)
  {
    return;
  }

  ui_clock.hour = 0U;
  ui_clock.day++;
  ui_clock.wday = (UBYTE)((ui_clock.wday + 1U) % 7U);
  if (ui_clock.day <= UI_ClockDaysInMonth(ui_clock.year, ui_clock.month))
  {
    return;
  }

  ui_clock.day = 1U;
  ui_clock.month++;
  if (ui_clock.month <= 12U)
  {
    return;
  }

  ui_clock.month = 1U;
  ui_clock.year++;
}

static uint32_t UI_ClockMinuteKey(void)
{
  return (((uint32_t)ui_clock.year) << 20) |
         (((uint32_t)ui_clock.month) << 16) |
         (((uint32_t)ui_clock.day) << 11) |
         (((uint32_t)ui_clock.hour) << 6) |
         (uint32_t)ui_clock.minute;
}

static void UI_ClockInit(uint32_t now_ms)
{
  ui_clock_tick_ms = now_ms;
  ui_home_clock_key = 0xFFFFFFFFUL;
}

static void UI_ClockUpdate(uint32_t now_ms)
{
  while ((now_ms - ui_clock_tick_ms) >= 1000UL)
  {
    ui_clock_tick_ms += 1000UL;
    UI_ClockAddOneSecond();
  }
}

static const char* UI_ClockWeekdayText(void)
{
  static const char* const names[7] = {
    "SUNDAY",
    "MONDAY",
    "TUESDAY",
    "WEDNESDAY",
    "THURSDAY",
    "FRIDAY",
    "SATURDAY"
  };

  return names[ui_clock.wday % 7U];
}

static void UI_DrawHomeClockFields(void)
{
  char time_text[8];
  char date_text[14];
  UWORD pale_text = LCD_RGB565(220U, 238U, 247U);

  (void)snprintf(time_text, sizeof(time_text), "%02u:%02u",
                 (unsigned int)ui_clock.hour,
                 (unsigned int)ui_clock.minute);
  (void)snprintf(date_text, sizeof(date_text), "%04u %02u %02u",
                 (unsigned int)ui_clock.year,
                 (unsigned int)ui_clock.month,
                 (unsigned int)ui_clock.day);

  LCD_DrawTextTransparent(28U, 72U, time_text, LCD_COLOR_WHITE, 4U);
  LCD_DrawTextTransparent(34U, 178U, date_text, pale_text, 2U);
  LCD_DrawCenteredTextInRectTransparent(0U, 222U, 240U, UI_ClockWeekdayText(), pale_text, 2U);
  ui_home_clock_key = UI_ClockMinuteKey();
}

static void UI_UpdateHomeClock(UBYTE force)
{
  if ((ui_page == UI_PAGE_HOME) &&
      ((force != 0U) || (ui_home_clock_key != UI_ClockMinuteKey())))
  {
    if (force != 0U)
    {
      UI_DrawHomeClockFields();
    }
    else
    {
      UI_ShowHome();
    }
  }
}

static void UI_UpdateHomeBattery(UBYTE force)
{
  const BatteryMonitorSnapshot *battery = BatteryMonitor_GetSnapshot();
  uint32_t show_mv = (HAL_GetTick() / UI_BATTERY_DISPLAY_TOGGLE_MS) & 1UL;
  uint32_t key = 0xFFFFFFFFUL;
  char text[16];
  UWORD bg = LCD_RGB565(16U, 55U, 84U);
  UWORD fg = LCD_RGB565(238U, 244U, 255U);

  if (ui_page != UI_PAGE_HOME)
  {
    return;
  }

  if ((battery != NULL) && (battery->valid != 0U))
  {
    if (show_mv != 0UL)
    {
      key = 0x80000000UL | (battery->batteryMillivolts & 0x7FFFFFFFUL);
      (void)snprintf(text,
                     sizeof(text),
                     "BAT %lumV",
                     (unsigned long)battery->batteryMillivolts);
    }
    else
    {
      key = (uint32_t)battery->percent;
      (void)snprintf(text, sizeof(text), "BAT %u%%", battery->percent);
    }
  }
  else
  {
    key = (show_mv != 0UL) ? 0xFFFFFFFEUL : 0xFFFFFFFFUL;
    (void)snprintf(text,
                   sizeof(text),
                   (show_mv != 0UL) ? "BAT --mV" : "BAT --%%");
  }

  if ((force == 0U) && (ui_home_battery_key == key))
  {
    return;
  }

  LCD_FillBox(152U, 18U, 72U, 18U, bg);
  LCD_DrawText(160U, 24U, text, fg, bg, 1U);
  ui_home_battery_key = key;
}

static UWORD UI_CurrentSportAccent(void)
{
  if (ui_page == UI_PAGE_WALK)
  {
    return LCD_RGB565(63U, 212U, 122U);
  }
  if (ui_page == UI_PAGE_RUN)
  {
    return LCD_RGB565(255U, 106U, 61U);
  }
  return LCD_RGB565(108U, 140U, 255U);
}

static UBYTE UI_CurrentSportIndex(void)
{
  return (selected_sport < UI_SPORT_COUNT) ? selected_sport : 0U;
}

static uint32_t UI_ExerciseElapsedSeconds(uint32_t now_ms)
{
  UBYTE sport = UI_CurrentSportIndex();

  if (exercise_running[sport] != 0U)
  {
    uint32_t elapsed_ms = now_ms - exercise_start_ms[sport];
    return (elapsed_ms * UI_EXERCISE_TIME_SCALE_NUM) / (1000UL * UI_EXERCISE_TIME_SCALE_DEN);
  }

  return exercise_elapsed_seconds[sport];
}

static void UI_FormatExerciseTime(uint32_t seconds, char* out, size_t out_size)
{
  uint32_t minutes = seconds / 60UL;
  uint32_t secs = seconds % 60UL;

  if (minutes > 99UL)
  {
    minutes = 99UL;
    secs = 59UL;
  }

  (void)snprintf(out, out_size, "%02lu:%02lu", (unsigned long)minutes, (unsigned long)secs);
}

static void UI_DrawSportTimeField(uint32_t seconds)
{
  char time_text[8];
  UWORD box0 = LCD_RGB565(18U, 44U, 82U);
  UWORD stat_text = LCD_RGB565(238U, 244U, 255U);

  UI_FormatExerciseTime(seconds, time_text, sizeof(time_text));
  LCD_FillBox(18U, 116U, 86U, 18U, box0);
  LCD_DrawText(20U, 119U, time_text, stat_text, box0, 2U);
  ui_sport_time_key[UI_CurrentSportIndex()] = seconds;
}

static uint32_t UI_WalkDistanceKey(float distance_m)
{
  if (distance_m <= 0.0f)
  {
    return 0UL;
  }
  if (distance_m >= 99990.0f)
  {
    return 9999UL;
  }
  return (uint32_t)((distance_m / 10.0f) + 0.5f);
}

static uint32_t UI_WalkSpeedKey(float speed_mps)
{
  if (speed_mps <= 0.0f)
  {
    return 0UL;
  }
  if (speed_mps >= 99.9f)
  {
    return 999UL;
  }
  return (uint32_t)((speed_mps * 10.0f) + 0.5f);
}

static void UI_FormatWalkDistance(uint32_t centi_km, char* out, size_t out_size)
{
  if (centi_km > 9999UL)
  {
    centi_km = 9999UL;
  }

  (void)snprintf(out, out_size, "%lu.%02lu",
                 (unsigned long)(centi_km / 100UL),
                 (unsigned long)(centi_km % 100UL));
}

static void UI_FormatWalkSpeed(uint32_t speed_x10, char* out, size_t out_size)
{
  if (speed_x10 > 999UL)
  {
    speed_x10 = 999UL;
  }

  (void)snprintf(out, out_size, "%lu.%lu",
                 (unsigned long)(speed_x10 / 10UL),
                 (unsigned long)(speed_x10 % 10UL));
}

static void UI_FormatWalkStep(uint32_t steps, char* out, size_t out_size)
{
  if (steps > 99999UL)
  {
    steps = 99999UL;
  }

  (void)snprintf(out, out_size, "%lu", (unsigned long)steps);
}

static void UI_DrawSportMainField(const char* value_text, const char* label_text)
{
  UWORD main_bg = LCD_RGB565(226U, 236U, 240U);
  UWORD muted = LCD_RGB565(40U, 50U, 62U);

  LCD_FillBox(0U, 42U, 240U, 58U, main_bg);
  LCD_DrawCenteredTextInRectTransparent(0U, 48U, 240U, value_text, LCD_COLOR_BLACK, 3U);
  LCD_DrawCenteredTextInRectTransparent(0U, 84U, 240U, label_text, muted, 1U);
}

static void UI_DrawWalkDistanceField(uint32_t centi_km)
{
  char text[8];

  UI_FormatWalkDistance(centi_km, text, sizeof(text));
  UI_DrawSportMainField(text, "KM");
  ui_walk_distance_key = centi_km;
}

static void UI_DrawWalkSpeedField(UWORD x, UWORD y, UWORD w, UWORD bg, uint32_t speed_x10)
{
  char text[6];
  UWORD stat_text = LCD_RGB565(238U, 244U, 255U);

  UI_FormatWalkSpeed(speed_x10, text, sizeof(text));
  LCD_FillBox(x, y, w, 16U, bg);
  LCD_DrawText(x, y, text, stat_text, bg, 1U);
}

static void UI_DrawWalkStepField(uint32_t steps)
{
  char text[8];
  UWORD box4 = LCD_RGB565(70U, 70U, 143U);
  UWORD stat_text = LCD_RGB565(238U, 244U, 255U);

  UI_FormatWalkStep(steps, text, sizeof(text));
  LCD_FillBox(166U, 195U, 54U, 16U, box4);
  LCD_DrawText(166U, 195U, text, stat_text, box4, 1U);
  ui_walk_step_key = steps;
}

static void UI_UpdateSportTimer(uint32_t now_ms, UBYTE force)
{
  uint32_t seconds;

  if ((UI_PageIsDetail(ui_page) == 0U) || (ui_exit_confirm_visible != 0U))
  {
    return;
  }

  seconds = UI_ExerciseElapsedSeconds(now_ms);
  if ((force != 0U) || (seconds != ui_sport_time_key[UI_CurrentSportIndex()]))
  {
    UI_DrawSportTimeField(seconds);
  }
}

static void UI_UpdateWalkDataDisplay(UBYTE force)
{
  uint32_t distance_key;
  uint32_t now_speed_key;
  uint32_t avg_speed_key;
  uint32_t step_key;

  if ((ui_page != UI_PAGE_WALK) || (ui_exit_confirm_visible != 0U))
  {
    return;
  }

  distance_key = UI_WalkDistanceKey(walk_metrics_output.distance_m);
  now_speed_key = UI_WalkSpeedKey(walk_metrics_output.instant_speed_mps);
  avg_speed_key = UI_WalkSpeedKey(walk_metrics_output.average_speed_mps);
  step_key = walk_metrics_output.step_count;

  if ((force != 0U) || (distance_key != ui_walk_distance_key))
  {
    UI_DrawWalkDistanceField(distance_key);
  }
  if ((force != 0U) || (now_speed_key != ui_walk_now_speed_key))
  {
    UI_DrawWalkSpeedField(20U, 195U, 42U, LCD_RGB565(22U, 104U, 63U), now_speed_key);
    ui_walk_now_speed_key = now_speed_key;
  }
  if ((force != 0U) || (avg_speed_key != ui_walk_avg_speed_key))
  {
    UI_DrawWalkSpeedField(93U, 195U, 48U, LCD_RGB565(128U, 72U, 29U), avg_speed_key);
    ui_walk_avg_speed_key = avg_speed_key;
  }
  if ((force != 0U) || (step_key != ui_walk_step_key))
  {
    UI_DrawWalkStepField(step_key);
  }
}

static void UI_UpdateWalkMetrics(uint32_t now_ms)
{
  IMU_SensorSample imu_sample;
  WalkMetricsImuSample walk_sample;
  uint16_t pending_samples;
  uint16_t samples_to_read;

  if (exercise_running[0] == 0U)
  {
    return;
  }

  pending_samples = IMU_Sensor_PendingSamples();
  samples_to_read = (pending_samples > 16U) ? 16U : pending_samples;

  for (uint16_t sample_index = 0U; sample_index < samples_to_read; sample_index++)
  {
    uint32_t samples_after = (uint32_t)(pending_samples - sample_index - 1U);

    if (!IMU_Sensor_Read(&imu_sample))
    {
      break;
    }

    walk_sample.tick_ms = now_ms - (samples_after * 20U);
    for (UBYTE i = 0U; i < 3U; i++)
    {
      walk_sample.accel_g[i] = imu_sample.accel_g[i];
      walk_sample.gyro_rad_s[i] = imu_sample.gyro_rad_s[i];
    }

    WalkMetrics_Update(&walk_metrics_state, &walk_sample, &walk_metrics_output);
  }
}

static uint8_t UI_SelectDisplayBpm(
    const Gh3018GoodixHrSpo2Snapshot *snapshot)
{
  uint8_t bpm = 0U;

  if (snapshot == NULL)
  {
    return 0U;
  }

  if ((snapshot->heartRate >= HR_DISPLAY_MIN_BPM) &&
      (snapshot->heartRate <= HR_DISPLAY_MAX_BPM))
  {
    bpm = snapshot->heartRate;
  }
  else if ((snapshot->ppgHeartRate >= HR_DISPLAY_MIN_BPM) &&
           (snapshot->ppgHeartRate <= HR_DISPLAY_MAX_BPM))
  {
    bpm = snapshot->ppgHeartRate;
  }
  else if ((snapshot->ppgDisplayedBpm >= HR_DISPLAY_MIN_BPM) &&
           (snapshot->ppgDisplayedBpm <= HR_DISPLAY_MAX_BPM))
  {
    bpm = snapshot->ppgDisplayedBpm;
  }

  return bpm;
}

static void UI_StartExercise(uint32_t now_ms)
{
  UBYTE sport = UI_CurrentSportIndex();
  const Gh3018GoodixHrSpo2Snapshot *snapshot;

  (void)Gh3018GoodixHrSpo2_ResetPpgBpm();
  snapshot = Gh3018GoodixHrSpo2_Start();
  gh3018_last_poll_tick = now_ms;
  if ((snapshot != NULL) && (snapshot->measurementActive != 0U))
  {
    (void)Gh3018GoodixHrSpo2_SetManualWear(1U);
  }
  exercise_elapsed_seconds[sport] = 0U;
  exercise_start_ms[sport] = now_ms;
  exercise_running[sport] = 1U;
  hr_ui_state[sport] = HR_UI_RUNNING;
  ui_sport_health_last_draw_tick = 0U;
  if (sport == 0U)
  {
    WalkMetrics_Start(&walk_metrics_state, now_ms);
    memset(&walk_metrics_output, 0, sizeof(walk_metrics_output));
    (void)IMU_Sensor_ResetFifo();
    ui_walk_distance_key = UI_WALK_FIELD_INVALID;
    ui_walk_now_speed_key = UI_WALK_FIELD_INVALID;
    ui_walk_avg_speed_key = UI_WALK_FIELD_INVALID;
    ui_walk_step_key = UI_WALK_FIELD_INVALID;
  }

  UI_DrawSportActionButton(UI_CurrentSportAccent());
  UI_UpdateSportTimer(now_ms, 1U);
  UI_UpdateWalkDataDisplay(1U);
  UI_UpdateSportHealthDisplay(now_ms, Gh3018GoodixHrSpo2_GetSnapshot(), 1U);
}

static void UI_StopExercise(uint32_t now_ms)
{
  UBYTE sport = UI_CurrentSportIndex();

  if (exercise_running[sport] != 0U)
  {
    exercise_elapsed_seconds[sport] = UI_ExerciseElapsedSeconds(now_ms);
    exercise_running[sport] = 0U;
    if (sport == 0U)
    {
      WalkMetrics_Stop(&walk_metrics_state);
    }
  }

  hr_ui_state[sport] = HR_UI_IDLE;
  ui_sport_health_last_draw_tick = 0U;
  (void)Gh3018GoodixHrSpo2_SetManualWear(0U);
  (void)Gh3018GoodixHrSpo2_ResetPpgBpm();
  (void)Gh3018GoodixHrSpo2_Stop();
  gh3018_last_poll_tick = now_ms;

  UI_DrawSportActionButton(UI_CurrentSportAccent());
  UI_UpdateSportTimer(now_ms, 1U);
  UI_UpdateWalkDataDisplay(1U);
  UI_UpdateSportHealthDisplay(now_ms, Gh3018GoodixHrSpo2_GetSnapshot(), 1U);
}

static void UI_HandleSportAction(
    uint32_t now_ms,
    const Gh3018GoodixHrSpo2Snapshot *snapshot)
{
  UBYTE sport = UI_CurrentSportIndex();

  (void)snapshot;

  if ((hr_ui_state[sport] == HR_UI_RUNNING) ||
      (exercise_running[sport] != 0U))
  {
    UI_StopExercise(now_ms);
    return;
  }

  UI_StartExercise(now_ms);
}

static void UI_ShowExitConfirm(UIPage target)
{
  UWORD panel = LCD_RGB565(18U, 28U, 42U);
  UWORD text = LCD_RGB565(238U, 244U, 255U);
  UWORD yes = LCD_RGB565(63U, 212U, 122U);
  UWORD no = LCD_RGB565(255U, 106U, 61U);
  UWORD button_text = LCD_RGB565(5U, 12U, 18U);

  ui_exit_confirm_target = target;
  ui_exit_confirm_visible = 1U;
  LCD_FillBox(20U, 68U, 200U, 140U, LCD_COLOR_BLACK);
  LCD_FillBox(24U, 72U, 192U, 132U, panel);
  LCD_DrawCenteredTextInRectTransparent(24U, 96U, 192U, "END SPORT", text, 2U);
  LCD_FillBox(42U, 152U, 72U, 38U, yes);
  LCD_FillBox(126U, 152U, 72U, 38U, no);
  LCD_DrawCenteredTextInRectTransparent(42U, 160U, 72U, "YES", button_text, 2U);
  LCD_DrawCenteredTextInRectTransparent(126U, 160U, 72U, "NO", button_text, 2U);
}

static void UI_HideExitConfirm(void)
{
  ui_exit_confirm_visible = 0U;
  UI_ShowPage(ui_page);
}

static void UI_ConfirmExerciseExit(uint32_t now_ms)
{
  UIPage target = ui_exit_confirm_target;

  ui_exit_confirm_visible = 0U;
  if (exercise_running[UI_CurrentSportIndex()] != 0U)
  {
    UI_StopExercise(now_ms);
  }
  UI_GotoPage(target);
}

static void UI_DrawSportActionButton(UWORD accent)
{
  UWORD bg = LCD_RGB565(5U, 13U, 26U);
  const char *label = "START";
  UBYTE sport = UI_CurrentSportIndex();

  if ((hr_ui_state[sport] == HR_UI_RUNNING) ||
      (exercise_running[sport] != 0U))
  {
    label = "STOP";
  }

  LCD_FillBox(52U, 248U, 136U, 28U, accent);
  LCD_DrawCenteredTextInRect(52U,
                             254U,
                             136U,
                             label,
                             bg,
                             accent,
                             2U);
}

static void UI_UpdateSportHealthDisplay(
    uint32_t now_ms,
    const Gh3018GoodixHrSpo2Snapshot *snapshot,
    UBYTE force)
{
  char hr_text[12];
  char spo2_text[8];
  UBYTE sport;
  UBYTE bpm = 0U;
  UBYTE worn = 0U;
  UWORD bg = LCD_RGB565(5U, 13U, 26U);
  UWORD heart = LCD_RGB565(130U, 18U, 18U);
  UWORD stat_text = LCD_RGB565(238U, 244U, 255U);
  UWORD stat_label = LCD_RGB565(142U, 158U, 178U);
  UWORD box1 = LCD_RGB565(16U, 91U, 118U);

  if ((UI_PageIsDetail(ui_page) == 0U) ||
      (ui_exit_confirm_visible != 0U))
  {
    return;
  }

  if ((force == 0U) &&
      ((now_ms - ui_sport_health_last_draw_tick) <
       HR_SPORT_HEALTH_REFRESH_MS))
  {
    return;
  }

  sport = UI_CurrentSportIndex();
  worn = ((hr_ui_state[sport] == HR_UI_RUNNING) ||
          (exercise_running[sport] != 0U)) ? 1U : 0U;

  if ((hr_ui_state[sport] == HR_UI_RUNNING) &&
      (worn != 0U))
  {
    bpm = UI_SelectDisplayBpm(snapshot);
    if (bpm != 0U)
    {
      (void)snprintf(hr_text,
                     sizeof(hr_text),
                     "HR %u",
                     (unsigned int)bpm);
    }
    else
    {
      (void)snprintf(hr_text, sizeof(hr_text), "HR --");
    }

    if ((snapshot != NULL) &&
        (snapshot->spo2Valid != 0U) &&
        ((snapshot->spo2 == 98U) || (snapshot->spo2 == 99U)))
    {
      (void)snprintf(spo2_text,
                     sizeof(spo2_text),
                     "%u%%",
                     (unsigned int)snapshot->spo2);
    }
    else
    {
      (void)snprintf(spo2_text, sizeof(spo2_text), "--%%");
    }
  }
  else
  {
    (void)snprintf(hr_text, sizeof(hr_text), "HR --");
    (void)snprintf(spo2_text, sizeof(spo2_text), "--%%");
  }

  LCD_FillBox(152U, 14U, 82U, 18U, bg);
  LCD_DrawText(157U, 18U, hr_text, heart, bg, 1U);

  LCD_FillBox(124U, 110U, 104U, 58U, box1);
  LCD_DrawText(132U, 119U, spo2_text, stat_text, box1, 2U);
  LCD_DrawText(132U, 149U, "SPO2", stat_label, box1, 1U);
  ui_sport_health_last_draw_tick = now_ms;
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

  if (UIAssets_DrawBackground(UI_ASSET_WATCH_BG) == 0U)
  {
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
  }

  LCD_DrawTextTransparent(18U, 18U, "BUAA TEAM", LCD_COLOR_WHITE, 2U);
  UI_UpdateHomeClock(1U);
  UI_UpdateHomeBattery(1U);
}

static void UI_ShowSportMenu(UBYTE selected)
{
  UWORD bg = LCD_RGB565(8U, 18U, 28U);
  UWORD accent = LCD_RGB565(63U, 212U, 122U);
  UWORD title = LCD_COLOR_BLACK;
  UWORD hint = LCD_RGB565(28U, 38U, 48U);
  UWORD card_title = LCD_RGB565(245U, 248U, 255U);
  UWORD card0 = LCD_RGB565(24U, 78U, 66U);
  UWORD card1 = LCD_RGB565(106U, 51U, 38U);
  UWORD card2 = LCD_RGB565(48U, 58U, 125U);
  UWORD card_text = LCD_RGB565(190U, 206U, 222U);

  (void)selected;

  if (UIAssets_DrawBackground(UI_ASSET_NAV_BG) == 0U)
  {
    LCD_FillScreenByRows(bg);
  }
  LCD_FillBox(0U, 0U, 240U, 5U, accent);
  LCD_DrawTextTransparent(18U, 18U, "SPORT", title, 2U);
  LCD_DrawTextTransparent(18U, 44U, "TAP MODE", hint, 1U);

  LCD_FillBox(18U, 70U, 204U, 44U, card0);
  LCD_DrawText(30U, 78U, "WALK", card_title, card0, 2U);
  LCD_DrawText(142U, 88U, "START", card_text, card0, 1U);

  LCD_FillBox(18U, 128U, 204U, 44U, card1);
  LCD_DrawText(30U, 136U, "RUN", card_title, card1, 2U);
  LCD_DrawText(142U, 146U, "START", card_text, card1, 1U);

  LCD_FillBox(18U, 186U, 204U, 44U, card2);
  LCD_DrawText(30U, 194U, "ROPE", card_title, card2, 2U);
  LCD_DrawText(142U, 204U, "START", card_text, card2, 1U);

}

static void UI_DrawSportDetail(const char* title_text, UWORD accent, const char* main_value, const char* main_label,
                               const char* v0, const char* v1, const char* v2, const char* v3, const char* v4,
                               const char* l0, const char* l1, const char* l2, const char* l3, const char* l4)
{
  UWORD bg = LCD_RGB565(5U, 13U, 26U);
  UWORD title = LCD_COLOR_BLACK;
  UWORD heart = LCD_RGB565(130U, 18U, 18U);
  UWORD stat_text = LCD_RGB565(238U, 244U, 255U);
  UWORD stat_label = LCD_RGB565(142U, 158U, 178U);
  UWORD box0 = LCD_RGB565(18U, 44U, 82U);
  UWORD box1 = LCD_RGB565(16U, 91U, 118U);
  UWORD box2 = LCD_RGB565(22U, 104U, 63U);
  UWORD box3 = LCD_RGB565(128U, 72U, 29U);
  UWORD box4 = LCD_RGB565(70U, 70U, 143U);
  uint32_t now_ms = HAL_GetTick();

  (void)v0;
  (void)v1;

  if (UIAssets_DrawBackground(UI_ASSET_SPORT_BG) == 0U)
  {
    LCD_FillScreenByRows(bg);
  }
  LCD_FillBox(0U, 0U, 240U, 5U, accent);
  LCD_DrawTextTransparent(12U, 14U, title_text, title, 2U);
  LCD_FillBox(152U, 14U, 82U, 18U, bg);
  LCD_DrawText(157U, 18U, "HR --", heart, bg, 1U);
  UI_DrawSportMainField(main_value, main_label);

  LCD_FillBox(12U, 110U, 104U, 58U, box0);
  UI_DrawSportTimeField(UI_ExerciseElapsedSeconds(now_ms));
  LCD_DrawText(20U, 149U, l0, stat_label, box0, 1U);

  LCD_FillBox(124U, 110U, 104U, 58U, box1);
  LCD_DrawText(132U, 119U, "--%", stat_text, box1, 2U);
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

  UI_DrawSportActionButton(accent);
  ui_sport_health_last_draw_tick = 0U;
  UI_UpdateSportHealthDisplay(
      now_ms,
      Gh3018GoodixHrSpo2_GetSnapshot(),
      1U);
}

static void UI_ShowHrSpo2Display(const Gh3018GoodixHrSpo2Snapshot *snapshot)
{
  UWORD bg = LCD_RGB565(5U, 13U, 26U);
  UWORD accent = LCD_RGB565(63U, 212U, 122U);
  UWORD title = LCD_RGB565(238U, 244U, 255U);
  UWORD muted = LCD_RGB565(142U, 158U, 178U);

  LCD_FillScreenByRows(bg);
  LCD_FillBox(0U, 0U, 240U, 5U, accent);
  LCD_DrawTextTransparent(14U, 14U, "GH3018 DIAG", title, 2U);
  LCD_DrawTextTransparent(14U, 40U, "GREEN RAW WEAR CHECK", muted, 1U);
  ui_hrspo2_last_draw_tick = 0U;
  UI_UpdateHrSpo2Display(HAL_GetTick(), snapshot, 1U);
}

static void UI_UpdateHrSpo2Display(
    uint32_t now_ms,
    const Gh3018GoodixHrSpo2Snapshot *snapshot,
    UBYTE force)
{
  char line[48];
  char bpm_text[12];
  char spo2_text[12];
  int32_t dark_delta;
  UWORD bg = LCD_RGB565(5U, 13U, 26U);
  UWORD panel = LCD_RGB565(18U, 44U, 82U);
  UWORD panel2 = LCD_RGB565(16U, 91U, 118U);
  UWORD stat_text = LCD_RGB565(238U, 244U, 255U);
  UWORD label = LCD_RGB565(142U, 158U, 178U);
  UWORD accent = LCD_RGB565(63U, 212U, 122U);
  UWORD warn = LCD_RGB565(255U, 196U, 87U);

  if (snapshot == NULL)
  {
    LCD_FillBox(14U, 70U, 212U, 32U, bg);
    LCD_DrawTextTransparent(16U, 78U, "WAIT SENSOR INIT", warn, 1U);
    return;
  }

  if ((force == 0U) &&
      ((now_ms - ui_hrspo2_last_draw_tick) < GH3018_HRSPO2_UI_REFRESH_MS))
  {
    return;
  }

  dark_delta = -snapshot->ppgWearDcDelta;

  if (snapshot->heartRateValid != 0U)
  {
    (void)snprintf(bpm_text, sizeof(bpm_text), "%uBPM",
                   (unsigned int)snapshot->heartRate);
  }
  else
  {
    (void)snprintf(bpm_text, sizeof(bpm_text), "--BPM");
  }

  if (snapshot->spo2Valid != 0U)
  {
    (void)snprintf(spo2_text, sizeof(spo2_text), "%u%%",
                   (unsigned int)snapshot->spo2);
  }
  else
  {
    (void)snprintf(spo2_text, sizeof(spo2_text), "--%%");
  }

  LCD_FillBox(14U, 64U, 100U, 46U, panel);
  LCD_DrawText(22U, 72U, "PPG BPM", label, panel, 1U);
  LCD_DrawText(22U, 90U, bpm_text, stat_text, panel, 2U);

  LCD_FillBox(126U, 64U, 100U, 46U, panel2);
  LCD_DrawText(134U, 72U, "SPO2 SIM", label, panel2, 1U);
  LCD_DrawText(134U, 90U, spo2_text, stat_text, panel2, 2U);

  LCD_FillBox(14U, 120U, 212U, 156U, bg);
  (void)snprintf(line,
                 sizeof(line),
                 "ST %u ACT %u W %u/%u",
                 (unsigned int)snapshot->status,
                 (unsigned int)snapshot->measurementActive,
                 (unsigned int)snapshot->wearingState,
                 (unsigned int)snapshot->ppgWearingState);
  LCD_DrawTextTransparent(16U, 122U, line, stat_text, 1U);

  (void)snprintf(line,
                 sizeof(line),
                 "RAW %ld NZ %lu",
                 (long)snapshot->rawPpg0,
                 (unsigned long)snapshot->rawNonzeroCount);
  LCD_DrawTextTransparent(16U, 140U, line, accent, 1U);

  (void)snprintf(line,
                 sizeof(line),
                 "EMPTY %ld",
                 (long)snapshot->ppgWearEmptyDc);
  LCD_DrawTextTransparent(16U, 158U, line, stat_text, 1U);

  (void)snprintf(line,
                 sizeof(line),
                 "DARK %ld TH %lu",
                 (long)dark_delta,
                 (unsigned long)snapshot->ppgWearDcThreshold);
  LCD_DrawTextTransparent(16U, 176U, line, warn, 1U);

  (void)snprintf(line,
                 sizeof(line),
                 "SCORE %u RSN %u AGE %lu",
                 (unsigned int)snapshot->ppgWearScore,
                 (unsigned int)snapshot->ppgWearReason,
                 (unsigned long)snapshot->ppgRawAgeMs);
  LCD_DrawTextTransparent(16U, 194U, line, stat_text, 1U);

  (void)snprintf(line,
                 sizeof(line),
                 "AC %lu Q %u INT %lu",
                 (unsigned long)snapshot->ppgAcRange,
                 (unsigned int)snapshot->ppgSignalQuality,
                 (unsigned long)snapshot->ppgIntervalAcceptedCount);
  LCD_DrawTextTransparent(16U, 212U, line, label, 1U);

  (void)snprintf(line,
                 sizeof(line),
                 "ACC %lu Z %lu J %lu",
                 (unsigned long)snapshot->ppgRawAcceptedCount,
                 (unsigned long)snapshot->ppgRawRejectedZeroCount,
                 (unsigned long)snapshot->ppgRawRejectedJumpCount);
  LCD_DrawTextTransparent(16U, 230U, line, label, 1U);

  (void)snprintf(line,
                 sizeof(line),
                 "I2C %lu/%lu/%lu CUR %d",
                 (unsigned long)snapshot->addressNackCount,
                 (unsigned long)snapshot->dataNackCount,
                 (unsigned long)snapshot->sclTimeoutCount,
                 (int)snapshot->led0CurrentX10);
  LCD_DrawTextTransparent(16U, 248U, line, label, 1U);

  ui_hrspo2_last_draw_tick = now_ms;
}

static void UI_ShowLock(void)
{
  LCD_FillScreenByRows(LCD_COLOR_BLACK);
}

static void UI_ShowPage(UIPage page)
{
  switch (page)
  {
  case UI_PAGE_HOME:
    UI_ShowHome();
    break;
  case UI_PAGE_NAV:
    UI_ShowSportMenu(selected_sport);
    break;
  case UI_PAGE_WALK:
    UI_DrawSportDetail("WALK", LCD_RGB565(63U, 212U, 122U), "0.00", "KM",
                       "00:00", "--%", "0.0", "0.0", "0",
                       "TIME", "SPO2", "NOW", "AVG", "STEP");
    UI_UpdateWalkDataDisplay(1U);
    break;
  case UI_PAGE_RUN:
    UI_DrawSportDetail("RUN", LCD_RGB565(255U, 106U, 61U), "4.32", "KM",
                       "00:26", "--%", "3.6", "2.9", "84",
                       "TIME", "SPO2", "NOW", "AVG", "CAD");
    break;
  case UI_PAGE_ROPE:
    UI_DrawSportDetail("ROPE", LCD_RGB565(108U, 140U, 255U), "860", "COUNT",
                       "00:12", "--%", "72", "68", "95",
                       "TIME", "SPO2", "NOW", "AVG", "KCAL");
    break;
  case UI_PAGE_LOCK:
  default:
    UI_ShowLock();
    break;
  }
}

static UIPage UI_SelectedSportPage(void)
{
  if (selected_sport == 0U)
  {
    return UI_PAGE_WALK;
  }
  if (selected_sport == 1U)
  {
    return UI_PAGE_RUN;
  }
  return UI_PAGE_ROPE;
}

static void UI_GotoPage(UIPage page)
{
  if (page == UI_PAGE_WALK)
  {
    selected_sport = 0U;
  }
  else if (page == UI_PAGE_RUN)
  {
    selected_sport = 1U;
  }
  else if (page == UI_PAGE_ROPE)
  {
    selected_sport = 2U;
  }

  if (page != ui_page)
  {
    ui_page = page;
    UI_ShowPage(ui_page);
  }
  else
  {
    return;
  }
}

static UIPage UI_NextSwipePage(UIPage page)
{
  if (page == UI_PAGE_HOME)
  {
    return UI_PAGE_NAV;
  }
  if (page == UI_PAGE_NAV)
  {
    return UI_PAGE_WALK;
  }
  if (page == UI_PAGE_WALK)
  {
    return UI_PAGE_RUN;
  }
  if (page == UI_PAGE_RUN)
  {
    return UI_PAGE_ROPE;
  }
  return UI_PAGE_HOME;
}

static UIPage UI_PrevSwipePage(UIPage page)
{
  if (page == UI_PAGE_HOME)
  {
    return UI_PAGE_ROPE;
  }
  if (page == UI_PAGE_ROPE)
  {
    return UI_PAGE_RUN;
  }
  if (page == UI_PAGE_RUN)
  {
    return UI_PAGE_WALK;
  }
  if (page == UI_PAGE_WALK)
  {
    return UI_PAGE_NAV;
  }
  return UI_PAGE_HOME;
}

static UBYTE UI_SportAtPoint(UWORD x, UWORD y, UBYTE* sport)
{
  (void)x;

  if (sport == NULL)
  {
    return 0U;
  }

  if ((y >= 64U) && (y < 122U))
  {
    *sport = 0U;
    return 1U;
  }
  if ((y >= 122U) && (y < 180U))
  {
    *sport = 1U;
    return 1U;
  }
  if ((y >= 180U) && (y < 238U))
  {
    *sport = 2U;
    return 1U;
  }

  return 0U;
}

static UBYTE UI_UseCoordinateRelease(uint32_t press_ms)
{
  const uint32_t tap_max_ms = 520U;
  const int16_t swipe_min_x = 45;
  const int16_t swipe_max_y = 85;
  int16_t dx;
  int16_t dy;
  UBYTE sport = 0U;

  if (ui_exit_confirm_visible != 0U)
  {
    if ((press_ms <= tap_max_ms) && (touch_last_has_xy != 0U))
    {
      if ((touch_last_x >= 42U) && (touch_last_x <= 114U) &&
          (touch_last_y >= 152U) && (touch_last_y <= 190U))
      {
        UI_ConfirmExerciseExit(HAL_GetTick());
      }
      else if ((touch_last_x >= 126U) && (touch_last_x <= 198U) &&
               (touch_last_y >= 152U) && (touch_last_y <= 190U))
      {
        UI_HideExitConfirm();
      }
    }
    return 1U;
  }

  if (touch_swipe_consumed != 0U)
  {
    return 1U;
  }

  if ((touch_down_has_xy != 0U) && (touch_last_has_xy != 0U))
  {
    dx = (int16_t)touch_last_x - (int16_t)touch_down_x;
    dy = (int16_t)touch_last_y - (int16_t)touch_down_y;

    if ((dx <= -swipe_min_x) && (dy > -swipe_max_y) && (dy < swipe_max_y))
    {
      UIPage target = UI_NextSwipePage(touch_down_page);
      if ((UI_PageIsDetail(touch_down_page) != 0U) &&
          (exercise_running[UI_CurrentSportIndex()] != 0U))
      {
        UI_ShowExitConfirm(target);
      }
      else
      {
        UI_GotoPage(target);
      }
      return 1U;
    }

    if ((dx >= swipe_min_x) && (dy > -swipe_max_y) && (dy < swipe_max_y))
    {
      UIPage target = UI_PrevSwipePage(touch_down_page);
      if ((UI_PageIsDetail(touch_down_page) != 0U) &&
          (exercise_running[UI_CurrentSportIndex()] != 0U))
      {
        UI_ShowExitConfirm(target);
      }
      else
      {
        UI_GotoPage(target);
      }
      return 1U;
    }
  }

  if ((ui_page == UI_PAGE_NAV) && (press_ms <= tap_max_ms) &&
      (((touch_down_has_xy != 0U) && (UI_SportAtPoint(touch_down_x, touch_down_y, &sport) != 0U)) ||
       ((touch_last_has_xy != 0U) && (UI_SportAtPoint(touch_last_x, touch_last_y, &sport) != 0U))))
  {
    selected_sport = sport;
    UI_GotoPage(UI_SelectedSportPage());
    return 1U;
  }

  if (UI_PageIsDetail(ui_page) != 0U)
  {
    if ((press_ms <= tap_max_ms) &&
        (((touch_down_has_xy != 0U) && (touch_down_y >= 244U)) ||
         ((touch_last_has_xy != 0U) && (touch_last_y >= 244U))))
    {
      UI_HandleSportAction(
          HAL_GetTick(),
          Gh3018GoodixHrSpo2_GetSnapshot());
      return 1U;
    }
  }

  return 0U;
}

static UBYTE UI_TryRealtimeSwipe(uint32_t now_ms)
{
  const int16_t swipe_min_x = 58;
  const int16_t swipe_max_y = 90;
  int16_t dx;
  int16_t dy;

  if ((touch_swipe_consumed != 0U) ||
      (ui_exit_confirm_visible != 0U) ||
      (ui_page == UI_PAGE_LOCK) ||
      (touch_down_has_xy == 0U) ||
      (touch_last_has_xy == 0U))
  {
    return 0U;
  }

  dx = (int16_t)touch_last_x - (int16_t)touch_down_x;
  dy = (int16_t)touch_last_y - (int16_t)touch_down_y;

  if ((dy <= -swipe_max_y) || (dy >= swipe_max_y))
  {
    return 0U;
  }

  if (dx <= -swipe_min_x)
  {
    touch_swipe_consumed = 1U;
    last_touch_ms = now_ms;
    if ((UI_PageIsDetail(touch_down_page) != 0U) &&
        (exercise_running[UI_CurrentSportIndex()] != 0U))
    {
      UI_ShowExitConfirm(UI_NextSwipePage(touch_down_page));
    }
    else
    {
      UI_GotoPage(UI_NextSwipePage(touch_down_page));
    }
    return 1U;
  }

  if (dx >= swipe_min_x)
  {
    touch_swipe_consumed = 1U;
    last_touch_ms = now_ms;
    if ((UI_PageIsDetail(touch_down_page) != 0U) &&
        (exercise_running[UI_CurrentSportIndex()] != 0U))
    {
      UI_ShowExitConfirm(UI_PrevSwipePage(touch_down_page));
    }
    else
    {
      UI_GotoPage(UI_PrevSwipePage(touch_down_page));
    }
    return 1U;
  }

  return 0U;
}

static UBYTE UI_PageIsDetail(UIPage page)
{
  return ((page == UI_PAGE_WALK) || (page == UI_PAGE_RUN) || (page == UI_PAGE_ROPE)) ? 1U : 0U;
}

static const Gh3018GoodixHrSpo2Snapshot *Gh3018GoodixHrSpo2Service_Update(
    uint32_t now_ms,
    const Gh3018GoodixHrSpo2Snapshot *snapshot)
{
  if (snapshot == NULL)
  {
    snapshot = Gh3018GoodixHrSpo2_GetSnapshot();
  }

  if ((snapshot->measurementActive != 0U) &&
      ((now_ms - gh3018_last_poll_tick) >=
       GH3018_HRSPO2_POLL_INTERVAL_MS))
  {
    gh3018_last_poll_tick = now_ms;
    snapshot = Gh3018GoodixHrSpo2_Poll();
  }

  return snapshot;
}

static void UI_LockScreen(void)
{
  screen_locked = 0U;
  ui_page = UI_PAGE_LOCK;
  lock_enter_ms = HAL_GetTick();
  lock_touch_candidate_ms = 0U;
  touch_suppress_until_release = 0U;
  touch_pressed_prev = 0U;
  UI_ShowPage(UI_PAGE_LOCK);
  LCD_1IN69_SetBackLight(0U);
}

static void UI_UnlockToHome(uint32_t now_ms)
{
  screen_locked = 0U;
  ui_page = UI_PAGE_HOME;
  last_touch_ms = now_ms;
  touch_down_ms = now_ms;
  touch_suppress_until_release = 1U;
  LCD_1IN69_SetBackLight(1000U);
  UI_ShowPage(ui_page);
}

static void UI_HandleTouchPressed(uint32_t now_ms, const TouchSample* sample)
{
  touch_down_ms = now_ms;
  touch_down_page = ui_page;
  touch_down_has_xy = 0U;
  touch_last_has_xy = 0U;
  touch_swipe_consumed = 0U;

  if ((sample != NULL) && (sample->has_xy != 0U))
  {
    touch_down_has_xy = 1U;
    touch_down_x = sample->x;
    touch_down_y = sample->y;
    touch_last_has_xy = 1U;
    touch_last_x = sample->x;
    touch_last_y = sample->y;
  }

  if (screen_locked != 0U)
  {
    UI_UnlockToHome(now_ms);
    return;
  }

  if (ui_page == UI_PAGE_LOCK)
  {
    UI_UnlockToHome(now_ms);
    return;
  }

  last_touch_ms = now_ms;
}

static void UI_HandleTouchReleased(uint32_t now_ms, uint32_t press_ms)
{
  const uint32_t long_press_ms = 650U;

  if (screen_locked != 0U)
  {
    return;
  }

  last_touch_ms = now_ms;

  if (UI_UseCoordinateRelease(press_ms) != 0U)
  {
    return;
  }

  if (ui_page == UI_PAGE_HOME)
  {
    if (press_ms >= long_press_ms)
    {
      UI_LockScreen();
    }
    else
    {
      selected_sport = 0U;
      ui_page = UI_PAGE_NAV;
      UI_ShowPage(ui_page);
    }
  }
  else if (ui_page == UI_PAGE_NAV)
  {
    if (press_ms >= long_press_ms)
    {
      ui_page = UI_SelectedSportPage();
      UI_ShowPage(ui_page);
    }
  }
  else if (UI_PageIsDetail(ui_page) != 0U)
  {
    if (press_ms >= long_press_ms)
    {
      ui_page = UI_PAGE_NAV;
      UI_ShowPage(ui_page);
    }
  }
}

static TouchSample Touch_ReadSample(void)
{
  TouchSample sample = {0U};
  TouchControllerSample controller_sample = TouchController_Sample(HAL_GetTick());

  if (controller_sample.pressed != 0U)
  {
    sample.pressed = 1U;
    sample.has_xy = 1U;
    sample.x = controller_sample.x;
    sample.y = controller_sample.y;
  }

  return sample;
}

void HAL_GPIO_EXTI_Falling_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == TP_INT_Pin)
  {
    TouchController_NotifyInterrupt();
  }
  else if (GPIO_Pin == MPU6050_INT_Pin)
  {
    /* MPU6050 samples are polled in the main loop. */
  }
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
  MX_ADC1_Init();
  MX_I2C2_Init();
  MX_I2C3_Init();
#if LCD_USE_HAL_SPI
  MX_SPI1_Init();
#endif
  MX_USART1_Debug_Init();
  /* USER CODE BEGIN 2 */
  if (DEV_Module_Init() != 0)
  {
    Error_Handler();
  }
  IMU_Sensor_Init();
  WalkMetrics_Reset(&walk_metrics_state);
  memset(&walk_metrics_output, 0, sizeof(walk_metrics_output));
  BatteryMonitor_Init(&hadc1);
  (void)BatteryMonitor_Update(HAL_GetTick());

  LCD_1IN69_SetBackLight(1000U);
  LCD_1IN69_Init(VERTICAL);
  TouchController_Init();
#if UI_ASSET_PROGRAMMER
  {
    uint32_t written_bytes = 0UL;
    LCD_1IN69_FillRect_FastStatic(0U, 0U, (UWORD)(LCD_1IN69.WIDTH - 1U), (UWORD)(LCD_1IN69.HEIGHT - 1U), LCD_COLOR_WHITE);
    LCD_DrawCenteredText("WRITE FLASH", 96U, LCD_COLOR_BLACK, LCD_COLOR_WHITE, 3U);
    LCD_DrawCenteredText("WAIT", 142U, LCD_COLOR_BLACK, LCD_COLOR_WHITE, 2U);
    if (UIAssetProgrammer_Run(&written_bytes) != 0U)
    {
      LCD_1IN69_FillRect_FastStatic(0U, 0U, (UWORD)(LCD_1IN69.WIDTH - 1U), (UWORD)(LCD_1IN69.HEIGHT - 1U), LCD_COLOR_WHITE);
      LCD_DrawCenteredText("FLASH OK", 104U, LCD_COLOR_BLACK, LCD_COLOR_WHITE, 3U);
      LCD_DrawCenteredText("POWER HOLD", 150U, LCD_COLOR_BLACK, LCD_COLOR_WHITE, 2U);
    }
    else
    {
      LCD_1IN69_FillRect_FastStatic(0U, 0U, (UWORD)(LCD_1IN69.WIDTH - 1U), (UWORD)(LCD_1IN69.HEIGHT - 1U), LCD_COLOR_WHITE);
      LCD_DrawCenteredText("FLASH FAIL", 104U, LCD_COLOR_BLACK, LCD_COLOR_WHITE, 3U);
      LCD_DrawCenteredText("CHECK ID", 150U, LCD_COLOR_BLACK, LCD_COLOR_WHITE, 2U);
    }
    (void)written_bytes;
    while (1)
    {
      HAL_Delay(1000U);
    }
  }
#endif
  ui_page = UI_PAGE_HOME;
  selected_sport = 0U;
  screen_locked = 0U;
  touch_pressed_prev = 0U;
  touch_suppress_until_release = 1U;
  UI_ClockInit(HAL_GetTick());
  last_touch_ms = HAL_GetTick();
  touch_last_active_ms = last_touch_ms;
  touch_down_ms = last_touch_ms;
  if (GH3018_DIAGNOSTIC_SCREEN_ENABLE != 0U)
  {
    UI_ShowHrSpo2Display(NULL);
  }
  else
  {
    (void)UIAssets_Init();
    LCD_ShowHelloBuaa();
    {
      uint32_t hello_start_ms = HAL_GetTick();
      (void)UIAssets_Preload();
      while ((HAL_GetTick() - hello_start_ms) < 5000UL)
      {
        HAL_Delay(20U);
      }
    }
    UI_ShowPage(ui_page);
  }

  printf("\r\nU575 wzx UI with START-gated GH3018 green PPG ready\r\n");
  const Gh3018GoodixHrSpo2Snapshot *hrspo2 = Gh3018GoodixHrSpo2_Init();
  PrintHrSpo2Snapshot("init", hrspo2);
  if (GH3018_DIAGNOSTIC_SCREEN_ENABLE != 0U)
  {
    hrspo2 = Gh3018GoodixHrSpo2_Start();
    UI_ShowHrSpo2Display(hrspo2);
    PrintHrSpo2Snapshot("diagnostic-start", hrspo2);
  }
  else
  {
    PrintHrSpo2Snapshot("ready", hrspo2);
  }
  uint32_t lastLogTick = HAL_GetTick();
  const BatteryMonitorSnapshot *battery = BatteryMonitor_GetSnapshot();
  Gh3018GoodixHrSpo2Status lastStatus = hrspo2->status;
  uint32_t lastResultRefreshCount = hrspo2->resultRefreshCount;
  uint8_t lastPpgHeartRate = hrspo2->ppgHeartRate;
  uint8_t lastPpgHeartRateValid = hrspo2->ppgHeartRateValid;
  uint8_t lastWearingState = hrspo2->wearingState;
  uint8_t lastSpo2 = hrspo2->spo2;
  uint8_t lastSpo2Valid = hrspo2->spo2Valid;
  int8_t lastCalcRet = hrspo2->hbdCalcRet;
  int8_t lastSetCurrentRet = hrspo2->hbdSetCurrentRet;
  int16_t lastLed0CurrentX10 = hrspo2->led0CurrentX10;
  int16_t lastLed1CurrentX10 = hrspo2->led1CurrentX10;
  gh3018_last_poll_tick = HAL_GetTick();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    uint32_t now_ms = HAL_GetTick();
    TouchSample touch_sample = Touch_ReadSample();
    UBYTE raw_touch_pressed = touch_sample.pressed;
    UBYTE touch_pressed = (touch_sample.has_xy != 0U) ? 1U : 0U;
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint8_t shouldLog = 0U;
    if (GH3018_DIAGNOSTIC_SCREEN_ENABLE != 0U)
    {
      battery = BatteryMonitor_Update(now_ms);
      hrspo2 = Gh3018GoodixHrSpo2Service_Update(now_ms, hrspo2);
      if ((hrspo2->status != lastStatus) ||
          (hrspo2->resultRefreshCount != lastResultRefreshCount) ||
          (hrspo2->ppgHeartRate != lastPpgHeartRate) ||
          (hrspo2->ppgHeartRateValid != lastPpgHeartRateValid) ||
          (hrspo2->wearingState != lastWearingState) ||
          (hrspo2->spo2 != lastSpo2) ||
          (hrspo2->spo2Valid != lastSpo2Valid) ||
          (hrspo2->hbdCalcRet != lastCalcRet) ||
          (hrspo2->hbdSetCurrentRet != lastSetCurrentRet) ||
          (hrspo2->led0CurrentX10 != lastLed0CurrentX10) ||
          (hrspo2->led1CurrentX10 != lastLed1CurrentX10))
      {
        shouldLog = 1U;
      }

      UI_UpdateHrSpo2Display(now_ms, hrspo2, shouldLog);

      if ((shouldLog != 0U) || ((now_ms - lastLogTick) >= 1000U))
      {
        PrintHrSpo2Snapshot("hrspo2", hrspo2);
        PrintBatterySnapshot("battery", battery);
        lastStatus = hrspo2->status;
        lastResultRefreshCount = hrspo2->resultRefreshCount;
        lastPpgHeartRate = hrspo2->ppgHeartRate;
        lastPpgHeartRateValid = hrspo2->ppgHeartRateValid;
        lastWearingState = hrspo2->wearingState;
        lastSpo2 = hrspo2->spo2;
        lastSpo2Valid = hrspo2->spo2Valid;
        lastCalcRet = hrspo2->hbdCalcRet;
        lastSetCurrentRet = hrspo2->hbdSetCurrentRet;
        lastLed0CurrentX10 = hrspo2->led0CurrentX10;
        lastLed1CurrentX10 = hrspo2->led1CurrentX10;
        lastLogTick = now_ms;
      }

      HAL_Delay(20U);
      continue;
    }

    /* Keep the clock moving before lock-screen branches can continue early. */
    UI_ClockUpdate(now_ms);
    UI_UpdateWalkMetrics(now_ms);
    UI_UpdateHomeClock(0U);
    battery = BatteryMonitor_Update(now_ms);
    UI_UpdateHomeBattery(0U);
    UI_UpdateSportTimer(now_ms, 0U);
    UI_UpdateWalkDataDisplay(0U);

    hrspo2 = Gh3018GoodixHrSpo2Service_Update(now_ms, hrspo2);
    UI_UpdateSportHealthDisplay(now_ms, hrspo2, 0U);
    if ((hrspo2->status != lastStatus) ||
        (hrspo2->resultRefreshCount != lastResultRefreshCount) ||
        (hrspo2->ppgHeartRate != lastPpgHeartRate) ||
        (hrspo2->ppgHeartRateValid != lastPpgHeartRateValid) ||
        (hrspo2->wearingState != lastWearingState) ||
        (hrspo2->spo2 != lastSpo2) ||
        (hrspo2->spo2Valid != lastSpo2Valid) ||
        (hrspo2->hbdCalcRet != lastCalcRet) ||
        (hrspo2->hbdSetCurrentRet != lastSetCurrentRet) ||
        (hrspo2->led0CurrentX10 != lastLed0CurrentX10) ||
        (hrspo2->led1CurrentX10 != lastLed1CurrentX10))
    {
      shouldLog = 1U;
    }

    if ((shouldLog != 0U) || ((now_ms - lastLogTick) >= 1000U))
    {
      PrintHrSpo2Snapshot("hrspo2", hrspo2);
      PrintBatterySnapshot("battery", battery);
      lastStatus = hrspo2->status;
      lastResultRefreshCount = hrspo2->resultRefreshCount;
      lastPpgHeartRate = hrspo2->ppgHeartRate;
      lastPpgHeartRateValid = hrspo2->ppgHeartRateValid;
      lastWearingState = hrspo2->wearingState;
      lastSpo2 = hrspo2->spo2;
      lastSpo2Valid = hrspo2->spo2Valid;
      lastCalcRet = hrspo2->hbdCalcRet;
      lastSetCurrentRet = hrspo2->hbdSetCurrentRet;
      lastLed0CurrentX10 = hrspo2->led0CurrentX10;
      lastLed1CurrentX10 = hrspo2->led1CurrentX10;
      lastLogTick = now_ms;
    }

    if ((raw_touch_pressed != 0U) && (touch_sample.has_xy != 0U))
    {
      touch_last_active_ms = now_ms;
    }
    else if ((raw_touch_pressed != 0U) &&
             ((now_ms - touch_last_active_ms) >= 180U))
    {
      raw_touch_pressed = 0U;
      touch_pressed = 0U;
      touch_sample.pressed = 0U;
    }

    if ((touch_pressed != 0U) && (touch_pressed_prev == 0U))
    {
      touch_down_ms = now_ms;
      touch_down_page = ui_page;
      touch_down_has_xy = 0U;
      touch_last_has_xy = 0U;
      touch_swipe_consumed = 0U;
    }

    if ((touch_pressed != 0U) && (touch_sample.has_xy != 0U))
    {
      if (touch_down_has_xy == 0U)
      {
        touch_down_has_xy = 1U;
        touch_down_x = touch_sample.x;
        touch_down_y = touch_sample.y;
        touch_down_page = ui_page;
      }

      touch_last_has_xy = 1U;
      touch_last_x = touch_sample.x;
      touch_last_y = touch_sample.y;

      if (touch_pressed_prev != 0U)
      {
        if (UI_TryRealtimeSwipe(now_ms) != 0U)
        {
          touch_pressed_prev = 1U;
          HAL_Delay(20U);
          continue;
        }
      }
    }

    if (ui_page == UI_PAGE_LOCK)
    {
      if (((now_ms - lock_enter_ms) >= 300UL) &&
          (touch_pressed != 0U))
      {
        if (lock_touch_candidate_ms == 0U)
        {
          lock_touch_candidate_ms = now_ms;
        }
        else if ((now_ms - lock_touch_candidate_ms) >= 40U)
        {
          UI_UnlockToHome(now_ms);
          touch_pressed_prev = 1U;
          HAL_Delay(20U);
          continue;
        }
      }
      else
      {
        lock_touch_candidate_ms = 0U;
      }

      touch_pressed_prev = touch_pressed;
      HAL_Delay(20U);
      continue;
    }

    if ((screen_locked != 0U) && (raw_touch_pressed != 0U))
    {
      UI_UnlockToHome(now_ms);
      touch_pressed_prev = 0U;
      HAL_Delay(20U);
      continue;
    }

    if (touch_suppress_until_release != 0U)
    {
      if (touch_pressed == 0U)
      {
        touch_suppress_until_release = 0U;
        touch_pressed_prev = 0U;
      }
      else
      {
        touch_pressed_prev = 1U;
      }
      HAL_Delay(20U);
      continue;
    }

    if ((touch_pressed != 0U) && (touch_pressed_prev == 0U))
    {
      UI_HandleTouchPressed(now_ms, &touch_sample);
    }
    else if ((touch_pressed == 0U) && (touch_pressed_prev != 0U))
    {
      uint32_t press_ms = now_ms - touch_down_ms;
      UI_HandleTouchReleased(now_ms, press_ms);
    }

    touch_pressed_prev = touch_pressed;
    HAL_Delay(20U);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
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
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADCDAC;
  PeriphClkInit.AdcDacClockSelection = RCC_ADCDACCLKSOURCE_HSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  __HAL_RCC_ADC12_CLK_ENABLE();

  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV2;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.GainCompensation = 0U;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.LowPowerAutoPowerOff = ADC_LOW_POWER_NONE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1U;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.NbrOfDiscConversion = 1U;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.ConversionDataManagement = ADC_CONVERSIONDATA_DR;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_OVERWRITTEN;
  hadc1.Init.SamplingTimeCommon1 = ADC_SAMPLETIME_814CYCLES;
  hadc1.Init.SamplingTimeCommon2 = ADC_SAMPLETIME_814CYCLES;
  hadc1.Init.LeftBitShift = ADC_LEFTBITSHIFT_NONE;
  hadc1.Init.OversamplingMode = DISABLE;
  hadc1.Init.TriggerFrequencyMode = ADC_TRIGGER_FREQ_LOW;
  hadc1.Init.VrefProtection = ADC_VREF_PPROT_NONE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc1,
                                  ADC_CALIB_OFFSET,
                                  ADC_SINGLE_ENDED) != HAL_OK)
  {
    Error_Handler();
  }

  sConfig.Channel = ADC_CHANNEL_3;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_814CYCLES;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0U;
  sConfig.OffsetRightShift = DISABLE;
  sConfig.OffsetSignedSaturation = DISABLE;
  sConfig.OffsetSaturation = DISABLE;
  sConfig.OffsetSign = ADC_OFFSET_SIGN_NEGATIVE;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00000E14;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief I2C3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C3_Init(void)
{

  /* USER CODE BEGIN I2C3_Init 0 */

  /* USER CODE END I2C3_Init 0 */

  /* USER CODE BEGIN I2C3_Init 1 */

  /* USER CODE END I2C3_Init 1 */
  hi2c3.Instance = I2C3;
  hi2c3.Init.Timing = 0x10707DBC;
  hi2c3.Init.OwnAddress1 = 0;
  hi2c3.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c3.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c3.Init.OwnAddress2 = 0;
  hi2c3.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c3.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c3.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c3) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c3, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c3, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C3_Init 2 */

  /* USER CODE END I2C3_Init 2 */

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
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, GH3018_RSTN_Pin|GH3018_HBD_ON_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GH3018_I2C_SDA_Pin|GH3018_I2C_SCL_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, LCD_RST_Pin|LCD_CS_Pin|LCD_DC_Pin|TP_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LCD_BLK_GPIO_Port, LCD_BLK_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : GH3018_RSTN_Pin GH3018_HBD_ON_Pin */
  GPIO_InitStruct.Pin = GH3018_RSTN_Pin|GH3018_HBD_ON_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GH3018_RSTN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : GH3018_I2C_SDA_Pin GH3018_I2C_SCL_Pin */
  GPIO_InitStruct.Pin = GH3018_I2C_SDA_Pin|GH3018_I2C_SCL_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : GH3018_INT_Pin */
  GPIO_InitStruct.Pin = GH3018_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : MPU6050_INT_Pin */
  GPIO_InitStruct.Pin = MPU6050_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MPU6050_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BAT_ADC_Pin */
  GPIO_InitStruct.Pin = BAT_ADC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BAT_ADC_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_RST_Pin LCD_CS_Pin LCD_DC_Pin TP_RST_Pin */
  GPIO_InitStruct.Pin = LCD_RST_Pin|LCD_CS_Pin|LCD_DC_Pin|TP_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pin : TP_INT_Pin */
  GPIO_InitStruct.Pin = TP_INT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(TP_INT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LCD_BLK_Pin */
  GPIO_InitStruct.Pin = LCD_BLK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LCD_BLK_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI8_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI8_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

static void PrintBatterySnapshot(
    const char *tag,
    const BatteryMonitorSnapshot *snapshot)
{
  if (snapshot == NULL)
  {
    printf("[%s] battery snapshot null\r\n", tag);
    return;
  }

  printf("[%s] valid=%u raw=%lu adc_mv=%lu bat_mv=%lu percent=%u samples=%lu err=%lu status=%d\r\n",
         tag,
         (unsigned int)snapshot->valid,
         (unsigned long)snapshot->rawAdc,
         (unsigned long)snapshot->adcMillivolts,
         (unsigned long)snapshot->batteryMillivolts,
         (unsigned int)snapshot->percent,
         (unsigned long)snapshot->sampleCount,
         (unsigned long)snapshot->errorCount,
         (int)snapshot->lastStatus);
}

/* USER CODE BEGIN 4 */
static void MX_USART1_Debug_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  uint32_t uartClockHz;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();

  GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  CLEAR_BIT(USART1->CR1, USART_CR1_UE);
  USART1->CR1 = 0U;
  USART1->CR2 = 0U;
  USART1->CR3 = 0U;

  uartClockHz = HAL_RCC_GetPCLK2Freq();
  USART1->BRR = (uartClockHz + (115200U / 2U)) / 115200U;
  SET_BIT(USART1->CR1, USART_CR1_TE | USART_CR1_RE | USART_CR1_UE);
}

int __io_putchar(int ch)
{
  while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0U) {
  }
  USART1->TDR = (uint8_t)ch;
  return ch;
}

static void PrintHrSpo2Snapshot(
    const char *tag,
    const Gh3018GoodixHrSpo2Snapshot *snapshot)
{
  if (snapshot == NULL)
  {
    printf("[%s] hrspo2 snapshot null\r\n", tag);
    return;
  }

  printf("[%s] st=%s active=%u sid=%lu comm=%d soft=%d "
         "ret=%d/%d/%d/%d/%d pin=%u/%u/%u int=%u "
         "map=%u cfg_step=%u/%u cfg_x10=%d/%d led_x10=%d/%d "
         "cnt=%lu/%lu/%lu/%lu sess=%lu/%lu/%lu/%lu "
         "raw=%u fifo=%u ppg=%ld/%ld max=%lu/%lu probe=%lu nz=%lu chg=%lu empty=%lu full=%lu "
         "ppg_hr=%u/%u/v%u q=%u "
         "ppg_auto bpm=%u score=%u v%u lag=%u best=%u/%u second=%u/%u reject=%u "
         "freeze=%u drift=%ld disp=%u pend=%u "
         "smp=%lu peak=%lu ac=%lu int_ms=%u "
         "ppg_diag cand=%lu acc_int=%lu short=%lu long=%lu outlier=%lu rej_ms=%u thr=%lu "
         "ppg_filter acc=%lu zero=%lu jump=%lu resync=%lu last=%ld thr=%lu "
         "wear_dc empty=%ld delta=%ld thr=%lu "
         "ppg_wear state=%u score=%u reason=%u stable=%lu raw_age=%lu peak_age=%lu "
         "cand_age=%lu int_age=%lu int_exp=%lu pend_exp=%lu bpm_gate=%u "
         "goodix_hr=%u/%u/v%u gwear=%u spo2=%u/%u/v%u sim=%u sw=%lu next=%lu wear=%u r=%u lvl=%ld invalid=%ld "
         "irq=%lu/%lu/%lu/%lu/%lu/%lu/%lu "
         "i2c=%lu/%lu recovery=%lu nack=%lu/%lu scl_to=%lu\r\n",
         tag,
         Gh3018GoodixHrSpo2_StatusName(snapshot->status),
         (unsigned int)snapshot->measurementActive,
         (unsigned long)snapshot->sessionId,
         (int)snapshot->commStatus,
         (int)snapshot->lastSoftI2cStatus,
         (int)snapshot->hbdSimpleInitRet,
         (int)snapshot->hbdHrSpo2StartRet,
         (int)snapshot->hbdSetCurrentRet,
         (int)snapshot->hbdGetCurrentRet,
         (int)snapshot->hbdCalcRet,
         (unsigned int)snapshot->rstnLevel,
         (unsigned int)snapshot->hbdOnLevel,
         (unsigned int)snapshot->intLevel,
         (unsigned int)snapshot->intStatus,
         (unsigned int)snapshot->ledLogicMap,
         (unsigned int)snapshot->channel0CurrentStep,
         (unsigned int)snapshot->channel1CurrentStep,
         (int)snapshot->channel0CurrentX10,
         (int)snapshot->channel1CurrentX10,
         (int)snapshot->led0CurrentX10,
         (int)snapshot->led1CurrentX10,
         (unsigned long)snapshot->pollCount,
         (unsigned long)snapshot->calcCount,
         (unsigned long)snapshot->resultRefreshCount,
         (unsigned long)snapshot->noDataCount,
         (unsigned long)snapshot->sessionPollCount,
         (unsigned long)snapshot->sessionCalcCount,
         (unsigned long)snapshot->sessionResultRefreshCount,
         (unsigned long)snapshot->sessionNoDataCount,
         (unsigned int)snapshot->rawDataLen,
         (unsigned int)snapshot->rawFifoCount,
         (long)snapshot->rawPpg0,
         (long)snapshot->rawPpg1,
         (unsigned long)snapshot->rawMaxPpg0,
         (unsigned long)snapshot->rawMaxPpg1,
         (unsigned long)snapshot->rawProbeCount,
         (unsigned long)snapshot->rawNonzeroCount,
         (unsigned long)snapshot->rawChangeCount,
         (unsigned long)snapshot->rawEmptyCount,
         (unsigned long)snapshot->rawBufferFullCount,
         (unsigned int)snapshot->ppgHeartRate,
         (unsigned int)snapshot->ppgHeartRateConfidence,
         (unsigned int)snapshot->ppgHeartRateValid,
         (unsigned int)snapshot->ppgSignalQuality,
         (unsigned int)snapshot->ppgAutoCorrBpm,
         (unsigned int)snapshot->ppgAutoCorrScore,
         (unsigned int)snapshot->ppgAutoCorrValid,
         (unsigned int)snapshot->ppgAutoCorrLag,
         (unsigned int)snapshot->ppgAutoCorrBestLag,
         (unsigned int)snapshot->ppgAutoCorrBestScore,
         (unsigned int)snapshot->ppgAutoCorrSecondLag,
         (unsigned int)snapshot->ppgAutoCorrSecondScore,
         (unsigned int)snapshot->ppgAutoCorrRejectReason,
         (unsigned int)snapshot->ppgMotionFreeze,
         (long)snapshot->ppgDcDrift1s,
         (unsigned int)snapshot->ppgDisplayedBpm,
         (unsigned int)snapshot->ppgPendingBpm,
         (unsigned long)snapshot->ppgSampleCount,
         (unsigned long)snapshot->ppgPeakCount,
         (unsigned long)snapshot->ppgAcRange,
         (unsigned int)snapshot->ppgLastIntervalMs,
         (unsigned long)snapshot->ppgCandidatePeakCount,
         (unsigned long)snapshot->ppgIntervalAcceptedCount,
         (unsigned long)snapshot->ppgIntervalTooShortCount,
         (unsigned long)snapshot->ppgIntervalTooLongCount,
         (unsigned long)snapshot->ppgIntervalRejectedOutlierCount,
         (unsigned int)snapshot->ppgLastRejectedIntervalMs,
         (unsigned long)snapshot->ppgPeakThreshold,
         (unsigned long)snapshot->ppgRawAcceptedCount,
         (unsigned long)snapshot->ppgRawRejectedZeroCount,
         (unsigned long)snapshot->ppgRawRejectedJumpCount,
         (unsigned long)snapshot->ppgRawResyncCount,
         (long)snapshot->ppgLastAcceptedRaw,
         (unsigned long)snapshot->ppgRawJumpThreshold,
         (long)snapshot->ppgWearEmptyDc,
         (long)snapshot->ppgWearDcDelta,
         (unsigned long)snapshot->ppgWearDcThreshold,
         (unsigned int)snapshot->ppgWearingState,
         (unsigned int)snapshot->ppgWearScore,
         (unsigned int)snapshot->ppgWearReason,
         (unsigned long)snapshot->ppgWearStableMs,
         (unsigned long)snapshot->ppgRawAgeMs,
         (unsigned long)snapshot->ppgPeakAgeMs,
         (unsigned long)snapshot->ppgCandidateAgeMs,
         (unsigned long)snapshot->ppgIntervalAgeMs,
         (unsigned long)snapshot->ppgIntervalExpiredCount,
         (unsigned long)snapshot->ppgPendingPeakExpiredCount,
         (unsigned int)snapshot->ppgBpmGateReason,
         (unsigned int)snapshot->goodixHeartRate,
         (unsigned int)snapshot->goodixHeartRateConfidence,
         (unsigned int)snapshot->goodixHeartRateValid,
         (unsigned int)snapshot->goodixWearingState,
         (unsigned int)snapshot->spo2,
         (unsigned int)snapshot->spo2Confidence,
         (unsigned int)snapshot->spo2Valid,
         (unsigned int)snapshot->spo2Simulated,
         (unsigned long)snapshot->spo2SimSwitchCount,
         (unsigned long)snapshot->spo2SimNextSwitchMs,
         (unsigned int)snapshot->wearingState,
         (unsigned int)snapshot->spo2RValue,
         (long)snapshot->spo2ValidLevel,
         (long)snapshot->spo2InvalidFlag,
         (unsigned long)snapshot->intChipResetCount,
         (unsigned long)snapshot->intNewDataCount,
         (unsigned long)snapshot->intFifoWatermarkCount,
         (unsigned long)snapshot->intFifoFullCount,
         (unsigned long)snapshot->intWearCount,
         (unsigned long)snapshot->intUnwearCount,
         (unsigned long)snapshot->intInvalidCount,
         (unsigned long)snapshot->i2cWriteCount,
         (unsigned long)snapshot->i2cReadCount,
         (unsigned long)snapshot->busRecoveryCount,
         (unsigned long)snapshot->addressNackCount,
         (unsigned long)snapshot->dataNackCount,
         (unsigned long)snapshot->sclTimeoutCount);
}

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
