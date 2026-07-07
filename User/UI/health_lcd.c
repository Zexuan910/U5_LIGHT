#include "health_lcd.h"

#include "LCD_1in69.h"

#include <stdio.h>

#define HEALTH_LCD_PAGE_PERIOD_MS    2000U
#define HEALTH_LCD_REFRESH_PERIOD_MS 250U

#define COLOR_BG        0x0841U
#define COLOR_PANEL     0x18E3U
#define COLOR_TEXT      0xFFFFU
#define COLOR_MUTED     0xBDF7U
#define COLOR_OK        0x07E0U
#define COLOR_WARN      0xFFE0U
#define COLOR_ERR       0xF800U
#define COLOR_WALK      0x07EFU
#define COLOR_RUN       0xFBE0U
#define COLOR_ROPE      0x6D9FU

static uint8_t s_initialized;
static uint8_t s_lastMode = 0xFFU;
static uint32_t s_lastRefreshTick;

static const uint8_t DIGIT_GLYPHS[10][5] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E},
    {0x00, 0x42, 0x7F, 0x40, 0x00},
    {0x42, 0x61, 0x51, 0x49, 0x46},
    {0x21, 0x41, 0x45, 0x4B, 0x31},
    {0x18, 0x14, 0x12, 0x7F, 0x10},
    {0x27, 0x45, 0x45, 0x45, 0x39},
    {0x3C, 0x4A, 0x49, 0x49, 0x30},
    {0x01, 0x71, 0x09, 0x05, 0x03},
    {0x36, 0x49, 0x49, 0x49, 0x36},
    {0x06, 0x49, 0x49, 0x29, 0x1E}
};

static const uint8_t LETTER_GLYPHS[26][5] = {
    {0x7E, 0x11, 0x11, 0x11, 0x7E},
    {0x7F, 0x49, 0x49, 0x49, 0x36},
    {0x3E, 0x41, 0x41, 0x41, 0x22},
    {0x7F, 0x41, 0x41, 0x22, 0x1C},
    {0x7F, 0x49, 0x49, 0x49, 0x41},
    {0x7F, 0x09, 0x09, 0x09, 0x01},
    {0x3E, 0x41, 0x49, 0x49, 0x7A},
    {0x7F, 0x08, 0x08, 0x08, 0x7F},
    {0x00, 0x41, 0x7F, 0x41, 0x00},
    {0x20, 0x40, 0x41, 0x3F, 0x01},
    {0x7F, 0x08, 0x14, 0x22, 0x41},
    {0x7F, 0x40, 0x40, 0x40, 0x40},
    {0x7F, 0x02, 0x0C, 0x02, 0x7F},
    {0x7F, 0x04, 0x08, 0x10, 0x7F},
    {0x3E, 0x41, 0x41, 0x41, 0x3E},
    {0x7F, 0x09, 0x09, 0x09, 0x06},
    {0x3E, 0x41, 0x51, 0x21, 0x5E},
    {0x7F, 0x09, 0x19, 0x29, 0x46},
    {0x46, 0x49, 0x49, 0x49, 0x31},
    {0x01, 0x01, 0x7F, 0x01, 0x01},
    {0x3F, 0x40, 0x40, 0x40, 0x3F},
    {0x1F, 0x20, 0x40, 0x20, 0x1F},
    {0x7F, 0x20, 0x18, 0x20, 0x7F},
    {0x63, 0x14, 0x08, 0x14, 0x63},
    {0x07, 0x08, 0x70, 0x08, 0x07},
    {0x61, 0x51, 0x49, 0x45, 0x43}
};

