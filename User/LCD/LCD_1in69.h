/*****************************************************************************
* | File        :   LCD_1IN69.h
* | Author      :   Waveshare team
* | Function    :   Hardware underlying interface
*
******************************************************************************/
#ifndef __LCD_1IN69_H
#define __LCD_1IN69_H

#include "DEV_Config.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define LCD_1IN69_HEIGHT 280
#define LCD_1IN69_WIDTH  240

#define HORIZONTAL 0
#define VERTICAL   1

#define LCD_1IN69_INIT_SEQUENCE_LEGACY   0
#define LCD_1IN69_INIT_SEQUENCE_P169H002 1

#ifndef LCD_1IN69_INIT_SEQUENCE
#define LCD_1IN69_INIT_SEQUENCE LCD_1IN69_INIT_SEQUENCE_P169H002
#endif

#ifndef LCD_1IN69_MADCTL_VERTICAL
#define LCD_1IN69_MADCTL_VERTICAL 0x00
#endif

#ifndef LCD_1IN69_MADCTL_HORIZONTAL
#define LCD_1IN69_MADCTL_HORIZONTAL 0x70
#endif

#ifndef LCD_1IN69_COLMOD
#define LCD_1IN69_COLMOD 0x55
#endif

#define LCD_COLOR_BLACK 0x0000U
#define LCD_COLOR_BLUE  0x001FU
#define LCD_COLOR_GREEN 0x07E0U
#define LCD_COLOR_RED   0xF800U
#define LCD_COLOR_WHITE 0xFFFFU

#define LCD_1IN69_CS_0  DEV_Digital_Write(DEV_CS_PIN, 0)
#define LCD_1IN69_CS_1  DEV_Digital_Write(DEV_CS_PIN, 1)

#define LCD_1IN69_RST_0 DEV_Digital_Write(DEV_RST_PIN, 0)
#define LCD_1IN69_RST_1 DEV_Digital_Write(DEV_RST_PIN, 1)

#define LCD_1IN69_DC_0  DEV_Digital_Write(DEV_DC_PIN, 0)
#define LCD_1IN69_DC_1  DEV_Digital_Write(DEV_DC_PIN, 1)

typedef struct {
    UWORD WIDTH;
    UWORD HEIGHT;
    UBYTE SCAN_DIR;
} LCD_1IN69_ATTRIBUTES;

extern LCD_1IN69_ATTRIBUTES LCD_1IN69;

void LCD_1IN69_Init(UBYTE Scan_dir);
void LCD_1IN69_Clear(UWORD Color);
void LCD_1IN69_FillScreen(UWORD Color);
void LCD_1IN69_FillRect(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color);
void LCD_1IN69_FillRect_FastStatic(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD Color);
void LCD_1IN69_DrawColorBars(void);
void LCD_1IN69_Display(UWORD *Image);
void LCD_1IN69_DisplayWindows(UWORD Xstart, UWORD Ystart, UWORD Xend, UWORD Yend, UWORD *Image);
void LCD_1IN69_DrawRGB565Bytes(UWORD Xstart, UWORD Ystart, UWORD Width, UWORD Height, const UBYTE *Data);
void LCD_1IN69_DrawPoint(UWORD X, UWORD Y, UWORD Color);
void LCD_1IN69_SetBackLight(UWORD Value);

#endif
