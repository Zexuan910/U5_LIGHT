#ifndef LCD_SELFTEST_H
#define LCD_SELFTEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef LCD_COLOR_BLACK
#define LCD_COLOR_BLACK 0x0000U
#endif
#ifndef LCD_COLOR_BLUE
#define LCD_COLOR_BLUE  0x001FU
#endif
#ifndef LCD_COLOR_GREEN
#define LCD_COLOR_GREEN 0x07E0U
#endif
#ifndef LCD_COLOR_RED
#define LCD_COLOR_RED   0xF800U
#endif
#ifndef LCD_COLOR_WHITE
#define LCD_COLOR_WHITE 0xFFFFU
#endif

void LCD_SelfTest_Run(uint16_t color);

#ifdef __cplusplus
}
#endif

#endif