static const uint8_t GLYPH_SPACE[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
static const uint8_t GLYPH_DASH[5] = {0x08, 0x08, 0x08, 0x08, 0x08};
static const uint8_t GLYPH_DOT[5] = {0x00, 0x60, 0x60, 0x00, 0x00};
static const uint8_t GLYPH_COLON[5] = {0x00, 0x36, 0x36, 0x00, 0x00};
static const uint8_t GLYPH_PERCENT[5] = {0x63, 0x13, 0x08, 0x64, 0x63};
static const uint8_t GLYPH_SLASH[5] = {0x20, 0x10, 0x08, 0x04, 0x02};
static const uint8_t GLYPH_PLUS[5] = {0x08, 0x08, 0x3E, 0x08, 0x08};
static const uint8_t GLYPH_UNKNOWN[5] = {0x7F, 0x41, 0x5D, 0x41, 0x7F};

static const uint8_t *glyph_for_char(char ch)
{
  if ((ch >= 'a') && (ch <= 'z')) {
    ch = (char)(ch - ('a' - 'A'));
  }
  if ((ch >= '0') && (ch <= '9')) {
    return DIGIT_GLYPHS[(uint8_t)(ch - '0')];
  }
  if ((ch >= 'A') && (ch <= 'Z')) {
    return LETTER_GLYPHS[(uint8_t)(ch - 'A')];
  }

  switch (ch) {
  case ' ':
    return GLYPH_SPACE;
  case '-':
    return GLYPH_DASH;
  case '.':
    return GLYPH_DOT;
  case ':':
    return GLYPH_COLON;
  case '%':
    return GLYPH_PERCENT;
  case '/':
    return GLYPH_SLASH;
  case '+':
    return GLYPH_PLUS;
  default:
    return GLYPH_UNKNOWN;
  }
}

static void draw_char(uint16_t x, uint16_t y, char ch, uint8_t scale, uint16_t color, uint16_t bg)
{
  const uint8_t *glyph = glyph_for_char(ch);
  const uint16_t cellW = (uint16_t)(6U * scale);
  const uint16_t cellH = (uint16_t)(8U * scale);

  LCD_1IN69_FillRect(x, y, cellW, cellH, bg);
  for (uint8_t col = 0U; col < 5U; ++col) {
    for (uint8_t row = 0U; row < 7U; ++row) {
      if ((glyph[col] & (uint8_t)(1U << row)) != 0U) {
        LCD_1IN69_FillRect((uint16_t)(x + (col * scale)),
                           (uint16_t)(y + (row * scale)),
                           scale,
                           scale,
                           color);
      }
    }
  }
}

static void draw_text(uint16_t x, uint16_t y, const char *text, uint8_t scale, uint16_t color, uint16_t bg)
{
  uint16_t cursorX = x;

  if (text == 0) {
    return;
  }

  while (*text != '\0') {
    draw_char(cursorX, y, *text, scale, color, bg);
    cursorX = (uint16_t)(cursorX + (6U * scale));
    text++;
  }
}

static void format_distance(char *buffer, size_t bufferSize, uint32_t distanceCm)
{
  if (distanceCm >= 100000UL) {
    const uint32_t kmX100 = (distanceCm + 500UL) / 1000UL;
    (void)snprintf(buffer, bufferSize, "%lu.%02luKM",
                   (unsigned long)(kmX100 / 100UL),
                   (unsigned long)(kmX100 % 100UL));
  } else {
    (void)snprintf(buffer, bufferSize, "%lu.%02luM",
                   (unsigned long)(distanceCm / 100UL),
                   (unsigned long)(distanceCm % 100UL));
  }
}

static void format_speed(char *buffer, size_t bufferSize, uint16_t speedCms)
{
  (void)snprintf(buffer, bufferSize, "%u.%02u",
                 (unsigned int)(speedCms / 100U),
                 (unsigned int)(speedCms % 100U));
}

static uint16_t mode_color(HealthUiMode mode)
{
  switch (mode) {
  case HEALTH_UI_MODE_WALK:
    return COLOR_WALK;
  case HEALTH_UI_MODE_RUN:
    return COLOR_RUN;
  case HEALTH_UI_MODE_ROPE:
    return COLOR_ROPE;
  default:
    return COLOR_TEXT;
  }
}

static void draw_box(uint16_t x,
                     uint16_t y,
                     uint16_t w,
                     uint16_t h,
                     const char *label,
                     const char *value,
                     uint8_t valueScale,
                     uint16_t accent)
{
  LCD_1IN69_FillRect(x, y, w, h, COLOR_PANEL);
  LCD_1IN69_FillRect(x, y, 3U, h, accent);
  draw_text((uint16_t)(x + 8U), (uint16_t)(y + 6U), label, 1U, COLOR_MUTED, COLOR_PANEL);
  draw_text((uint16_t)(x + 8U), (uint16_t)(y + 22U), value, valueScale, COLOR_TEXT, COLOR_PANEL);
}

static void draw_header(const HealthUiSnapshot *snapshot, HealthUiMode mode, uint16_t accent)
{
  char hrText[10];
  char spo2Text[10];

  LCD_1IN69_FillRect(0U, 0U, LCD_1IN69_WIDTH, 56U, COLOR_BG);
  LCD_1IN69_FillRect(0U, 0U, LCD_1IN69_WIDTH, 5U, accent);
  draw_text(10U, 12U, HealthUi_ModeName(mode), 2U, accent, COLOR_BG);

  if (snapshot->heartRateValid != 0U) {
    (void)snprintf(hrText, sizeof(hrText), "HR %3u", (unsigned int)snapshot->heartRate);
  } else {
    (void)snprintf(hrText, sizeof(hrText), "HR  --");
  }

  if (snapshot->spo2Valid != 0U) {
    (void)snprintf(spo2Text, sizeof(spo2Text), "O2 %3u%%", (unsigned int)snapshot->spo2);
  } else {
    (void)snprintf(spo2Text, sizeof(spo2Text), "O2  --");
  }

  draw_text(9U, 36U, hrText, 1U, COLOR_TEXT, COLOR_BG);
  draw_text(126U, 36U, spo2Text, 1U, COLOR_TEXT, COLOR_BG);
}

static void draw_status(const HealthUiSnapshot *snapshot)
{
  char ghText[12];
  char mpuText[12];
  const uint16_t ghColor = (snapshot->ghError != 0U) ? COLOR_ERR : ((snapshot->ghReady != 0U) ? COLOR_OK : COLOR_WARN);
  const uint16_t mpuColor = (snapshot->mpuError != 0U) ? COLOR_ERR : ((snapshot->mpuReady != 0U) ? COLOR_OK : COLOR_WARN);

  if (snapshot->ghError != 0U) {
    (void)snprintf(ghText, sizeof(ghText), "GH ERR");
  } else if (snapshot->ghReady != 0U) {
    (void)snprintf(ghText, sizeof(ghText), "GH OK");
  } else {
    (void)snprintf(ghText, sizeof(ghText), "GH WAIT");
  }

  if (snapshot->mpuError != 0U) {
    (void)snprintf(mpuText, sizeof(mpuText), "MPU ERR");
  } else if (snapshot->mpuReady != 0U) {
    (void)snprintf(mpuText, sizeof(mpuText), "MPU OK");
  } else {
    (void)snprintf(mpuText, sizeof(mpuText), "MPU WAIT");
  }

  LCD_1IN69_FillRect(0U, 250U, LCD_1IN69_WIDTH, 30U, COLOR_BG);
  draw_text(10U, 258U, ghText, 1U, ghColor, COLOR_BG);
  draw_text(126U, 258U, mpuText, 1U, mpuColor, COLOR_BG);
}

static void draw_walk_page(const HealthUiSnapshot *snapshot, uint16_t accent)
{
  char value[20];

  (void)snprintf(value, sizeof(value), "%lu", (unsigned long)snapshot->steps);
  draw_box(12U, 62U, 216U, 72U, "STEPS", value, 3U, accent);

  format_distance(value, sizeof(value), snapshot->distanceCm);
  draw_box(12U, 144U, 104U, 44U, "DIST", value, 1U, accent);

  format_speed(value, sizeof(value), snapshot->instantSpeedCms);
  draw_box(124U, 144U, 104U, 44U, "SPD M/S", value, 1U, accent);

  format_speed(value, sizeof(value), snapshot->averageSpeedCms);
  draw_box(12U, 196U, 104U, 44U, "AVG M/S", value, 1U, accent);

  (void)snprintf(value, sizeof(value), "%uCM", (unsigned int)snapshot->strideCm);
  draw_box(124U, 196U, 104U, 44U, "STRIDE", value, 1U, accent);
}

static void draw_run_page(const HealthUiSnapshot *snapshot, uint16_t accent)
{
  char value[20];

  format_distance(value, sizeof(value), snapshot->distanceCm);
  draw_box(12U, 62U, 216U, 72U, "DISTANCE", value, 3U, accent);

  (void)snprintf(value, sizeof(value), "%lu", (unsigned long)snapshot->steps);
  draw_box(12U, 144U, 104U, 44U, "STEPS", value, 1U, accent);

  format_speed(value, sizeof(value), snapshot->instantSpeedCms);
  draw_box(124U, 144U, 104U, 44U, "SPD M/S", value, 1U, accent);

  format_speed(value, sizeof(value), snapshot->averageSpeedCms);
  draw_box(12U, 196U, 104U, 44U, "AVG M/S", value, 1U, accent);

  (void)snprintf(value, sizeof(value), "%u", (unsigned int)snapshot->cadenceSpm);
  draw_box(124U, 196U, 104U, 44U, "CAD SPM", value, 1U, accent);
}

static void draw_rope_page(const HealthUiSnapshot *snapshot, uint16_t accent)
{
  char value[28];

  (void)snprintf(value, sizeof(value), "%lu", (unsigned long)snapshot->ropeCount);
  draw_box(12U, 62U, 216U, 72U, "COUNT", value, 3U, accent);

  (void)snprintf(value, sizeof(value), "%u.%02u",
                 (unsigned int)(snapshot->activityX100 / 100U),
                 (unsigned int)(snapshot->activityX100 % 100U));
  draw_box(12U, 144U, 104U, 44U, "ACT", value, 1U, accent);

  (void)snprintf(value, sizeof(value), "%u", (unsigned int)snapshot->cadenceSpm);
  draw_box(124U, 144U, 104U, 44U, "CAD SPM", value, 1U, accent);

  (void)snprintf(value, sizeof(value), "%d %d %d",
                 (int)snapshot->accelMg[0],
                 (int)snapshot->accelMg[1],
                 (int)snapshot->accelMg[2]);
  draw_box(12U, 196U, 104U, 44U, "ACC MG", value, 1U, accent);

  (void)snprintf(value, sizeof(value), "%d %d %d",
                 (int)(snapshot->gyroMdps[0] / 1000),
                 (int)(snapshot->gyroMdps[1] / 1000),
                 (int)(snapshot->gyroMdps[2] / 1000));
  draw_box(124U, 196U, 104U, 44U, "GYR D/S", value, 1U, accent);
}

void HealthLcd_Init(void)
{
  s_initialized = 1U;
  s_lastMode = 0xFFU;
  s_lastRefreshTick = 0U;
  LCD_1IN69_Clear(COLOR_BG);
}

void HealthLcd_Update(const HealthUiSnapshot *snapshot, uint32_t nowMs)
{
  const HealthUiMode mode = (HealthUiMode)((nowMs / HEALTH_LCD_PAGE_PERIOD_MS) % HEALTH_UI_MODE_COUNT);
  const uint16_t accent = mode_color(mode);
  uint8_t pageChanged;

  if (snapshot == 0) {
    return;
  }

  if (s_initialized == 0U) {
    HealthLcd_Init();
  }

  pageChanged = (s_lastMode != (uint8_t)mode) ? 1U : 0U;
  if ((pageChanged == 0U) && ((nowMs - s_lastRefreshTick) < HEALTH_LCD_REFRESH_PERIOD_MS)) {
    return;
  }

  s_lastMode = (uint8_t)mode;
  s_lastRefreshTick = nowMs;

  draw_header(snapshot, mode, accent);

  switch (mode) {
  case HEALTH_UI_MODE_WALK:
    draw_walk_page(snapshot, accent);
    break;
  case HEALTH_UI_MODE_RUN:
    draw_run_page(snapshot, accent);
    break;
  case HEALTH_UI_MODE_ROPE:
  default:
    draw_rope_page(snapshot, accent);
    break;
  }

  draw_status(snapshot);
}
