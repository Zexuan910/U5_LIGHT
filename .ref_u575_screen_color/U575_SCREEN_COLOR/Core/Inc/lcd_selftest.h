#ifndef LCD_SELFTEST_H
#define LCD_SELFTEST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_COLOR_BLACK 0x0000U
#define LCD_COLOR_BLUE  0x001FU
#define LCD_COLOR_GREEN 0x07E0U
#define LCD_COLOR_RED   0xF800U
#define LCD_COLOR_WHITE 0xFFFFU

void LCD_SelfTest_Run(uint16_t color);

#ifdef __cplusplus
}
#endif

#endif
