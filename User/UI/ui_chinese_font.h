#ifndef UI_CHINESE_FONT_H
#define UI_CHINESE_FONT_H

#include "DEV_Config.h"

UWORD UIChinese_TextWidth(const char* text, UWORD scale);
void UIChinese_DrawText(UWORD x, UWORD y, const char* text, UWORD color, UWORD scale);
void UIChinese_DrawCenteredInRect(UWORD x, UWORD y, UWORD width, const char* text, UWORD color, UWORD scale);

#endif
